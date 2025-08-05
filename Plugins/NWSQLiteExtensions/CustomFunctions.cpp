#include "nwnx.hpp"
#include "API/Database.hpp"
#include "API/CGameObject.hpp"
#include "API/CNWSModule.hpp"
#include "API/CScriptCompiler.hpp"
#include "API/CVirtualMachine.hpp"

using namespace NWNXLib;
using namespace NWNXLib::API;

enum ReturnType
{
    Int = 1,
    Float = 2,
    String = 3,
    Object = 4,
};

struct CustomFunction
{
    std::string scriptChunk;
    int32_t functionHash;
    int32_t argCount;
    ReturnType returnType;
    bool deterministic;
};

using ArgValue = std::variant<int32_t, double, std::string, ObjectID>;
struct CacheKey
{
    int32_t functionHash;
    std::vector<ArgValue> args;

    bool operator==(const CacheKey& other) const
    {
        return functionHash == other.functionHash && args == other.args;
    }
};

struct CacheKeyHash
{
    std::size_t operator()(const CacheKey& key) const
    {
        constexpr std::size_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
        constexpr std::size_t FNV_PRIME = 1099511628211ULL;
        std::size_t hash = FNV_OFFSET_BASIS;
        std::size_t funcHash = std::hash<int32_t>{}(key.functionHash);
        hash ^= funcHash;
        hash *= FNV_PRIME;

        for (const auto& arg : key.args)
        {
            std::size_t argHash = std::visit([](auto&& val)
            {
                return std::hash<std::decay_t<decltype(val)>>{}(val);
            }, arg);

            hash ^= argHash;
            hash *= FNV_PRIME;
        }

        return hash;
    }
};

std::unordered_map<std::string, CustomFunction> s_customFunctions;
std::unordered_map<CacheKey, ArgValue, CacheKeyHash> s_resultCache;

template<typename... Args>
void SetSqliteError(sqlite3_context* context, const char* format, Args... args)
{
    char buffer[512];
    snprintf(buffer, sizeof(buffer), format, args...);
    sqlite3_result_error(context, buffer, -1);
}

static ObjectID GetObjectIDFromSqliteValue(sqlite3_value *arg)
{
    ObjectID oid = Constants::OBJECT_INVALID;
    if (sqlite3_value_type(arg) == SQLITE_INTEGER)
        oid = sqlite3_value_int(arg);
    else if (sqlite3_value_type(arg) == SQLITE_TEXT)
        oid = Utils::StringToObjectID(reinterpret_cast<const char*>(sqlite3_value_text(arg)));
    return oid;
}

static void CustomFunctionCallback(sqlite3_context* context, int argc, sqlite3_value** argv)
{
    auto *pCustomFunction = static_cast<CustomFunction*>(sqlite3_user_data(context));
    if (!pCustomFunction)
    {
        sqlite3_result_error(context, "Internal error: Invalid CustomFunction data pointer", -1);
        return;
    }

    const int expectedArgCount = pCustomFunction->argCount + 1;
    if (argc != expectedArgCount)
    {
        SetSqliteError(context, "Argument count mismatch: expected %d, got %d", expectedArgCount, argc);
        return;
    }

    std::vector<ArgValue> args;
    for (int i = 0; i < argc; ++i)
    {
        int argType = sqlite3_value_type(argv[i]);
        switch (argType)
        {
            case SQLITE_INTEGER:
                args.emplace_back(sqlite3_value_int(argv[i]));
                break;
            case SQLITE_TEXT:
                args.emplace_back(std::string(reinterpret_cast<const char*>(sqlite3_value_text(argv[i]))));
                break;
            case SQLITE_FLOAT:
                args.emplace_back(sqlite3_value_double(argv[i]));
                break;
            default:
                SetSqliteError(context, "Unsupported argument type %d at position %d", argType, i + 1);
                return;
        }
    }

    std::optional<CacheKey> cacheKey;
    if (pCustomFunction->deterministic)
    {
        cacheKey = CacheKey{pCustomFunction->functionHash, args};
        if (const auto it = s_resultCache.find(*cacheKey); it != s_resultCache.end())
        {
            std::visit([&](const auto& val)
            {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, int32_t>)
                {
                    sqlite3_result_int(context, val);
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    sqlite3_result_double(context, val);
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    sqlite3_result_text(context, val.c_str(), -1, SQLITE_TRANSIENT);
                }
                else if constexpr (std::is_same_v<T, ObjectID>)
                {
                    sqlite3_result_int(context, val);
                }
            }, it->second);

            return;
        }
    }

    auto *pScriptVars = Utils::GetScriptVarTable(Utils::GetModule());
    for (int i = 0; i < argc; ++i)
    {
        CExoString varName = CExoString::F("CF_ARG_%i", i);
        std::visit([&](const auto& val)
        {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, int32_t>)
            {
                pScriptVars->SetInt(varName, val);
                pScriptVars->SetObject(varName, val);
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                auto exoVal = CExoString(val.c_str());
                pScriptVars->SetString(varName, exoVal);
            }
            else if constexpr (std::is_same_v<T, double>)
            {
                pScriptVars->SetFloat(varName, val);
            }
        }, args[i]);
    }

    const ObjectID oidSelf = GetObjectIDFromSqliteValue(argv[0]);
    auto *pVM = Globals::VirtualMachine();

    if (pVM->RunScriptChunk(pCustomFunction->scriptChunk.c_str(), oidSelf, oidSelf != Constants::OBJECT_INVALID, false))
    {
        SetSqliteError(context, "Script execution failed for function hash %d with error: %s",
            pCustomFunction->functionHash, pVM->m_pJitCompiler->m_sCapturedError.CStr());
        return;
    }

    auto storeInCache = [&](const auto& value)
    {
        if (cacheKey.has_value())
        {
            s_resultCache[std::move(*cacheKey)] = value;
        }
    };

    if (pVM->GetRunScriptReturnValueType() == Constants::VMAuxCodeType::Integer && pCustomFunction->returnType == ReturnType::Int)
    {
        int32_t retVal;
        if (pVM->GetRunScriptReturnValueInteger(&retVal))
        {
            storeInCache(retVal);
            sqlite3_result_int(context, retVal);
            return;
        }

        SetSqliteError(context, "Failed to retrieve integer return value from script (hash: %d)", pCustomFunction->functionHash);
        return;
    }

    if (pVM->GetRunScriptReturnValueType() == Constants::VMAuxCodeType::Float && pCustomFunction->returnType == ReturnType::Float)
    {
        float retVal;
        if (pVM->GetRunScriptReturnValueFloat(&retVal))
        {
            storeInCache(retVal);
            sqlite3_result_double(context, retVal);
            return;
        }

        SetSqliteError(context, "Failed to retrieve float return value from script (hash: %d)", pCustomFunction->functionHash);
        return;
    }

    if (pVM->GetRunScriptReturnValueType() == Constants::VMAuxCodeType::String && pCustomFunction->returnType == ReturnType::String)
    {
        CExoString retVal;
        if (pVM->GetRunScriptReturnValueString(&retVal))
        {
            storeInCache(retVal);
            sqlite3_result_text(context, retVal.CStr(), -1, SQLITE_TRANSIENT);
            return;
        }

        SetSqliteError(context, "Failed to retrieve string return value from script (hash: %d)", pCustomFunction->functionHash);
        return;
    }

    if (pVM->GetRunScriptReturnValueType() == Constants::VMAuxCodeType::Object && pCustomFunction->returnType == ReturnType::Object)
    {
        ObjectID retVal;
        if (pVM->GetRunScriptReturnValueObject(&retVal))
        {
            storeInCache(retVal);
            sqlite3_result_int(context, retVal);
            return;
        }

        SetSqliteError(context, "Failed to retrieve object return value from script (hash: %d)", pCustomFunction->functionHash);
        return;
    }

    SetSqliteError(context, "Return type mismatch? (function hash: %d)", pCustomFunction->functionHash);
}

NWNX_EXPORT ArgumentStack RegisterCustomFunction(ArgumentStack&& args)
{
    const auto name = args.extract<std::string>();
    const auto scriptChunk = args.extract<std::string>();
    const auto argCount = args.extract<int32_t>();
    const auto returnType = static_cast<ReturnType>(args.extract<int32_t>());
    const bool deterministic = !!args.extract<int32_t>();

    CustomFunction function;
    function.scriptChunk = scriptChunk;
    function.functionHash = CExoString(scriptChunk).GetHash();
    function.argCount = argCount;
    function.returnType = returnType;
    function.deterministic = deterministic;

    s_customFunctions[name] = function;

    int flags = SQLITE_UTF8 | SQLITE_DIRECTONLY;
    if (function.deterministic)
        flags |= SQLITE_DETERMINISTIC;

    auto *pModule = Utils::GetModule();
    int rc = sqlite3_create_function_v2(
        pModule->m_sqlite3->connection().get(),
        name.c_str(),
        argCount + 1,
        flags,
        &s_customFunctions[name],
        CustomFunctionCallback,
        nullptr,
        nullptr,
        nullptr);

    return rc == SQLITE_OK;
}

NWNX_EXPORT ArgumentStack ClearFunctionResultCache(ArgumentStack&&)
{
    s_resultCache.clear();
    return {};
}
