//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include <sstream>
#include "boost/mysql/value.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using namespace testing;
using namespace date::literals;
using namespace std::chrono;
using boost::mysql::value;

namespace
{

// tests for operator== and operator!=
struct ValueEqualityTest : public Test
{
    std::vector<value> values = makevalues(
        std::int32_t(20),
        std::int64_t(-1),
        std::uint32_t(0xffffffff),
        std::uint64_t(0x100000000),
        3.14f,
        8.89,
        boost::mysql::date(1_d/10/2019_y),
        boost::mysql::date(1_d/10/2019_y) + std::chrono::hours(10),
        boost::mysql::time(std::chrono::seconds(-10)),
        std::uint32_t(2010),
        nullptr
    );
    std::vector<value> values_copy = values;
    std::vector<value> other_values = makevalues(
        std::int32_t(10),
        std::int64_t(-22),
        std::uint32_t(0xff6723),
        std::uint64_t(222),
        -3.0f,
        8e24,
        boost::mysql::date(1_d/9/2019_y),
        boost::mysql::date(1_d/9/2019_y) + std::chrono::hours(10),
        boost::mysql::time(std::chrono::seconds(10)),
        std::uint32_t(1900),
        nullptr
    );
};

TEST_F(ValueEqualityTest, OperatorsEqNe_DifferentType_ReturnNotEquals)
{
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        for (std::size_t j = 0; j < i; ++j)
        {
            EXPECT_FALSE(values.at(i) == values.at(j)) << "i=" << i << ", j=" << j;
            EXPECT_TRUE(values.at(i) != values.at(j))  << "i=" << i << ", j=" << j;
        }
    }
}

TEST_F(ValueEqualityTest, OperatorsEqNe_SameTypeDifferentValue_ReturnNotEquals)
{
    // Note: nullptr_t (the last value) can't have other value than nullptr
    // so it is excluded from this test
    for (std::size_t i = 0; i < values.size() - 1; ++i)
    {
        EXPECT_FALSE(values.at(i) == other_values.at(i)) << "i=" << i;
        EXPECT_TRUE(values.at(i) != other_values.at(i))  << "i=" << i;
    }
}

TEST_F(ValueEqualityTest, OperatorsEqNe_SameTypeSameValue_ReturnEquals)
{
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        EXPECT_TRUE(values.at(i) == values_copy.at(i))  << "i=" << i;
        EXPECT_FALSE(values.at(i) != values_copy.at(i)) << "i=" << i;
    }
}

// Tests for operator<<
struct ValueStreamParams
{
    value input;
    std::string expected;

    template <typename T>
    ValueStreamParams(T input, std::string expected): input(input), expected(std::move(expected)) {}
};
std::ostream& operator<<(std::ostream& os, const ValueStreamParams& v) { return os << v.expected; }

struct ValueStreamTest : TestWithParam<ValueStreamParams>
{
};

TEST_P(ValueStreamTest, OutputStream_Trivial_ProducesExpectedString)
{
    std::ostringstream ss;
    ss << GetParam().input;
    EXPECT_EQ(ss.str(), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(Default, ValueStreamTest, Values(
    ValueStreamParams(std::int32_t(42), "42"),
    ValueStreamParams(std::int32_t(-90), "-90"),
    ValueStreamParams(std::uint32_t(42), "42"),
    ValueStreamParams(std::int64_t(42), "42"),
    ValueStreamParams(std::int64_t(-90), "-90"),
    ValueStreamParams(std::uint64_t(42), "42"),
    ValueStreamParams("a_string", "a_string"),
    ValueStreamParams(2.43f, "2.43"),
    ValueStreamParams(8.12, "8.12"),
    ValueStreamParams(makedate(2019, 9, 1), "2019-09-01"),
    ValueStreamParams(maket(0, 0, 0), "00:00:00:000000"),
    ValueStreamParams(maket(24, 0, 0), "24:00:00:000000"),
    ValueStreamParams(maket(210, 59, 59, 100), "210:59:59:000100"),
    ValueStreamParams(-maket(839, 20, 35, 999999), "-839:20:35:999999"),
    ValueStreamParams(maket(0, 2, 5), "00:02:05:000000"),
    ValueStreamParams(-maket(0, 21, 45), "-00:21:45:000000"),
    ValueStreamParams(maket(0, 0, 1, 234000), "00:00:01:234000"),
    ValueStreamParams(-maket(0, 0, 1, 234000), "-00:00:01:234000"),
    ValueStreamParams(makedt(2019, 1, 8, 9, 20, 11, 123), "2019-01-08 09:20:11.000123"),
    ValueStreamParams(makedt(2019, 1, 8), "2019-01-08 00:00:00.000000"),
    ValueStreamParams(nullptr, "<NULL>")
));

}
