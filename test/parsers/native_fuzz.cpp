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
#include "../test.hpp"
#include <random>
#include <string>
#include <variant>
#include <vector>

// Adversarial-input ("fuzz") coverage for the native request parsers,
// native_target (URL path) and native_query (query string). The existing
// parser test files are example-based; these exercise overflow, malformed,
// oversized and random inputs. A fixed PRNG seed keeps any failure reproducible.

BOOST_AUTO_TEST_SUITE(native_fuzz_tests)

using namespace system;
using namespace network::rpc;
using namespace network::http;
using object_t = network::rpc::object_t;

namespace {

constexpr uint32_t fuzz_seed = 0x6c696266; // 'libf'

// Numeric tokens that probe overflow, sign handling, padding and bases.
const std::vector<std::string> bad_numbers
{
    "256", "4294967296", "99999999999999999999", "18446744073709551616",
    "-1", "+5", "0x10", "1e3", " 7", "07", "00", ""
};

// 64-hex-char is the only valid hash; each of these is malformed.
const std::vector<std::string> malformed_hashes
{
    "",
    std::string(63, 'a'),
    std::string(65, 'a'),
    std::string(64, 'g'),
    std::string(128, 'f'),
    "0x" + std::string(62, '0')
};

std::string random_string(std::mt19937& rng, size_t max_len)
{
    std::uniform_int_distribution<size_t> length{ 0, max_len };
    std::uniform_int_distribution<int> octet{ 0, 255 };
    std::string out{};
    const auto size = length(rng);
    out.reserve(size);
    for (size_t i = 0; i < size; ++i)
        out.push_back(static_cast<char>(octet(rng)));

    return out;
}

// A path that looks structured (versioned, segmented) but carries adversarial
// values in each slot - far more likely to reach deep parser branches than
// purely random bytes.
std::string structured_path(std::mt19937& rng)
{
    static const std::vector<std::string> targets
    {
        "configuration", "top", "block", "tx", "input", "output", "address",
        "v", "", "BLOCK", "../etc"
    };
    static const std::vector<std::string> parts
    {
        "hash", "height", "header", "context", "txs", "tx", "filter",
        "subscribe", "details", "script", "witness", "spender", "spenders",
        "confirmed", "unconfirmed", "balance", ""
    };

    std::uniform_int_distribution<size_t> a_target{ 0, targets.size() - 1 };
    std::uniform_int_distribution<size_t> a_part{ 0, parts.size() - 1 };
    std::uniform_int_distribution<size_t> a_number{ 0, bad_numbers.size() - 1 };
    std::uniform_int_distribution<size_t> a_hash{ 0, malformed_hashes.size() - 1 };
    std::uniform_int_distribution<int> segments{ 0, 6 };
    std::uniform_int_distribution<int> kind{ 0, 3 };

    std::string path{ "/v" };
    path += bad_numbers[a_number(rng)];
    const auto count = segments(rng);
    for (int i = 0; i < count; ++i)
    {
        path.push_back('/');
        switch (kind(rng))
        {
            case 0: path += targets[a_target(rng)]; break;
            case 1: path += parts[a_part(rng)]; break;
            case 2: path += bad_numbers[a_number(rng)]; break;
            default: path += malformed_hashes[a_hash(rng)]; break;
        }
    }

    return path;
}

} // namespace

// native_target: integer overflow must be rejected, not silently wrapped.
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native_fuzz__native_target__version_overflow__rejected)
{
    // The version field is uint8; anything above 255 is out of range.
    request_t out{};
    BOOST_REQUIRE_MESSAGE(native_target(out, "/v256/top"),
        "version 256 accepted (uint8 overflow)");
    BOOST_REQUIRE_MESSAGE(native_target(out, "/v4294967296/top"),
        "version 2^32 accepted (uint8 overflow)");
    BOOST_REQUIRE_MESSAGE(native_target(out, "/v99999999999999999999/top"),
        "version 10^20 accepted (uint8 overflow)");
}

BOOST_AUTO_TEST_CASE(native_fuzz__native_target__height_overflow__rejected)
{
    // height/index/position are uint32; 2^32 and above are out of range.
    request_t out{};
    BOOST_REQUIRE_MESSAGE(native_target(out, "/v1/block/height/4294967296"),
        "height 2^32 accepted (uint32 overflow)");
    BOOST_REQUIRE_MESSAGE(
        native_target(out, "/v1/block/height/99999999999999999999"),
        "height 10^20 accepted (uint32 overflow)");

    const std::string hash(64, '0');
    BOOST_REQUIRE_MESSAGE(
        native_target(out, "/v1/output/" + hash + "/4294967296"),
        "output index 2^32 accepted (uint32 overflow)");
}

// native_target: malformed hashes must be rejected.
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native_fuzz__native_target__malformed_hash__rejected)
{
    request_t out{};
    for (const auto& hash: malformed_hashes)
    {
        const auto ec = native_target(out, "/v1/tx/" + hash);
        BOOST_REQUIRE_MESSAGE(ec, "malformed hash accepted: [" << hash << "]");
    }
}

// native_target: generative - never crashes, and success implies a well-formed
// request (non-empty method, object params).
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native_fuzz__native_target__generative__no_crash)
{
    std::mt19937 rng{ fuzz_seed };
    size_t iterations{};
    for (; iterations < 20000u; ++iterations)
    {
        const auto path = ((iterations % 2u) == 0u) ?
            structured_path(rng) : random_string(rng, 256u);

        request_t out{};
        if (!native_target(out, path))
        {
            BOOST_REQUIRE_MESSAGE(!out.method.empty(),
                "success with empty method, path=[" << path << "]");
            BOOST_REQUIRE(out.params.has_value());
            BOOST_REQUIRE(
                std::holds_alternative<object_t>(out.params.value()));
        }
    }

    BOOST_REQUIRE_EQUAL(iterations, 20000u);
}

BOOST_AUTO_TEST_CASE(native_fuzz__native_target__oversized_input__no_crash)
{
    request_t out{};
    native_target(out, "/v1/" + std::string(2u * 1024u * 1024u, 'a'));
    native_target(out, "/v1/block/height/" + std::string(100000u, '9'));
    native_target(out, std::string(4096u, '/'));
    BOOST_REQUIRE(true);
}

// native_query: generative - never crashes, and a true result implies a
// resolved (known) media type.
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(native_fuzz__native_query__generative__no_crash)
{
    std::mt19937 rng{ fuzz_seed ^ 0x55u };
    const media_types accepts{ media_type::application_json };
    size_t iterations{};
    for (; iterations < 20000u; ++iterations)
    {
        const auto target = ((iterations % 2u) == 0u) ?
            structured_path(rng) : random_string(rng, 256u);

        request_t out{};
        out.params = object_t{};
        if (native_query(out, target, accepts))
            BOOST_REQUIRE(get_media(out) != media_type::unknown);
    }

    BOOST_REQUIRE_EQUAL(iterations, 20000u);
}

BOOST_AUTO_TEST_CASE(native_fuzz__native_query__malformed_query__no_crash)
{
    const std::vector<std::string> targets
    {
        "/?format=", "/?format=JSON", "/?format=json&format=text",
        "/?witness=TRUE", "/?stop=1", "/?&&&&", "/?=", "/?x=%",
        "/?" + std::string(100000u, 'a'),
        "/?format=" + std::string(100000u, 'z'),
        std::string("/?a=\0b", 6u)
    };

    const media_types accepts{ media_type::application_json };
    for (const auto& target: targets)
    {
        request_t out{};
        out.params = object_t{};
        native_query(out, target, accepts);
    }

    BOOST_REQUIRE(true);
}

BOOST_AUTO_TEST_SUITE_END()
