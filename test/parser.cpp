/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#include "test.hpp"
#include <sstream>

BOOST_AUTO_TEST_SUITE(parser_tests)

using namespace bc::system::chain;

constexpr auto p2kh_test = system::wallet::prefix::p2kh::test::btc;
constexpr auto p2kh_main = system::wallet::prefix::p2kh::main::btc;
constexpr auto identifier_test = 118034699_u32;
constexpr auto identifier_main = 3652501241_u32;

// context

BOOST_AUTO_TEST_CASE(parser__context__no_option__mainnet)
{
    const char* argv[]{ "bs" };
    std::ostringstream error{};
    BOOST_REQUIRE(parser::context(1, argv, error) == selection::mainnet);
    BOOST_REQUIRE(error.str().empty());
}

BOOST_AUTO_TEST_CASE(parser__context__testnet3__expected)
{
    const char* argv[]{ "bs", "--network", "testnet3" };
    std::ostringstream error{};
    BOOST_REQUIRE(parser::context(3, argv, error) == selection::testnet3);
    BOOST_REQUIRE(error.str().empty());
}

BOOST_AUTO_TEST_CASE(parser__context__regtest_assigned__expected)
{
    const char* argv[]{ "bs", "--network=regtest" };
    std::ostringstream error{};
    BOOST_REQUIRE(parser::context(2, argv, error) == selection::regtest);
    BOOST_REQUIRE(error.str().empty());
}

BOOST_AUTO_TEST_CASE(parser__context__unknown_network__none_with_error)
{
    const char* argv[]{ "bs", "--network", "signet" };
    std::ostringstream error{};
    BOOST_REQUIRE(parser::context(3, argv, error) == selection::none);
    BOOST_REQUIRE(!error.str().empty());
}

// construct

BOOST_AUTO_TEST_CASE(parser__construct__mainnet__mainnet_defaults)
{
    const settings::embedded_pages pages{};
    const parser instance(selection::mainnet, pages, pages);
    const auto& configured = instance.configured;
    BOOST_REQUIRE(configured.context == selection::mainnet);
    BOOST_REQUIRE_EQUAL(configured.network.identifier, identifier_main);
    BOOST_REQUIRE_EQUAL(configured.server.wallet.p2kh_prefix, p2kh_main);
    BOOST_REQUIRE(configured.bitcoin.forks.difficult);
    BOOST_REQUIRE_EQUAL(configured.network.outbound.seeds.size(), 8u);
    BOOST_REQUIRE_EQUAL(configured.network.outbound.seeds.front().port(), 8333u);
}

BOOST_AUTO_TEST_CASE(parser__construct__testnet3__testnet3_defaults)
{
    const settings::embedded_pages pages{};
    const parser instance(selection::testnet3, pages, pages);
    const auto& configured = instance.configured;
    BOOST_REQUIRE(configured.context == selection::testnet3);
    BOOST_REQUIRE_EQUAL(configured.network.identifier, identifier_test);
    BOOST_REQUIRE_EQUAL(configured.server.wallet.p2kh_prefix, p2kh_test);
    BOOST_REQUIRE(!configured.bitcoin.forks.difficult);
    BOOST_REQUIRE_EQUAL(configured.network.outbound.seeds.size(), 4u);
    BOOST_REQUIRE_EQUAL(configured.network.outbound.seeds.front().port(), 18333u);
}

BOOST_AUTO_TEST_CASE(parser__construct__regtest__no_seeds)
{
    const settings::embedded_pages pages{};
    const parser instance(selection::regtest, pages, pages);
    BOOST_REQUIRE(instance.configured.context == selection::regtest);
    BOOST_REQUIRE(instance.configured.network.outbound.seeds.empty());
}

// parse

BOOST_AUTO_TEST_CASE(parser__parse__network_option__context_set_settings_retained)
{
    const char* argv[]{ "bs", "--network", "testnet3" };
    const settings::embedded_pages pages{};
    parser instance(selection::testnet3, pages, pages);
    std::ostringstream error{};
    BOOST_REQUIRE(instance.parse(3, argv, error));
    BOOST_REQUIRE(error.str().empty());
    BOOST_REQUIRE(instance.configured.context == selection::testnet3);
    BOOST_REQUIRE_EQUAL(instance.configured.network.identifier, identifier_test);
    BOOST_REQUIRE_EQUAL(instance.configured.server.wallet.p2kh_prefix, p2kh_test);
}

BOOST_AUTO_TEST_CASE(parser__parse__unknown_network__false_with_error)
{
    const char* argv[]{ "bs", "--network", "signet" };
    const settings::embedded_pages pages{};
    parser instance(selection::mainnet, pages, pages);
    std::ostringstream error{};
    BOOST_REQUIRE(!instance.parse(3, argv, error));
    BOOST_REQUIRE(!error.str().empty());
}

BOOST_AUTO_TEST_SUITE_END()
