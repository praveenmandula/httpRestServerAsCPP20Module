export module userHandler;

import <string>;
import <vector>;
import <mutex>;
import <algorithm>;
import <memory>;

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

public:
    userHandler() {
        // Create SQLite database instance
        database = db::getDBAccessPtr(db::DbType::SQLite);

        // Open the database file (or create if not exists)
        database->open("users.db");

        // Ensure users table exists
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

    http::Response getUsers(const http::Request& req) {
        Log(logger::level::debug_, "getUsers got called");
        http::Response res;
        res.headers["Content-Type"] = "application/json";

        std::lock_guard<std::mutex> lock(mtx);
        auto rows = database->query("SELECT id, name, email FROM users;");

        json::Json arr = json::Json::array();
        for (const auto& row : rows) {
            arr.push_back(userToJson(recordToUser(row)));
        }

        res.body = json::stringify(arr);
        return res;
    }

    http::Response createUser(const http::Request& req) {
        Log(logger::level::debug_, "createUser got called");
        http::Response res;
        res.headers["Content-Type"] = "application/json";

        auto req_json = json::parse(req.body);
        std::string name = req_json.get("name", std::string("NewUser"));
        std::string email = req_json.get("email", std::string("new@example.com"));

        std::lock_guard<std::mutex> lock(mtx);
        database->execute(
            "INSERT INTO users (name, email) VALUES ('" + name + "', '" + email + "');"
        );

        // Fetch last inserted user
        auto rows = database->query("SELECT id, name, email FROM users ORDER BY id DESC LIMIT 1;");
        User u = recordToUser(rows.front());

        json::Json resp = json::Json::object();
        resp["status"] = "success";
        resp["user"] = userToJson(u);
        res.body = json::stringify(resp);
        return res;
    }

    http::Response updateUser(const http::Request& req) {
        Log(logger::level::debug_, "updateUser got called");
        http::Response res;
        res.headers["Content-Type"] = "application/json";

        auto req_json = json::parse(req.body);
        std::string path = req.path;
        size_t pos = path.find_last_of('/');
        int id = std::stoi(path.substr(pos + 1));

        std::string name = req_json.get("name", std::string(""));
        std::string email = req_json.get("email", std::string(""));

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

    http::Response deleteUser(const http::Request& req) {
        Log(logger::level::debug_, "deleteUser got called");
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

        database->execute("DELETE FROM users WHERE id = " + std::to_string(id) + ";");

        json::Json resp = json::Json::object();
        resp["status"] = "success";
        resp["message"] = "User deleted";
        res.body = json::stringify(resp);
        return res;
    }
};
