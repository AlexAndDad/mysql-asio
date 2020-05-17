//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "serialization_test_common.hpp"
#include "boost/mysql/detail/protocol/prepared_statement_messages.hpp"
#include <forward_list>
#include <array>

using namespace boost::mysql::test;
using namespace boost::mysql::detail;
using boost::mysql::value;

namespace
{

INSTANTIATE_TEST_SUITE_P(ComStmtPrepare, SerializeTest, testing::Values(
    serialization_testcase(com_stmt_prepare_packet{
        string_eof("SELECT * from three_rows_table WHERE id = ?")
    }, {
        0x16, 0x53, 0x45, 0x4c, 0x45, 0x43, 0x54, 0x20,
        0x2a, 0x20, 0x66, 0x72, 0x6f, 0x6d, 0x20, 0x74,
        0x68, 0x72, 0x65, 0x65, 0x5f, 0x72, 0x6f, 0x77,
        0x73, 0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x20,
        0x57, 0x48, 0x45, 0x52, 0x45, 0x20, 0x69, 0x64,
        0x20, 0x3d, 0x20, 0x3f
    }, "regular")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(ComStmtPrepareResponse, DeserializeSpaceTest, testing::Values(
    serialization_testcase(com_stmt_prepare_ok_packet{
        int4(1), // statement id
        int2(2), // number of fields
        int2(3), // number of params
        int2(0), // warnings
    }, {
        0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x03, 0x00,
        0x00, 0x00, 0x00
    }, "regular")
), test_name_generator);

// Helper for composing ComStmtExecute tests
template <typename Collection = std::vector<value>>
serialization_testcase make_stmt_execute_test(
    std::uint32_t stmt_id,
    std::uint8_t flags,
    std::uint32_t itercount,
    std::uint8_t new_params_flag,
    std::vector<value>&& params,
    std::vector<std::uint8_t>&& buffer,
    std::string&& test_name
)
{
    auto params_shared = std::make_shared<Collection>(std::begin(params), std::end(params));
    return serialization_testcase(
        com_stmt_execute_packet<typename Collection::const_iterator> {
            int4(stmt_id),
            int1(flags),
            int4(itercount),
            int1(new_params_flag),
            params_shared->begin(),
            params_shared->end()
        },
        std::move(buffer),
        std::move(test_name),
        0, // capabilities
        params_shared
    );
}

INSTANTIATE_TEST_SUITE_P(ComStmtExecute, SerializeTest, testing::Values(
    make_stmt_execute_test(1, 0x80, 1, 1, // stmt ID, flags, itercount, new params
        { value(std::uint64_t(0xabffffabacadae)) }, {
            0x17, 0x01, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x08, 0x80, 0xae, 0xad,
            0xac, 0xab, 0xff, 0xff, 0xab, 0x00
        },
        "uint64_t"
    ),
    make_stmt_execute_test(1, 0, 1, 1, // stmt ID, flags, itercount, new params
        { value(std::int64_t(-0xabffffabacadae)) }, {
            0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x08, 0x00, 0x52, 0x52,
            0x53, 0x54, 0x00, 0x00, 0x54, 0xff
        },
        "int64_t"
    ),
    make_stmt_execute_test(1, 0, 1, 1, // stmt ID, flags, itercount, new params
        { value(std::string_view("test")) }, {
            0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x0f, 0x00, 0x04, 0x74,
            0x65, 0x73, 0x74
        },
        "string_view"
    ),
    make_stmt_execute_test(1, 0, 1, 1, // stmt ID, flags, itercount, new params
        { value(3.14e20f) }, {
            0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x01, 0x2d,
            0x88, 0x61
        },
        "float"
    ),
    make_stmt_execute_test(1, 0, 1, 1, // stmt ID, flags, itercount, new params
        { value(2.1e214) }, {
            0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x05, 0x00, 0x56, 0xc0,
            0xee, 0xa6, 0x95, 0x30, 0x6f, 0x6c
        },
        "double"
    ),
    make_stmt_execute_test(1, 0, 1, 1, // stmt ID, flags, itercount, new params
        { value(makedate(2010, 9, 3)) }, {
            0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x0a, 0x00, 0x04, 0xda,
            0x07, 0x09, 0x03
        },
        "date"
    ),
    make_stmt_execute_test(1, 0, 1, 1, // stmt ID, flags, itercount, new params
        { value(makedt(2010, 9, 3, 10, 30, 59, 231800)) }, {
            0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x0c, 0x00, 0x0b, 0xda,
            0x07, 0x09, 0x03, 0x0a, 0x1e, 0x3b, 0x78, 0x89,
            0x03, 0x00
        },
        "datetime"
    ),
    make_stmt_execute_test(1, 0, 1, 1, // stmt ID, flags, itercount, new params
        { value(maket(230, 30, 59, 231800)) }, {
            0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x0b, 0x00, 0x0c, 0x00,
            0x09, 0x00, 0x00, 0x00, 0x0e, 0x1e, 0x3b, 0x78,
            0x89, 0x03, 0x00
        },
        "time"
    ),
    make_stmt_execute_test(1, 0, 1, 1, // stmt ID, flags, itercount, new params
        { value(nullptr) }, {
            0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x01, 0x01, 0x06, 0x00
        },
        "null"
    ),
    make_stmt_execute_test(2, 0, 1, 1, makevalues(
            std::uint64_t(0xabffffabacadae),
            std::int64_t(-0xabffffabacadae),
            std::string_view("test"),
            nullptr,
            2.1e214,
            makedate(2010, 9, 3),
            makedt(2010, 9, 3, 10, 30, 59, 231800),
            maket(230, 30, 59, 231800),
            nullptr
        ), {
            0x17, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x08, 0x01, 0x01, 0x08, 0x80, 0x08, 0x00, 0x0f, 0x00, 0x06,
            0x00, 0x05, 0x00, 0x0a, 0x00, 0x0c, 0x00, 0x0b,
            0x00, 0x06, 0x00, 0xae, 0xad, 0xac, 0xab, 0xff,
            0xff, 0xab, 0x00, 0x52, 0x52, 0x53, 0x54, 0x00,
            0x00, 0x54, 0xff, 0x04, 0x74, 0x65, 0x73, 0x74,
            0x56, 0xc0, 0xee, 0xa6, 0x95, 0x30, 0x6f, 0x6c,
            0x04, 0xda, 0x07, 0x09, 0x03, 0x0b, 0xda, 0x07,
            0x09, 0x03, 0x0a, 0x1e, 0x3b, 0x78, 0x89, 0x03,
            0x00, 0x0c, 0x00, 0x09, 0x00, 0x00, 0x00, 0x0e,
            0x1e, 0x3b, 0x78, 0x89, 0x03, 0x00
        },
        "several_params"
    ),
    make_stmt_execute_test<std::forward_list<value>>(1, 0x80, 1, 1, // stmt ID, flags, itercount, new params
        { value(std::uint64_t(0xabffff)) }, {
            0x17, 0x01, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x08, 0x80, 0xff, 0xff,
            0xab, 0x00, 0x00, 0x00, 0x00, 0x00
        },
        "forward_list_iterator"
    )
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(ComStmtClose, SerializeTest, testing::Values(
    serialization_testcase(com_stmt_close_packet{int4(1)}, {0x19, 0x01, 0x00, 0x00, 0x00}, "regular")
), test_name_generator);

}
