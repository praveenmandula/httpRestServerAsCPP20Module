// import our user-defined modules
import logger;
import http;
import userHandler;
import os;

// import cpp specific modules
import <iostream>;
import <string>;
import <fstream>;
import <sstream>;

int main() {
    // configure log level
    logger::logger::get().set_level(logger::level::debug_);

    // create usrHndlr object to handle users related rest request
    userHandler usrhndlr;

    // Create a server on port 8080, backlog=64, threads=8
    http::Server server(8080, 64, 8);

    // Add HomePage Route
    server.add_route("/", [](const http::Request&) {
        Log(logger::level::debug_, "Home page got called");
        http::Response res;
        res.headers["Content-Type"] = "text/html; charset=utf-8";
        res.body = os::read_file("index.html");  // load homepage from file
        return res;
    });

    server.add_route("/api/users", [&](const http::Request& req) {
        Log(logger::level::debug_, "Method" , req.method," Path=",req.path);

        if (req.method == "GET") {
            return usrhndlr.getUsers(req);
        }
        if (req.method == "POST") {
            return usrhndlr.createUser(req);
        }
        if (req.method == "PUT") {
            return usrhndlr.updateUser(req);
        }
        if (req.method == "DELETE") {
            return usrhndlr.deleteUser(req);
        }

        http::Response res;
        res.status = 404;
        res.body = R"({ "status":"error","message":"Not Found" })";
        return res;
    });

    // Start server (runs poll loop in background thread)
    server.start();

    Log(logger::level::info, "Server running on http://localhost:8080");
    Log(logger::level::info, "Press ENTER to stop...");

    std::cin.get(); // just wait here

    // Stop server gracefully
    server.stop();
}
