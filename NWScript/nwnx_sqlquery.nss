const string NWNX_SqlQuery = "NWNX_SqlQuery";

const int NWNX_SQLQUERY_STATE_EMPTY     = 0;
const int NWNX_SQLQUERY_STATE_NEW       = 1;
const int NWNX_SQLQUERY_STATE_ROW       = 2;
const int NWNX_SQLQUERY_STATE_DONE      = 3;

// Get the query associated with the sqlquery sqlQuery.
// - sqlQuery: A sqlquery.
// * Returns: The query or "" on error/not set.
string NWNX_SqlQuery_GetQuery(sqlquery sqlQuery);

// Get the state of the sqlquery sqlQuery.
// - sqlQuery: A sqlquery.
// * Returns: A NWNX_SQLQUERY_STATE_* constant.
int NWNX_SqlQuery_GetState(sqlquery sqlQuery);

string NWNX_SqlQuery_GetQuery(sqlquery sqlQuery)
{
    NWNXPushSqlquery(sqlQuery);
    NWNXCall(NWNX_SqlQuery, "GetQuery");
    return NWNXPopString();
}

int NWNX_SqlQuery_GetState(sqlquery sqlQuery)
{
    NWNXPushSqlquery(sqlQuery);
    NWNXCall(NWNX_SqlQuery, "GetState");
    return NWNXPopInt();
}
