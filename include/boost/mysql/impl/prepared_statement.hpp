//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP

#include "boost/mysql/detail/network_algorithms/execute_statement.hpp"
#include "boost/mysql/detail/network_algorithms/close_statement.hpp"
#include "boost/mysql/detail/auxiliar/stringize.hpp"
#include "boost/mysql/detail/auxiliar/check_completion_token.hpp"
#include <boost/beast/core/bind_handler.hpp>

template <typename Stream>
template <typename ForwardIterator>
void boost::mysql::prepared_statement<Stream>::check_num_params(
    ForwardIterator first,
    ForwardIterator last,
    error_code& err,
    error_info& info
) const
{
    auto param_count = std::distance(first, last);
    if (param_count != num_params())
    {
        err = detail::make_error_code(errc::wrong_num_params);
        info.set_message(detail::stringize(
                "prepared_statement::execute: expected ", num_params(), " params, but got ", param_count));
    }
}


template <typename Stream>
template <typename ForwardIterator>
boost::mysql::resultset<Stream> boost::mysql::prepared_statement<Stream>::execute(
    ForwardIterator params_first,
    ForwardIterator params_last,
    error_code& err,
    error_info& info
) const
{
    assert(valid());

    mysql::resultset<Stream> res;
    detail::clear_errors(err, info);

    // Verify we got passed the right number of params
    check_num_params(params_first, params_last, err, info);
    if (!err)
    {
        detail::execute_statement(
            *channel_,
            stmt_msg_.statement_id.value,
            params_first,
            params_last,
            res,
            err,
            info
        );
    }

    return res;
}

template <typename Stream>
template <typename ForwardIterator>
boost::mysql::resultset<Stream> boost::mysql::prepared_statement<Stream>::execute(
    ForwardIterator params_first,
    ForwardIterator params_last
) const
{
    error_code err;
    error_info info;
    auto res = execute(params_first, params_last, err, info);
    detail::check_error_code(err, info);
    return res;
}

template <typename StreamType>
template <typename ForwardIterator, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::prepared_statement<StreamType>::execute_signature
)
boost::mysql::prepared_statement<StreamType>::async_execute(
    ForwardIterator params_first,
    ForwardIterator params_last,
    CompletionToken&& token,
    error_info* info
) const
{
    detail::conditional_clear(info);
    detail::check_completion_token<CompletionToken, execute_signature>();

    // Check we got passed the right number of params
    error_code err;
    error_info nonnull_info;
    check_num_params(params_first, params_last, err, nonnull_info);
    if (err)
    {
        detail::conditional_assign(info, std::move(nonnull_info));
        boost::asio::async_completion<CompletionToken, execute_signature> completion (token);
        // TODO: is executor correctly preserved here?
        boost::asio::post(
            channel_->next_layer().get_executor(),
            boost::beast::bind_front_handler(
                std::move(completion.completion_handler),
                err,
                resultset<StreamType>()
            )
        );
        return completion.result.get();
    }
    else
    {
        // Actually execute the statement
        return detail::async_execute_statement(
            *channel_,
            stmt_msg_.statement_id.value,
            params_first,
            params_last,
            std::forward<CompletionToken>(token),
            info
        );
    }
}

template <typename StreamType>
void boost::mysql::prepared_statement<StreamType>::close(
    error_code& code,
    error_info& info
)
{
    assert(valid());
    detail::clear_errors(code, info);
    detail::close_statement(*channel_, id(), code, info);
}

template <typename StreamType>
void boost::mysql::prepared_statement<StreamType>::close()
{
    assert(valid());
    error_code code;
    error_info info;
    detail::close_statement(*channel_, id(), code, info);
    detail::check_error_code(code, info);
}

template <typename StreamType>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::prepared_statement<StreamType>::close_signature
)
boost::mysql::prepared_statement<StreamType>::async_close(
    CompletionToken&& token,
    error_info* info
)
{
    assert(valid());
    detail::conditional_clear(info);
    detail::check_completion_token<CompletionToken, close_signature>();
    return detail::async_close_statement(
        *channel_,
        id(),
        std::forward<CompletionToken>(token),
        info
    );
}

#endif /* INCLUDE_BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP_ */
