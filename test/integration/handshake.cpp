/*
 * handshake.cpp
 *
 *  Created on: Oct 22, 2019
 *      Author: ruben
 */

#include "boost/mysql/connection.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"
#include <boost/asio/use_future.hpp>
#include <cstdlib>

namespace net = boost::asio;
using namespace testing;
using namespace boost::mysql::test;

using boost::mysql::detail::make_error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::error_code;
using boost::mysql::detail::stringize;

namespace
{

bool skip_sha256()
{
	return std::getenv("MYSQL_SKIP_SHA256_TESTS") != nullptr;
}

struct HandshakeTest : public IntegTest,
					   public testing::WithParamInterface<network_testcase>
{
	auto do_handshake()
	{
		connection_params.set_ssl(boost::mysql::ssl_options(GetParam().ssl));
		return GetParam().net->handshake(conn, connection_params);
	}

	void set_credentials(std::string_view user, std::string_view password)
	{
		connection_params.set_username(user);
		connection_params.set_password(password);
	}

	// does handshake and verifies it went OK
	void do_handshake_ok()
	{
		auto result = do_handshake();
		result.validate_no_error();
		validate_ssl(GetParam().ssl);
	}

	void load_sha256_cache(const char* user, const char* password)
	{
		check_call(stringize("mysql -u ", user, " -p", password, " -e \"\""));
	}

	void clear_sha256_cache()
	{
		check_call("mysql -u root -e \"FLUSH PRIVILEGES\"");
	}
};

// mysql_native_password
TEST_P(HandshakeTest, MysqlNativePassword_SuccessfulLogin)
{
	set_credentials("mysqlnp_user", "mysqlnp_password");
	do_handshake_ok();
}

TEST_P(HandshakeTest, MysqlNativePasswordEmptyPassword_SuccessfulLogin)
{
	set_credentials("mysqlnp_empty_password_user", "");
	do_handshake_ok();
}

TEST_P(HandshakeTest, MysqlNativePasswordBadPassword_FailedLogin)
{
	set_credentials("mysqlnp_user", "bad_password");
	auto result = do_handshake();
	result.validate_error(errc::access_denied_error, {"access denied", "mysqlnp_user"});
}

// caching_sha2_password
TEST_P(HandshakeTest, CachingSha2PasswordCacheHit_SuccessfulLogin)
{
	if (skip_sha256())
	{
		GTEST_SKIP();
	}

	set_credentials("csha2p_user", "csha2p_password");

	// Force the server load the user into the cache
	load_sha256_cache("csha2p_user", "csha2p_password");

	// Note: this should succeed even for non-TLS connections, as
	// it uses the initial challenge, and does NOT send any plaintext password
	do_handshake_ok();
}

TEST_P(HandshakeTest, CachingSha2PasswordCacheMiss_SuccessfulLoginIfSsl)
{
	if (skip_sha256())
	{
		GTEST_SKIP();
	}

	set_credentials("csha2p_user", "csha2p_password");

	clear_sha256_cache();

	if (should_use_ssl(GetParam().ssl))
	{
		do_handshake_ok();
	}
	else
	{
		// A cache miss would force us send a plaintext password over
		// a non-TLS connection, so we fail
		auto result = do_handshake();
		result.validate_error(errc::auth_plugin_requires_ssl, {});
	}
}

TEST_P(HandshakeTest, CachingSha2PasswordEmptyPasswordCacheHit_SuccessfulLogin)
{
	if (skip_sha256())
	{
		GTEST_SKIP();
	}

	set_credentials("csha2p_empty_password_user", "");
	load_sha256_cache("csha2p_empty_password_user", "");
	do_handshake_ok();
}

TEST_P(HandshakeTest, CachingSha2PasswordEmptyPasswordCacheMiss_SuccessfulLogin)
{
	if (skip_sha256())
	{
		GTEST_SKIP();
	}

	set_credentials("csha2p_empty_password_user", "");
	clear_sha256_cache();
	do_handshake_ok();
}

TEST_P(HandshakeTest, NoDatabase_SuccessfulLogin)
{
	connection_params.set_database("");
	do_handshake_ok();
}

TEST_P(HandshakeTest, BadUser)
{
	connection_params.set_username("non_existing_user");
	auto result = do_handshake();
	EXPECT_NE(result.err, error_code());
	// TODO: if default auth plugin is unknown, unknown auth plugin is returned instead of access denied
	// EXPECT_EQ(errc, make_error_code(mysql::errc::access_denied_error));
}

TEST_P(HandshakeTest, BadDatabase)
{
	connection_params.set_database("bad_database");
	auto result = do_handshake();
	result.validate_error(errc::dbaccess_denied_error, {"database", "bad_database"});
}

// This will run the handshake tests over all ssl_mode options available,
// including ssl_mode::enable. We expect the same results as for ssl_mode::require
MYSQL_NETWORK_TEST_SUITE_EX(HandshakeTest, true);


} // anon namespace
