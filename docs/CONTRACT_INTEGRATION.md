# Contract integration boundary audit

Inspected 2026-09-25 against the local STNC Core, STN Chain and stn-chain.org sources.

## Confirmed interfaces

- Chain `docs/CONTRACTS.md` defines STCT drafts, seven STNT type-5 actions,
  stable draft-derived addresses, scoped authority and accepted lifecycle state.
- Chain `includes/stn_rpc.h:36` and `src/stn_node_service.c:91` expose
  CONTRACT_STATE as 26 bytes: state, type, sequence, creation value,
  participant count and terms length. It does not return participant entries,
  terms bytes, canonical draft bytes, votes or authority evidence.
- Chain block retrieval exists. Recovering immutable draft evidence from
  accepted blocks is a possible client indexing approach; it must handle
  reorganizations and must not independently replay Contract consensus to
  declare accepted state. The absence of a full Contract query is not the
  absence of the Contract Engine.
- The inspected ChAoS MVC explorer client exposes info and block retrieval;
  it supplies no additional Contract or authority API.
- Core stores a wallet key and uses its public key for mining identity
  derivation. It currently has no Contract identity selection or authority
  evidence provisioning service.

## Security decision requiring operator direction

Chain `src/stn_chain.c:485` requires both a valid action signature and matching
scoped authority evidence backed by an accepted, unrevoked root-issued grant.
A participant role, wallet key or mining identity does not supply that grant.

Before implementing Contract signing, specify whether the operator is to use:

1. The existing Core wallet key as the Contract actor, with corresponding
   externally issued scoped grants; or
2. A separately provisioned organizational identity and its scoped grants.

Also identify how operators receive/import that authority evidence. Core can
construct the defined evidence fields, but cannot grant itself authority.
These choices affect key custody and organizational identity, and are not
selected by the current client source or the job order. Execution pauses under
job-order section 16's security-sensitive decision exception.

No Chain protocol or consensus changes were made. Contract construction,
actions, the GUI and remaining job-order workflows are not claimed complete.
The bounded transfer CLI refactor is independent of this decision.
