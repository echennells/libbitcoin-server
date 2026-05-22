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

// configuration (http)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native__configuration__json__expected_object)
{
    const auto response = get_json("/v1/configuration?format=json");
    BOOST_REQUIRE(response.is_object());

    // handle_get_configuration emits exactly these six boolean fields.
    const auto& object = response.as_object();
    BOOST_REQUIRE(object.contains("address"));
    BOOST_REQUIRE(object.contains("filter"));
    BOOST_REQUIRE(object.contains("turbo"));
    BOOST_REQUIRE(object.contains("witness"));
    BOOST_REQUIRE(object.contains("retarget"));
    BOOST_REQUIRE(object.contains("difficult"));
    BOOST_REQUIRE(object.at("address").is_bool());
    BOOST_REQUIRE(object.at("filter").is_bool());
    BOOST_REQUIRE(object.at("turbo").is_bool());
    BOOST_REQUIRE(object.at("witness").is_bool());
    BOOST_REQUIRE(object.at("retarget").is_bool());
    BOOST_REQUIRE(object.at("difficult").is_bool());
}

BOOST_AUTO_TEST_CASE(native__configuration__mainnet_forks__retarget_and_difficult_true)
{
    // native_setup_fixture configures chain::selection::mainnet, whose forks
    // enable both retargeting and difficulty enforcement.
    const auto response = get_json("/v1/configuration?format=json");
    BOOST_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BOOST_REQUIRE_EQUAL(object.at("retarget").as_bool(), true);
    BOOST_REQUIRE_EQUAL(object.at("difficult").as_bool(), true);
}

BOOST_AUTO_TEST_CASE(native__configuration__text__not_acceptable)
{
    // Configuration is json-only; non-json media is rejected by the handler.
    BOOST_REQUIRE_EQUAL(get_status("/v1/configuration?format=text"),
        http::status::not_acceptable);
}

BOOST_AUTO_TEST_CASE(native__configuration__data__not_acceptable)
{
    BOOST_REQUIRE_EQUAL(get_status("/v1/configuration?format=data"),
        http::status::not_acceptable);
}

BOOST_AUTO_TEST_CASE(native__configuration__xml__not_acceptable)
{
    // Unknown format is rejected by native_query before dispatch.
    BOOST_REQUIRE_EQUAL(get_status("/v1/configuration?format=xml"),
        http::status::not_acceptable);
}

BOOST_AUTO_TEST_CASE(native__configuration__no_format__not_acceptable)
{
    // No format query and no Accept header: native_query fails -> 406.
    BOOST_REQUIRE_EQUAL(get_status("/v1/configuration"),
        http::status::not_acceptable);
}

// configuration (websocket)
// ----------------------------------------------------------------------------

// Characterizes bug C1: handle_get_configuration passes a 64-byte size hint to
// send_json, and on the websocket send path that hint is a hard cap -- the JSON
// object is truncated to exactly 64 bytes and arrives malformed. The HTTP path
// grows its buffer and is unaffected (see native__configuration__json__expected_
// object above, which passes). This test pins the current buggy behavior; a fix
// to the ws send path will trip it, at which point it should be changed to
// assert the whole object.
BOOST_AUTO_TEST_CASE(native__ws_configuration__json__truncated_to_hint_bug_c1)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto text = ws_get_text("/v1/configuration?format=json");

    // BUG: should be the entire JSON object; instead capped to the send_json hint.
    BOOST_REQUIRE_EQUAL(text.size(), 64u);
}

BOOST_AUTO_TEST_CASE(native__ws_configuration__xml__error_eof)
{
    BOOST_REQUIRE(!ws_upgrade());
    BOOST_REQUIRE(ws_dropped("/v1/configuration?format=xml"));
}

BOOST_AUTO_TEST_SUITE_END()
