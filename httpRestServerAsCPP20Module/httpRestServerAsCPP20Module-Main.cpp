// import our user-defined modules
import logger;
import http;
import odbc;
import dbhelper;
import userHandler;
import os;

// import cpp specific modules
import <iostream>;
import <string>;
import <filesystem>;
import <fstream>;
import <sstream>;


inline std::string read_file(const std::string& relative_path) {
    std::filesystem::path base = os::get_executable_dir();
    std::filesystem::path full_path = base / "FrontEnd" / relative_path;

    std::ifstream file(full_path, std::ios::binary);
    if (!file) {
        return "<h1>404 Not Found</h1><p>File: " + full_path.string() + "</p>";
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int checkDB() {
// Without helper
#if 1
    auto db = createDatabase();
    std::string connStr = "DSN=MySqlExpress;Trusted_Connection=Yes;";

    if (!db->connect(connStr)) {
        std::cerr << "Failed to connect\n";
        return 1;
    }

    db->execute("IF OBJECT_ID('TestTable', 'U') IS NULL "
        "CREATE TABLE TestTable (id INT PRIMARY KEY, name NVARCHAR(100));");

    db->execute("INSERT INTO TestTable (id, name) VALUES (1, 'Windows Auth Row');");

    QueryResult res = db->query("SELECT id, name FROM TestTable;");
    if (!res.success) {
        std::cerr << "Query failed\n";
    }
    else {
        for (auto& row : res.rows) {
            for (auto& col : row) std::cout << col << " ";
            std::cout << "\n";
        }
    }
    db->disconnect();
#endif
 // with helper
#if 0
    // Create the database object (ODBC implementation)
    auto db = createDatabase();

    // Connect using your DSN (Windows Authentication)
    std::string connStr = "DSN=MySqlExpress;Trusted_Connection=Yes;";
    if (!db->connect(connStr)) {
        std::cerr << "Failed to connect to database\n";
        return 1;
    }

    // Create the helper
    auto helper = createDatabaseHelper(std::move(db));

    // Create a transactions table
    if (!helper->createTable("transactions", {
            {"id", "INT PRIMARY KEY"},
            {"amount", "DECIMAL(10,2)"},
            {"status", "VARCHAR(20)"},
            {"created_at", "DATETIME"}
        }))
    {
        std::cerr << "Failed to create table\n";
        return 1;
    }

    // Insert a sample transaction
    if (!helper->insert("transactions",
        { "id", "amount", "status", "created_at" },
        { "1", "100.50", "pending", "2025-08-29 16:30:00" }))
    {
        std::cerr << "Failed to insert transaction\n";
        return 1;
    }

    // Update transaction status
    if (!helper->update("transactions",
        { {"status", "completed"} },
        "id = 1"))
    {
        std::cerr << "Failed to update transaction\n";
        return 1;
    }

    // Query transactions
    auto result = helper->select("transactions", { "id", "amount", "status", "created_at" });
    if (!result.success) {
        std::cerr << "Failed to query transactions\n";
        return 1;
    }

    // Print results
    std::cout << "Transactions:\n";
    for (auto& row : result.rows) {
        for (auto& col : row) std::cout << col << " ";
        std::cout << "\n";
    }
#endif
    return 0;
}

int main() {
    // configure log level
    logger::logger::get().set_level(logger::level::debug_);

    userHandler usrhndlr;

    // Create a server on port 8080, backlog=64, threads=8
    http::Server server(8080, 64, 8);

    // Add HomePage Route
    server.add_route("/", [](const http::Request&) {
        Log(logger::level::debug_, "Home page got called");
        http::Response res;
        res.headers["Content-Type"] = "text/html; charset=utf-8";
        res.body = read_file("index.html");  // load homepage from file
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
