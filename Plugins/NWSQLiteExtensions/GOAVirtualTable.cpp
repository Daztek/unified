#include "nwnx.hpp"
#include "API/Database.hpp"
#include "API/CAppManager.hpp"
#include "API/CServerExoApp.hpp"
#include "API/CGameObjectArray.hpp"
#include "API/CGameObject.hpp"
#include "API/CNWSObject.hpp"
#include "API/CNWSArea.hpp"
#include "API/CNWSModule.hpp"
#include "API/CNWSUUID.hpp"


using namespace NWNXLib;
using namespace NWNXLib::API;

static bool s_GOAVirtualTableEnabled = false;
constexpr const char* MODULE_DATABASE_LABEL = "Module()";
static int SetupGOAVirtualTableModule(sqlite3 *db);

void GOAVirtualTable() __attribute__((constructor));
void GOAVirtualTable()
{
    if ((s_GOAVirtualTableEnabled = Config::Get<bool>("ENABLE_GOA_VIRTUAL_TABLE_MODULE", false)))
    {
        LOG_INFO("Enabling the GameObjectArray Virtual Table Module for the module database.");
        static Hooks::Hook s_DatabaseSetupHook = Hooks::HookFunction(&NWSQLite::Database::Setup,
        +[](NWSQLite::Database *pThis) -> void
        {
            s_DatabaseSetupHook->CallOriginal<void>(pThis);

            if (strcmp(pThis->m_label.c_str(), MODULE_DATABASE_LABEL) == 0)
            {
                if (SetupGOAVirtualTableModule(pThis->connection().get()) != SQLITE_OK)
                {
                    LOG_ERROR("Failed to setup the GameObjectArray Virtual Table Module.");
                    s_GOAVirtualTableEnabled = false;
                }
            }
        }, Hooks::Order::Early);
    }

    Commands::Register("goa", [](std::string&, std::string&)
    {
        auto moduleDatabase = Utils::GetModule()->m_sqlite3;
        *moduleDatabase << "SELECT oid, type, tag FROM gameobjects WHERE type > 4;" >>
        [](uint32_t oid, int32_t type, const std::string &tag)
        {
            LOG_DEBUG("OID: %i - TYPE: %s - TAG: %s", oid, Constants::ObjectType::ToString(type), tag);
        };
    });
}

typedef struct goa_tab goa_tab;
struct goa_tab
{
    sqlite3_vtab base;
};

typedef struct goa_cursor goa_cursor;
struct goa_cursor
{
    sqlite3_vtab_cursor base;
    ObjectID currentObjectId;
    uint32_t objectTypeFilter;
};

namespace GOAColumns
{
    enum TYPE
    {
        ObjectId = 0,
        ObjectType,
        AreaId,
        PositionX,
        PositionY,
        PositionZ,
        Tag,
        UUID,
        Num, // Keep Last
    };
    constexpr int32_t MAX   = 7;
    constexpr int32_t NUM   = MAX + 1;
    static_assert(MAX == UUID);
    static_assert(NUM == Num);

    constexpr const char* ToColumnWithType(const unsigned value)
    {
        constexpr const char* TYPE_STRINGS[] =
        {
            "oid INTEGER",
            "type INTEGER",
            "oid_area INTEGER",
            "pos_x REAL",
            "pos_y REAL",
            "pos_z REAL",
            "tag TEXT",
            "uuid TEXT",
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

static int goaConnect(sqlite3 *db, void *pAux, int argc, const char* const *argv, sqlite3_vtab **ppVtab, char **pzErr)
{
    (void)pAux; (void)argc; (void)argv; (void)pzErr;

    LOG_DEBUG("Creating Virtual GameObjectArray Table");

    const CExoString sSchema = GOAColumns::GetSchema();
    LOG_DEBUG("  Schema: %s", sSchema);

    goa_tab *pNewVtab = nullptr;
    int32_t ret = sqlite3_declare_vtab(db, sSchema.CStr());
    if (ret == SQLITE_OK)
    {
        pNewVtab = static_cast<goa_tab*>(sqlite3_malloc(sizeof(goa_tab)));
        if (!pNewVtab)
            return SQLITE_NOMEM;

        memset(pNewVtab, 0, sizeof(goa_tab));
        sqlite3_vtab_config(db, SQLITE_VTAB_DIRECTONLY);
    }

    *ppVtab = reinterpret_cast<sqlite3_vtab*>(pNewVtab);
    return ret;
}

static int goaDisconnect(sqlite3_vtab *pVtab)
{
    auto *pgoaVtab = reinterpret_cast<goa_tab*>(pVtab);
    sqlite3_free(pgoaVtab);
    return SQLITE_OK;
}

static int goaOpen(sqlite3_vtab*, sqlite3_vtab_cursor **ppCursor)
{
    auto *pCursor = static_cast<goa_cursor*>(sqlite3_malloc(sizeof(goa_cursor)));
    if(!pCursor) return SQLITE_NOMEM;
    memset(pCursor, 0, sizeof(goa_cursor));
    *ppCursor = &pCursor->base;
    return SQLITE_OK;
}

static int goaClose(sqlite3_vtab_cursor *cur)
{
    auto *pCursor = reinterpret_cast<goa_cursor*>(cur);
    sqlite3_free(pCursor);
    return SQLITE_OK;
}

static int goaNext(sqlite3_vtab_cursor *cur)
{
    auto *pCursor = reinterpret_cast<goa_cursor*>(cur);
    auto *pGOA = Globals::AppManager()->m_pServerExoApp->GetObjectArray();

    CGameObject *pGameObject;
    if (pCursor->currentObjectId <= Constants::MAXOBJECTID)
    {
        do
        {
            pGameObject = Utils::GetGameObject(++pCursor->currentObjectId);
        }
        while ((!pGameObject || (pCursor->objectTypeFilter && pGameObject->m_nObjectType != pCursor->objectTypeFilter)) &&
                pCursor->currentObjectId < pGOA->m_nNextObjectArrayID[0]);

        if (pCursor->currentObjectId == pGOA->m_nNextObjectArrayID[0])
        {
            pCursor->currentObjectId = Constants::MAXCHAROBJID;
            if (!Utils::GetGameObject(pCursor->currentObjectId))
            {
                do
                {
                    pGameObject = Utils::GetGameObject(--pCursor->currentObjectId);
                }
                while ((!pGameObject || (pCursor->objectTypeFilter && pGameObject->m_nObjectType != pCursor->objectTypeFilter)) &&
                       pCursor->currentObjectId > pGOA->m_nNextCharArrayID[0]);
            }
        }
    }
    else if (pCursor->currentObjectId >= Constants::MINCHAROBJID)
    {
        do
        {
            pGameObject = Utils::GetGameObject(--pCursor->currentObjectId);
        }
        while ((!pGameObject || (pCursor->objectTypeFilter && pGameObject->m_nObjectType != pCursor->objectTypeFilter)) &&
                pCursor->currentObjectId > pGOA->m_nNextCharArrayID[0]);
    }

    return SQLITE_OK;
}

static int goaColumn(sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int column)
{
    auto *pCursor = reinterpret_cast<goa_cursor*>(cur);

    if (sqlite3_vtab_nochange(ctx))
        return SQLITE_OK;

    if (auto *pGameObject = Utils::GetGameObject(pCursor->currentObjectId))
    {
        switch (column)
        {
            case GOAColumns::ObjectId:
                sqlite3_result_int(ctx, pGameObject->m_idSelf);
            break;

            case GOAColumns::ObjectType:
                sqlite3_result_int(ctx, pGameObject->m_nObjectType);
            break;

            case GOAColumns::AreaId:
            {
                if (auto *pObject = Utils::AsNWSObject(pGameObject))
                    sqlite3_result_int(ctx, pObject->m_oidArea);
            }
            break;

            case GOAColumns::PositionX:
            {
                if (auto *pObject = Utils::AsNWSObject(pGameObject))
                    sqlite3_result_double(ctx, pObject->m_vPosition.x);
            }
            break;

            case GOAColumns::PositionY:
            {
                if (auto *pObject = Utils::AsNWSObject(pGameObject))
                    sqlite3_result_double(ctx, pObject->m_vPosition.y);
            }
            break;

            case GOAColumns::PositionZ:
            {
                if (auto *pObject = Utils::AsNWSObject(pGameObject))
                    sqlite3_result_double(ctx, pObject->m_vPosition.z);
            }
            break;

            case GOAColumns::Tag:
            {
                if (auto *pObject = Utils::AsNWSObject(pGameObject))
                    sqlite3_result_text(ctx, pObject->m_sTag.CStr(), -1, SQLITE_TRANSIENT);
                else if (auto *pArea = Utils::AsNWSArea(pGameObject))
                    sqlite3_result_text(ctx, pArea->m_sTag.CStr(), -1, SQLITE_TRANSIENT);
                else if (auto *pModule = Utils::AsNWSModule(pGameObject))
                    sqlite3_result_text(ctx, pModule->m_sTag.CStr(), -1, SQLITE_TRANSIENT);
            }
            break;

            case GOAColumns::UUID:
            {
                if (auto *pObject = Utils::AsNWSObject(pGameObject))
                    sqlite3_result_text(ctx, pObject->m_pUUID.m_uuid.CStr(), -1, SQLITE_TRANSIENT);
                else if (auto *pArea = Utils::AsNWSArea(pGameObject))
                    sqlite3_result_text(ctx, pArea->m_pUUID.m_uuid.CStr(), -1, SQLITE_TRANSIENT);
            }
            break;

            default:
                LOG_DEBUG("Forgot to implement a column?");
            break;
        }
    }
    else
    {
        LOG_DEBUG("Tried to get row data for a non-existent object");
    }

    return SQLITE_OK;
}

static int goaRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid)
{
    auto *pCursor = reinterpret_cast<goa_cursor*>(cur);
    *pRowid = pCursor->currentObjectId;
    return SQLITE_OK;
}

static int goaEOF(sqlite3_vtab_cursor *cur)
{
    auto *pCursor = reinterpret_cast<goa_cursor*>(cur);
    auto *pGOA = Globals::AppManager()->m_pServerExoApp->GetObjectArray();
    return (pCursor->currentObjectId <= Constants::MAXOBJECTID && pCursor->currentObjectId >= pGOA->m_nNextObjectArrayID[0]) ||
           (pCursor->currentObjectId >= Constants::MINCHAROBJID && pCursor->currentObjectId <= pGOA->m_nNextCharArrayID[0]);
}

static int goaFilter(sqlite3_vtab_cursor *cur, int idxNum, const char *idxStr, int argc, sqlite3_value **argv)
{
    (void)idxStr;
    auto *pCursor = reinterpret_cast<goa_cursor*>(cur);

    pCursor->currentObjectId = 0;
    pCursor->objectTypeFilter = 0;

    if (idxNum == 1)
    {
        if (argc == 1)
        {
            if (sqlite3_value_type(argv[0]) == SQLITE_INTEGER)
            {
                int32_t objectType = sqlite3_value_int(argv[0]);
                LOG_DEBUG("ObjectFilter Constraint -> EQ = %i", objectType);
                pCursor->objectTypeFilter = objectType;
            }
        }
        else
        {
            LOG_DEBUG("Multiple Object Filter Constraints?");
        }
    }

    return SQLITE_OK;
}

static int goaBestIndex(sqlite3_vtab*, sqlite3_index_info *pIndexInfo)
{
    LOG_DEBUG("Constraints: %i", pIndexInfo->nConstraint);
    int nNumObjectTypeConstraints = 0;
    int nEQConstraintIndex = -1;
    for (int i = 0; i < pIndexInfo->nConstraint; i++)
    {
        auto constraint = pIndexInfo->aConstraint[i];

        LOG_DEBUG("%i -> Col: %i, Op: %i, Useable: %i", i, constraint.iColumn, constraint.op, constraint.usable);

        if (!constraint.usable || constraint.iColumn != GOAColumns::ObjectType)
           continue;

        nNumObjectTypeConstraints++;

        if (constraint.op == SQLITE_INDEX_CONSTRAINT_EQ)
        {
            nEQConstraintIndex = i;
        }
    }

    if (nNumObjectTypeConstraints == 1 && nEQConstraintIndex != -1)
    {
        pIndexInfo->idxNum = 1;
        pIndexInfo->aConstraintUsage[nEQConstraintIndex].argvIndex = 1;
    }

    return SQLITE_OK;
}

static int SetupGOAVirtualTableModule(sqlite3 *db)
{
    static sqlite3_module goaModule =
    {
        0,
        nullptr,
        goaConnect,
        goaBestIndex,
        goaDisconnect,
        goaDisconnect,
        goaOpen,
        goaClose,
        goaFilter,
        goaNext,
        goaEOF,
        goaColumn,
        goaRowid,
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
        nullptr,
        nullptr
    };

    return sqlite3_create_module_v2(db, "gameobjects", &goaModule, nullptr, nullptr);
}
