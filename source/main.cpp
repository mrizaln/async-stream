#include "async/async.hpp"
#include "tcp_server.hpp"

#include <fmt/core.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <exception>
#include <string>
#include <thread>
#include <tuple>

#define MAX_MESSAGE_SIZE 1024

std::atomic<bool> g_interrupt{ false };

std::string& removeNewLine(std::string& str)
{
    if (!str.empty() && str.back() == '\n') {
        str.pop_back();
    }
    if (!str.empty() && str.back() == '\r') {
        str.pop_back();
    }
    return str;
}

async::Awaitable<void> handle(TcpConnection&& conn)
{
    std::ignore = conn.address(true);

    while (true) {
        if (auto n = co_await conn.send("> Pick a name: "); !n.has_value()) {
            spdlog::error("[{}] Failed to send message: {}", conn.lastAddress(), n.error().message());
            spdlog::info("[{}] Connection closed", conn.lastAddress());
            break;
        } else if (n == 0) {
            spdlog::info("[{}] Connection closed", conn.lastAddress());
            break;
        }

        auto message = co_await conn.receive(MAX_MESSAGE_SIZE);
        if (!message.has_value()) {
            spdlog::error("[{}] Failed to receive message: {}", conn.lastAddress(), message.error().message());
            break;
        } else {
            removeNewLine(*message);
            spdlog::debug("[{}] Name: {:?}", conn.lastAddress(), *message);
        }

        auto allAlphaNum = std::ranges::all_of(*message, [](char c) { return std::isalnum(c); });
        if (!allAlphaNum) {
            if (auto n = co_await conn.send("Invalid name. Only alphanumeric characters are allowed.\n");
                !n.has_value()) {
                break;
            }
            continue;
        } else {
            conn.setName(std::move(*message));
            co_await conn.send(fmt::format("Welcome, {}!\n", conn.getName()));
            co_await conn.broadcast(fmt::format("[<Server>]: {} joined the chat!\n", conn.getName()));
        }

        async::SteadyTimer timer{ co_await async::this_coro::executor };

        while (true) {
            auto message = co_await conn.receive(MAX_MESSAGE_SIZE);
            if (!message.has_value()) {
                spdlog::error("[{}] Failed to receive message: {}", conn.lastAddress(), message.error().message());
                break;
            }

            co_await conn.broadcast(fmt::format("[{}]: {}\n", conn.getName(), removeNewLine(*message)));
        }

        co_await conn.broadcast(fmt::format("[Server]: {} left the chat!\n", conn.getName()));
    }
};

async::Awaitable<void> accept(TcpServer& server)
{
    spdlog::info("[Server] Listening at port: {}", server.getPort());
    while (!g_interrupt) {
        auto conn = co_await server.listen();

        if (!conn.has_value()) {
            spdlog::error("[Server] Failed to listen: {}", conn.error().message());
            break;
        }
        spdlog::info("[Server] New connection from {}", conn->address(false));

        auto context = co_await async::this_coro::executor;
        async::spawn(context, [conn = std::move(conn).value()]() mutable -> async::Awaitable<void> {
            co_await handle(std::move(conn));
        });
    }
}

int main()
{
    spdlog::set_default_logger(spdlog::stderr_color_st("logger"));
    spdlog::set_pattern("[%H:%M:%S %z] [%^-%L-%$] %v");
    spdlog::set_level(spdlog::level::debug);

    fmt::println("Hello from 'async-stream'!");
    std::signal(SIGINT, [](int) {
        g_interrupt = true;
        g_interrupt.notify_all();
    });

    async::IoContext context;
    TcpServer        server{ context, 55555 };

    async::spawn(context, accept(server), [](std::exception_ptr e_ptr) {
        if (e_ptr) {
            g_interrupt = true;
            std::rethrow_exception(e_ptr);
        } else {
            spdlog::info("[Server] Ended safely");
        }
    });

    std::jthread couroutinesThread{ [&] { context.run(); } };

    g_interrupt.wait(false);    // wait for interrupt

    server.stop();
    context.stop();

    return 0;
}
