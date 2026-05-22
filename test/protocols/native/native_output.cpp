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
#include "native_setup_fixture.hpp"

using namespace system;
using namespace boost::beast;

BOOST_FIXTURE_TEST_SUITE(native_tests, native_ten_block_setup_fixture)

// block1's coinbase has exactly one output (50 BTC, pay-to-public-key), and it
// is unspent within the ten-block store.
namespace {

std::string coinbase_txid(const chain::block& block)
{
    return encode_hash(block.transactions_ptr()->front()->hash(false));
}

const std::string absent_txid{
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef" };

} // namespace

// outputs -- all outputs of a transaction (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__outputs__json__expected)
{
    const auto response = get_json(
        "/v1/output/" + coinbase_txid(test::block1) + "?format=json");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 1u);
}

BOOST_AUTO_TEST_CASE(native__outputs__text__expected)
{
    const auto body = get_text(
        "/v1/output/" + coinbase_txid(test::block1) + "?format=text");
    BOOST_REQUIRE(!body.empty());
}

BOOST_AUTO_TEST_CASE(native__outputs__data__expected)
{
    const auto body = get_data(
        "/v1/output/" + coinbase_txid(test::block1) + "?format=data");
    BOOST_REQUIRE(!body.empty());
}

BOOST_AUTO_TEST_CASE(native__outputs__absent_hash__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/output/" + absent_txid + "?format=json"),
        http::status::not_found);
}

BOOST_AUTO_TEST_CASE(native__outputs__xml__not_acceptable)
{
    BOOST_REQUIRE_EQUAL(get_status(
        "/v1/output/" + coinbase_txid(test::block1) + "?format=xml"),
        http::status::not_acceptable);
}

// output -- single output by index (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__output__json__expected)
{
    const auto response = get_json(
        "/v1/output/" + coinbase_txid(test::block1) + "/0?format=json");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE(response.as_object().contains("value"));
}

BOOST_AUTO_TEST_CASE(native__output__index_out_of_range__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status(
        "/v1/output/" + coinbase_txid(test::block1) + "/9?format=json"),
        http::status::not_found);
}

// output/script (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__output_script__data__expected)
{
    const auto body = get_data(
        "/v1/output/" + coinbase_txid(test::block1) + "/0/script?format=data");
    BOOST_REQUIRE(!body.empty());
}

// output/spender and output/spenders (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__output_spender__unspent__not_found)
{
    // The block1 coinbase output is unspent within the ten-block store.
    BOOST_REQUIRE_EQUAL(get_status(
        "/v1/output/" + coinbase_txid(test::block1) + "/0/spender?format=json"),
        http::status::not_found);
}

BOOST_AUTO_TEST_CASE(native__output_spenders__unspent__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status(
        "/v1/output/" + coinbase_txid(test::block1) + "/0/spenders?format=json"),
        http::status::not_found);
}

// output/subscribe (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__output_subscribe__no_stop__returns_output)
{
    // With no stop flag, output/subscribe responds as a plain output query.
    const auto response = get_json("/v1/output/" +
        coinbase_txid(test::block1) + "/0/subscribe?format=json");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE(response.as_object().contains("value"));
}

// output (websocket)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__ws_output__data__expected)
{
    BOOST_REQUIRE(!ws_upgrade());
    const auto body = ws_get_data(
        "/v1/output/" + coinbase_txid(test::block1) + "/0?format=data");
    BOOST_REQUIRE(!body.empty());
}

// Characterizes bug C1: output json over websocket is truncated to the
// send_json size hint (two * binary size), smaller than the JSON object, so it
// arrives unparseable. The HTTP path is unaffected (see native__output__json__
// expected above). Assert the full object once the ws send path is fixed.
BOOST_AUTO_TEST_CASE(native__ws_output__json__truncated_bug_c1)
{
    BOOST_REQUIRE(!ws_upgrade());
    const auto response = ws_get_json(
        "/v1/output/" + coinbase_txid(test::block1) + "/0?format=json");
    BOOST_REQUIRE(!response.is_object());
}

BOOST_AUTO_TEST_SUITE_END()
