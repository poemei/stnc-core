# STNC Core

STNC Core is the native C desktop client for STN Chain. One set of portable
services supports the Windows GUI and administrative CLI. Chain determines
accepted balances, transactions and Contract state.

**Version:** 0.1.0-dev. This is a development build, not a production release.

Build and run from an MSVC developer command prompt:

```text
build
build\stnc-core.exe
```

The default launch opens the GUI. `stnc-core gui` also opens it;
`stnc-core run` starts the persistent console runtime. Existing CLI commands
remain available through `stnc-core help`.

The GUI provides Overview, Wallet, Send, Contracts, Mining, Network, Activity and
Identity. Contract drafting/export and accepted summary lookup are implemented.
Full accepted Contract details and lifecycle submission await the documented
Chain authority/detail read interfaces; unavailable operations are labeled.
Identity private custody is separate from the economic wallet.

No browser runtime, web server, C++, C#, .NET or third-party GUI framework is
required. Win32 presentation is isolated under platforms/windows. Local creation
and drafting work while the Chain connection is unavailable; accepted state is
never invented or optimistically updated.

- [Operator guide](docs/OPERATOR_GUIDE.md)
- [Contract integration and limitations](docs/CONTRACT_INTEGRATION.md)
- [Required Chain interface implementation order](docs/CHAIN_INTERFACE_ORDER.md)
- [Changelog](docs/CHANGELOG.md)
- [Roadmap](docs/ROADMAP.md)
- [Contributing](docs/CONTRIBUTING.md)

**Small. Deterministic. Easy to use.** Probable != Determinate.
