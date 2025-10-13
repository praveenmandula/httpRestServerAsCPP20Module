export module os;

import <filesystem>;
import <vector>;
import <system_error>;

// import cpp specific modules
import <iostream>;
import <string>;
import <fstream>;
import <sstream>;

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

export namespace os {
    // Returns the absolute path to the running executable.
    inline std::filesystem::path get_executable_path() {
#if defined(_WIN32)
        // Use wide-char API to handle Unicode paths; grow buffer if needed.
        std::vector<wchar_t> buf(512);
        for (;;) {
            DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
            if (len == 0) {
                // Fallback: current_path
                return std::filesystem::current_path();
            }
            if (len < buf.size() - 1) {
                // Success
                return std::filesystem::path(std::wstring(buf.data(), len));
            }
            // Buffer too small; enlarge and retry.
            buf.resize(buf.size() * 2);
        }

#elif defined(__APPLE__)
        // _NSGetExecutablePath may return a path with symlinks; realpath to canonicalize.
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size); // size needed
        std::vector<char> buf(size + 1);
        if (_NSGetExecutablePath(buf.data(), &size) != 0) {
            return std::filesystem::current_path();
        }
        // Resolve to real, absolute path if possible.
        char resolved[PATH_MAX];
        if (realpath(buf.data(), resolved)) {
            return std::filesystem::path(resolved);
        }
        return std::filesystem::path(buf.data());

#else
        // Linux & other ELF platforms with /proc
        std::vector<char> buf(1024);
        for (;;) {
            ssize_t len = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
            if (len == -1) {
                return std::filesystem::current_path();
            }
            if (static_cast<size_t>(len) < buf.size() - 1) {
                buf[static_cast<size_t>(len)] = '\0';
                return std::filesystem::path(buf.data());
            }
            buf.resize(buf.size() * 2);
        }
#endif
    }

    // Returns the directory containing the running executable.
    inline std::filesystem::path get_executable_dir() {
        return get_executable_path().parent_path();
    }

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

} // namespace os
