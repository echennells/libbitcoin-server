# ECH-125 verification — gettxout unspendable-output filter

Verification record for the fix commit on this branch
(`Filter unspendable outputs from gettxout (bitcoind parity)`).

## The defect

`handle_get_tx_out` returned a full result object for any archived output that
is not terminal and not spent. libbitcoin's archive retains every output and
models only spent/unspent (it has no UTXO-set concept), whereas bitcoind
excludes provably-unspendable outputs from its UTXO set
(`CCoinsViewCache::AddCoin` skips `IsUnspendable`) and so returns `null` for
them. gettxout on an OP_RETURN output (e.g. a coinbase witness commitment)
therefore reported a live UTXO where Bitcoin Core reports `null`.

## The fix

A bitcoind-surface helper `is_utxo_set_excluded(script)` mirroring Core's
`CScript::IsUnspendable` exactly:

```cpp
static bool is_utxo_set_excluded(const chain::script& script) NOEXCEPT
{
    return chain::script::is_pay_op_return_pattern(script.ops())
        || script.serialized_size(false) > chain::max_script_size;
}
```

`is_pay_op_return_pattern` is byte-for-byte Core's `*begin() == OP_RETURN`
clause; `max_script_size` (10000) equals Core's `MAX_SCRIPT_SIZE`. This is
deliberately Core's criterion, **not** `chain::script::is_unspendable` (which
keys on a reserved/invalid leading opcode and would diverge). The helper is
shared so the sibling UTXO methods (`gettxoutsetinfo`, `scantxoutset`) can reuse
the identical rule.

## Build (2026-08-08)

Full server stack built from this commit, with the four dependency repos pinned
to the differential rig's HEADs so the only delta from the running node is this
patch:

- system `35b3e4c5`, database `b277b285`, network `41caed22`, node `ada11dc7`
- `install-gnu.sh … --build-config=release --build-link=static -j4`
- Result: **compiles clean** (`BUILD_EXIT=0`).

## Runtime A/B vs Bitcoin Core (testnet3, tip 5,104,815)

Patched `bs` served the live testnet3 store (opened clean at tip) and was
probed against Core on four classes:

| probe               | Core | bs (patched) | result |
|---------------------|------|--------------|--------|
| OP_RETURN outpoint  | null | null         | PASS — the fix (stock bs returned a full coin) |
| p2wpkh control      | coin | coin         | PASS — no regression |
| spent outpoint      | null | null         | PASS — unchanged |
| bogus txid          | null | null         | PASS — unchanged |

**All four match Core.** The change affects exactly the unspendable-output case
and nothing else.

## Scope note

The mismatch is a family across the UTXO-shaped surfaces, but the fix location
differs per surface. bitcoind `getutxos` (REST) is **declared but not
implemented** — `get_utxos`/`get_utxos_confirmed` appear in the interface tuple
(`bitcoind_rest.hpp:47-48`) with no corresponding `handle_get_utxos`
subscription in `protocol_bitcoind_rest.cpp:54-62`, so there is no handler for
`is_utxo_set_excluded` to guard. It becomes relevant only if that endpoint is
implemented. (An earlier revision of this file claimed it was structurally
identical and could reuse the helper; that read a declaration as an
implementation and was wrong.) The Electrum `listunspent` reference
(Fulcrum/electrs) also excludes OP_RETURN, but excludes it at *index* time — so
an Electrum-parity fix, if wanted, belongs in the address indexer, not here. The
native protocol has no Core reference and is unaffected by design.
