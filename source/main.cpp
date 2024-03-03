#include "async/async.hpp"
#include "tcp_server2.hpp"

#include <atomic>
#include <csignal>
#include <exception>
#include <fmt/core.h>
#include <thread>

std::atomic<bool> g_interrupt{ false };

async::Awaitable<void> echo(TcpServer& server)
{
    while (!g_interrupt) {
        fmt::println("Server listening at port: {}", server.getPort());
        auto conn = co_await server.listen();

        if (!conn.has_value()) {
            fmt::println("Server failed to listen: {}", conn.error().message());
            break;
        }
        fmt::println("Accepted connection from {}", conn->address());

        auto message = co_await conn->receive(1024);
        auto n       = co_await conn->send(message.value());
    }
}

void sigHandler(int /* sig */)
{
    g_interrupt = true;
    g_interrupt.notify_all();
}

int main()
{
    fmt::println("Hello from 'async-stream'!");
    std::signal(SIGINT, sigHandler);

    async::IoContext context;
    TcpServer        server{ context, 55555 };

    async::co_spawn(context, echo(server), [](std::exception_ptr e_ptr) {
        if (e_ptr) {
            g_interrupt = true;
            std::rethrow_exception(e_ptr);
        } else {
            fmt::println("Server ended safely");
        }
    });

    std::jthread couroutinesThread{ [&] { context.run(); } };

    g_interrupt.wait(false);

    server.stop();
    context.stop();

    return 0;
}
