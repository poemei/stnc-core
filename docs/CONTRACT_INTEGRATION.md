# Contract integration status

The section-16 identity choice is resolved: Contracts use a separate Core STN
identity and identity.key. Wallet private keys never sign Contract actions.
The existing wallet-derived public mining identity remains unchanged.

Implemented and deterministically tested:

- Identity status, creation, public address and actor public key, no-replacement
  private persistence, and service-owned signing.
- STCT Draft/0 construction for all six types, five participant roles, ordered
  participants, exact terms, explicit creation value, and local address derivation.
- Native drafting and export, CLI drafting from terms files, and accepted
  summary lookup through existing method 11.
- STNT type-5 action construction for all seven actions using the identity
  signing service, exact Chain domain/field ordering, next sequence, and supplied
  evidence matching actor/action/origin context. This builder does not submit,
  establish authority or infer an accepted transition.

## Participant identity representation

The executable Chain authority/approval implementation uses canonical Ed25519
public keys as actors and compares participant bytes directly with those keys
(stn_identity_derive, stn_contract_vote_eligible). Generic typed-address derivation
produces stn0_ text from SHA256(source); a public key cannot be recovered from
that hash. Drafting accepts the actor public key shown on Core's Identity page,
while Identity also shows its derived stn0_ address. It does not strip an address
prefix and mistake a digest for the signing actor.

The broad Contract documentation's reference to identity-address identifiers
needs clarification against these implemented approval semantics; no consensus
change is made here. Drafting from an address alone would additionally need an
accepted public-key resolution interface or a supplied public key.

## Deliberately unavailable surfaces

The current protocol reports Contract summary state, type, sequence, creation
value, participant count and terms byte count. It does not return participant
or terms bytes, or accepted scoped grant evidence. The inspected ChAoS MVC
explorer exposes block/info access, not these additional reads.

The GUI labels lifecycle signing/submission unavailable. Create, Amend, Approve,
Reject, Execute, Revoke and Close are not exposed as working network operations.
Full accepted participant/terms/vote presentation, grant lookup and action
submission/accepted-state refresh depend on the two APIs specified in
[CHAIN_INTERFACE_ORDER.md](CHAIN_INTERFACE_ORDER.md). No nonexistent method is
called and no cached or imported grant is called accepted.

The proposed read interfaces are Chain repository work, not consensus redesign.
The implementation order specifies requests, responses, owners, bounds, failure
behavior and tests. GUI, wallet, transfers, identity, mining, network, activity,
drafting and existing Contract lookup proceed independently. Full end-to-end
Contract lifecycle qualification is not claimed.
