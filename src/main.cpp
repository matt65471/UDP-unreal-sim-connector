#include "rocket/telemetry.hpp"

#include <csignal>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {

volatile std::sig_atomic_t keep_running = 1;

void stop(int) {
    keep_running = 0;
}

struct Options {
    std::string bind_address = "0.0.0.0";
    std::uint16_t port = 5762;
    std::string log_path = "telemetry.jsonl";
    bool log_enabled = true;
    bool raw = false;
};

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket invalid_socket = INVALID_SOCKET;

std::string socket_error(const char* operation) {
    return std::string(operation) + " failed with Winsock error " +
           std::to_string(WSAGetLastError());
}

class SocketRuntime {
public:
    SocketRuntime() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }
    ~SocketRuntime() { WSACleanup(); }
    SocketRuntime(const SocketRuntime&) = delete;
    SocketRuntime& operator=(const SocketRuntime&) = delete;
};
#else
using NativeSocket = int;
constexpr NativeSocket invalid_socket = -1;

std::string socket_error(const char* operation) {
    return std::string(operation) + " failed: " + std::strerror(errno);
}

class SocketRuntime {
public:
    SocketRuntime() = default;
};
#endif

void print_usage(const char* executable) {
    std::cout
        << "Usage: " << executable << " [options]\n"
        << "  --bind ADDRESS   Local interface to bind (default 0.0.0.0)\n"
        << "  --port PORT      UDP port to listen on (default 5762)\n"
        << "  --log PATH       Append raw packets to PATH (default telemetry.jsonl)\n"
        << "  --no-log         Do not write a capture file\n"
        << "  --raw            Print each raw JSON packet\n"
        << "  --help           Show this help\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        auto require_value = [&](const char* option) -> std::string {
            if (++index >= argc) {
                throw std::runtime_error(std::string(option) + " requires a value");
            }
            return argv[index];
        };

        if (argument == "--bind") {
            options.bind_address = require_value("--bind");
        } else if (argument == "--port") {
            const unsigned long port = std::stoul(require_value("--port"));
            if (port == 0 || port > 65535) {
                throw std::runtime_error("port must be between 1 and 65535");
            }
            options.port = static_cast<std::uint16_t>(port);
        } else if (argument == "--log") {
            options.log_path = require_value("--log");
        } else if (argument == "--no-log") {
            options.log_enabled = false;
        } else if (argument == "--raw") {
            options.raw = true;
        } else if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("unknown option: " + argument);
        }
    }
    return options;
}

class SocketHandle {
public:
    explicit SocketHandle(NativeSocket descriptor) : descriptor_(descriptor) {}
    ~SocketHandle() {
        if (descriptor_ != invalid_socket) {
#ifdef _WIN32
            ::closesocket(descriptor_);
#else
            ::close(descriptor_);
#endif
        }
    }
    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    NativeSocket get() const { return descriptor_; }

private:
    NativeSocket descriptor_;
};

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        [[maybe_unused]] SocketRuntime socket_runtime;

        const NativeSocket descriptor = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (descriptor == invalid_socket) {
            throw std::runtime_error(socket_error("socket"));
        }
        SocketHandle socket(descriptor);

        sockaddr_in local_address{};
        local_address.sin_family = AF_INET;
        local_address.sin_port = htons(options.port);
        if (::inet_pton(AF_INET, options.bind_address.c_str(), &local_address.sin_addr) != 1) {
            throw std::runtime_error("invalid IPv4 bind address: " + options.bind_address);
        }

        if (::bind(socket.get(), reinterpret_cast<const sockaddr*>(&local_address),
                   sizeof(local_address)) < 0) {
            throw std::runtime_error(socket_error("bind"));
        }

        // Wake periodically so Ctrl-C can stop the process even on platforms that
        // automatically restart recvfrom() after a signal.
#ifdef _WIN32
        const DWORD receive_timeout_ms = 250;
        if (::setsockopt(socket.get(), SOL_SOCKET, SO_RCVTIMEO,
                         reinterpret_cast<const char*>(&receive_timeout_ms),
                         sizeof(receive_timeout_ms)) < 0) {
            throw std::runtime_error(socket_error("setsockopt"));
        }
#else
        const timeval receive_timeout{0, 250000};
        if (::setsockopt(socket.get(), SOL_SOCKET, SO_RCVTIMEO, &receive_timeout,
                         sizeof(receive_timeout)) < 0) {
            throw std::runtime_error(socket_error("setsockopt"));
        }
#endif

        std::ofstream log;
        if (options.log_enabled) {
            log.open(options.log_path, std::ios::app);
            if (!log) {
                throw std::runtime_error("cannot open log file: " + options.log_path);
            }
        }

        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);

        std::cout << "Listening for Unreal rocket telemetry on "
                  << options.bind_address << ':' << options.port << '\n';
        if (options.log_enabled) {
            std::cout << "Appending raw JSON packets to " << options.log_path << '\n';
        }
        std::cout << "Press Ctrl-C to stop.\n\n";

        std::uint64_t packet_count = 0;
        std::uint64_t invalid_count = 0;
        char buffer[65536];

        while (keep_running) {
            sockaddr_in sender{};
#ifdef _WIN32
            int sender_length = sizeof(sender);
            const int bytes_received = ::recvfrom(
                socket.get(), buffer, static_cast<int>(sizeof(buffer) - 1), 0,
                reinterpret_cast<sockaddr*>(&sender), &sender_length);
#else
            socklen_t sender_length = sizeof(sender);
            const ssize_t bytes_received = ::recvfrom(
                socket.get(), buffer, sizeof(buffer) - 1, 0,
                reinterpret_cast<sockaddr*>(&sender), &sender_length);
#endif

            if (bytes_received < 0) {
#ifdef _WIN32
                const int error = WSAGetLastError();
                if (error == WSAEINTR || error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
                    continue;
                }
#else
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
#endif
                throw std::runtime_error(socket_error("recvfrom"));
            }

            buffer[bytes_received] = '\0';
            std::string packet(buffer, static_cast<std::size_t>(bytes_received));
            while (!packet.empty() && (packet.back() == '\n' || packet.back() == '\r')) {
                packet.pop_back();
            }

            char sender_ip[INET_ADDRSTRLEN]{};
            ::inet_ntop(AF_INET, &sender.sin_addr, sender_ip, sizeof(sender_ip));

            if (log) {
                log << packet << '\n';
                log.flush();
            }

            std::string parse_error;
            const auto telemetry = rocket::parse_telemetry(packet, parse_error);
            if (!telemetry) {
                ++invalid_count;
                std::cerr << "Invalid packet from " << sender_ip << ':' << ntohs(sender.sin_port)
                          << ": " << parse_error << '\n';
                if (options.raw) {
                    std::cerr << packet << '\n';
                }
                continue;
            }

            ++packet_count;
            std::cout << '#' << std::setw(6) << std::setfill('0') << packet_count
                      << std::setfill(' ')
                      << " from=" << sender_ip
                      << " phase=" << std::left << std::setw(16) << telemetry->phase << std::right
                      << " met=" << std::fixed << std::setprecision(2)
                      << std::setw(8) << telemetry->mission_time_seconds << "s"
                      << " agl=" << std::setw(10) << std::setprecision(1)
                      << telemetry->altitude_agl_ft << "ft"
                      << " vs=" << std::showpos << std::setw(9)
                      << telemetry->vertical_speed_fps << std::noshowpos << "ft/s"
                      << " thrust=" << std::setw(7) << telemetry->thrust_lbf << "lbf\n";

            if (options.raw) {
                std::cout << packet << '\n';
            }
        }

        std::cout << "\nStopped after " << packet_count << " valid packet(s) and "
                  << invalid_count << " invalid packet(s).\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "udp_receiver: " << exception.what() << '\n';
        return 1;
    }
}
