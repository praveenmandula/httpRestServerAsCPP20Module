export module http;

import logger;
import threadpool;

import <string>;
import <unordered_map>;
import <functional>;
import <thread>;
import <atomic>;
import <mutex>;
import <iostream>;
import <sstream>;
import <vector>;
import <cstring>;
import <csignal>;

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socket_t = SOCKET;
#define CLOSESOCK closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
using socket_t = int;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#define CLOSESOCK close
#endif

export namespace http {

    struct Request {
        std::string method;
        std::string path;
        std::string version;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    struct Response {
        int status = 200;
        std::string reason;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    using Handler = std::function<Response(const Request&)>;

    namespace detail {
        inline std::string reason_phrase(int status) {
            switch (status) {
            case 200: return "OK";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            default: return "";
            }
        }
    }

    class Server {
    public:
        explicit Server(int port,
            int backlog = 64,
            int io_in_threads = 4,
            int worker_threads = 8,
            int io_out_threads = 4,
            size_t max_header_bytes = 32 * 1024,
            size_t max_body_bytes = 4 * 1024 * 1024)
            : port_(port), backlog_(backlog),
            max_header_bytes_(max_header_bytes), max_body_bytes_(max_body_bytes),
            running_(false), listen_sock_(INVALID_SOCKET),
            io_in_pool_(io_in_threads),
            worker_pool_(worker_threads),
            io_out_pool_(io_out_threads) {
        }

        ~Server() { stop(); }

        void start() {
#ifdef _WIN32
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
                Log(logger::level::error, "WSAStartup failed");
                throw std::runtime_error("WSAStartup failed");
            }
#endif
            listen_sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
            if (listen_sock_ == INVALID_SOCKET) {
                Log(logger::level::error, "create socket failed");
                throw std::runtime_error("socket() failed");
            }

#ifdef _WIN32
            int opt = 1;
            setsockopt(listen_sock_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*)&opt, sizeof(opt));
#else
            int opt = 1;
            setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port_);

            if (::bind(listen_sock_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
                CLOSESOCK(listen_sock_);
                Log(logger::level::error, "socket bind failed");
                throw std::runtime_error("bind() failed");
            }

            if (::listen(listen_sock_, backlog_) == SOCKET_ERROR) {
                CLOSESOCK(listen_sock_);
                Log(logger::level::error, "listen failed");
                throw std::runtime_error("listen() failed");
            }

            running_ = true;
            accept_thread_ = std::thread(&Server::poll_loop, this);
        }

        void stop() {
            if (!running_) return;
            running_ = false;
            if (listen_sock_ != INVALID_SOCKET) {
                CLOSESOCK(listen_sock_);
                listen_sock_ = INVALID_SOCKET;
            }
            if (accept_thread_.joinable()) accept_thread_.join();
#ifdef _WIN32
            WSACleanup();
#endif
        }

        void add_route(const std::string& path, Handler h) {
            std::lock_guard<std::mutex> lk(route_mtx_);
            routes_[path] = std::move(h);
        }

    private:
#ifdef _WIN32
        using poll_count_t = ULONG;
#else
        using poll_count_t = nfds_t;
#endif

        void poll_loop() {
            std::vector<pollfd> fds;
            fds.push_back({ listen_sock_, POLLIN, 0 });

            while (running_) {
#ifdef _WIN32
                int n = WSAPoll(fds.data(), static_cast<poll_count_t>(fds.size()), 1000);
#else
                int n = ::poll(fds.data(), static_cast<poll_count_t>(fds.size()), 1000);
#endif
                if (n <= 0) continue;

                for (size_t i = 0; i < fds.size(); ++i) {
                    if (!(fds[i].revents & POLLIN)) continue;

                    if (fds[i].fd == listen_sock_) {
                        sockaddr_in client{};
                        socklen_t len = sizeof(client);
                        socket_t s = ::accept(listen_sock_, (sockaddr*)&client, &len);
                        if (s != INVALID_SOCKET) fds.push_back({ s, POLLIN, 0 });
                    }
                    else {
                        socket_t s = fds[i].fd;

                        // Push socket read & parse to I/O-In pool
                        io_in_pool_.Enqueue([this, s]() {
                            std::string raw;
                            if (!recv_until_crlfcrlf(s, raw, max_header_bytes_)) { CLOSESOCK(s); return; }

                            Request req;
                            size_t header_end = 0;
                            if (!parse_request_headers(raw, req, header_end)) { CLOSESOCK(s); return; }

                            auto it = req.headers.find("Content-Length");
                            if (it != req.headers.end()) {
                                size_t content_length = std::stoul(it->second);
                                req.body = raw.substr(header_end + 4);
                                while (req.body.size() < content_length) {
                                    char buf[4096];
                                    int n = ::recv(s, buf, sizeof(buf), 0);
                                    if (n <= 0) { CLOSESOCK(s); return; }
                                    req.body.append(buf, n);
                                }
                            }

                            // Push business logic to worker pool
                            worker_pool_.Enqueue([this, req = std::move(req), s]() mutable {
                                Response res = dispatch(req);

                                // Push response sending to I/O-Out pool
                                io_out_pool_.Enqueue([s, res = std::move(res)]() mutable {
                                    send_response(s, res);
                                    CLOSESOCK(s);
                                    });
                                });
                            });

                        fds.erase(fds.begin() + i);
                        --i;
                    }
                }
            }
        }

        Response dispatch(const Request& req) {
            Handler h;
            {
                std::lock_guard<std::mutex> lk(route_mtx_);
                auto it = routes_.find(req.path);
                if (it != routes_.end()) h = it->second;
                else {
                    for (auto& [route, handler] : routes_) {
                        if (req.path.rfind(route + "/", 0) == 0) { h = handler; break; }
                    }
                }
            }
            if (h) return h(req);
            Response r;
            r.status = 404;
            r.reason = "Not Found";
            r.body = "Not Found\n";
            Log(logger::level::debug_, "Route not found: ", req.path);
            return r;
        }

        static bool recv_until_crlfcrlf(socket_t s, std::string& out, size_t max_bytes) {
            char buf[1024];
            while (out.find("\r\n\r\n") == std::string::npos) {
                int n = ::recv(s, buf, sizeof(buf), 0);
                if (n <= 0) return false;
                out.append(buf, n);
                if (out.size() > max_bytes) return false;
            }
            return true;
        }

        static bool parse_request_headers(const std::string& raw, Request& req, size_t& header_end) {
            header_end = raw.find("\r\n\r\n");
            if (header_end == std::string::npos) return false;
            std::istringstream iss(raw.substr(0, header_end));
            std::string line;
            if (!std::getline(iss, line)) return false;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream start(line);
            start >> req.method >> req.path >> req.version;

            while (std::getline(iss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) break;
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string key = line.substr(0, pos);
                    std::string val = line.substr(pos + 1);
                    if (!val.empty() && val[0] == ' ') val.erase(0, 1);
                    req.headers[key] = val;
                }
            }
            return true;
        }

        static void send_response(socket_t s, const Response& res) {
            std::ostringstream oss;
            std::string reason = res.reason.empty() ? detail::reason_phrase(res.status) : res.reason;
            oss << "HTTP/1.1 " << res.status << ' ' << reason << "\r\n";
            for (auto& [k, v] : res.headers) oss << k << ": " << v << "\r\n";
            oss << "Content-Length: " << res.body.size() << "\r\n";
            oss << "Connection: close\r\n\r\n";
            oss << res.body;
            send_all(s, oss.str());
        }

        static void send_all(socket_t s, const std::string& data) {
            const char* p = data.data();
            size_t left = data.size();
            while (left > 0) {
                int n = ::send(s, p, static_cast<int>(left), 0);
                if (n <= 0) break;
                left -= n;
                p += n;
            }
        }

        int port_;
        int backlog_;
        size_t max_header_bytes_;
        size_t max_body_bytes_;

        std::atomic<bool> running_;
        socket_t listen_sock_;
        std::thread accept_thread_;

        std::mutex route_mtx_;
        std::unordered_map<std::string, Handler> routes_;

        ThreadPool io_in_pool_;
        ThreadPool worker_pool_;
        ThreadPool io_out_pool_;
    };

} // namespace http
