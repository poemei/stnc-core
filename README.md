# STNC Core

STNC Core is the desktop client for the STN Chain network.

It is being developed as a small, deterministic, platform-agnostic application providing users with access to STN Chain functionality without duplicating Chain consensus behavior inside the client.

STNC Core will provide a unified interface for:

- Chain synchronization and status
- STN Chain identity
- Wallet functionality
- Contract management
- Background mining
- Mining configuration
- Peer discovery and connectivity
- User configuration

STNC Core communicates with STN Chain through the native STNC protocol.

---

## Development Status

**Current Version:** `0.1.0-dev`

**Current Development Phase:** Phase 1 — Core Runtime Foundation

STNC Core is under active development and is not currently a production-qualified release.

### Implemented

- Core runtime lifecycle
- Explicit runtime state management
- Controlled initialization
- Controlled execution
- Controlled shutdown
- Platform abstraction
- Windows platform backend
- Windows command-line build
- `/W4` development compilation

Current runtime lifecycle:

~~~text
UNINITIALIZED
      |
      | initialize
      v
INITIALIZED
      |
      | run
      v
RUNNING
      |
      | stop
      v
STOPPING
      |
      | shutdown
      v
STOPPED
~~~

### In Development

Phase 1 will establish the remaining application foundation, including:

- Logging
- Configuration
- Windows application hosting
- Runtime error handling
- Controlled startup and shutdown behavior

### Planned

Later development phases will introduce:

- STNC v2 connectivity
- Peer bootstrap and discovery
- Automatic peer selection
- Chain synchronization
- Windows desktop interface
- Wallet and identity management
- Background mining
- CPU, GPU, and USB ASIC mining selection
- STN-Stratum integration
- Contract management
- Recovery and resilience
- Linux platform support
- Production qualification

---

## Architecture

STNC Core is an application client.

It does not independently define or replace STN Chain consensus.

~~~text
STNC Core
    |
    | STNC
    v
STN Chain
~~~

Chain state presented by STNC Core is obtained through defined STN Chain interfaces. Validation and acceptance of Chain state remain the responsibility of STN Chain consensus.

Platform-specific behavior is isolated behind platform abstractions.

~~~text
STNC Core Runtime
        |
        v
Platform Interface
        |
        +----------------+
        |                |
        v                v
     Windows           Linux