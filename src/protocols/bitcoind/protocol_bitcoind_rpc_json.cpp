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
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

#include <memory>
#include <mutex>
#include <utility>
#include <variant>
#include <vector>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

uint32_t protocol_bitcoind_rpc::median_time_past(const node::query& query,
    const database::header_link& link) NOEXCEPT
{
    chain::context ctx{};
    return query.get_context(ctx, link) ? ctx.median_time_past : 0_u32;
}

// bitcoind accepts boolean or number for the getblock/getrawtransaction
// verbosity parameters (both were originally boolean and remain so in
// common clients), so a typed number subscription cannot match them.
bool protocol_bitcoind_rpc::parse_verbosity(double& verbosity,
    const network::rpc::value_t& value, double missing) NOEXCEPT
{
    using namespace network::rpc;
    const auto& inner = value.value();

    if (std::holds_alternative<null_t>(inner))
        verbosity = missing;
    else if (std::holds_alternative<boolean_t>(inner))
        verbosity = std::get<boolean_t>(inner) ? 1.0 : 0.0;
    else if (std::holds_alternative<number_t>(inner))
        verbosity = std::get<number_t>(inner);
    else
        return false;

    return true;
}

// Sum of work from genesis to the identified header, as bitcoind encodes
// it (chain_state spans the chain to accumulate, as with organization).
// Work to a given header is immutable, so a single-entry cache requires no
// invalidation and absorbs the dominant access pattern (repeated queries
// at the tip). Distinct historical queries still pay the span computation.
std::string protocol_bitcoind_rpc::to_chain_work(const node::query& query,
    const system::settings& settings, const hash_digest& hash) NOEXCEPT
{
    using cache_t = std::pair<hash_digest, std::string>;
    static std::mutex mutex{};
    static std::shared_ptr<const cache_t> cache{};

    // The span computation runs outside the lock; only access is guarded.
    {
        const std::lock_guard<std::mutex> lock{ mutex };
        if (cache && cache->first == hash)
            return cache->second;
    }

    const auto state = query.get_chain_state(settings, hash);
    if (!state)
        return {};

    const auto work = encode_hash(from_uintx(state->cumulative_work()));
    {
        const std::lock_guard<std::mutex> lock{ mutex };
        cache = std::make_shared<const cache_t>(hash, work);
    }

    return work;
}

void protocol_bitcoind_rpc::inject_block_context(boost::json::object& out,
    const node::query& query, const system::settings& settings,
    const database::header_link& link, const chain::header& header) NOEXCEPT
{
    size_t height{};
    if (!query.get_height(height, link))
        return;

    const auto top = query.get_top_confirmed();
    const auto confirmed = query.is_confirmed_block(link);
    out["height"] = height;
    out["confirmations"] = add1(floored_subtract(top, height));
    out["mediantime"] = median_time_past(query, link);

    const auto chain_work = to_chain_work(query, settings, header.hash());
    if (!chain_work.empty())
        out["chainwork"] = chain_work;

    if (header.previous_block_hash() != null_hash)
        out["previousblockhash"] = encode_hash(header.previous_block_hash());

    if (confirmed && height < top)
        out["nextblockhash"] = encode_hash(
            query.get_header_key(query.to_confirmed(add1(height))));
}

void protocol_bitcoind_rpc::inject_tx_context(boost::json::object& out,
    const node::query& query, const database::tx_link& link) NOEXCEPT
{
    size_t height{};
    if (!query.get_tx_height(height, link))
    {
        out["confirmations"] = 0;
        return;
    }

    const auto block = query.to_confirmed(height);
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(block);
    out["in_active_chain"] = true;
    out["blockhash"] = encode_hash(query.get_header_key(block));
    out["confirmations"] = add1(floored_subtract(top, height));
    if (header)
    {
        out["blocktime"] = header->timestamp();
        out["time"] = header->timestamp();
    }
}

boost::json::object protocol_bitcoind_rpc::header_to_bitcoind(
    const chain::header& header) NOEXCEPT
{
    return boost::json::object
    {
        { "hash", encode_hash(header.hash()) },
        { "version", header.version() },
        { "versionHex", encode_base16(to_big_endian(header.version())) },
        { "merkleroot", encode_hash(header.merkle_root()) },
        { "time", header.timestamp() },
        { "nonce", header.nonce() },
        { "bits", encode_base16(to_big_endian(header.bits())) },
        { "difficulty", header.difficulty() }
    };
}

std::string protocol_bitcoind_rpc::chain_name(const node::query& query) NOEXCEPT
{
    const auto genesis = query.get_header_key(query.to_confirmed(zero));

    // TODO: create signet chain selector.
    using selection = chain::selection;
    constexpr auto signet = base16_hash(
        "00000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6");
    static const std::vector<std::pair<hash_digest, std::string>> networks
    {
        { system::settings{ selection::mainnet }.genesis_block.hash(), "main" },
        { system::settings{ selection::testnet3 }.genesis_block.hash(), "test" },
        { system::settings{ selection::regtest }.genesis_block.hash(), "regtest" },
        { signet, "signet" }
    };

    for (const auto& [hash, name]: networks)
        if (hash == genesis)
            return name;

    return "unknown";
}

} // namespace server
} // namespace libbitcoin
