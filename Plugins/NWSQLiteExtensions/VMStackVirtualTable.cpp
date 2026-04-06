#include "nwnx.hpp"
#include "API/CVirtualMachine.hpp"
#include "API/Database.hpp"


using namespace NWNXLib;
using namespace NWNXLib::API;

struct VMStackRow
{
    int32_t recursionLevel;
    int32_t depth;
    std::string function;
    std::string name;
    Constants::VMAuxCodeType::TYPE type;
    int32_t stackLocation;
};

static std::vector<VMStackRow> s_VMStackRows;
static bool s_VMStackVirtualTableEnabled = false;
constexpr const char* MODULE_DATABASE_LABEL = "Module()";
static int SetupVMStackVirtualTableModule(sqlite3 *db);

void VMStackVirtualTable() __attribute__((constructor));
void VMStackVirtualTable()
{
    if ((s_VMStackVirtualTableEnabled = Config::Get<bool>("ENABLE_VMSTACK_VIRTUAL_TABLE_MODULE", false)))
    {
        LOG_INFO("Enabling the VMStack Virtual Table Module for the module database.");
        static Hooks::Hook s_DatabaseSetupHook = Hooks::HookFunction(&NWSQLite::Database::Setup,
        +[](NWSQLite::Database *pThis) -> void
        {
            s_DatabaseSetupHook->CallOriginal<void>(pThis);

            if (strcmp(pThis->m_label.c_str(), MODULE_DATABASE_LABEL) == 0)
            {
                if (SetupVMStackVirtualTableModule(pThis->connection().get()) != SQLITE_OK)
                {
                    LOG_ERROR("Failed to setup the VMStack Virtual Table Module.");
                    s_VMStackVirtualTableEnabled = false;
                }
            }
        }, Hooks::Order::Early);
    }
}

static void PopulateVMStackRows()
{
    s_VMStackRows.clear();
    s_VMStackRows.reserve(25);

    for (int32_t recursionLevel = Globals::VirtualMachine()->m_nRecursionLevel; recursionLevel >= 0; recursionLevel--)
    {
        for (int32_t depth = 0; depth < 128; depth++)
        {
            auto stackFrame = Globals::VirtualMachine()->GetStackFrame(depth, recursionLevel);

            if (!stackFrame.IsValid() || stackFrame.functionName == "#loader" || stackFrame.functionName == "#globals")
                break;

            for (const auto&[varName, varData] : stackFrame.stackVariables)
            {
                if (varData.auxType == Constants::VMAuxCodeType::Integer ||
                    varData.auxType == Constants::VMAuxCodeType::Float ||
                    varData.auxType == Constants::VMAuxCodeType::String ||
                    varData.auxType == Constants::VMAuxCodeType::Object ||
                    varData.auxType == Constants::VMAuxCodeType::EngSt7)
                {
                    s_VMStackRows.emplace_back(VMStackRow{recursionLevel, depth, stackFrame.functionName, varName, varData.auxType, varData.stackLocation});
                }
            }
        }
    }
}

typedef struct vms_tab vms_tab;
struct vms_tab
{
    sqlite3_vtab base;
};

typedef struct vms_cursor vms_cursor;
struct vms_cursor
{
    sqlite3_vtab_cursor base;
    uint32_t currentRow;
};

namespace VMStackColumns
{
    enum TYPE
    {
        RecursionLevel = 0,
        Depth,
        Function,
        Name,
        Type,
        StackLocation,
        Value,
        Num, // Keep Last
    };
    constexpr int32_t MAX = 6;
    constexpr int32_t NUM = MAX + 1;
    static_assert(MAX == Value);
    static_assert(NUM == Num);

    constexpr const char* ToColumnWithType(const unsigned value)
    {
        constexpr const char* TYPE_STRINGS[] =
        {
            "recursion_level INTEGER",
            "depth INTEGER",
            "function TEXT",
            "name TEXT",
            "type INTEGER",
            "stack_location INTEGER",
            "value", // No affinity
        };
        static_assert(std::size(TYPE_STRINGS) == NUM);
        return (value > MAX) ? "(invalid)" : TYPE_STRINGS[value];
    }

    static CExoString GetSchema()
    {
        CExoString sSchema = "CREATE TABLE x(";
        for (int i = 0; i < NUM; i++)
        {
            sSchema.Format("%s%s%s", sSchema.CStr(), i ? ", " : "", ToColumnWithType(i));
        }
        sSchema.Format("%s);", sSchema.CStr());
        return sSchema;
    }
}

static int vmsConnect(sqlite3 *db, void *pAux, int argc, const char* const *argv, sqlite3_vtab **ppVtab, char **pzErr)
{
    (void)pAux; (void)argc; (void)argv; (void)pzErr;

    LOG_DEBUG("Creating Virtual VMStack Table");

    const CExoString sSchema = VMStackColumns::GetSchema();
    LOG_DEBUG("  Schema: %s", sSchema);

    vms_tab *pNewVtab = nullptr;
    const int32_t ret = sqlite3_declare_vtab(db, sSchema.CStr());
    if (ret == SQLITE_OK)
    {
        pNewVtab = static_cast<vms_tab*>(sqlite3_malloc(sizeof(vms_tab)));
        if (!pNewVtab)
            return SQLITE_NOMEM;

        memset(pNewVtab, 0, sizeof(vms_tab));
        //sqlite3_vtab_config(db, SQLITE_VTAB_DIRECTONLY);
    }

    *ppVtab = reinterpret_cast<sqlite3_vtab*>(pNewVtab);
    return ret;
}

static int vmsDisconnect(sqlite3_vtab *pVtab)
{
    auto *pVmsVtab = reinterpret_cast<vms_tab*>(pVtab);
    sqlite3_free(pVmsVtab);
    return SQLITE_OK;
}

static int vmsOpen(sqlite3_vtab*, sqlite3_vtab_cursor **ppCursor)
{
    auto *pCursor = static_cast<vms_cursor*>(sqlite3_malloc(sizeof(vms_cursor)));
    if(!pCursor) return SQLITE_NOMEM;
    memset(pCursor, 0, sizeof(vms_cursor));
    *ppCursor = &pCursor->base;
    PopulateVMStackRows();
    return SQLITE_OK;
}

static int vmsClose(sqlite3_vtab_cursor *cur)
{
    auto *pCursor = reinterpret_cast<vms_cursor*>(cur);
    sqlite3_free(pCursor);
    return SQLITE_OK;
}

static int vmsNext(sqlite3_vtab_cursor *cur)
{
    auto *pCursor = reinterpret_cast<vms_cursor*>(cur);
    pCursor->currentRow++;
    return SQLITE_OK;
}

static int vmsColumn(sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int column)
{
    auto *pCursor = reinterpret_cast<vms_cursor*>(cur);

    if (sqlite3_vtab_nochange(ctx))
        return SQLITE_OK;

    auto *pVM = Globals::VirtualMachine();
    auto &[recursionLevel, depth, function, name, type, stackLocation] = s_VMStackRows[pCursor->currentRow];

    switch (column)
    {
        case VMStackColumns::RecursionLevel:
            sqlite3_result_int(ctx, recursionLevel);
        break;

        case VMStackColumns::Depth:
            sqlite3_result_int(ctx, depth);
        break;

        case VMStackColumns::Function:
            sqlite3_result_text(ctx, function.c_str(), -1, SQLITE_TRANSIENT);
        break;

        case VMStackColumns::Name:
            sqlite3_result_text(ctx, name.c_str(), -1, SQLITE_TRANSIENT);
        break;

        case VMStackColumns::Type:
            sqlite3_result_int(ctx, type);
        break;

        case VMStackColumns::StackLocation:
            sqlite3_result_int(ctx, stackLocation);
        break;

        case VMStackColumns::Value:
        {
            switch (type)
            {
                case Constants::VMAuxCodeType::Integer:
                    sqlite3_result_int(ctx, pVM->GetStackIntegerValue(stackLocation));
                break;

                case Constants::VMAuxCodeType::Float:
                    sqlite3_result_double(ctx, pVM->GetStackFloatValue(stackLocation));
                break;

                case Constants::VMAuxCodeType::String:
                    sqlite3_result_text(ctx, pVM->GetStackStringValue(stackLocation).CStr(), -1, SQLITE_TRANSIENT);
                break;

                case Constants::VMAuxCodeType::Object:
                    sqlite3_result_int(ctx, pVM->GetStackObjectValue(stackLocation));
                break;

                case Constants::VMAuxCodeType::EngSt7:
                    sqlite3_result_text(ctx, pVM->GetStackJsonValue(stackLocation).m_shared->m_json.dump().c_str(), -1, SQLITE_TRANSIENT);
                break;

                default:
                    LOG_DEBUG("How did this happen?");
                    break;
            }
            break;
        }

        default:
            LOG_DEBUG("Forgot to implement a column?");
        break;
    }


    return SQLITE_OK;
}

static int vmsRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid)
{
    auto *pCursor = reinterpret_cast<vms_cursor*>(cur);
    *pRowid = pCursor->currentRow;
    return SQLITE_OK;
}

static int vmsEOF(sqlite3_vtab_cursor *cur)
{
    auto *pCursor = reinterpret_cast<vms_cursor*>(cur);
    return pCursor->currentRow >= s_VMStackRows.size();
}

static int vmsFilter(sqlite3_vtab_cursor *cur, int idxNum, const char *idxStr, int argc, sqlite3_value **argv)
{
    (void)idxNum; (void)idxStr; (void)argc; (void)argv;
    auto *pCursor = reinterpret_cast<vms_cursor*>(cur);
    pCursor->currentRow = 0;
    return SQLITE_OK;
}

static int vmsBestIndex(sqlite3_vtab*, sqlite3_index_info*)
{
    return SQLITE_OK;
}

static int vmsUpdate(sqlite3_vtab*, int argc, sqlite3_value **argv, sqlite_int64 *pRowid)
{
    if (argc > 1 && sqlite3_value_type(argv[0]) == SQLITE_INTEGER)
    {
        const sqlite_int64 rowId = sqlite3_value_int64(argv[0]);
        if (rowId < 0 || static_cast<size_t>(rowId) >= s_VMStackRows.size())
            return SQLITE_ERROR;

        const auto &stackRow = s_VMStackRows[rowId];
        constexpr int32_t VALUE_ROW = 2 + VMStackColumns::Value;
        if (argc > VALUE_ROW && sqlite3_value_type(argv[VALUE_ROW]) != SQLITE_NULL)
        {
            auto *pVM = Globals::VirtualMachine();

            switch (stackRow.type)
            {
                case Constants::VMAuxCodeType::Integer:
                {
                    if (sqlite3_value_type(argv[VALUE_ROW]) == SQLITE_INTEGER)
                        pVM->SetStackIntegerValue(stackRow.stackLocation, sqlite3_value_int(argv[VALUE_ROW]));
                    else
                        return SQLITE_MISMATCH;
                    break;
                }

                case Constants::VMAuxCodeType::Float:
                {
                    if (sqlite3_value_type(argv[VALUE_ROW]) == SQLITE_FLOAT || sqlite3_value_type(argv[VALUE_ROW]) == SQLITE_INTEGER)
                        pVM->SetStackFloatValue(stackRow.stackLocation, sqlite3_value_double(argv[VALUE_ROW]));
                    else
                        return SQLITE_MISMATCH;
                    break;
                }

                case Constants::VMAuxCodeType::String:
                {
                    if (sqlite3_value_type(argv[VALUE_ROW]) == SQLITE_TEXT)
                    {
                        const auto *text = reinterpret_cast<const char*>(sqlite3_value_text(argv[VALUE_ROW]));
                        pVM->SetStackStringValue(stackRow.stackLocation, CExoString(text));
                    }
                    else
                        return SQLITE_MISMATCH;
                    break;
                }

                case Constants::VMAuxCodeType::Object:
                {
                    if (sqlite3_value_type(argv[VALUE_ROW]) == SQLITE_INTEGER)
                        pVM->SetStackObjectValue(stackRow.stackLocation, sqlite3_value_int(argv[VALUE_ROW]));
                    else
                        return SQLITE_MISMATCH;
                    break;
                }

                case Constants::VMAuxCodeType::EngSt7:
                {
                    if (sqlite3_value_type(argv[VALUE_ROW]) == SQLITE_TEXT)
                    {
                        const auto *text = reinterpret_cast<const char*>(sqlite3_value_text(argv[VALUE_ROW]));
                        JsonEngineStructure j;
                        j.m_shared->m_json = json::parse(text);
                        pVM->SetStackJsonValue(stackRow.stackLocation, &j);
                    }
                    else
                        return SQLITE_MISMATCH;
                    break;
                }

                default:
                    return SQLITE_READONLY;
            }

            *pRowid = rowId;
            return SQLITE_OK;
        }
    }

    return SQLITE_READONLY;
}

static int SetupVMStackVirtualTableModule(sqlite3 *db)
{
    static sqlite3_module vmsModule =
    {
        0,
        nullptr,
        vmsConnect,
        vmsBestIndex,
        vmsDisconnect,
        vmsDisconnect,
        vmsOpen,
        vmsClose,
        vmsFilter,
        vmsNext,
        vmsEOF,
        vmsColumn,
        vmsRowid,
        vmsUpdate,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };

    return sqlite3_create_module_v2(db, "vmstack", &vmsModule, nullptr, nullptr);
}
