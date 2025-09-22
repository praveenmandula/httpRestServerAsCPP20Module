export module userHandler;

import <string>;
import <vector>;
import <mutex>;
import <algorithm>;

import http;
import json;
import logger;

export struct User {
    int id;
    std::string name;
    std::string email;
};

export class userHandler {
private:
    std::vector<User> users;
    int nextId;
    std::mutex mtx;

    json::Json userToJson(const User& u) {
        json::Json j = json::Json::object();
        j["id"] = static_cast<double>(u.id); // store numbers as double
        j["name"] = u.name;
        j["email"] = u.email;
        return j;
    }

public:
    userHandler() : nextId(3) {
        users.push_back({ 1, "Alice", "alice@example.com" });
        users.push_back({ 2, "Bob", "bob@example.com" });
    }

    http::Response getUsers(const http::Request& req) {
        Log(logger::level::debug_, "getUsers got called");
        http::Response res;
        res.headers["Content-Type"] = "application/json";
        std::lock_guard<std::mutex> lock(mtx);
        json::Json arr_json = json::Json::array();
        for (const auto& u : users) arr_json.push_back(userToJson(u));
        res.body = json::stringify(arr_json);
        return res;
    }

    http::Response createUser(const http::Request& req) {
        Log(logger::level::debug_, "createUser got called");
        http::Response res;
        res.headers["Content-Type"] = "application/json";
        auto req_json = json::parse(req.body);

        User u;
        {
            std::lock_guard<std::mutex> lock(mtx);
            u.id = nextId++;
            u.name = req_json.get("name", std::string("NewUser"));
            u.email = req_json.get("email", std::string("new@example.com"));
            users.push_back(u);
        }

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

        // Extract user id from path (last part after /)
        std::string path = req.path;
        size_t pos = path.find_last_of('/');
        int id = std::stoi(path.substr(pos + 1));

        std::lock_guard<std::mutex> lock(mtx);
        for (auto& user : users) {
            if (user.id == id) {
                user.name = req_json.get("name", user.name);
                user.email = req_json.get("email", user.email);

                json::Json resp = json::Json::object();
                resp["status"] = "success";
                resp["user"] = userToJson(user);
                res.body = json::stringify(resp);
                return res;
            }
        }

        res.body = R"({ "status":"error","message":"User not found" })";
        return res;
    }

    http::Response deleteUser(const http::Request& req) {
        Log(logger::level::debug_, "deleteUser got called");
        http::Response res;
        res.headers["Content-Type"] = "application/json";

        // Extract user id from path
        std::string path = req.path;
        size_t pos = path.find_last_of('/');
        int id = std::stoi(path.substr(pos + 1));

        std::lock_guard<std::mutex> lock(mtx);
        auto it = std::remove_if(users.begin(), users.end(),
            [&](const User& u) { return u.id == id; });

        if (it != users.end()) {
            users.erase(it, users.end());
            json::Json resp = json::Json::object();
            resp["status"] = "success";
            resp["message"] = "User deleted";
            res.body = json::stringify(resp);
        }
        else {
            res.body = R"({ "status":"error","message":"User not found" })";
        }
        return res;
    }
};
