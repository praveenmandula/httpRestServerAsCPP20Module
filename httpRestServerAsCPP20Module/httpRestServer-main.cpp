// import user-defined modules
import logger;
import http;
import userHandler;
import os;

// import C++ standard modules
import <iostream>;
import <string>;
import <fstream>;
import <sstream>;

int main() {
    // Configure logging
    logger::logger::get().set_level(logger::level::debug_);

    // Create user handler for REST endpoints
    userHandler usrhndlr;

    // Create HTTP server on port 8080
    http::Server server(8080, 64, 8);

    // === Serve Home Page ===
    server.add_route("/", [](const http::Request&) {
        Log(logger::level::debug_, "Home page requested");
        http::Response res;
        res.headers["Content-Type"] = "text/html; charset=utf-8";
        res.body = os::read_file("FrontEnd/index.html");
        return res;
        });

    // === Serve style.css ===
    server.add_route("/style.css", [](const http::Request&) {
        Log(logger::level::debug_, "CSS requested");
        http::Response res;
        res.headers["Content-Type"] = "text/css; charset=utf-8";
        res.body = os::read_file("FrontEnd/style.css");
        return res;
        });

    // === Serve script.js ===
    server.add_route("/script.js", [](const http::Request&) {
        Log(logger::level::debug_, "JavaScript requested");
        http::Response res;
        res.headers["Content-Type"] = "application/javascript; charset=utf-8";
        res.body = os::read_file("FrontEnd/script.js");
        return res;
        });

    // === REST API: /api/users ===
    server.add_route("/api/users", [&](const http::Request& req) {
        Log(logger::level::debug_, "API call: ", req.method, " ", req.path);

        if (req.method == "GET")
            return usrhndlr.getUsers(req);
        if (req.method == "POST")
            return usrhndlr.createUser(req);
        if (req.method == "PUT")
            return usrhndlr.updateUser(req);
        if (req.method == "DELETE")
            return usrhndlr.deleteUser(req);

        http::Response res;
        res.status = 404;
        res.body = R"({ "status":"error","message":"Not Found" })";
        return res;
        });

    // === Start server ===
    server.start();

    Log(logger::level::info, "Server running on http://localhost:8080");
    Log(logger::level::info, "Press ENTER to stop...");

    std::cin.get(); // wait for ENTER key

    // Graceful shutdown
    server.stop();
    Log(logger::level::info, "Server stopped.");

    return 0;
}
