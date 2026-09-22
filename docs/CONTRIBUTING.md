# Contributing to STNC Core

Thank you for contributing to STNC Core.

STNC Core is the application client for the STN Chain network. Contributions must preserve the deterministic behavior, protocol boundaries, platform independence, and engineering principles established by STN Chain.

The objective is not to make STNC Core complicated.

The objective is to make it dependable.

> Small. Deterministic. Easy to use.

---

## 1. Engineering Authority

STNC Core is a consumer of STN Chain functionality.

It does not independently define STN Chain consensus, canonical protocol behavior, identity rules, mining validity, contract validity, or accepted Chain state.

Development must remain consistent with:

- the current STN Chain whitepaper;
- the current STN Chain protocol implementation;
- applicable STN Chain project policies;
- the STNC Core roadmap.

When application convenience conflicts with deterministic Chain behavior, deterministic Chain behavior takes precedence.

Do not recreate Chain consensus inside STNC Core.

---

## 2. Architecture Boundary

The fundamental relationship is:

~~~text
STNC Core
    |
    | STNC
    v
STN Chain
~~~

STNC Core may:

- connect to STN Chain;
- discover candidate peers;
- synchronize its presented view of Chain state;
- submit supported records and transactions;
- consume accepted Chain records;
- manage user identities;
- provide wallet functionality;
- manage contracts;
- coordinate user-configured mining;
- present Chain information to the user.

STNC Core must not independently decide what constitutes accepted Chain state.

Synchronization and reorganization are determined by STN Chain consensus and exposed through defined STN Chain interfaces.

---

## 3. Determinism

Deterministic behavior is mandatory wherever behavior can affect protocol interpretation or interaction with STN Chain.

Contributions must not introduce:

- ambiguous serialization;
- native structure serialization for protocol objects;
- platform-dependent protocol representation;
- locale-dependent protocol conversion;
- floating-point consensus values;
- undocumented protocol interpretation;
- probabilistic acceptance behavior;
- alternate consensus rules;
- silent correction of malformed protocol data.

Equivalent logical protocol objects must produce equivalent canonical representations regardless of platform.

Probable is not determinate.

---

## 4. Keep It Small

Prefer the smallest implementation that completely satisfies the requirement.

Avoid introducing:

- unnecessary abstraction;
- speculative frameworks;
- dependency-heavy solutions;
- oversized class or subsystem hierarchies;
- duplicate state;
- duplicate protocol implementations;
- unnecessary background services;
- functionality without a current requirement.

A contribution should be understandable without requiring a map of the entire application.

If a problem can be solved correctly with a small deterministic implementation, use the small implementation.

---

## 5. Language

STNC Core is developed in C.

Portable application code should remain compatible with the project's selected ISO C baseline.

Do not introduce another implementation language into the Core runtime without explicit project approval.

Platform APIs may be accessed through platform-specific C implementations.

---

## 6. Project Structure

The project follows this general structure:

~~~text
stnc-core/
├── includes/
├── src/
├── platforms/
│   ├── windows/
│   └── linux/
├── docs/
├── build/
└── build.cmd
~~~

### `includes/`

Public and internal interfaces shared by portable STNC Core components.

### `src/`

Platform-independent STNC Core implementation.

### `platforms/`

Operating-system-specific implementations.

Platform-specific code must remain behind defined interfaces whenever practical.

### `docs/`

Project development and architecture documentation.

### `build/`

Generated build output.

Generated build artifacts must not be committed unless specifically required by the project.

---

## 7. Platform Independence

STNC Core is platform agnostic by architecture.

Windows is the initial development target.

Linux support will use the same portable Core behavior with platform-specific services provided behind the platform abstraction.

The intended relationship is:

~~~text
                 STNC Core
                     |
                     v
              Portable Runtime
                     |
              Platform Interface
                /          \
               v            v
           Windows        Linux
~~~

Platform-specific implementations must not alter:

- STNC protocol behavior;
- canonical encoding;
- identifiers;
- Chain interpretation;
- identity representation;
- contract interpretation;
- mining validity;
- other protocol-visible behavior.

Do not place Windows-specific code directly into portable Core components when the behavior belongs behind the platform interface.

Compilation on a platform does not by itself establish platform qualification.

---

## 8. Build System

STNC Core uses a command-line build process.

The current Windows development build is driven by:

~~~text
build.cmd
~~~

Do not introduce:

- Visual Studio solution files;
- Visual Studio project files;
- CMake;
- alternate build frameworks;

unless the project explicitly adopts them.

The development repository should remain usable without requiring an IDE-specific project format.

---

## 9. Compiler Warnings

Development builds should compile with the project's configured warning level.

The current Windows build uses:

~~~text
/W4
~~~

New warnings introduced by a contribution should be treated as defects.

Do not silence a legitimate warning merely to obtain a clean build.

Fix the underlying condition.

---

## 10. STNC Protocol

STNC Core communicates with STN Chain through STNC.

Application code should use defined STNC interfaces rather than:

- reading Chain persistence directly;
- interpreting internal Chain database structures;
- recreating consensus validation;
- creating application-specific substitutes for existing STNC operations.

Protocol changes belong in the appropriate STN Chain development process.

A Core feature requiring a protocol capability that does not exist should identify the missing capability rather than silently inventing an incompatible substitute.

---

## 11. Peer Discovery

Peer discovery provides connectivity.

It does not provide consensus authority.

Candidate peers may eventually be obtained from:

- built-in bootstrap peers;
- the STN Chain website peer directory;
- previously validated cached peers;
- other approved discovery mechanisms.

The initial bootstrap endpoint is:

~~~text
--peer chain01.stn-chain.org:18473
~~~

A bootstrap peer is a location from which network participation may begin.

It is not inherently trusted.

The same rule applies to discovery information supplied by `stn-chain.org`.

~~~text
Peer != Authority
Seed != Authority
Directory != Authority
~~~

Candidate peers must be validated through the appropriate STNC behavior before Core relies on them for connectivity.

---

## 12. stn-chain.org Integration

The STN Chain website is implemented using ChAoS MVC.

STNC Core may consume approved machine-readable services published through `stn-chain.org`, including future peer-discovery functionality.

Website services must remain external application services.

Do not couple STNC Core to:

- ChAoS MVC internals;
- ChAoS MVC database structures;
- server-side PHP implementation details;
- administrative interfaces;
- private website state.

Core should consume a defined public interface.

The website may help Core discover the network.

It does not determine the Chain.

---

## 13. Identity and Wallet Development

STN Chain identity uses deterministic typed addresses in the `stn0_` namespace.

An address identifies an identity.

An address alone does not prove control.

Identity-related contributions must preserve the distinction:

~~~text
Address
    identifies

Signature
    authenticates

Authority
    permits

Consensus
    accepts
~~~

Wallet functionality must not invent economic meaning that has not been established by STN Chain Economy.

In particular, the presence of a miner identity does not by itself establish:

- balance;
- reward;
- payment destination;
- compensation entitlement;
- economic ownership;
- additional consensus authority.

---

## 14. Mining

STNC Core will support optional background mining.

Mining must remain user configurable.

Supported implementations may include:

- CPU;
- GPU;
- USB ASIC;
- dedicated mining hardware;
- other compatible hardware.

Hardware class and hashrate do not grant additional consensus authority.

Protocol-valid low-hashrate participation must not be rejected merely because the miner is slow.

Hardware-specific implementations must produce work compatible with the same STN Chain mining rules.

STNC Core mining must not create alternate validity rules.

---

## 15. Contracts

STN Chain contracts are deterministic protocol-defined state machines.

Do not introduce:

- EVM;
- general-purpose virtual machines;
- arbitrary bytecode;
- arbitrary script execution;
- Solidity;
- gas;
- Web3 execution semantics.

STNC Core provides the user interface and application functionality required to work with contracts.

Contract validity and state transitions remain governed by STN Chain.

---

## 16. Error Handling

Failures should be explicit.

Prefer:

~~~text
operation
    |
    +--> success
    |
    +--> defined failure
~~~

Avoid behavior such as:

~~~text
operation
    |
    +--> maybe worked
    |
    +--> silently corrected
    |
    +--> silently ignored
~~~

Invalid state transitions, malformed data, unavailable required services, and unsupported protocol behavior should fail predictably.

Do not hide errors that affect correctness.

---

## 17. State Ownership

Each piece of runtime state should have a clear owner.

Avoid maintaining multiple independent representations of the same authoritative information.

Where possible:

~~~text
one state
one owner
one defined interface