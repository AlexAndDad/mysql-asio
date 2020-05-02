//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_TEXT_DESERIALIZATION_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_TEXT_DESERIALIZATION_IPP

#include <cstdlib>
#include <cmath>
#include <boost/lexical_cast/try_lexical_convert.hpp>
#include <date/date.h>

namespace boost {
namespace mysql {
namespace detail {

inline errc deserialize_text_value_impl(
    std::string_view from,
    date& to
)
{
    constexpr std::size_t size = 4 + 2 + 2 + 2; // year, month, day, separators
    if (from.size() != size)
        return errc::protocol_value_error;
    unsigned year, month, day;
    char buffer [size + 1] {};
    memcpy(buffer, from.data(), from.size());
    int parsed = sscanf(buffer, "%4u-%2u-%2u", &year, &month, &day);
    if (parsed != 3)
        return errc::protocol_value_error;
    ::date::year_month_day result (::date::year(year)/::date::month(month)/::date::day(day));
    if (!result.ok())
        return errc::protocol_value_error;
    if (result > max_date || result < min_date)
        return errc::protocol_value_error;
    to = result;
    return errc::ok;
}

inline errc deserialize_text_value_impl(
    std::string_view from,
    time& to,
    unsigned decimals
)
{
    // size check
    constexpr std::size_t min_size = 2 + 2 + 2 + 2; // hours, mins, seconds, no micros
    constexpr std::size_t max_size = min_size + 1 + 1 + 7; // hour extra character, sign and micros
    decimals = std::min(decimals, 6u);
    if (from.size() < min_size || from.size() > max_size)
        return errc::protocol_value_error;

    // Parse it
    int hours;
    unsigned minutes, seconds, micros = 0;
    char buffer [max_size + 1] {};
    memcpy(buffer, from.data(), from.size());
    int parsed = decimals ? sscanf(buffer, "%4d:%2u:%2u.%6u", &hours, &minutes, &seconds, &micros) : // sign adds 1 char
                            sscanf(buffer, "%4d:%2u:%2u", &hours, &minutes, &seconds);
    if ((decimals && parsed != 4) || (!decimals && parsed != 3))
        return errc::protocol_value_error;
    micros *= static_cast<unsigned>(std::pow(10, 6 - decimals));
    hours = std::abs(hours);
    bool is_negative = from[0] == '-';

    // Sum it
    auto res = std::chrono::hours(hours) + std::chrono::minutes(minutes) +
               std::chrono::seconds(seconds) + std::chrono::microseconds(micros);
    if (is_negative)
    {
        res = -res;
    }

    // Range check
    if (res > max_time || res < min_time)
        return errc::protocol_value_error;

    to = res;
    return errc::ok;
}

inline errc deserialize_text_value_impl(
    std::string_view from,
    datetime& to,
    unsigned decimals
)
{
    // Length check
    constexpr std::size_t min_size = 4 + 5*2 + 5; // year, month, day, hour, minute, seconds, separators
    decimals = std::min(decimals, 6u);
    std::size_t expected_size = min_size + (decimals ? decimals + 1 : 0);
    if (from.size() != expected_size)
        return errc::protocol_value_error;

    // Date part
    date dt;
    auto err = deserialize_text_value_impl(from.substr(0, 10), dt);
    if (err != errc::ok)
        return err;

    // Time of day part
    time time_of_day;
    err = deserialize_text_value_impl(from.substr(11), time_of_day, decimals);
    if (err != errc::ok)
        return err;
    constexpr auto max_time_of_day = std::chrono::hours(24) - std::chrono::microseconds(1);
    if (time_of_day < std::chrono::seconds(0) || time_of_day > max_time_of_day)
        return errc::protocol_value_error;

    // Sum it up
    to = dt + time_of_day;
    return errc::ok;
}


template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>, errc>
deserialize_text_value_impl(std::string_view from, T& to)
{
    bool ok = boost::conversion::try_lexical_convert(from.data(), from.size(), to);
    return ok ? errc::ok : errc::protocol_value_error;
}

inline errc deserialize_text_value_impl(std::string_view from, std::string_view& to)
{
    to = from;
    return errc::ok;
}

template <typename T, typename... Args>
errc deserialize_text_value_to_variant(std::string_view from, value& to, Args&&... args)
{
    T value;
    auto err = deserialize_text_value_impl(from, value, std::forward<Args>(args)...);
    if (err == errc::ok)
    {
        to = value;
    }
    return err;
}

inline bool is_next_field_null(
    deserialization_context& ctx
)
{
    int1 type_byte;
    errc err = deserialize(type_byte, ctx);
    if (err == errc::ok)
    {
        if (type_byte.value == 0xfb)
        {
            return true; // it was null, do not rewind
        }
        ctx.rewind(1); // it was not null, rewind (this byte is part of the actual message)
    }
    return false;
}

} // detail
} // mysql
} // boost

inline boost::mysql::errc boost::mysql::detail::deserialize_text_value(
    std::string_view from,
    const field_metadata& meta,
    value& output
)
{
    switch (meta.protocol_type())
    {
    case protocol_field_type::tiny:
    case protocol_field_type::short_:
    case protocol_field_type::int24:
    case protocol_field_type::long_:
    case protocol_field_type::year:
        return meta.is_unsigned() ?
                deserialize_text_value_to_variant<std::uint32_t>(from, output) :
                deserialize_text_value_to_variant<std::int32_t>(from, output);
    case protocol_field_type::longlong:
        return meta.is_unsigned() ?
                deserialize_text_value_to_variant<std::uint64_t>(from, output) :
                deserialize_text_value_to_variant<std::int64_t>(from, output);
    case protocol_field_type::float_:
        return deserialize_text_value_to_variant<float>(from, output);
    case protocol_field_type::double_:
        return deserialize_text_value_to_variant<double>(from, output);
    case protocol_field_type::timestamp:
    case protocol_field_type::datetime:
        return deserialize_text_value_to_variant<datetime>(from, output, meta.decimals());
    case protocol_field_type::date:
        return deserialize_text_value_to_variant<date>(from, output);
    case protocol_field_type::time:
        return deserialize_text_value_to_variant<time>(from, output, meta.decimals());
    // True string types
    case protocol_field_type::varchar:
    case protocol_field_type::var_string:
    case protocol_field_type::string:
    case protocol_field_type::tiny_blob:
    case protocol_field_type::medium_blob:
    case protocol_field_type::long_blob:
    case protocol_field_type::blob:
    case protocol_field_type::enum_:
    case protocol_field_type::set:
    // Anything else that we do not know how to interpret, we return as a binary string
    case protocol_field_type::decimal:
    case protocol_field_type::bit:
    case protocol_field_type::newdecimal:
    case protocol_field_type::geometry:
    default:
        return deserialize_text_value_to_variant<std::string_view>(from, output);
    }
}


boost::mysql::error_code boost::mysql::detail::deserialize_text_row(
    deserialization_context& ctx,
    const std::vector<field_metadata>& fields,
    std::vector<value>& output
)
{
    output.resize(fields.size());
    for (std::vector<value>::size_type i = 0; i < fields.size(); ++i)
    {
        bool is_null = is_next_field_null(ctx);
        if (is_null)
        {
            output[i] = nullptr;
        }
        else
        {
            string_lenenc value_str;
            errc err = deserialize(value_str, ctx);
            if (err != errc::ok)
                return make_error_code(err);
            err = deserialize_text_value(value_str.value, fields[i], output[i]);
            if (err != errc::ok)
                return make_error_code(err);
        }
    }
    if (!ctx.empty())
        return make_error_code(errc::extra_bytes);
    return error_code();
}



#endif
