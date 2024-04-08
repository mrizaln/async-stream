#pragma once

#include "async/as_expected.hpp"

#include <asio.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <tl/expected.hpp>

#include <concepts>
#include <utility>

namespace async
{
    using DefaultToken = AsExpected<asio::use_awaitable_t<>>;
    using TcpAcceptor  = DefaultToken::as_default_on_t<asio::ip::tcp::acceptor>;
    using TcpSocket    = DefaultToken::as_default_on_t<asio::ip::tcp::socket>;
    using SteadyTimer  = DefaultToken::as_default_on_t<asio::steady_timer>;
    using IoContext    = asio::io_context;
    using Executor     = IoContext::executor_type;
    using Error        = asio::error_code;

    template <typename T>
    using Expect = tl::expected<T, Error>;

    template <typename T>
    using Awaitable = asio::awaitable<T>;

    using asio::ip::tcp;

    template <typename Exec, typename Awaited>
    auto spawn(Exec&& ex, Awaited&& func)
    {
        return co_spawn(std::forward<Exec>(ex), std::forward<Awaited>(func), asio::detached);
    }

    template <typename Exec, typename Awaited, typename Completion>
    auto spawn(Exec&& ex, Awaited&& func, Completion&& completion)
    {
        return co_spawn(
            std::forward<Exec>(ex),    //
            std::forward<Awaited>(func),
            std::forward<Completion>(completion)
        );
    }

    template <typename E>
    auto unexpected(E&& e)
    {
        return tl::make_unexpected(std::forward<E>(e));
    }

    namespace this_coro = asio::this_coro;
    namespace operators = asio::experimental::awaitable_operators;
}
