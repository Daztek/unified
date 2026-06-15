#include "nwnx.hpp"

using namespace NWNXLib;
using namespace NWNXLib::API;

NWNX_EXPORT ArgumentStack GetQuery(ArgumentStack&& args)
{
    const auto sql = args.extract<SqlQueryEngineStructure>();
    return sql.m_shared->m_query;
}

NWNX_EXPORT ArgumentStack GetState(ArgumentStack&& args)
{
    const auto sql = args.extract<SqlQueryEngineStructure>();
    return static_cast<int32_t>(sql.m_shared->m_state);
}
