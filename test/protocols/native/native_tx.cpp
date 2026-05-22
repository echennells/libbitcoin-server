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

// The ten-block store holds the genesis block plus mainnet blocks 1-9, each
// with exactly one (coinbase) transaction. block1's coinbase is the subject.
namespace {

// Reversed-hex (display) txid of a block's coinbase, as native_target expects
// in the URL path (it decodes the token via decode_hash).
std::string coinbase_txid(const chain::block& block)
{
    return encode_hash(block.transactions_ptr()->front()->hash(false));
}

// A syntactically valid txid that is absent from the store.
const std::string absent_txid{
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef" };

} // namespace

// tx (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__tx__json__expected)
{
    const auto response = get_json(
        "/v1/tx/" + coinbase_txid(test::block1) + "?format=json");
    BOOST_REQUIRE(response.is_object());
    BOOST_REQUIRE(response.as_object().contains("inputs"));
    BOOST_REQUIRE(response.as_object().contains("outputs"));
}

BOOST_AUTO_TEST_CASE(native__tx__text__expected)
{
    const auto cb = test::block1.transactions_ptr()->front();
    const auto body = get_text(
        "/v1/tx/" + coinbase_txid(test::block1) + "?format=text");

    // Text media is the hex encoding, so twice the serialized byte count.
    BOOST_REQUIRE_EQUAL(body.size(), 2u * cb->serialized_size(false));
}

BOOST_AUTO_TEST_CASE(native__tx__data__expected)
{
    const auto cb = test::block1.transactions_ptr()->front();
    const auto body = get_data(
        "/v1/tx/" + coinbase_txid(test::block1) + "?format=data");
    BOOST_REQUIRE_EQUAL(body.size(), cb->serialized_size(false));
}

BOOST_AUTO_TEST_CASE(native__tx__absent_hash__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/tx/" + absent_txid + "?format=json"),
        http::status::not_found);
}

BOOST_AUTO_TEST_CASE(native__tx__xml__not_acceptable)
{
    BOOST_REQUIRE_EQUAL(get_status(
        "/v1/tx/" + coinbase_txid(test::block1) + "?format=xml"),
        http::status::not_acceptable);
}

BOOST_AUTO_TEST_CASE(native__tx__no_format__not_acceptable)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/tx/" + coinbase_txid(test::block1)),
        http::status::not_acceptable);
}

// tx/header (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__tx_header__json__expected)
{
    const auto response = get_json(
        "/v1/tx/" + coinbase_txid(test::block1) + "/header?format=json");
    BOOST_REQUIRE(response.is_object());

    // handle_get_tx_header injects the confirmed height; block1 is height 1.
    BOOST_REQUIRE(response.as_object().contains("height"));
    BOOST_REQUIRE_EQUAL(response.as_object().at("height").to_number<uint64_t>(),
        1u);
}

BOOST_AUTO_TEST_CASE(native__tx_header__absent_hash__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status(
        "/v1/tx/" + absent_txid + "/header?format=json"),
        http::status::not_found);
}

// tx/details (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__tx_details__json__expected)
{
    const auto response = get_json(
        "/v1/tx/" + coinbase_txid(test::block1) + "/details?format=json");
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE(object.contains("confirmed"));
    BOOST_REQUIRE(object.at("coinbase").is_bool());
    BOOST_REQUIRE_EQUAL(object.at("coinbase").as_bool(), true);
}

BOOST_AUTO_TEST_CASE(native__tx_details__text__not_acceptable)
{
    // tx/details is json-only.
    BOOST_REQUIRE_EQUAL(get_status(
        "/v1/tx/" + coinbase_txid(test::block1) + "/details?format=text"),
        http::status::not_acceptable);
}

BOOST_AUTO_TEST_CASE(native__tx_details__absent_hash__not_found)
{
    BOOST_REQUIRE_EQUAL(get_status(
        "/v1/tx/" + absent_txid + "/details?format=json"),
        http::status::not_found);
}

// tx (websocket)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__ws_tx__text__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    // send_text takes no size hint, so text is not affected by bug C1.
    const auto cb = test::block1.transactions_ptr()->front();
    const auto body = ws_get_text(
        "/v1/tx/" + coinbase_txid(test::block1) + "?format=text");
    BOOST_REQUIRE_EQUAL(body.size(), 2u * cb->serialized_size(false));
}

BOOST_AUTO_TEST_CASE(native__ws_tx__data__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    // send_chunk takes no size hint, so data is not affected by bug C1.
    const auto cb = test::block1.transactions_ptr()->front();
    const auto body = ws_get_data(
        "/v1/tx/" + coinbase_txid(test::block1) + "?format=data");
    BOOST_REQUIRE_EQUAL(body.size(), cb->serialized_size(false));
}

// Characterizes bug C1 blast radius: handle_get_tx calls send_json with a
// size hint of two * serialized_size, which is smaller than the JSON form of
// the transaction. On the websocket path that hint caps the frame, so the tx
// arrives as truncated, unparseable JSON. The HTTP path is unaffected (see
// native__tx__json__expected above, which passes). Change this to assert the
// full object once the ws send path stops treating the hint as a hard limit.
BOOST_AUTO_TEST_CASE(native__ws_tx__json__truncated_bug_c1)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json(
        "/v1/tx/" + coinbase_txid(test::block1) + "?format=json");

    // BUG C1: truncated -> does not parse as an object.
    BOOST_REQUIRE(!response.is_object());
}

BOOST_AUTO_TEST_SUITE_END()
