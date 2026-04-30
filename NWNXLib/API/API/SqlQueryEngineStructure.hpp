#pragma once
#include "nwn_api.hpp"
#include "CExoString.hpp"
#include "Database.hpp"

#ifdef NWN_API_PROLOGUE
NWN_API_PROLOGUE(SqlQueryEngineStructure)
#endif

enum class SqlQueryEngineStructureState { EMPTY, NEW, ROW, DONE };
struct SqlQueryEngineStructureShared
{
    using State = SqlQueryEngineStructureState;

    ~SqlQueryEngineStructureShared()
    {
        sqlite3_finalize(m_stmt);
    }

    const uint64_t m_id;
    CExoString m_query;
    std::shared_ptr<NWSQLite::Database> m_db;
    sqlite3_stmt* m_stmt = nullptr;
    int m_errcode = 0;
    CExoString m_errstr;
    State m_state = State::EMPTY;
};

struct SqlQueryEngineStructure : public SharedPtrEngineStructure<SqlQueryEngineStructureShared>
{
    SqlQueryEngineStructure();
    virtual ~SqlQueryEngineStructure() {}
    bool IsEmpty() const override;
    void Clear() override;
    void Unlink() override { assert(!"not implemented"); }
};


#ifdef NWN_API_EPILOGUE
NWN_API_EPILOGUE(SqlQueryEngineStructure)
#endif
