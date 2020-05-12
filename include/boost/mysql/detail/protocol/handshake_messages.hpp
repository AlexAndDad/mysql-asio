//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_HANDSHAKE_MESSAGES_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_HANDSHAKE_MESSAGES_HPP

#include "boost/mysql/detail/protocol/serialization.hpp"
#include "boost/mysql/detail/auxiliar/static_string.hpp"

namespace boost {
namespace mysql {
namespace detail {

// initial handshake
struct handshake_packet
{
    using auth_buffer_type = static_string<8 + 0xff>;

    // int<1>     protocol version     Always 10
    string_null server_version;
    int4 connection_id;
    auth_buffer_type auth_plugin_data; // not an actual protocol field, the merge of two fields
    int4 capability_falgs; // merge of the two parts - not an actual field
    int1 character_set; // default server a_protocol_character_set, only the lower 8-bits
    int2 status_flags; // server_status_flags
    string_null auth_plugin_name;
};

template <>
struct get_struct_fields<handshake_packet>
{
    static constexpr auto value = std::make_tuple(
        &handshake_packet::server_version,
        &handshake_packet::connection_id,
        &handshake_packet::auth_plugin_data,
        &handshake_packet::capability_falgs,
        &handshake_packet::character_set,
        &handshake_packet::status_flags,
        &handshake_packet::auth_plugin_name
    );
};

template <>
struct serialization_traits<handshake_packet, serialization_tag::struct_with_fields> :
    noop_serialize<handshake_packet>
{
    static inline errc deserialize_(deserialization_context& ctx, handshake_packet& output) noexcept;
};

// response
struct handshake_response_packet
{
    int4 client_flag; // capabilities
    int4 max_packet_size;
    int1 character_set;
    // string[23]     filler     filler to the size of the handhshake response packet. All 0s.
    string_null username;
    string_lenenc auth_response; // we require CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
    string_null database; // only to be serialized if CLIENT_CONNECT_WITH_DB
    string_null client_plugin_name; // we require CLIENT_PLUGIN_AUTH
    // CLIENT_CONNECT_ATTRS: not implemented
};

template <>
struct get_struct_fields<handshake_response_packet>
{
    static constexpr auto value = std::make_tuple(
        &handshake_response_packet::client_flag,
        &handshake_response_packet::max_packet_size,
        &handshake_response_packet::character_set,
        &handshake_response_packet::username,
        &handshake_response_packet::auth_response,
        &handshake_response_packet::database,
        &handshake_response_packet::client_plugin_name
    );
};

template <>
struct serialization_traits<handshake_response_packet, serialization_tag::struct_with_fields> :
    noop_deserialize<handshake_response_packet>
{
    static inline std::size_t get_size_(const serialization_context& ctx,
            const handshake_response_packet& value) noexcept;
    static inline void serialize_(serialization_context& ctx,
            const handshake_response_packet& value) noexcept;
};

// SSL request
struct ssl_request
{
    int4 client_flag;
    int4 max_packet_size;
    int1 character_set;
    string_fixed<23> filler {};
};

template <>
struct get_struct_fields<ssl_request>
{
    static constexpr auto value = std::make_tuple(
        &ssl_request::client_flag,
        &ssl_request::max_packet_size,
        &ssl_request::character_set,
        &ssl_request::filler
    );
};

// auth switch request
struct auth_switch_request_packet
{
    string_null plugin_name;
    string_eof auth_plugin_data;
};

template <>
struct get_struct_fields<auth_switch_request_packet>
{
    static constexpr auto value = std::make_tuple(
        &auth_switch_request_packet::plugin_name,
        &auth_switch_request_packet::auth_plugin_data
    );
};

// response
struct auth_switch_response_packet
{
    string_eof auth_plugin_data;
};

template <>
struct get_struct_fields<auth_switch_response_packet>
{
    static constexpr auto value = std::make_tuple(
        &auth_switch_response_packet::auth_plugin_data
    );
};

template <>
struct serialization_traits<auth_switch_request_packet, serialization_tag::struct_with_fields> :
    noop_serialize<auth_switch_request_packet>
{
    static inline errc deserialize_(deserialization_context& ctx,
            auth_switch_request_packet& output) noexcept;
};

// more data (like auth switch request, but for the same plugin)
struct auth_more_data_packet
{
    string_eof auth_plugin_data;
};

template <>
struct get_struct_fields<auth_more_data_packet>
{
    static constexpr auto value = std::make_tuple(
        &auth_more_data_packet::auth_plugin_data
    );
};


} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/handshake_messages.ipp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_HANDSHAKE_MESSAGES_HPP_ */
