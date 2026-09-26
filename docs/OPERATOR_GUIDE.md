# STNC Core desktop and CLI

Run `build\stnc-core.exe` (or `stnc-core gui`) for the native Windows interface.
Run `stnc-core run` for the persistent console runtime. Existing administrative
commands remain available; `stnc-core help` prints usage. Closing the GUI requests
orderly service shutdown; it may wait for a bounded network operation to end.

## Desktop

- **Overview:** RPC connection, last peer/Chain tip comparison, accepted height,
  selected peer, wallet/balance, identity and mining status.
- **Wallet:** create when absent; inspect address, validity and accepted balance.
  Existing files are never recreated through the UI.
- **Send:** enter a canonical destination and positive whole units, review the
  confirmation, and submit through the CLI's transfer service. Results distinguish
  admission/rejection, optional transaction ID and accepted balance. A failed
  transport can leave submission outcome unknown. Core never automatically
  resubmits or locally debits a balance.
- **Contracts:** search by address for accepted summary state. Draft any of six
  types, enter participant actor public keys and roles, edit terms, and save exact
  canonical STCT bytes under a new filename. The returned address identifies a
  local draft, not acceptance. Lifecycle actions and full accepted details are
  unavailable pending the Chain read APIs.
- **Mining:** ON/OFF, existing 1% or 2% CPU limit, configured/active backend,
  Stratum connection, hashrate and work counts. Enabling requires a valid stored
  wallet. The default is `stratum.stn-chain.org:18475`. No hashrate threshold is
  imposed. Starting mining leaves Core running. Backend configuration remains
  available through `mining backend`; CPU and automatic are usable, GPU/USB are
  reserved. Accepted participation totals are unavailable.
- **Network:** RPC endpoint, P2P, selected peer, candidate/qualified counts,
  latency, capabilities and accepted height.
- **Activity:** only the latest five entries. Full history stays in the log file,
  which another process can read while Core runs.
- **Identity:** create and inspect a separate STN actor identity. Participants
  enter its public key in drafts. Its private key is never shown.

Core operations run on one worker thread. The window remains responsive during
network calls. Copied public snapshots cross the frontend boundary; Core/network,
configuration, logging and cryptographic operations are serialized on that owner
thread. Local wallet, identity and drafting remain usable when Chain is offline.
Unavailable accepted state is displayed as unavailable, not zero.

## CLI additions

```text
stnc-core gui
stnc-core run
stnc-core identity status
stnc-core identity create
stnc-core identity show
stnc-core contract state <stnc0_address>
stnc-core contract draft <type 1-6> <created> <terms-file> <new-output.stct> [<actor-public-key>:<role 1-5> ...]
```

Creation values are explicit unsigned 64-bit integers (the GUI initially offers
Unix seconds). Terms files are exact bytes; GUI terms use UTF-8 and preserve
entered line breaks. Bounds: 32 participants, 65,536 terms bytes. Order is
preserved. Roles: 1 Participant, 2 Issuer, 3 Recipient, 4 Approver, 5 Attestor.
Types: 1 Generic, 2 Work Offer, 3 Contributor Agreement, 4 Policy,
5 Organizational Decision, 6 Service Agreement.

## Local custody and status limits

`wallet.key`, `identity.key`, `config.json` and `stnc-core.log` live beside the
executable. The wallet format is unchanged. Identity uses an independent random
Ed25519 seed/public pair in a versioned STNI file. Private files use atomic
CREATE_NEW and a protected current-user-only Windows DACL. This is OS access
control, not passphrase encryption. Corrupt stores are reported invalid and never
silently replaced. No normal service status, CLI or GUI exposes private keys.

Creating an identity, holding a wallet, mining, or entering a role does not issue
a grant. See [Contract integration](CONTRACT_INTEGRATION.md) and the exact
[Chain implementation order](CHAIN_INTERFACE_ORDER.md) for required read APIs.

## Qualification

`build.cmd` runs all existing suites plus identity custody/signing/DACL, draft
codec/export, action construction, client boundary and native GUI smoke tests.
GUI smoke uses a temporary isolated directory and loopback endpoints, creates
only disposable keys, exercises every page, checks wallet gating and independent
identity creation, toggles mining without process exit, reads the live log and
closes cleanly. It writes overview/Contract BMP captures to build for visual
review. Tests do not submit money or Contract actions to a live Chain.
