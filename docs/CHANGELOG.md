### CHANGELOG.md

markdown
# STNC Core Changelog

All notable development changes to STNC Core are documented here.

STNC Core is currently under active development. Development versions do not represent a production-qualified release.

---

## [0.1.0-dev] - 2026-09-22

### Added

- Initial STNC Core runtime.
- Explicit runtime states:
  - `UNINITIALIZED`
  - `INITIALIZED`
  - `RUNNING`
  - `STOPPING`
  - `STOPPED`
- Controlled runtime initialization.
- Persistent runtime execution.
- Controlled runtime shutdown.
- Runtime stop-request interface.
- Runtime state query interface.
- Platform abstraction interface.
- Initial Windows platform backend.
- Windows platform initialization and shutdown lifecycle.
- Windows console stop handling.
- Operator-requested shutdown using `Ctrl+C`.
- Portable runtime logging.
- Information, warning, and error logging interfaces.
- Persistent local configuration subsystem.
- Executable-relative `config.json`.
- Automatic default configuration creation when `config.json` is absent.
- Existing configuration loading and validation.
- Configurable `peer`.
- Configurable `port`.
- Command-line Windows build using `build.cmd`.
- Build output to `build\stnc-core.exe`.
- `/W4` compiler warning level for development builds.

### Configuration

The initial configuration schema is:

```json
{
    "peer": "chain01.stn-chain.org",
    "port": 18473
}
```

### Architecture

- Established separation between the portable STNC Core runtime and platform-specific implementations.
- Platform-specific behavior is isolated behind the STNC platform interface.
- STNC Core does not implement STN Chain consensus behavior.
- Chain-facing functionality will be introduced through defined STNC interfaces in later development phases.
- No dependency on Bitcoin JSON-RPC, EVM, Web3, or other external blockchain application architectures has been introduced.

### Qualified Development Checks

The current Windows development build has been verified to:

- compile successfully;
- initialize the STNC Core runtime;
- initialize the Windows platform backend;
- enter the Core runtime;
- perform controlled shutdown;
- release the platform lifecycle;
- terminate normally.

Observed development execution:

~~~text
STNC Core
STNC Core stopped.
~~~

### Current Scope

Implemented:

- Runtime lifecycle
- Runtime state management
- Platform abstraction
- Windows platform backend
- Windows command-line build

Not yet implemented:

- Logging
- Configuration
- STNC v2 connectivity
- Peer discovery
- stn-chain.org peer-directory integration
- Chain synchronization
- Wallet and identity management
- Mining
- STN-Stratum integration
- Contract management
- Desktop graphical interface
- Linux platform backend
- Production qualification