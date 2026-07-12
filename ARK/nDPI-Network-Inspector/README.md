# nDPI Network Inspector

This KSword plugin runs nDPI 5.0 in a separate x64 process. It captures live
IPv4 TCP/UDP traffic with the Windows `SIO_RCVALL` raw-socket API, tracks
bidirectional flows, and writes UTF-8 JSON Lines events to stdout. KSword never
loads nDPI or the plugin into its own process.

Administrator privileges are required by Windows for raw packet capture. The
default KSword invocation captures the first active non-loopback IPv4 interface
for 60 seconds. The process can be cancelled from the plugin result window.

## Build

Requirements:

- Visual Studio 2022 Build Tools with the MSVC v143 toolset
- Windows 10/11 SDK

Run from this directory:

```powershell
.\build.ps1 -Configuration Release
```

The script builds the pinned nDPI source project and copies
`bin\x64\Release\ndpi-network-inspector.exe` to the plugin root. To create the
marketplace archive:

```powershell
.\package.ps1
```

## Protocol smoke checks

```powershell
.\ndpi-network-inspector.exe --ksword-plugin info --
.\ndpi-network-inspector.exe --ksword-plugin selftest --
.\ndpi-network-inspector.exe --ksword-plugin capture -- --target-kind network --duration 10
```

The second command requires an elevated shell. Optional `--interface IPV4`
selects a specific local IPv4 address.

## Source layout

- `src/`: KSword protocol, capture loop, and stateful flow classifier
- `third_party/nDPI/`: pinned nDPI 5.0 source, bridge, license, and notices
- `plugin.json`: KSword plugin manifest and generic scan-table visualization

See `NOTICE` and `LICENSE.txt` for third-party attribution and licensing.
