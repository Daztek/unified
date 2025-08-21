#include "nwnx.hpp"

#include "API/CVirtualMachine.hpp"
#include "API/CNWSVirtualMachineCommands.hpp"
#include <regex>


namespace Optimizations {

using namespace NWNXLib;
using namespace NWNXLib::API;

struct CachedRegExp
{
    std::regex regex;
    std::string pattern;
    std::regex_constants::syntax_option_type flags;

    CachedRegExp(const std::string& p, const std::regex_constants::syntax_option_type f) : regex(p, f), pattern(p), flags(f) {}
};

static std::unordered_map<size_t, CachedRegExp> s_CachedRegExp;
static size_t GetRegExpKey(const std::string& pattern, std::regex_constants::syntax_option_type flags)
{
    std::hash<std::string> stringHasher;
    std::hash<std::regex_constants::syntax_option_type> flagsHasher;
    const size_t patternHash = stringHasher(pattern);
    const size_t flagsHash = flagsHasher(flags);
    return patternHash ^ (flagsHash << 1);
}

static const std::regex& GetCachedRegExp(const std::string& pattern,  std::regex_constants::syntax_option_type flags)
{
    size_t key = GetRegExpKey(pattern, flags);
    if (auto it = s_CachedRegExp.find(key); it != s_CachedRegExp.end())
    {
        if (it->second.pattern == pattern && it->second.flags == flags)
            return it->second.regex;
        s_CachedRegExp.erase(it);
    }
    auto result = s_CachedRegExp.emplace(key, CachedRegExp(pattern, flags));
    return result.first->second.regex;
}

static std::regex_constants::syntax_option_type NWScriptToSyntaxRegexConst(const int32_t nFlags)
{
    auto rFlags = std::regex_constants::ECMAScript;
    if (constexpr int REGEXP_ECMASCRIPT = 0;    nFlags & REGEXP_ECMASCRIPT) rFlags |= std::regex_constants::ECMAScript;
    else if (constexpr int REGEXP_BASIC = 1;    nFlags & REGEXP_BASIC)      rFlags |= std::regex_constants::basic;
    else if (constexpr int REGEXP_EXTENDED = 2; nFlags & REGEXP_EXTENDED)   rFlags |= std::regex_constants::extended;
    else if (constexpr int REGEXP_AWK = 4;      nFlags & REGEXP_AWK)        rFlags |= std::regex_constants::awk;
    else if (constexpr int REGEXP_GREP = 8;     nFlags & REGEXP_GREP)       rFlags |= std::regex_constants::grep;
    else if (constexpr int REGEXP_EGREP = 16;   nFlags & REGEXP_EGREP)      rFlags |= std::regex_constants::egrep;
    if (constexpr int REGEXP_ICASE = 32;        nFlags & REGEXP_ICASE)      rFlags |= std::regex_constants::icase;
    if (constexpr int REGEXP_NOSUBS = 64;       nFlags & REGEXP_NOSUBS)     rFlags |= std::regex_constants::nosubs;
    return rFlags;
}

static std::regex_constants::match_flag_type NWScriptToMatchRegexConst(const int32_t nFlags)
{
    auto rFlags = std::regex_constants::match_default;
    if (constexpr int REGEXP_MATCH_NOT_BOL = 1;         nFlags & REGEXP_MATCH_NOT_BOL)     rFlags |= std::regex_constants::match_not_bol;
    if (constexpr int REGEXP_MATCH_NOT_EOL = 2;         nFlags & REGEXP_MATCH_NOT_EOL)     rFlags |= std::regex_constants::match_not_eol;
    if (constexpr int REGEXP_MATCH_NOT_BOW = 4;         nFlags & REGEXP_MATCH_NOT_BOW)     rFlags |= std::regex_constants::match_not_bow;
    if (constexpr int REGEXP_MATCH_NOT_EOW  = 8;        nFlags & REGEXP_MATCH_NOT_EOW)     rFlags |= std::regex_constants::match_not_eow;
    if (constexpr int REGEXP_MATCH_ANY = 16;            nFlags & REGEXP_MATCH_ANY)         rFlags |= std::regex_constants::match_any;
    if (constexpr int REGEXP_MATCH_NOT_NULL = 32;       nFlags & REGEXP_MATCH_NOT_NULL)    rFlags |= std::regex_constants::match_not_null;
    if (constexpr int REGEXP_MATCH_CONTINUOUS  = 64;    nFlags & REGEXP_MATCH_CONTINUOUS)  rFlags |= std::regex_constants::match_continuous;
    if (constexpr int REGEXP_MATCH_PREV_AVAIL = 128;    nFlags & REGEXP_MATCH_PREV_AVAIL)  rFlags |= std::regex_constants::match_prev_avail;
    if (constexpr int REGEXP_FORMAT_SED = 256;          nFlags & REGEXP_FORMAT_SED)        rFlags |= std::regex_constants::format_sed;
    if (constexpr int REGEXP_FORMAT_NO_COPY = 512;      nFlags & REGEXP_FORMAT_NO_COPY)    rFlags |= std::regex_constants::format_no_copy;
    if (constexpr int REGEXP_FORMAT_FIRST_ONLY = 1024;  nFlags & REGEXP_FORMAT_FIRST_ONLY) rFlags |= std::regex_constants::format_first_only;
    return rFlags;
}

void CacheRegExp() __attribute__((constructor));
void CacheRegExp()
{
    if (Config::Get<bool>("CACHE_REGEXP", false))
    {
        LOG_INFO("Caching RegExp");

        static Hooks::Hook s_ExecuteCommandRegExp = Hooks::HookFunction(&CNWSVirtualMachineCommands::ExecuteCommandRegExp,
        +[](CNWSVirtualMachineCommands *, int32_t nCommandId, int32_t) -> int32_t
        {
            auto *pVM = Globals::VirtualMachine();
            CExoString sRegExp;
            CExoString sValue;
            int32_t nSyntaxFlags;
            int32_t nMatchFlags;
            JsonEngineStructure jResult;

            if (!pVM->StackPopString(&sRegExp) ||
                !pVM->StackPopString(&sValue) ||
                !pVM->StackPopInteger(&nSyntaxFlags) ||
                !pVM->StackPopInteger(&nMatchFlags))
                return Constants::VMError::StackUnderflow;

            const auto rxFlags = NWScriptToSyntaxRegexConst(nSyntaxFlags);
            const auto matchFlags = NWScriptToMatchRegexConst(nMatchFlags);

            try
            {
                std::string value = sValue;
                std::regex cachedRegExp = GetCachedRegExp(sRegExp.CStr(), rxFlags);
                json array = json::array();

                if (nCommandId == Constants::VMCommand::RegExpMatch)
                {
                    if (std::smatch match; std::regex_search(value, match, cachedRegExp, matchFlags))
                    {
                        for (size_t i = 0; i < match.size(); i++)
                            array.push_back(match[i]);
                    }
                }
                else if (nCommandId == Constants::VMCommand::RegExpIterate)
                {
                    auto begin = std::sregex_iterator(value.begin(), value.end(), cachedRegExp, matchFlags);
                    auto end = std::sregex_iterator();

                    for (std::sregex_iterator i = begin; i != end; ++i)
                    {
                        json matches = json::array();
                        for (size_t index = 0; index < i->size(); index++)
                        {
                            matches.emplace_back(i->str(index));
                        }
                        array.push_back(matches);
                    }
                }
                jResult.m_shared->m_json = array;
            }
            catch (const std::exception& err)
            {
                jResult = JsonEngineStructure({}, err.what());
            }

            if (!pVM->StackPushEngineStructure(Constants::VMStructure::Json, &jResult))
                return Constants::VMError::StackOverflow;

            return 0;
        }, Hooks::Order::Final);

        static Hooks::Hook s_ExecuteCommandRegExpReplace = Hooks::HookFunction(&CNWSVirtualMachineCommands::ExecuteCommandRegExpReplace,
        +[](CNWSVirtualMachineCommands*, int32_t, int32_t) -> int32_t
        {
            auto *pVM = Globals::VirtualMachine();
            CExoString sRegExp;
            CExoString sValue;
            CExoString sReplacement;
            int32_t nSyntaxFlags = 0;
            int32_t nMatchFlags = 0;
            CExoString sResult = sValue;

            if (!pVM->StackPopString(&sRegExp) ||
                !pVM->StackPopString(&sValue) ||
                !pVM->StackPopString(&sReplacement) ||
                !pVM->StackPopInteger(&nSyntaxFlags) ||
                !pVM->StackPopInteger(&nMatchFlags))
                return Constants::VMError::StackUnderflow;

            const auto rxFlags = NWScriptToSyntaxRegexConst(nSyntaxFlags);
            const auto matchFlags = NWScriptToMatchRegexConst(nMatchFlags);

            try
            {
                const std::string text = sValue;
                const std::regex cachedRegExp = GetCachedRegExp(sRegExp.CStr(), rxFlags);
                sResult = std::regex_replace(text, cachedRegExp, sReplacement.CStr(), matchFlags);
            }
            catch (const std::exception& err)
            {
                LOG_ERROR("RegExpReplace(): Error in regexp '%s': '%s'\n", sRegExp.CStr(), err.what());
            }

            if (!pVM->StackPushString(sResult))
                return Constants::VMError::StackOverflow;

            return 0;
        }, Hooks::Order::Final);
    }
}

}
