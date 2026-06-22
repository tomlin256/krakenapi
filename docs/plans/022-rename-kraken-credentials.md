# Plan 022 — Rename Kraken `Credentials` → `KrakenCredentials`

## Motivation

Every other adapter names its credential struct after the exchange
(`BinanceCredentials`, `CoinbaseCredentials`, `CryptoComCredentials`) and pairs it
with a `*Auth` signer. Kraken alone calls its struct the bare
`exchange::kraken::rest::Credentials`, which is ambiguous at call sites that touch
more than one exchange and inconsistent with the rest of the codebase (it already
has a `KrakenAuth`). Rename it to **`KrakenCredentials`**.

This is a **breaking change** (accepted — consistent with the project's no-compat
-shim philosophy from plan 014, so **no deprecated alias** is kept).

## Decision

- New name: **`KrakenCredentials`** (in the same `exchange::kraken::rest`
  namespace, mirroring `exchange::binance::rest::BinanceCredentials`).
- **Out of scope:** `exchange::kraken::ws::WsCredentials` (the WS session-token
  wrapper) — its `Ws` prefix + `ws` namespace already disambiguate it; it is not
  the ambiguous "bare Credentials". Note it here so leaving it is a deliberate
  choice, not an oversight.

## Scope (sized by grep — ~109 bare `Credentials` references)

`\bCredentials\b` cleanly excludes `WsCredentials`, the `*Credentials` compounds,
and `read_toml_credentials`, so the rename is a safe whole-word replace.

- **Library:** `include/exchange/kraken/auth.hpp` (struct + `KrakenAuth` ctor),
  `rest_api.hpp` (`build(const Credentials&)`), `rest_client.hpp` +
  `rest_client.inl` (`execute(req, const Credentials&)`), `src/kraken/auth.cpp`,
  `src/kraken/rest_api.cpp`.
- **Tests:** `test_client.cpp`, `test_kraken_auth.cpp`, `test_signature.cpp`,
  `test_rest_requests.cpp`.
- **Examples:** `private_rest.cpp`, `private_ws.cpp`, `kraken_example.cpp`.
- **Living docs:** `CLAUDE.md`, `docs/agent-add-exchange.md`, and the
  `001-appendix-migration-guide.md` (add the `Credentials → KrakenCredentials`
  row). **Historical plan docs (001/004/006/011/016/018/020…) are left as-is** —
  they record past state and should not be retro-edited.
- One **comment** mention in `include/exchange/common/credentials_file.hpp`
  ("every adapter's Credentials::from_file") — genericise the wording.

## Steps

### Step 1 — Library + tests + examples
- Whole-word rename `Credentials` → `KrakenCredentials` across the library, test,
  and example files above (verify each diff hunk — no `WsCredentials` /
  `read_toml_credentials` / compound caught).
- Full `cmake --build build` + `ctest` green.
- **Done:** builds and all tests pass; `grep -rn "\bCredentials\b" include src tests/unit`
  returns only `WsCredentials`/compounds/`read_toml_credentials` (zero bare hits).
- Commit.

### Step 2 — Docs
- Update `CLAUDE.md`, `agent-add-exchange.md`, the migration guide, and the
  `credentials_file.hpp` comment.
- Mark plan 022 Done in `docs/plans.md`.
- Commit.

## Sequencing (relative to plan 021, in flight)

Recommended: **finish plan 021 first** (Steps 4 Coinbase, 5 Crypto.com, 6 docs),
then execute 022. 021's remaining code steps don't touch Kraken's `Credentials`,
so they're unaffected; 022 will re-touch the few Kraken credential lines 021's doc
step writes (small, mechanical rework) in exchange for keeping each plan a clean,
independently-committed unit. Alternative on request: do 022 now (before 021's doc
step) so the docs are written once with the final name.

## Self-review — risks

- **Whole-word safety:** the one risk is over-matching; mitigated by `\bCredentials\b`
  and a per-hunk review, plus the post-rename grep assertion in Step 1's Done.
- **Breaking change:** intentional, no alias; folds into the same 0.5.0 breaking
  bump already planned in 021 (no separate bump needed).
- **No behavioural change:** pure rename — existing tests passing unchanged is the
  proof.
