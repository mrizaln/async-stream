#pragma once

#include "async/async.hpp"

#include <fmt/core.h>
#include <tl/expected.hpp>

#include <string>
#include <unordered_map>

class TcpServer;

// proxy class for the connection
class TcpConnection
{
public:
    using Id = std::size_t;

    TcpConnection(TcpServer* server);
    ~TcpConnection();

    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    TcpConnection(const TcpConnection&)            = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    async::Awaitable<async::Expect<std::size_t>> send(const std::string& message);
    async::Awaitable<async::Expect<std::string>> receive(std::size_t size);

    async::Awaitable<void> broadcast(const std::string& message);

    std::string address(bool invalidateCache);
    std::string lastAddress() const;

    void               setName(std::string name) { m_name = std::move(name); }
    const std::string& getName() const { return m_name; }

    Id getId() const { return m_id; }

private:
    inline static Id s_idCounter = 0;

    Id          m_id;
    std::string m_cachedAddr;
    std::string m_name;
    TcpServer*  m_server;
};

class TcpServer
{
public:
    friend TcpConnection;

    TcpServer(async::IoContext& context, unsigned short port);
    ~TcpServer();

    TcpServer()                            = delete;
    TcpServer(TcpServer&&)                 = delete;
    TcpServer& operator=(TcpServer&&)      = delete;
    TcpServer(const TcpServer&)            = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    async::Awaitable<async::Expect<TcpConnection>> listen();

    void              stop();
    async::TcpSocket* getConnection(TcpConnection::Id id);
    bool              removeConnection(TcpConnection::Id id);

    unsigned short getPort() { return m_port; }

private:
    using ConnectionList = std::unordered_map<TcpConnection::Id, async::TcpSocket>;
    async::IoContext&  m_context;
    async::TcpAcceptor m_acceptor;

    ConnectionList m_connections;

    ConnectionList& getConnections() { return m_connections; }

    const unsigned short m_port;
};
