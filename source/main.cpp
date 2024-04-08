#include "async/async.hpp"
#include "tcp_server.hpp"

#include <fmt/core.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>

std::atomic<bool> g_interrupt{ false };

std::optional<long> toLong(std::string_view str)
{
    try {
        return std::stol(std::string{ str });
    } catch (const std::exception&) {
        return {};
    }
}

async::Awaitable<void> handle(TcpConnection&& conn)
{
    std::ignore = conn.address(true);

    while (true) {
        if (auto n = co_await conn.send("> Enter a number: "); !n.has_value()) {
            spdlog::error("[{}] Failed to send message: {}", conn.lastAddress(), n.error().message());
            spdlog::info("[{}] Connection closed", conn.lastAddress());
            break;
        } else if (n == 0) {
            spdlog::info("[{}] Connection closed", conn.lastAddress());
            break;
        }

        auto message = co_await conn.receive(1024);
        if (!message.has_value()) {
            spdlog::error("[{}] Failed to receive message: {}", conn.lastAddress(), message.error().message());
            break;
        }

        auto number = toLong(*message);
        if (!number.has_value()) {
            co_await conn.send("Invalid value!\n");
            continue;
        }

        async::SteadyTimer timer{ co_await async::this_coro::executor };

        auto now   = std::chrono::steady_clock::now();
        auto delta = std::chrono::milliseconds{ 0 };

        for (; number > 0; --number.value()) {
            using namespace std::chrono_literals;
            using namespace async::operators;

            auto newNow = std::chrono::steady_clock::now();
            delta       = std::chrono::duration_cast<std::chrono::milliseconds>(newNow - now);
            now         = newNow;

            timer.expires_after(100ms);

            auto [e1, e2] = co_await (
                conn.send(fmt::format("Here is your number: {} ({}ms)\n", *number, delta.count()))    //
                && timer.async_wait()
            );

            if (!e1.has_value()) {
                spdlog::error("[{}] Failed to send message: {}", conn.lastAddress(), e1.error().message());
                break;
            } else if (!e2.has_value()) {
                spdlog::error("[{}] Failed to wait for timer: {}", conn.lastAddress(), e2.error().message());
                break;
            }
        }
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
