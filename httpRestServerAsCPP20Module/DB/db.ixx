export module db;

import <string>;
import <vector>;
import <map>;
import <memory>;

export namespace db {

    using Record = std::map<std::string, std::string>;

    class IDatabase {
    public:
        virtual ~IDatabase() = default;

        virtual void open(const std::string& connection_string) = 0;
        virtual void close() = 0;
        virtual void execute(const std::string& sql) = 0;
        virtual std::vector<Record> query(const std::string& sql) = 0;
    };

    // Factory function type
    using DatabaseFactory = std::unique_ptr<IDatabase>(*)();

} // namespace db
