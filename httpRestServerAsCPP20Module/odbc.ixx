// Put includes BEFORE the export module line
#ifdef _WIN32
#undef UNICODE           // disable Windows UNICODE macro
#include <windows.h>
#endif

// include ODBC SQL Headers
#include <sql.h>
#include <sqlext.h>

export module odbc;

import <string>;
import <vector>;
import <stdexcept>;
import <iostream>;

export struct QueryResult {
    std::vector<std::vector<std::string>> rows;
    bool success{ false };
};

export struct IDatabase {
    virtual ~IDatabase() = default;
    virtual bool connect(const std::string& connStr) = 0;
    virtual void disconnect() = 0;
    virtual bool execute(const std::string& sql) = 0;
    virtual QueryResult query(const std::string& sql) = 0;
};

// Implementation with ODBC
class odbc : public IDatabase {
    SQLHENV env{ nullptr };
    SQLHDBC dbc{ nullptr };
    SQLHSTMT stmt{ nullptr };
public:
    ~odbc() { disconnect(); }

    bool connect(const std::string& connStr) override {
        if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env) != SQL_SUCCESS) return false;
        SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);

        if (SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc) != SQL_SUCCESS) return false;

        SQLCHAR outstr[1024];
        SQLSMALLINT outstrlen;
        SQLRETURN ret = SQLDriverConnectA(
            dbc, nullptr,
            (SQLCHAR*)connStr.c_str(), SQL_NTS,
            outstr, sizeof(outstr), &outstrlen,
            SQL_DRIVER_COMPLETE);

        return SQL_SUCCEEDED(ret);
    }

    void disconnect() override {
        if (stmt) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); stmt = nullptr; }
        if (dbc) { SQLDisconnect(dbc); SQLFreeHandle(SQL_HANDLE_DBC, dbc); dbc = nullptr; }
        if (env) { SQLFreeHandle(SQL_HANDLE_ENV, env); env = nullptr; }
    }

    bool execute(const std::string& sql) override {
        if (SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt) != SQL_SUCCESS) return false;
        SQLRETURN ret = SQLExecDirectA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        stmt = nullptr;
        return SQL_SUCCEEDED(ret);
    }

    QueryResult query(const std::string& sql) override {
        QueryResult result;
        if (SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt) != SQL_SUCCESS) return result;

        SQLRETURN ret = SQLExecDirectA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
        if (!SQL_SUCCEEDED(ret)) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); stmt = nullptr; return result; }

        SQLSMALLINT numCols;
        SQLNumResultCols(stmt, &numCols);

        while (SQLFetch(stmt) == SQL_SUCCESS) {
            std::vector<std::string> row;
            for (SQLUSMALLINT i = 1; i <= numCols; i++) {
                char buf[256];
                SQLLEN indicator;
                if (SQLGetData(stmt, i, SQL_C_CHAR, buf, sizeof(buf), &indicator) == SQL_SUCCESS) {
                    if (indicator == SQL_NULL_DATA) row.emplace_back("NULL");
                    else row.emplace_back(buf);
                }
                else {
                    row.emplace_back("ERR");
                }
            }
            result.rows.push_back(std::move(row));
        }

        result.success = true;
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        stmt = nullptr;
        return result;
    }
};

export std::unique_ptr<IDatabase> createDatabase() {
    return std::make_unique<odbc>();
}
