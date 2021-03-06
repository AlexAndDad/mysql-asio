//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;

namespace
{


template <typename Stream>
struct CloseStatementTest : NetworkTest<Stream>
{

    void ExistingOrClosedStatement()
    {
        auto* net = this->GetParam().net;

        // Prepare a statement
        auto stmt = net->prepare_statement(this->conn, "SELECT * FROM empty_table");
        stmt.validate_no_error();

        // Verify it works fine
        auto exec_result = net->execute_statement(stmt.value, {});
        exec_result.validate_no_error();
        exec_result.value.fetch_all(); // clean all packets sent for this execution

        // Close the statement
        auto close_result = net->close_statement(stmt.value);
        close_result.validate_no_error();

        // Close it again
        close_result = net->close_statement(stmt.value);
        close_result.validate_no_error();

        // Verify close took effect
        exec_result = net->execute_statement(stmt.value, {});
        exec_result.validate_error(boost::mysql::errc::unknown_stmt_handler, {"unknown prepared statement"});
    }
};

BOOST_MYSQL_NETWORK_TEST_SUITE(CloseStatementTest)
BOOST_MYSQL_NETWORK_TEST(CloseStatementTest, ExistingOrClosedStatement)


}

