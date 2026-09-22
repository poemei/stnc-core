# STNC Core Development Roadmap

## 1. Purpose

STNC Core is the primary desktop client for interacting with STN Chain.

The intended user experience draws from the operational simplicity of the Bitcoin Core desktop client and the wallet/contract-management concepts of the original Ethereum Wallet, while remaining entirely native to STN Chain.

STNC Core is not:

- a Chain consensus node;
- a replacement for STN Chain;
- a replacement for STN-Stratum;
- an EVM client;
- an Ethereum implementation;
- a Bitcoin implementation.

STNC Core is an application-facing participant that communicates with STN Chain through the STNC binary protocol.

The STN Chain whitepaper establishes:

```text
STN Core != Authority
```

STNC Core presents and interacts with accepted Chain state.

STN Chain remains responsible for deterministic validation, consensus, reorganization, and accepted state.

---

# 2. Engineering Requirements

STNC Core development follows the STN Chain engineering doctrine:

```text
SMALL
DETERMINISTIC
EASY TO USE
```

Development will proceed through bounded increments.

Platform-specific functionality must remain behind defined interfaces wherever practical.

The initial qualified desktop target is Windows x64.

The architecture must not unnecessarily prevent future implementation on additional supported platforms.

Consensus-visible behavior belongs to STN Chain and must not be independently recreated or modified by STNC Core.

---

# 3. Target User Experience

STNC Core should operate as a persistent desktop application.

A normal user should be able to:

1. Install STNC Core.
2. Start the application.
3. Automatically locate an available STN Chain peer.
4. Synchronize with Chain.
5. See current synchronization and connection status.
6. Create and manage an STN wallet.
7. Send supported Chain transactions.
8. Enable or disable background mining.
9. Select automatic, CPU, GPU, or USB ASIC mining where supported.
10. Create and manage STN contracts.
11. Configure network, mining, wallet, and application behavior.
12. Leave STNC Core running without continual administration.

The application should provide a desktop experience similar in operational character to Bitcoin Core:

```text
Start
  |
  v
Discover
  |
  v
Connect
  |
  v
Synchronize
  |
  v
Operate
  |
  +--> Wallet
  |
  +--> Mining
  |
  +--> Contracts
  |
  +--> Chain Status
```

---

# 4. Phase 1 — Core Runtime Foundation

**Status: COMPLETE**

The first development phase establishes the actual STNC Core application architecture.

This replaces the previous CLI prototype as the architectural starting point.

The runtime establishes bounded services for:

```text
STNC Core
|
+-- Configuration
+-- Chain Connection
+-- Synchronization
+-- Wallet
+-- Mining
+-- Contracts
+-- Platform
```

Initial implementation should include:

- application initialization;
- application state;
- configuration loading;
- configuration persistence;
- logging;
- platform abstraction;
- controlled startup;
- controlled shutdown;
- Windows application host.

The graphical interface should remain separate from the underlying runtime.

### Phase 1 Exit Requirement

STNC Core launches as a Windows application, initializes its runtime, loads configuration, maintains application state, and shuts down cleanly.

---

# 5. Phase 2 — Current STNC Connectivity

Implement the STNC client against the current STN Chain implementation.

The previous Core CLI must not define the protocol implementation.

Current STN Chain behavior and the STN Chain whitepaper are authoritative.

STNC Core should support:

- STNC connection establishment;
- STNC RPC v2;
- long-lived sessions;
- deterministic request construction;
- deterministic response handling;
- connection-state reporting;
- disconnect detection;
- reconnect;
- timeout handling;
- incompatible-peer handling;
- clean connection shutdown.

The default STN Chain service port is:

```text
18473
```

### Phase 2 Exit Requirement

STNC Core can connect to a current STN Chain server and continuously obtain and display actual Chain status.

---

# 6. Phase 3 — Peer Bootstrap and Discovery

STNC Core will ship with a built-in bootstrap peer:

```text
chain01.stn-chain.org:18473
```

This is a seed.

It is not an authority.

The STN Chain whitepaper explicitly establishes:

```text
Peer != Authority
Seed != Authority
```

STNC Core will additionally use a machine-readable peer directory provided by the Chaos MVC implementation at stn-chain.org.

Conceptually:

```text
Built-in Seed
      |
      +------------------+
                         |
stn-chain.org Peer API --+
                         |
                         v
                  Candidate Peers
                         |
                         v
                     Resolve
                         |
                         v
                  STNC Validation
                         |
                         v
                 Reachability Test
                         |
                         v
                  Latency Measure
                         |
                         v
                  Peer Selection
```

The stn-chain.org API is a discovery mechanism only.

It does not provide consensus authority.

The Chaos MVC peer directory should expose the same peer ecosystem represented through the public `/peers` implementation without requiring STNC Core to understand Chaos MVC internals.

STNC Core should maintain a small local cache of previously validated peers.

This permits operation when the public peer-directory service is temporarily unavailable.

### Phase 3 Exit Requirement

A fresh STNC Core installation with no manually configured Chain server can automatically locate and connect to an operational STN Chain peer.

---

# 7. Phase 4 — Automatic Peer Selection

STNC Core should automatically choose an appropriate synchronization peer.

"Nearest" should represent network proximity and practical availability rather than purely geographic distance.

Candidate evaluation may include:

- DNS resolution;
- IP resolution;
- STNC reachability;
- protocol compatibility;
- response time;
- connection latency;
- peer health.

Only successfully validated STN Chain endpoints should remain candidates.

A basic selection sequence is:

```text
Resolve Candidate
       |
       v
Connect
       |
       v
Validate STNC
       |
       v
Measure
       |
       v
Candidate Accepted
```

The best currently available candidate becomes the active synchronization source.

Other valid candidates remain available for failover.

Peer-selection behavior is local operating policy.

It must never become consensus behavior.

### Phase 4 Exit Requirement

STNC Core automatically chooses a healthy STN Chain peer without requiring normal users to understand or configure Chain networking.

---

# 8. Phase 5 — Chain Synchronization

STNC Core must synchronize its presented Chain state through defined STN Chain interfaces.

STNC Core does not independently determine accepted Chain state.

The relationship remains:

```text
Peer
 |
 v
Chain Evidence / State Interface
 |
 v
STN Chain Consensus
 |
 v
Accepted State
 |
 v
STNC
 |
 v
STNC Core Presentation
```

The desktop application should continuously expose:

- connected peer;
- connection status;
- Chain height;
- synchronization status;
- synchronization progress;
- latest Chain update;
- peer latency;
- reconnect state.

If the active peer becomes unavailable:

```text
Active Peer Lost
       |
       v
Known Candidate Set
       |
       v
Select Healthy Peer
       |
       v
Reconnect
       |
       v
Continue Synchronization
```

### Phase 5 Exit Requirement

STNC Core can begin from an incomplete or stale state, reach the current accepted state reported through STN Chain interfaces, survive peer loss, select another peer, and continue operation.

---

# 9. Phase 6 — Windows Desktop Interface

Once networking and synchronization are stable, development moves to the full Windows desktop interface.

Primary application areas should be:

```text
Overview
Wallet
Contracts
Mining
Chain
Settings
```

## Overview

The Overview should provide immediate operational awareness.

Example:

```text
STNC Core

CHAIN

Status:        Connected
Height:        18,421
Synchronization: 100%

NETWORK

Peer:          chain01.stn-chain.org:18473
Latency:       31 ms

MINING

Status:        Running
Backend:       CPU

WALLET

Address:
stn0_...

RECENT ACTIVITY

...
```

The exact interface can evolve, but STNC Core should provide useful running status without requiring a console.

### Phase 6 Exit Requirement

Normal STNC Core operation can be performed entirely through the Windows desktop application.

---

# 10. Phase 7 — Wallet and Identity

STNC Core will provide native STN wallet functionality.

Wallet implementation must follow the identity, address, signature, transaction, and economic mechanisms actually implemented by STN Chain.

It must not inherit Bitcoin or Ethereum assumptions.

Wallet responsibilities will eventually include:

- key generation;
- secure key storage;
- STN address generation;
- address management;
- transaction construction;
- transaction signing;
- transaction submission;
- transaction history;
- transaction status;
- supported balance reporting;
- identity management;
- future key rotation;
- future revocation mechanisms.

Functionality that depends upon unfinished Chain economics must remain deferred until the corresponding deterministic Chain rules exist.

STNC Core must not invent economic behavior.

### Phase 7 Exit Requirement

STNC Core can securely maintain an STN identity and perform the wallet operations supported by the qualified STN Chain implementation.

---

# 11. Phase 8 — Background Mining

Mining should operate as a background STNC Core service.

The user must be able to completely disable mining.

Example configuration:

```text
Mining

[ ] Disabled
[x] Enabled

Backend:

(*) Automatic
( ) CPU
( ) GPU
( ) USB ASIC

CPU Limit:

<2%

Stratum:

stratum.stn-chain.org:18475
```

## Automatic Backend Selection

Automatic mode should select from qualified available hardware.

Conceptually:

```text
USB ASIC available?
        |
       YES ------> USB ASIC
        |
       NO
        v
GPU available?
        |
       YES ------> GPU
        |
       NO
        v
       CPU
```

CPU is the fallback backend.

The `<2%` Core mining rule applies to CPU resource usage.

Explicit user configuration may override automatic backend selection where the selected backend is supported and available.

The STN Chain whitepaper establishes that protocol-valid mining participation is independent of hardware class.

Potential participants include:

- CPU;
- GPU;
- USB ASIC;
- dedicated ASIC;
- ARM;
- low-hashrate miners;
- STN-Stratum miners.

Hashrate does not grant protocol authority.

STNC Core mining should use the qualified STN-Stratum interface rather than implementing an alternative consensus mechanism.

### Phase 8 Exit Requirement

STNC Core can continuously perform supported background mining while remaining responsive as a wallet, synchronization client, and desktop application.

Mining can be enabled, disabled, and configured without restarting the application where practical.

---

# 12. Phase 9 — Contract Management

STNC Core will become the primary desktop interface for STN Chain contracts.

The user experience may draw inspiration from the original Ethereum Wallet's concept of integrated contract management.

The implementation remains entirely STN-native.

STN contracts are deterministic addressed agreements.

They are not arbitrary executable programs.

The STN Chain whitepaper explicitly rejects:

```text
NO EVM
NO GENERAL-PURPOSE VM
NO ARBITRARY BYTECODE
NO ARBITRARY SCRIPT EXECUTION
NO GAS
```

The Contract interface should eventually provide views such as:

```text
Contracts

Draft
Issued
Awaiting My Signature
Awaiting Others
Approved
Executed
Closed
Revoked
```

Supported operations may include:

- create;
- inspect;
- issue;
- sign;
- approve;
- reject;
- amend;
- execute;
- revoke;
- close;
- monitor contract state.

The interface must follow the contract states and transitions implemented by STN Chain.

STNC Core must not independently define contract validity.

### Phase 9 Exit Requirement

A supported STN contract can complete its normal lifecycle through STNC Core while all validation and state-transition authority remains with STN Chain.

---

# 13. Phase 10 — User Configuration

STNC Core should provide end-user control without requiring unnecessary technical knowledge.

Configuration areas should include:

## Application

- launch behavior;
- background operation;
- minimize behavior;
- startup behavior;
- notifications;
- logging.

## Chain

- automatic peer selection;
- preferred peer;
- known peers;
- automatic failover;
- bootstrap behavior;
- synchronization behavior.

## Mining

- mining enabled/disabled;
- automatic backend;
- CPU;
- GPU;
- USB ASIC;
- CPU resource limit;
- Stratum endpoint.

## Wallet

- wallet location;
- identity selection;
- backup operations;
- security options.

## Contracts

- contract notifications;
- signature notifications;
- pending-action notifications.

Two basic Chain connection modes should be available:

```text
AUTOMATIC

Seed
  +
Peer Directory
  +
Known Peers
  |
  v
Choose Healthy Peer
  |
  v
Automatic Failover
```

and:

```text
PREFERRED PEER

User-selected Peer

[x] Permit Automatic Failover
```

### Phase 10 Exit Requirement

An ordinary user can configure STNC Core through the desktop interface without manually editing configuration files.

---

# 14. Phase 11 — Recovery and Resilience

STNC Core must fail predictably and recover cleanly.

Required scenarios include:

- unavailable bootstrap seed;
- unavailable stn-chain.org peer API;
- DNS failure;
- unreachable peer;
- incompatible STNC peer;
- malformed peer response;
- peer loss during synchronization;
- Chain reorganization;
- stale local state;
- Stratum disconnect;
- stale mining work;
- GPU disappearance;
- USB ASIC removal;
- wallet failure;
- invalid configuration;
- corrupted configuration;
- application shutdown during synchronization;
- application shutdown during mining;
- restart after interrupted operation.

Loss of one discovery source must not unnecessarily disable an established STNC Core installation.

### Phase 11 Exit Requirement

STNC Core survives expected network, service, hardware, and application interruptions without silently corrupting state or assuming authority it does not possess.

---

# 15. Phase 12 — Qualification

Qualification begins with Windows x64.

Compilation alone does not establish platform qualification.

Testing must demonstrate predictable behavior.

Qualification should cover:

```text
Startup
Configuration
Discovery
STNC Connection
Synchronization
Failover
Wallet
Mining
Contracts
Shutdown
Recovery
```

Negative testing is required.

Once Windows behavior is qualified, additional platform work can proceed behind the established platform abstractions.

Consensus-visible behavior remains entirely governed by STN Chain.

---

# 16. Release Milestones

## STNC Core v0.1 — Connected Core

Target:

- real Windows desktop application;
- Core runtime;
- configuration;
- STNC RPC v2 connectivity;
- built-in `chain01.stn-chain.org:18473` seed;
- stn-chain.org peer-directory integration;
- automatic peer selection;
- synchronization;
- running Chain status;
- reconnect;
- peer failover.

This is the first point at which the application should properly be considered STNC Core rather than a protocol-development utility.

---

## STNC Core v0.2 — Wallet Core

Target:

- identity;
- key management;
- STN addresses;
- supported transaction creation;
- signing;
- submission;
- transaction status;
- wallet interface.

Only functionality supported by the qualified Chain implementation should be exposed.

---

## STNC Core v0.3 — Mining Core

Target:

- background mining;
- enable/disable mining;
- STN-Stratum integration;
- automatic backend selection;
- CPU backend;
- qualified GPU backend;
- qualified USB ASIC backend;
- configurable CPU resource limit;
- mining status.

---

## STNC Core v0.4 — Contract Core

Target:

- contract dashboard;
- contract creation;
- contract inspection;
- signatures;
- approvals;
- rejection;
- supported deterministic state transitions;
- contract history;
- contract notifications.

Implementation depends upon the qualified STN Chain contract engine.

---

## STNC Core v0.5 — Integrated Desktop Core

Target:

```text
Wallet
+
Contracts
+
Mining
+
Synchronization
+
Peer Management
+
Configuration
+
Recovery
```

The application should now provide the intended persistent desktop STN Chain experience.

---

## STNC Core v1.0 — Production Qualification

Target:

- qualified Windows x64 release;
- qualified STNC operation;
- qualified synchronization;
- qualified peer discovery and failover;
- qualified wallet functionality;
- qualified supported mining backends;
- qualified contract management;
- recovery testing;
- negative testing;
- security review;
- complete user documentation;
- complete developer documentation.

---

# 17. Development Order

The implementation sequence is:

```text
Runtime Foundation
        |
        v
Current STNC Client
        |
        v
Peer Discovery
        |
        v
Automatic Peer Selection
        |
        v
Chain Synchronization
        |
        v
Windows Desktop Interface
        |
        v
Wallet / Identity
        |
        v
Background Mining
        |
        v
Contract Management
        |
        v
Full Configuration
        |
        v
Recovery / Resilience
        |
        v
Qualification
```

Each phase should establish a working bounded capability before development advances to the next major subsystem.

---

# 18. Architectural Boundary

STNC Core must preserve the following relationship:

```text
                       STN CHAIN
                 Consensus Authority
                       :18473
                          |
                         STNC
                          |
                          v
                    STNC CORE
               Application Platform
                          |
          +---------------+---------------+
          |               |               |
          v               v               v
       Wallet         Contracts        Chain UI
                                          |
                                          |
                                     Mining Control
                                          |
                                          v
                                     STN-Stratum
                                        :18475
                                          |
                     +--------------------+--------------------+
                     |                    |                    |
                     v                    v                    v
                    CPU                  GPU                USB ASIC
```

STNC Core provides usability.

STN-Stratum provides mining coordination.

STN Chain provides validation and consensus.

The stn-chain.org Chaos MVC peer service provides discovery.

None of those application or discovery services replace Chain consensus.

---

# 19. Governing Development Principle

STNC Core should eventually feel like something the user simply installs and runs.

The user should not need to understand peer topology, STNC framing, Chain synchronization internals, Stratum job handling, or hardware discovery merely to participate.

The normal experience should be:

```text
Install
   |
   v
Start
   |
   v
Connect
   |
   v
Synchronize
   |
   v
Ready
```

From there:

```text
Wallet
Mining
Contracts
Chain Status
```

are ordinary parts of one application.

The implementation remains governed by the STN Chain engineering doctrine:

**Small. Deterministic. Easy to use.**