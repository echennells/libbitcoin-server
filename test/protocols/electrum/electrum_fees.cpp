/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "../../test.hpp"
#include "electrum_setup_fixture.hpp"

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

using namespace system;
static const code wrong_version{ server::error::wrong_version };
static const code not_implemented{ server::error::not_implemented };
static const code invalid_argument{ server::error::invalid_argument };

// blockchain.estimatefee

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__no_number__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__float_number__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto result = get_error(R"({"id":801,"method":"blockchain.estimatefee","params":[42.42]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__mode_invalid_version__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto result = get_error(R"({"id":801,"method":"blockchain.estimatefee","params":[42,"basic"]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__nvalid_mode__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto result = get_error(R"({"id":801,"method":"blockchain.estimatefee","params":[42,"bogus"]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__uninitialized__negative_one)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[0,"basic"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), -1);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__zero_basic__negative_one)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    // Trigger node chaser event to initialize fee estimator.
    notify(node::chase::block, { 9_u32 });

    const auto response = get(R"({"id":801,"method":"blockchain.estimatefee","params":[0,"basic"]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_int64());

    // None of the first 10 blocks have fees, so no estimate is obtained.
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), -1);
}

// blockchain.relayfee

BOOST_AUTO_TEST_CASE(electrum__blockchain_relay_fee__default__expected)
{
    config_.node.minimum_fee_rate = 42.42f;
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":90,"method":"blockchain.relayfee","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("id").is_int64());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 90);
    BOOST_REQUIRE_EQUAL(response.at("result").as_double(), config_.node.minimum_fee_rate);
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_relay_fee__obsoleted__wrong_version)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto result = get_error(R"({"id":801,"method":"blockchain.relayfee","params":[]})" "\n");
    BOOST_REQUIRE_EQUAL(result, wrong_version.value());
}

// --- bug V2: to_integer upper-boundary undefined behavior --------------------
// estimatefee's target argument is converted from double to size_t by
// to_integer (libbitcoin-system math/cast.ipp). Its range check rejects
// `integer > static_cast<double>(numeric_limits<size_t>::max())`. But
// static_cast<double>(2^64-1) rounds UP to 2^64 (2^64-1 is not representable in
// a 53-bit mantissa), so the effective bound is 2^64 and the check is
// `integer > 2^64`. The far-out-of-range and negative cases below are rejected
// correctly; only the exact value 2^64 escapes.

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__target_far_above_range__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    // 1e30 > 2^64: correctly rejected by the range check.
    const auto result = get_error(
        R"({"id":801,"method":"blockchain.estimatefee","params":[1e30]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__negative_target__invalid_argument)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    // Negative target: correctly rejected (size_t min is 0, exact as a double).
    const auto result = get_error(
        R"({"id":801,"method":"blockchain.estimatefee","params":[-1]})" "\n");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

// Characterizes bug V2. 18446744073709551616 == 2^64; JSON parses it to the
// double 2^64 -- the same value static_cast<double>(SIZE_MAX) rounds to. The
// range check `2^64 > 2^64` is false, so 2^64 is NOT rejected; to_integer then
// performs static_cast<size_t>(2^64), which is undefined behavior (2^64 does
// not fit size_t). The handler proceeds with a garbage target and returns -1.
// Correct behavior is invalid_argument (cf. the two cases above and the
// estimatefee[42.42] test). A fix to to_integer's bound will trip this test; it
// should then assert invalid_argument. NOTE: this pins UB-dependent behavior --
// observed as result -1 on this build (gcc/x86-64, debug); a different toolchain
// could differ.
BOOST_AUTO_TEST_CASE(electrum__blockchain_estimate_fee__target_two_pow_64__not_rejected_bug_v2)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_6));

    const auto response = get(
        R"({"id":801,"method":"blockchain.estimatefee","params":[18446744073709551616]})" "\n");

    // BUG V2: an out-of-size_t-range target is accepted instead of rejected.
    REQUIRE_NO_THROW_TRUE(response.at("error").is_null());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), -1);
}

BOOST_AUTO_TEST_SUITE_END()
