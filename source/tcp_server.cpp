#include "tcp_server.hpp"

#include <spdlog/spdlog.h>

#include <utility>

// TcpConnection

TcpConnection::TcpConnection(TcpServer* server)
    : m_id{ ++s_idCounter }
    , m_server{ server }
{
}

TcpConnection::~TcpConnection()
{
    assert(m_server && "TcpServer should outlive any TcpConnection!");
    if (m_id != 0 && !m_server->removeConnection(m_id)) {
        spdlog::warn("[{}] Somehow the connection is already removed???", m_cachedAddr);
    }
}

TcpConnection::TcpConnection(TcpConnection&& other) noexcept
    : m_id{ std::exchange(other.m_id, 0) }
    , m_server{ other.m_server }
{
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept
{
    if (this != &other) {
        m_id     = std::exchange(other.m_id, 0);
        m_server = other.m_server;
    }
    return *this;
}

async::Awaitable<async::Expect<std::size_t>> TcpConnection::send(const std::string& message)
{
    auto* socket = m_server->getConnection(m_id);
    assert(socket && "TcpConnection should be valid!");
    return socket->async_send(asio::buffer(message));
}

async::Awaitable<async::Expect<std::string>> TcpConnection::receive(std::size_t size)
{
    auto* socket = m_server->getConnection(m_id);
    assert(socket && "TcpConnection should be valid!");

    std::string buffer(size, '\0');
    auto        res = co_await socket->async_read_some(asio::buffer(buffer));

    co_return res.transform([&](auto n) { return buffer.substr(0, n); });
}

async::Awaitable<void> TcpConnection::broadcast(const std::string& message)
{
    for (auto& [id, sock] : m_server->getConnections()) {
        if (id != m_id) {
            // ignore errors, since it's just a broadcast
            std::ignore = co_await sock.async_send(asio::buffer(message));
        }
    }
}

std::string TcpConnection::address(bool invalidateCache)
{
    if (!m_cachedAddr.empty() && !invalidateCache) {
        return m_cachedAddr;
    }

    auto* socket = m_server->getConnection(m_id);
    assert(socket && "TcpConnection should be valid!");

    std::error_code errc;
    auto            remote = socket->remote_endpoint(errc);

    m_cachedAddr = errc ? "<unknown>" : fmt::format("{}:{}", remote.address().to_string(), remote.port());
    return m_cachedAddr;
}

std::string TcpConnection::lastAddress() const
{
    return m_cachedAddr;
}

// TcpServer

TcpServer::TcpServer(async::IoContext& context, unsigned short port)
    : m_context{ context }
    , m_acceptor{ m_context, { async::tcp::v4(), port } }
    , m_port{ port }
{
    m_acceptor.set_option(async::tcp::acceptor::reuse_address(true));
    m_acceptor.listen();
}

TcpServer::~TcpServer()
{
    if (m_acceptor.is_open()) {
        stop();
    }
}

async::Awaitable<async::Expect<TcpConnection>> TcpServer::listen()
{
    auto result = co_await m_acceptor.async_accept();
    co_return result.transform([this](auto&& sock) {
        TcpConnection conn{ this };
        m_connections.emplace(conn.getId(), std::move(sock));
        return conn;
    });
}

void TcpServer::stop()
{
    m_acceptor.cancel();
    m_acceptor.close();
}

async::TcpSocket* TcpServer::getConnection(TcpConnection::Id id)
{
    auto find = m_connections.find(id);
    if (find != m_connections.end()) {
        return &find->second;
    }
    return nullptr;
}

bool TcpServer::removeConnection(TcpConnection::Id id)
{
    return m_connections.erase(id) != 0;
}
