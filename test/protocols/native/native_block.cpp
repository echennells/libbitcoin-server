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

// top (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__top__json__expected)
{
    const auto response = get_json("/v1/top?format=json");
    BOOST_REQUIRE(response.is_int64());
    BOOST_REQUIRE_EQUAL(response.as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(native__top__text__expected)
{
    const auto body = get_text("/v1/top?format=text");
    BOOST_REQUIRE_EQUAL(body, "09");
}

BOOST_AUTO_TEST_CASE(native__top__data__expected)
{
    const auto body = get_data("/v1/top?format=data");
    BOOST_REQUIRE_EQUAL(body, base16_chunk("09"));
}

BOOST_AUTO_TEST_CASE(native__top__xml__not_acceptable)
{
    const auto status = get_status("/v1/top?format=xml");
    BOOST_REQUIRE_EQUAL(status, http::status::not_acceptable);
}

BOOST_AUTO_TEST_CASE(native__top__default__not_acceptable)
{
    // No format query and no Accept header: native_query fails -> 406.
    const auto status = get_status("/v1/top");
    BOOST_REQUIRE_EQUAL(status, http::status::not_acceptable);
}

// subscribe

BOOST_AUTO_TEST_CASE(native__top_subscribe__json__expected)
{
    const auto response = get_json("/v1/top/subscribe?format=json");
    BOOST_REQUIRE(response.is_int64());
    BOOST_REQUIRE_EQUAL(response.as_int64(), 9);
}

// top (websockets)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__ws_upgrade__always__success)
{
    const auto ec = ws_upgrade();
    BOOST_REQUIRE_MESSAGE(!ec, ec.message());
}

BOOST_AUTO_TEST_CASE(native__ws_top__json__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json("/v1/top?format=json");
    BOOST_REQUIRE(response.is_int64());
    BOOST_REQUIRE_EQUAL(response.as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(native__ws_top__text__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_text("/v1/top?format=text");
    BOOST_REQUIRE_EQUAL(response, "09");
}

BOOST_AUTO_TEST_CASE(native__ws_top__data__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_data("/v1/top?format=data");
    BOOST_REQUIRE_EQUAL(response, base16_chunk("09"));
}

BOOST_AUTO_TEST_CASE(native__ws_top__xml__error_eof)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_dropped("/v1/top?format=xml"));
}

// subscribe

BOOST_AUTO_TEST_CASE(native__ws_top_subscribe__json__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json("/v1/top/subscribe?format=json");
    BOOST_REQUIRE(response.is_int64());
    BOOST_REQUIRE_EQUAL(response.as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(native__ws_top_subscribe__stop__empty)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_get_text("/v1/top/subscribe?stop=true");
    BOOST_REQUIRE(response.empty());
}

BOOST_AUTO_TEST_CASE(native__ws_top_subscribe__repeat__idempotent)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response1 = ws_get_json("/v1/top/subscribe?format=json");
    BOOST_REQUIRE(response1.is_int64());
    BOOST_REQUIRE_EQUAL(response1.as_int64(), 9);

    const auto response2 = ws_get_json("/v1/top/subscribe?format=json");
    BOOST_REQUIRE(response2.is_int64());
    BOOST_REQUIRE_EQUAL(response2.as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(native__ws_top_subscribe__progressive_notify__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    BOOST_REQUIRE(query_.set(test::bogus_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11, database::context{ 0, 11, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block12, database::context{ 0, 12, 0 }, false, false));

    const auto response = ws_get_json("/v1/top/subscribe?format=json");
    BOOST_REQUIRE(response.is_int64());
    BOOST_REQUIRE_EQUAL(response.as_int64(), 9);

    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block10.hash()), true));
    BOOST_REQUIRE_EQUAL(ws_get_text("/v1/top/subscribe?format=text"), "0a");

    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::bogus_block11.hash()), true));
    notify(node::chase::organized, node::header_t{ 11 });

    BOOST_REQUIRE_EQUAL(to_string(ws_receive()), "0b");
}

// Characterizes bug C1 on the websocket NOTIFICATION path (ECH-33 reach).
// do_block emits notify_json(value_from(encode_base16(hash)), two * hash_size):
// a 64-char hash quoted to a 66-byte JSON string, against a 64-byte size hint
// (two * hash_size omits the two JSON quote characters). channel_http::notify
// routes through the same write -> async_write_http -> socket::body_write path
// as a normal send (socket.cpp:436) -- it is literally the same function, not a
// parallel one -- so the notification fragments by the identical C1 mechanism:
// body_write loops writer.get() and async_write's each size_hint-sized chunk,
// and each websocket async_write is a complete FIN message. The notification is
// delivered as a 64-byte message plus a 2-byte message; a subscriber reading
// one message per notification gets an unparseable JSON fragment. This pins the
// current buggy framing; a fix to the ws write path will trip it (then assert
// one 66-byte message). notify_text / notify_chunk are single-shot, unaffected.
BOOST_AUTO_TEST_CASE(native__ws_block_subscribe__notify_json__fragmented_bug_c1)
{
    BOOST_REQUIRE(!ws_upgrade());

    BOOST_REQUIRE(query_.set(test::bogus_block10,
        database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::bogus_block11,
        database::context{ 0, 11, 0 }, false, false));

    // Register a json block subscription. The initial subscribe response is the
    // top hash and is itself C1-fragmented (send_json, same two * hash_size
    // hint); drain its two fragments (64 + 2) before driving the notification.
    BOOST_REQUIRE_EQUAL(
        ws_get_data("/v1/block/subscribe?format=json").size(), 64u);
    BOOST_REQUIRE_EQUAL(ws_receive().size(), 2u);

    // Fire the chase event that drives do_block -> notify_json.
    notify(node::chase::organized, node::header_t{ 11 });

    // BUG C1: the single logical notification arrives as two ws messages.
    BOOST_REQUIRE_EQUAL(ws_receive().size(), 64u);
    BOOST_REQUIRE_EQUAL(ws_receive().size(), 2u);
}

BOOST_AUTO_TEST_SUITE_END()
