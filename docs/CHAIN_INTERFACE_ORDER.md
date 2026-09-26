# Chain implementation order: accepted Contract detail and scoped authority lookup

Status: proposed read-only interface work, **not implemented or advertised by Core**.
Repository owner: `poemei/stn-chain`. This Core job changes no Chain sources.
No consensus, Contract lifecycle, authority, signature, address or economic
semantics are to change. Existing method 11 and its 26-byte response stay intact.

## Shared transport and consistency requirements

Use STNC v2 framing and READ authorization. Allocate two new method identifiers
in `includes/stn_rpc.h` after checking the current registry; 12 and 13 were free
in the inspected checkout, but Core must not send them before Chain assigns them.
Names below are proposed API names, not existing methods. Every integer below is
unsigned big-endian. All fixed byte spans have exact lengths; no NUL terminators,
native structs, JSON, implicit padding or text normalization appear on the wire.

Each request carries a required 32-byte expected accepted tip, obtained from
INFO. Reconstruct/acquire one accepted snapshot under the same storage exclusion
as existing snapshot reads. If the tip differs, return STALE with no payload.
Never mix fields from different snapshots. The returned tip and height identify
an observation, not a guarantee against later reorganization. Core must discard
stale cached applicability and refresh before action construction. Submission
remains independently validated by Chain.

Common failures: malformed/version-unsupported payload = INVALID/VERSION;
missing READ authorization = FORBIDDEN; no usable accepted snapshot = UNAVAILABLE;
snapshot/provider failure = PROVIDER; output capacity too small = CAPACITY.
Every failure has zero response payload; do not truncate or return successful
empty objects. Unknown methods on older nodes remain METHOD. Validate lengths
and bounded counts before allocation or copying, reject trailing bytes and
check arithmetic before size additions. An implementation must update request
and response shape validation in `src/stn_rpc.c`, not just dispatch.

## 1. GET_CONTRACT_DETAIL

Request, exactly 104 bytes:

| Offset | Bytes | Value |
| --- | ---: | --- |
| 0 | 2 | Detail version 1 |
| 2 | 32 | Expected accepted tip |
| 34 | 70 | Canonical lowercase `stnc0_` address |

Success header, exactly 56 bytes, followed by the three variable spans:

| Offset | Bytes | Value |
| --- | ---: | --- |
| 0 | 2 | Detail version 1 |
| 2 | 32 | Accepted snapshot tip |
| 34 | 8 | Accepted snapshot height |
| 42 | 4 | Canonical immutable DRAFT length D |
| 46 | 4 | Canonical current Contract length C |
| 50 | 2 | Eligible approver count |
| 52 | 2 | Required approval count |
| 54 | 2 | Accepted vote count V |
| 56 | D | Exact canonical origin STCT bytes |
| 56+D | C | Canonical STCT encoding of accepted current fields |
| 56+D+C | 32*V | Accepted voter actor public keys, in snapshot vote order |

D and C are each 32..66,656 bytes, V is 0..32. Maximum success payload is
134,392 bytes. The origin must decode as DRAFT/0 and hash to the requested
address. Both STCT objects must pass the existing structural codec; existing
lineage validation must match immutable fields. Counts must agree with the
snapshot and remain within its bounds. Return accepted state and sequence
unchanged; do not calculate a new transition for presentation. Missing Contract
= NOT_FOUND. Do not return a locally pending Contract as accepted.

Owner: accepted `stn_chain_state.contracts`, `stn_contract_snapshot`, and
`stn_contract_state_entry` (`canonical_draft`, `current`, vote offset/count,
eligible/required counts). Extend `src/stn_node_service.c` alongside method 11.
The current view may have borrowed participant bytes; extract the at-most-32
participants through `stn_contract_participant_at` into an explicit temporary
array before calling the encoder. Never serialize native memory or mutate the
snapshot. This detail lookup is sufficient for address search and acting on a
known Contract; a global Contract enumeration interface is not required here.

## 2. FIND_ACCEPTED_AUTHORITY

Request, exactly 130 bytes:

| Offset | Bytes | Value |
| --- | ---: | --- |
| 0 | 2 | Lookup version 1 |
| 2 | 32 | Expected accepted tip |
| 34 | 32 | Canonical actor Ed25519 public key |
| 66 | 32 | Existing versioned authority action token |
| 98 | 32 | Existing versioned authority context token |

The opaque action/context tokens use existing validators and exact matching.
For Contracts use existing `stn_contract_authority_action` and
`stn_contract_authority_context` over the immutable DRAFT, including CREATE
before that draft is accepted. Do not substitute the full Contract hash for
the existing context format: it is version, domain, and the first 30 hash bytes.

Success, exactly 268 bytes:

| Offset | Bytes | Value |
| --- | ---: | --- |
| 0 | 2 | Lookup version 1 |
| 2 | 32 | Accepted snapshot tip |
| 34 | 8 | Accepted snapshot height |
| 42 | 32 | Existing canonical grant identifier |
| 74 | 194 | Exact accepted grant envelope |

Search only accepted `stn_lifecycle_state.grant_bytes/grant_count` and exclude
grants whose IDs are revoked in `lifecycle.authority`. Use existing identity,
grant-ID and scope functions; validate actor encoding and tokens. Select the
first exact subject/action/context match in accepted grant order; returning
one applicable grant is sufficient for action construction, so no unbounded
list or pagination is necessary. The envelope's 97-byte evidence at offset 33
must match the query. Do not invent or re-sign an envelope. No matching accepted,
unrevoked grant = NOT_FOUND. This result reports accepted grant evidence only;
it does not bypass any retirement, participant, sequence, vote, lifecycle or
other checks at submission/consensus evaluation.

Owner: accepted `stn_chain_state.lifecycle` and its grant/revocation state.
Follow the ownership and exclusion patterns already used by the Chain snapshot
provider. `src/stn_chain.c:contract_authorized` documents the corresponding
accepted-grant membership and revocation checks. The client receives public
evidence only; no root or actor private key is exported.

## Qualification required in Chain

Add codec and node-service tests for both read methods: exact known bytes,
endianness, malformed versions/lengths/namespaces/hex, maximum-size Contract,
invalid roles/counts, empty terms, truncated/extra bytes, missing object/grant,
insufficient capacity, disconnected/unavailable provider, READ permission,
wrong actor/action/context, accepted versus pending grant, revoked grant,
multiple matching grants in deterministic order, stale expected tip, and a
reorganization invalidating a previously returned grant or Contract state.
Prove snapshot bytes and counts are unchanged by successful and failed reads.
Replay the existing accepted-history fixtures and return the same Contract
state/votes; preserve all pre-existing protocol and consensus tests. Qualify
Windows and Linux builds and document assigned IDs/wire examples in RPC.md.

## Core integration after deployment

Add strict response codecs and bounded reads; verify echoed tip, all lengths,
address/draft hash, actor and scope against the request. Keep local imports and
caches explicitly non-authoritative. Populate participant/terms/vote detail from
the response, select reported applicable evidence, sign through identity.key,
submit using the existing transaction service, and refresh accepted state.
On STALE, METHOD, NOT_FOUND or UNAVAILABLE disable dependent actions with the
specific reason. Never turn admission or a requested transition into accepted
state. Live grant lookup does not issue a grant: root operators provision grants
through Chain's existing grant protocol outside Core.
