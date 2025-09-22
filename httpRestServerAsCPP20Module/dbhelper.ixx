export module dbhelper;

import odbc;   // import our database module
import <string>;
import <vector>;
import <memory>;
import <utility>;

export struct IDatabaseHelper {
    virtual ~IDatabaseHelper() = default;

    virtual bool createTable(const std::string& tableName,
        const std::vector<std::pair<std::string, std::string>>& columns) = 0;

    virtual bool insert(const std::string& tableName,
        const std::vector<std::string>& columns,
        const std::vector<std::string>& values) = 0;

    virtual bool update(const std::string& tableName,
        const std::vector<std::pair<std::string, std::string>>& setColumns,
        const std::string& whereClause) = 0;

    virtual QueryResult select(const std::string& tableName,
        const std::vector<std::string>& columns,
        const std::string& whereClause = "") = 0;
};

class DatabaseHelper : public IDatabaseHelper {
    std::unique_ptr<IDatabase> db;
public:
    DatabaseHelper(std::unique_ptr<IDatabase> database)
        : db(std::move(database)) {
    }

    bool createTable(const std::string& tableName,
        const std::vector<std::pair<std::string, std::string>>& columns) override
    {
        std::string sql = "CREATE TABLE IF NOT EXISTS " + tableName + " (";
        for (size_t i = 0; i < columns.size(); i++) {
            sql += columns[i].first + " " + columns[i].second;
            if (i != columns.size() - 1) sql += ", ";
        }
        sql += ")";
        return db->execute(sql);
    }

    bool insert(const std::string& tableName,
        const std::vector<std::string>& columns,
        const std::vector<std::string>& values) override
    {
        if (columns.size() != values.size()) return false;

        std::string sql = "INSERT INTO " + tableName + " (";
        for (size_t i = 0; i < columns.size(); i++) {
            sql += columns[i];
            if (i != columns.size() - 1) sql += ", ";
        }
        sql += ") VALUES (";
        for (size_t i = 0; i < values.size(); i++) {
            sql += "'" + values[i] + "'";
            if (i != values.size() - 1) sql += ", ";
        }
        sql += ")";
        return db->execute(sql);
    }

    bool update(const std::string& tableName,
        const std::vector<std::pair<std::string, std::string>>& setColumns,
        const std::string& whereClause) override
    {
        std::string sql = "UPDATE " + tableName + " SET ";
        for (size_t i = 0; i < setColumns.size(); i++) {
            sql += setColumns[i].first + " = '" + setColumns[i].second + "'";
            if (i != setColumns.size() - 1) sql += ", ";
        }
        if (!whereClause.empty()) sql += " WHERE " + whereClause;
        return db->execute(sql);
    }

    QueryResult select(const std::string& tableName,
        const std::vector<std::string>& columns,
        const std::string& whereClause = "") override
    {
        std::string sql = "SELECT ";
        for (size_t i = 0; i < columns.size(); i++) {
            sql += columns[i];
            if (i != columns.size() - 1) sql += ", ";
        }
        sql += " FROM " + tableName;
        if (!whereClause.empty()) sql += " WHERE " + whereClause;
        return db->query(sql);
    }
};

export std::unique_ptr<IDatabaseHelper> createDatabaseHelper(std::unique_ptr<IDatabase> db) {
    return std::make_unique<DatabaseHelper>(std::move(db));
}
