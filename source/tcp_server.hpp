#pragma once

#include "async/async.hpp"

#include <fmt/core.h>
#include <tl/expected.hpp>

#include <optional>
#include <string>

class TcpConnection
{
public:
    TcpConnection(async::TcpSocket&& socket)
        : m_socket{ std::move(socket) }
    {
    }
    TcpConnection() = default;

    async::Awaitable<async::Expect<std::size_t>> send(const std::string& message)
    {
        assert(m_socket.has_value() && "TcpConnection should be valid!");
        return m_socket->async_send(asio::buffer(message));
    }

    async::Awaitable<async::Expect<std::string>> receive(std::size_t size)
    {
        assert(m_socket.has_value() && "TcpConnection should be valid!");
        std::string buffer(size, '\0');
        auto        res = co_await m_socket->async_read_some(asio::buffer(buffer));
        co_return res.transform([&](auto n) { return buffer.substr(0, n); });
    }

    std::string address() const
    {
        assert(m_socket.has_value() && "TcpConnection should be valid!");
        return m_socket->remote_endpoint().address().to_string();
    }

    void cancel() { m_socket->cancel(); }
    void close() { m_socket->close(); }

private:
    std::optional<async::TcpSocket> m_socket;    // optional to be able to default-construct TcpConnection
};

class TcpServer
{
public:
    TcpServer(async::IoContext& context, unsigned short port)
        : m_context{ context }
        , m_acceptor{ m_context, { async::tcp::v4(), port } }
        , m_port{ port }
    {
        m_acceptor.set_option(async::tcp::acceptor::reuse_address(true));
        m_acceptor.listen();
    }

    ~TcpServer()
    {
        if (m_acceptor.is_open()) {
            stop();
        }
    }

    async::Awaitable<async::Expect<TcpConnection>> listen()
    {
        auto result = co_await m_acceptor.async_accept();
        co_return result.transform([](auto&& sock) { return TcpConnection{ std::move(sock) }; });
    }

    void stop()
    {
        m_acceptor.cancel();
        m_acceptor.close();
    }

    unsigned short getPort() { return m_port; }

private:
    async::IoContext&  m_context;
    async::TcpAcceptor m_acceptor;

    const unsigned short m_port;
};
