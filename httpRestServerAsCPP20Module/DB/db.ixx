export module db;

import <string>;
import <vector>;
import <map>;
import <memory>;

export namespace db {

    using Record = std::map<std::string, std::string>;

    // Abstract database interface
    class IDatabase {
    public:
        virtual ~IDatabase() = default;
        virtual void open(const std::string& path) = 0;
        virtual void close() = 0;
        virtual void execute(const std::string& sql) = 0;
        virtual std::vector<Record> query(const std::string& sql) = 0;
    };

    // Supported database types
    export enum class DbType {
        SQLite,
        //PostgreSQL, ODBC, etc. (future)
    };

    // Factory declaration only
    export std::unique_ptr<IDatabase> getDBAccessPtr(DbType type);
}
