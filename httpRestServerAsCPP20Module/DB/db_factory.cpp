module db;
import sqlite;

std::unique_ptr<db::IDatabase> db::getDBAccessPtr(DbType type) {
    switch (type) {
    case DbType::SQLite:
        return std::make_unique<sqlite::SQLiteDB>();
    default:
        return nullptr;
    }
}
