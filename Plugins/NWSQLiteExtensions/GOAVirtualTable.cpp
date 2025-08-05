#include "nwnx.hpp"
#include "API/Database.hpp"
#include "API/CAppManager.hpp"
#include "API/CServerExoApp.hpp"
#include "API/CGameObjectArray.hpp"
#include "API/CGameObject.hpp"
#include "API/CNWSObject.hpp"
#include "API/CNWSArea.hpp"
#include "API/CNWSModule.hpp"


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
    uint32_t objectTypeEQFilter;
    ObjectID areaIdEQFilter;
    char tagEQFilter[65];
    bool tagEQFilterActive;
    char tagLikeFilter[65];
    bool tagLikeFilterActive;
};

namespace GOAColumns
{
    enum TYPE
    {
        ObjectId = 0,
        ObjectType,
        AreaId,
        Tag,
        Num, // Keep Last
    };
    constexpr int32_t MAX   = 3;
    constexpr int32_t NUM   = MAX + 1;
    static_assert(MAX == Tag);
    static_assert(NUM == Num);

    constexpr const char* ToColumnWithType(const unsigned value)
    {
        constexpr const char* TYPE_STRINGS[] =
        {
            "oid INTEGER",
            "type INTEGER",
            "areaid INTEGER",
            "tag TEXT",
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

enum FilterFlags : int
{
    None = 0,
    ObjectTypeEQ = 1,
    AreaIdEQ = 2,
    TagEQ = 4,
    TagLike = 8
};

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

static bool goaObjectPassesFilters(goa_cursor *pCursor, CGameObject *pGameObject)
{
    if (!pGameObject)
        return false;

     if (pCursor->objectTypeEQFilter && pGameObject->m_nObjectType != pCursor->objectTypeEQFilter)
        return false;

    if (pCursor->areaIdEQFilter)
    {
        if (auto *pObject = Utils::AsNWSObject(pGameObject))
        {
            if (pObject->m_oidArea != pCursor->areaIdEQFilter)
                return false;
        }
        else
        {
            return false;
        }
    }

    if (pCursor->tagEQFilterActive)
    {
        const char* tag = nullptr;
        if (auto *pObject = Utils::AsNWSObject(pGameObject))
            tag = pObject->m_sTag.CStr();
        else if (auto *pArea = Utils::AsNWSArea(pGameObject))
            tag = pArea->m_sTag.CStr();
        else if (auto *pModule = Utils::AsNWSModule(pGameObject))
            tag = pModule->m_sTag.CStr();

        if (!tag || strcmp(tag, pCursor->tagEQFilter) != 0)
            return false;
    }

    if (pCursor->tagLikeFilterActive)
    {
        const char* tag = nullptr;
        if (auto *pObject = Utils::AsNWSObject(pGameObject))
            tag = pObject->m_sTag.CStr();
        else if (auto *pArea = Utils::AsNWSArea(pGameObject))
            tag = pArea->m_sTag.CStr();
        else if (auto *pModule = Utils::AsNWSModule(pGameObject))
            tag = pModule->m_sTag.CStr();

        if (!tag || sqlite3_strlike(pCursor->tagLikeFilter, tag, 0) != 0)
            return false;
    }

    return true;
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
        while (!goaObjectPassesFilters(pCursor, pGameObject) && pCursor->currentObjectId < pGOA->m_nNextObjectArrayID[0]);

        if (pCursor->currentObjectId == pGOA->m_nNextObjectArrayID[0])
        {
            pCursor->currentObjectId = Constants::MAXCHAROBJID;
            if (!Utils::GetGameObject(pCursor->currentObjectId))
            {
                do
                {
                    pGameObject = Utils::GetGameObject(--pCursor->currentObjectId);
                }
                while (!goaObjectPassesFilters(pCursor, pGameObject) && pCursor->currentObjectId > pGOA->m_nNextCharArrayID[0]);
            }
        }
    }
    else if (pCursor->currentObjectId >= Constants::MINCHAROBJID)
    {
        do
        {
            pGameObject = Utils::GetGameObject(--pCursor->currentObjectId);
        }
        while (!goaObjectPassesFilters(pCursor, pGameObject) && pCursor->currentObjectId > pGOA->m_nNextCharArrayID[0]);
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
                else
                    sqlite3_result_int(ctx, Constants::OBJECT_INVALID);
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
    pCursor->objectTypeEQFilter = 0;
    pCursor->areaIdEQFilter = 0;
    pCursor->tagEQFilterActive = false;
    pCursor->tagEQFilter[0] = '\0';
    pCursor->tagLikeFilterActive = false;
    pCursor->tagLikeFilter[0] = '\0';

    int argIndex = 0;
    if (idxNum & FilterFlags::ObjectTypeEQ)
    {
        if (argIndex < argc && sqlite3_value_type(argv[argIndex]) == SQLITE_INTEGER)
        {
            int32_t objectType = sqlite3_value_int(argv[argIndex]);
            pCursor->objectTypeEQFilter = objectType;
        }
        argIndex++;
    }

    if (idxNum & FilterFlags::AreaIdEQ)
    {
        if (argIndex < argc && sqlite3_value_type(argv[argIndex]) == SQLITE_INTEGER)
        {
            ObjectID areaId = sqlite3_value_int(argv[argIndex]);
            pCursor->areaIdEQFilter = areaId;
        }
        argIndex++;
    }

    if (idxNum & FilterFlags::TagEQ)
    {
        if (argIndex < argc && sqlite3_value_type(argv[argIndex]) == SQLITE_TEXT)
        {
            if (auto *tag = reinterpret_cast<const char*>(sqlite3_value_text(argv[argIndex])))
            {
                sqlite3_snprintf(sizeof(pCursor->tagEQFilter), pCursor->tagEQFilter, "%s", tag);
                pCursor->tagEQFilterActive = true;
            }
        }
        argIndex++;
    }

    if (idxNum & FilterFlags::TagLike)
    {
        if (argIndex < argc && sqlite3_value_type(argv[argIndex]) == SQLITE_TEXT)
        {
            if (auto *likePattern = reinterpret_cast<const char*>(sqlite3_value_text(argv[argIndex])))
            {
                sqlite3_snprintf(sizeof(pCursor->tagLikeFilter), pCursor->tagLikeFilter, "%s", likePattern);
                pCursor->tagLikeFilterActive = true;
            }
        }
    }

    if (!goaObjectPassesFilters(pCursor, Utils::GetGameObject(pCursor->currentObjectId)))
        goaNext(cur);

    return SQLITE_OK;
}

static int goaBestIndex(sqlite3_vtab*, sqlite3_index_info *pIndexInfo)
{
    int objectTypeEQIndex = -1;
    int areaIdEQIndex = -1;
    int tagEQIndex = -1;
    int tagLikeIndex = -1;

    for (int i = 0; i < pIndexInfo->nConstraint; i++)
    {
        const auto constraint = pIndexInfo->aConstraint[i];

        if (!constraint.usable)
            continue;

        if (constraint.op == SQLITE_INDEX_CONSTRAINT_EQ)
        {
            if (constraint.iColumn == GOAColumns::ObjectType)
                objectTypeEQIndex = i;
            else if (constraint.iColumn == GOAColumns::AreaId)
                areaIdEQIndex = i;
            else if (constraint.iColumn == GOAColumns::Tag)
                tagEQIndex = i;
        }
        else if (constraint.op == SQLITE_INDEX_CONSTRAINT_LIKE)
        {
            if (constraint.iColumn == GOAColumns::Tag)
                tagLikeIndex = i;
        }
    }

    int idxNum = FilterFlags::None;
    int argvIndex = 1;

    if (objectTypeEQIndex != -1)
    {
        idxNum |= FilterFlags::ObjectTypeEQ;
        pIndexInfo->aConstraintUsage[objectTypeEQIndex].argvIndex = argvIndex++;
        pIndexInfo->aConstraintUsage[objectTypeEQIndex].omit = 1;
    }

    if (areaIdEQIndex != -1)
    {
        idxNum |= FilterFlags::AreaIdEQ;
        pIndexInfo->aConstraintUsage[areaIdEQIndex].argvIndex = argvIndex++;
        pIndexInfo->aConstraintUsage[areaIdEQIndex].omit = 1;
    }

    if (tagEQIndex != -1)
    {
        idxNum |= FilterFlags::TagEQ;
        pIndexInfo->aConstraintUsage[tagEQIndex].argvIndex = argvIndex++;
        pIndexInfo->aConstraintUsage[tagEQIndex].omit = 1;
    }

    if (tagLikeIndex != -1)
    {
        idxNum |= FilterFlags::TagLike;
        pIndexInfo->aConstraintUsage[tagLikeIndex].argvIndex = argvIndex++;
        pIndexInfo->aConstraintUsage[tagLikeIndex].omit = 1;
    }

    pIndexInfo->idxNum = idxNum;

    double baseCost = 10000.0;
    int filters = 0;
    if (idxNum & FilterFlags::ObjectTypeEQ) filters++;
    if (idxNum & FilterFlags::AreaIdEQ) filters++;
    if (idxNum & FilterFlags::TagEQ) filters++;
    if (idxNum & FilterFlags::TagLike) filters++;

    if (filters > FilterFlags::None)
        pIndexInfo->estimatedCost = baseCost / (filters * 10.0);
    else
        pIndexInfo->estimatedCost = baseCost;

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
