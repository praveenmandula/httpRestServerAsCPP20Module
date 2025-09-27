export module logger;
import threadpool;   // your thread pool module

import <chrono>;
import <format>;
import <fstream>;
import <iostream>;
import <memory>;
import <mutex>;
import <source_location>;
import <string>;
import <string_view>;
import <vector>;
import <atomic>;
import <sstream>;
import <tuple>;
import <format>;   // for {} formatting
import <utility>;
import <filesystem>;
import <iomanip>;


export namespace logger {

    // ---------- Levels ----------
    export enum class level : uint8_t {
        trace, debug_, info, warn, error, critical, off
    };

    export inline std::string_view to_string(level l) {
        switch (l) {
        case level::trace:    return "TRACE";
        case level::debug_:   return "DEBUG";
        case level::info:     return "INFO";
        case level::warn:     return "WARN";
        case level::error:    return "ERROR";
        case level::critical: return "CRITICAL";
        case level::off:      return "OFF";
        }
        return "?";
    }

    // ---------- Sink interface ----------
    export struct sink {
        virtual ~sink() = default;
        virtual void write(std::string_view line, level lvl) = 0;
    };

    // ---------- Console sink ----------
    export class console_sink : public sink {
    public:
        void write(std::string_view line, level lvl) override {
            FILE* out = (lvl >= level::error) ? stderr : stdout;
            std::fwrite(line.data(), 1, line.size(), out);
            std::fwrite("\n", 1, 1, out);
            std::fflush(out);
        }
    };

    // ---------- File sink ----------
    export class file_sink : public sink {
    public:
        explicit file_sink(std::string path) : file_(path, std::ios::app) {}

        void write(std::string_view line, level) override {
            std::lock_guard lk(mtx_);
            file_ << line << "\n";
            file_.flush();
        }

    private:
        std::ofstream file_;
        std::mutex mtx_;
    };

    // ---------- Logger ----------
    export class logger {
    public:
        static logger& get() {
            static logger inst;
            return inst;
        }

        void set_level(level lvl) { min_level_.store(lvl); }

        void add_sink(std::unique_ptr<sink> s) {
            std::lock_guard lk(sink_mtx_);
            sinks_.push_back(std::move(s));
        }

        template <typename... Args>
        void log(level lvl,
            std::string_view module,
            Args&&... args,
            const std::source_location& loc = std::source_location::current())
        {
            if (lvl < min_level_.load()) return;

            auto module_copy = std::string(module);
            auto args_tuple = std::make_tuple(std::forward<Args>(args)...);

            pool_.Enqueue([=, this,
                module_copy = std::move(module_copy),
                args_tuple = std::move(args_tuple)]() mutable {
                std::ostringstream oss;
                std::apply([&](auto&&... unpacked) { (oss << ... << unpacked); }, args_tuple);

                auto msg = oss.str();
                auto line = format_line(lvl, module_copy + ": " + msg, loc);

                std::lock_guard lk(sink_mtx_);
                for (auto& s : sinks_) {
                    s->write(line, lvl);
                }
            });
        }

    private:
        logger(size_t workers = 2)
            : pool_(workers)  // thread pool with N workers
        {
            sinks_.push_back(std::make_unique<console_sink>());
        }

        std::string format_line(level lvl, const std::string& msg, const std::source_location& loc) {
            std::ostringstream oss;

            // filename without path or extension
            std::filesystem::path p(loc.file_name());

            // clean up function name (remove template args and namespaces)
            std::string fn = loc.function_name();
            auto tpos = fn.find('<');
            if (tpos != std::string::npos) fn = fn.substr(0, tpos); // remove template params
            auto npos = fn.rfind("::");
            if (npos != std::string::npos) fn = fn.substr(npos + 2);  // keep only function name

            oss << "[" << to_string(lvl) << "] "
                << p.stem().string()  // filename only
                << "::" << fn          // cleaned function name
                << " - " << msg;

            return oss.str();
        }

        std::vector<std::unique_ptr<sink>> sinks_;
        std::atomic<level> min_level_{ level::trace };
        std::mutex sink_mtx_;

        ThreadPool pool_;  // <--- using our own threadpool
    };

    // ---------- Exported Log helper Method  ----------
    
    // --------- Format-string style (with {}) ----------
    export template <typename... Args>
        inline void Log(level lvl, std::string_view fmt, Args&&... args) {
        logger::get().log(lvl, std::vformat(fmt, std::make_format_args(args...)));
    }

    // --------- Streaming style (like <<) ----------
    export template <typename... Args>
        inline void Log(level lvl, Args&&... args) {
        std::ostringstream oss;
        (oss << ... << args); // fold expression to stream all args
        logger::get().log(lvl, oss.str());
    }

} // namespace logx

// Usage
//logger::get().set_level(level::info);
//logger::get().add_sink(std::make_unique<file_sink>("httpServer.log"));
//Log(level::info, "Hello {}, {}", "world", 42);
//Log(level::debug_, "Debugging value = {}", 1234);
//Log(level::warn, "Warning about {}", "something");
//Log(level::error, "Error code {}", -99);