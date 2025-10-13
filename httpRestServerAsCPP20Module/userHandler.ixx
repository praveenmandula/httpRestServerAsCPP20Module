export module userHandler;

import <string>;
import <vector>;
import <mutex>;
import <algorithm>;
import <memory>;
import <sstream>;

import http;
import json;
import logger;
import db;

export struct User {
    int id;
    std::string name;
    std::string email;
};

export class userHandler {
private:
    std::unique_ptr<db::IDatabase> database;
    std::mutex mtx;

    json::Json userToJson(const User& u) {
        json::Json j = json::Json::object();
        j["id"] = static_cast<double>(u.id);
        j["name"] = u.name;
        j["email"] = u.email;
        return j;
    }

    User recordToUser(const db::Record& rec) {
        User u{};
        u.id = std::stoi(rec.at("id"));
        u.name = rec.at("name");
        u.email = rec.at("email");
        return u;
    }

    static int parseIntOrDefault(const std::string& s, int def) {
        try { return std::stoi(s); }
        catch (...) { return def; }
    }

public:
    userHandler() {
        database = db::getDBAccessPtr(db::DbType::SQLite);
        database->open("users.db");
        initialize();
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mtx);
        database->execute(
            "CREATE TABLE IF NOT EXISTS users ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT,"
            "email TEXT);"
        );
    }

    // ==============================
    // GET /api/users?page=1&limit=10
    // ==============================
    http::Response getUsers(const http::Request& req) {
        Log(logger::level::debug_, "getUsers called");

        http::Response res;
        res.headers["Content-Type"] = "application/json";

        // Parse query parameters
        int page = 1;
        int limit = 10;

        if (req.query.contains("page"))
            page = parseIntOrDefault(req.query.at("page"), 1);
        if (req.query.contains("limit"))
            limit = parseIntOrDefault(req.query.at("limit"), 10);

        if (page < 1) page = 1;
        if (limit < 1) limit = 10;

        int offset = (page - 1) * limit;

        Log(logger::level::debug_, std::format("Page={}, Limit={}, Offset={}", page, limit, offset));

        std::lock_guard<std::mutex> lock(mtx);

        // Fetch total count for pagination
        auto countRows = database->query("SELECT COUNT(*) AS total FROM users;");
        int total = std::stoi(countRows.front().at("total"));

        // Fetch paginated users
        std::ostringstream query;
        query << "SELECT id, name, email FROM users "
            << "ORDER BY id ASC LIMIT " << limit
            << " OFFSET " << offset << ";";
        auto rows = database->query(query.str());

        json::Json arr = json::Json::array();
        for (const auto& row : rows)
            arr.push_back(userToJson(recordToUser(row)));

        json::Json response = json::Json::object();
        response["page"] = static_cast<double>(page);
        response["limit"] = static_cast<double>(limit);
        response["total"] = static_cast<double>(total);
        response["users"] = arr;

        res.body = json::stringify(response);
        return res;
    }

    // ==============================
    // POST /api/users
    // ==============================
    http::Response createUser(const http::Request& req) {
        Log(logger::level::debug_, "createUser called");

        http::Response res;
        res.headers["Content-Type"] = "application/json";
        auto req_json = json::parse(req.body);

        std::string name = req_json.get("name", std::string("New User"));
        std::string email = req_json.get("email", std::string("new@example.com"));

        Log(logger::level::debug_, std::format("name={}, email={}", name, email));

        std::lock_guard<std::mutex> lock(mtx);
        database->execute(
            "INSERT INTO users (name, email) VALUES ('" + name + "', '" + email + "');"
        );

        auto rows = database->query("SELECT id, name, email FROM users ORDER BY id DESC LIMIT 1;");
        User u = recordToUser(rows.front());

        json::Json resp = json::Json::object();
        resp["status"] = "success";
        resp["user"] = userToJson(u);
        res.body = json::stringify(resp);
        return res;
    }

    // ==============================
    // PUT /api/users/{id}
    // ==============================
    http::Response updateUser(const http::Request& req) {
        Log(logger::level::debug_, "updateUser called");

        http::Response res;
        res.headers["Content-Type"] = "application/json";

        auto req_json = json::parse(req.body);
        std::string path = req.path;
        size_t pos = path.find_last_of('/');
        int id = std::stoi(path.substr(pos + 1));

        std::string name = req_json.get("name", std::string(""));
        std::string email = req_json.get("email", std::string(""));

        Log(logger::level::debug_, std::format("id={}, name={}, email={}", id, name, email));

        std::lock_guard<std::mutex> lock(mtx);
        database->execute(
            "UPDATE users SET "
            "name = '" + name + "', "
            "email = '" + email + "' "
            "WHERE id = " + std::to_string(id) + ";"
        );

        auto rows = database->query("SELECT id, name, email FROM users WHERE id = " + std::to_string(id) + ";");
        if (rows.empty()) {
            res.body = R"({ "status":"error","message":"User not found" })";
            return res;
        }

        User u = recordToUser(rows.front());
        json::Json resp = json::Json::object();
        resp["status"] = "success";
        resp["user"] = userToJson(u);
        res.body = json::stringify(resp);
        return res;
    }

    // ==============================
    // DELETE /api/users/{id}
    // ==============================
    http::Response deleteUser(const http::Request& req) {
        Log(logger::level::debug_, "deleteUser called");

        http::Response res;
        res.headers["Content-Type"] = "application/json";

        std::string path = req.path;
        size_t pos = path.find_last_of('/');
        int id = std::stoi(path.substr(pos + 1));

        std::lock_guard<std::mutex> lock(mtx);
        auto rows = database->query("SELECT id FROM users WHERE id = " + std::to_string(id) + ";");
        if (rows.empty()) {
            res.body = R"({ "status":"error","message":"User not found" })";
            return res;
        }

        Log(logger::level::debug_, std::format("id=", id));

        database->execute("DELETE FROM users WHERE id = " + std::to_string(id) + ";");

        json::Json resp = json::Json::object();
        resp["status"] = "success";
        resp["message"] = "User deleted";
        res.body = json::stringify(resp);
        return res;
    }
};
