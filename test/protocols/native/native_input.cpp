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

// block1's coinbase has exactly one input (a coinbase input, no witness).
namespace {

std::string coinbase_txid(const chain::block& block)
{
    return encode_hash(block.transactions_ptr()->front()->hash(false));
}

const std::string absent_txid{
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef" };

} // namespace

// inputs -- all inputs of a transaction (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__inputs__json__expected)
{
    const auto response = get_json(
        "/v1/input/" + coinbase_txid(test::block1) + "?format=json");
    BOOST_REQUIRE(response.is_array());
    BOOST_REQUIRE_EQUAL(response.as_array().size(), 1u);
}

BOOST_AUTO_TEST_CASE(native__inputs__text__expected)
{
    const auto body = get_text(
        "/v1/input/" + coinbase_txid(test::block1) + "?format=text");
    BOOST_REQUIRE(!body.empty());
}

BOOST_AUTO_TEST_CASE(native__inputs__data__expected)
{
    const auto body = get_data(
        "/v1/input/" + coinbase_txid(test::block1) + "?format=data");
    BOOST_REQUIRE(!body.empty());
}

BOOST_AUTO_TEST_CASE(native__inputs__absent_hash__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/input/" + absent_txid + "?format=json"),
        http::status::not_found);
}

BOOST_AUTO_TEST_CASE(native__inputs__xml__not_acceptable)
{
    BOOST_REQUIRE_EQUAL(get_status(
        "/v1/input/" + coinbase_txid(test::block1) + "?format=xml"),
        http::status::not_acceptable);
}

// input -- single input by index (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__input__json__expected)
{
    const auto response = get_json(
        "/v1/input/" + coinbase_txid(test::block1) + "/0?format=json");
    BOOST_REQUIRE(response.is_object());
}

BOOST_AUTO_TEST_CASE(native__input__index_out_of_range__not_found)
{
    // The coinbase tx has a single input; index 9 does not exist.
    BOOST_REQUIRE_EQUAL(get_status(
        "/v1/input/" + coinbase_txid(test::block1) + "/9?format=json"),
        http::status::not_found);
}

// input/script (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__input_script__data__expected)
{
    const auto body = get_data(
        "/v1/input/" + coinbase_txid(test::block1) + "/0/script?format=data");
    BOOST_REQUIRE(!body.empty());
}

// input/witness (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__input_witness__coinbase__not_found)
{
    // The block1 coinbase input carries no witness; the handler 404s.
    BOOST_REQUIRE_EQUAL(get_status(
        "/v1/input/" + coinbase_txid(test::block1) + "/0/witness?format=data"),
        http::status::not_found);
}

// inputs (websocket)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__ws_inputs__data__expected)
{
    BOOST_REQUIRE(!ws_upgrade());
    const auto body = ws_get_data(
        "/v1/input/" + coinbase_txid(test::block1) + "?format=data");
    BOOST_REQUIRE(!body.empty());
}

// Characterizes bug C1: inputs json over websocket is truncated to the
// send_json size hint (two * binary size), smaller than the JSON array, so it
// arrives unparseable. The HTTP path is unaffected (see native__inputs__json__
// expected above). Assert the full array once the ws send path is fixed.
BOOST_AUTO_TEST_CASE(native__ws_inputs__json__truncated_bug_c1)
{
    BOOST_REQUIRE(!ws_upgrade());
    const auto response = ws_get_json(
        "/v1/input/" + coinbase_txid(test::block1) + "?format=json");
    BOOST_REQUIRE(!response.is_array());
}

BOOST_AUTO_TEST_SUITE_END()
