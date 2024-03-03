#pragma once

#include "async/as_expected.hpp"

#include <asio.hpp>
#include <asio/experimental/awaitable_operators.hpp>

#include <concepts>
#include <tl/expected.hpp>

namespace async
{
    // async
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

    using asio::bind_cancellation_slot;
    using asio::co_spawn;
    using asio::ip::tcp;

    template <typename To, typename From>
        requires std::default_initializable<To>
    std::tuple<Error, To> transformIfNoError(std::tuple<Error, From>&& from, std::invocable<From&&> auto&& transform)
    {
        if (std::get<0>(from)) {
            return std::make_tuple(std::get<0>(from), To{});
        }
        return std::make_tuple(Error{}, std::invoke(transform, std::move(std::get<1>(from))));
    }

    template <typename To, typename From>
        requires std::constructible_from<To, From> && std::default_initializable<To>
    std::tuple<Error, To> transformIfNoError(std::tuple<Error, From>&& from)
    {
        auto [e, v] = std::move(from);
        if (std::get<0>(from)) {
            return std::make_tuple<Error, To>(std::move(e), {});
        }
        return std::make_tuple<Error, To>({}, { std::move(v) });
    }

    namespace operators = asio::experimental::awaitable_operators;
}
