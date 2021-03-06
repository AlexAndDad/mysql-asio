//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_VALUE_HPP
#define BOOST_MYSQL_IMPL_VALUE_HPP

#include "boost/mysql/detail/auxiliar/container_equals.hpp"

namespace boost {
namespace mysql {
namespace detail {

inline bool is_out_of_range(
    const date& d
)
{
    return d < min_date || d > max_date;
}

// Range checks
static_assert(date::min() <= min_date);
static_assert(date::max() >= max_date);
static_assert(datetime::min() <= min_datetime);
static_assert(datetime::max() >= max_datetime);
static_assert(time::min() <= min_time);
static_assert(time::max() >= max_time);

struct print_visitor
{
    std::ostream& os;

    print_visitor(std::ostream& os): os(os) {};

    template <typename T>
    void operator()(const T& value) const { os << value; }

    void operator()(const date& value) const { ::date::operator<<(os, value); }
    void operator()(const time& value) const
    {
        char buffer [100] {};
        const char* sign = value < std::chrono::microseconds(0) ? "-" : "";
        auto hours = std::abs(std::chrono::duration_cast<std::chrono::hours>(value).count());
        auto mins = std::abs(std::chrono::duration_cast<std::chrono::minutes>(value % std::chrono::hours(1)).count());
        auto secs = std::abs(std::chrono::duration_cast<std::chrono::seconds>(value % std::chrono::minutes(1)).count());
        auto micros = std::abs((value % std::chrono::seconds(1)).count());

        snprintf(buffer, sizeof(buffer), "%s%02d:%02u:%02u.%06u",
            sign,
            static_cast<int>(hours),
            static_cast<unsigned>(mins),
            static_cast<unsigned>(secs),
            static_cast<unsigned>(micros)
        );

        os << buffer;
    }
    void operator()(const datetime& value) const { ::date::operator<<(os, value); }
    void operator()(std::nullptr_t) const { os << "<NULL>"; }
};

template <typename T>
constexpr std::optional<T> get_optional_noconv(
    const value::variant_type& v
) noexcept
{
    auto* res = std::get_if<T>(&v);
    return res ? std::optional<T>(*res) : std::optional<T>();
}

// Helper to make integer constructors work
template <typename T>
constexpr decltype(auto) value_cast_int(const T& v) noexcept
{
    if constexpr (std::is_unsigned_v<T>)
    {
        return std::uint64_t(v);
    }
    else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
    {
        return std::int64_t(v);
    }
    else
    {
        return v;
    }
}

} // detail
} // mysql
} // boost

template <typename T>
constexpr boost::mysql::value::value(
    const T& v
) noexcept :
    repr_(detail::value_cast_int(v))
{
}

template <typename T>
constexpr std::optional<T> boost::mysql::value::get_optional() const noexcept
{
    return detail::get_optional_noconv<T>(repr_);
}

template <>
constexpr std::optional<std::uint64_t>
boost::mysql::value::get_optional<std::uint64_t>() const noexcept
{
    auto res = detail::get_optional_noconv<std::uint64_t>(repr_);
    if (!res)
    {
        auto* val = std::get_if<std::int64_t>(&repr_);
        if (val && *val >= 0)
        {
            res = static_cast<std::uint64_t>(*val);
        }
    }
    return res;
}

template <>
constexpr std::optional<std::int64_t>
boost::mysql::value::get_optional<std::int64_t>() const noexcept
{
    auto res = detail::get_optional_noconv<std::int64_t>(repr_);
    if (!res)
    {
        auto* val = std::get_if<std::uint64_t>(&repr_);
        if (val && *val <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            res = static_cast<std::int64_t>(*val);
        }
    }
    return res;
}

template <>
constexpr std::optional<double>
boost::mysql::value::get_optional<double>() const noexcept
{
    auto res = detail::get_optional_noconv<double>(repr_);
    if (!res)
    {
        auto* val = std::get_if<float>(&repr_);
        if (val)
        {
            res = *val;
        }
    }
    return res;
}

template <typename T>
constexpr T boost::mysql::value::get() const
{
    auto res = get_optional<T>();
    if (!res)
        throw std::bad_variant_access();
    return *res;
}

inline std::ostream& boost::mysql::operator<<(
    std::ostream& os,
    const value& value
)
{
    std::visit(detail::print_visitor(os), value.to_variant());
    return os;
}

template <typename... Types>
std::array<boost::mysql::value, sizeof...(Types)>
boost::mysql::make_values(
    Types&&... args
)
{
    return std::array<value, sizeof...(Types)>{value(std::forward<Types>(args))...};
}


#endif
