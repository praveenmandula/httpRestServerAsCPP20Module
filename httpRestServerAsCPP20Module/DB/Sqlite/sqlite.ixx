export module sqlite;

#include "sqlite3.h"
import db;
import <string>;
import <vector>;
import <map>;
import <memory>;
import <stdexcept>;

export namespace sqlite {

    class SQLiteDB : public db::IDatabase {
    public:
        SQLiteDB() = default;
        ~SQLiteDB() { close(); }

        void open(const std::string& path) override {
            if (db_) close();
            if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
                throw std::runtime_error(sqlite3_errmsg(db_));
            }
        }

        void close() override {
            if (db_) {
                sqlite3_close(db_);
                db_ = nullptr;
            }
        }

        void execute(const std::string& sql) override {
            char* errmsg = nullptr;
            if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
                std::string err = errmsg ? errmsg : "Unknown error";
                sqlite3_free(errmsg);
                throw std::runtime_error(err);
            }
        }

        std::vector<db::Record> query(const std::string& sql) override {
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                throw std::runtime_error(sqlite3_errmsg(db_));
            }

            std::vector<db::Record> result;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                db::Record row;
                int cols = sqlite3_column_count(stmt);
                for (int i = 0; i < cols; ++i) {
                    const char* col = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                    row[sqlite3_column_name(stmt, i)] = col ? col : "";
                }
                result.push_back(std::move(row));
            }
            sqlite3_finalize(stmt);
            return result;
        }

    private:
        sqlite3* db_ = nullptr;
    };

    // Factory function for DB wrapper
    std::unique_ptr<db::IDatabase> make_sqlite() {
        return std::make_unique<SQLiteDB>();
    }

} // namespace sqlite
