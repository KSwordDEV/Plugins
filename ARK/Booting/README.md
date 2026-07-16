# KSword Booting Tab Plugin

Independent, process-isolated KSword Tab plugin for configuring the Windows
UEFI boot logo through the official HackBGRT release. This project and its
HackBGRT payload are deliberately kept outside the KSword main repository and
outside KSword release packages.

## Read this before use

HackBGRT changes the UEFI boot chain. A failed or incorrect installation can
make Windows unbootable. Before installing or updating:

1. Create bootable Windows recovery media.
2. Verify that the firmware boot menu can still start **Windows Boot Manager**.
3. Back up the BitLocker recovery key and suspend BitLocker protection.
4. Expect TPM-dependent features such as Windows PIN and some anti-cheat tools
   to stop working until the boot state is repaired or re-enrolled.
5. If Secure Boot is enabled, read `HackBGRT/shim.md` first. The MOK/shim trust
   steps after reboot are intentionally manual and cannot be automated.
6. Use UEFI firmware and, as upstream recommends, keep only one bootable drive
   connected during automatic setup.

The firmware/vendor logo may appear briefly before HackBGRT runs. That is an
upstream limitation, not an installation failure. This software is provided
without warranty and is used at your own risk.

## Safety model

- KSword loads no plugin DLL. It launches `BootingPlugin.exe` in a separate
  process and embeds only the process-owned `WS_CHILD` window.
- The plugin validates the parent HWND and KSword host PID before creating UI.
- KSword validates the returned HWND, owning PID, direct parent, and
  `WS_CHILD` style before embedding it.
- The red danger banner is permanently outside the scrollable content region.
- Install/update requires UEFI + administrator + exact official payload hash,
  three acknowledgements, the phrase `HACKBGRT`, and a final confirmation.
- Uninstall requires the phrase `UNINSTALL` and a final confirmation.
- The plugin works in an isolated copy under
  `%LOCALAPPDATA%\KSword\Plugins\booting\HackBGRT-2.6.0`.
- It invokes only documented `setup.exe batch` commands. Install/update uses
  `disable install enable-bcdedit`; it never offers `enable-overwrite`, the
  legacy overwrite path, or a direct firmware/EFI writer.
- `Dry-run` generates the same configuration but prepends HackBGRT's official
  `dry-run` command so EFI and NVRAM are not modified.

## Build

Requirements: Visual Studio 2022 Build Tools with the Desktop C++ workload and
a Windows 10/11 SDK.

```powershell
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
& $msbuild .\KSwordBootingPlugin.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1
```

The build target copies `plugin.json`, notices, and the unmodified `HackBGRT\`
payload beside the executable. The loadable plugin directory is
`x64\Release\`.

## Install as a KSword plugin

Copy the complete Release output under a plugin root using `booting` as the
directory name:

```text
plugin\
└── booting\
    ├── plugin.json
    ├── BootingPlugin.exe
    ├── LICENSE.txt
    ├── NOTICE
    └── HackBGRT\
        ├── setup.exe
        ├── LICENSE
        ├── shim.md
        └── ...
```

Point a development build at a separate plugin root with
`KSWORD_PLUGIN_ROOT`; a packaged KSword installation may use its normal
`plugin\booting` directory. Open the top-level **Plugins → Booting** tab to start the plugin. Merely
discovering the manifest does not execute it.

## Protocol

KSword launches:

```text
BootingPlugin.exe --ksword-plugin tab -- --parent-hwnd <decimal HWND> --host-pid <PID>
```

After validating the host and creating its direct child window, the plugin
writes one JSON Lines event to stdout:

```json
{"protocol":"ksword-plugin/1","plugin_id":"booting","event":"tab_ready","hwnd":"123456"}
```

Run `BootingPlugin.exe` without arguments only for standalone UI inspection.

## Host style injection

When launched by KSword, the native Win32 UI consumes the host style contract from
`KSWORD_PLUGIN_THEME` plus `KSWORD_PLUGIN_COLOR_WINDOW`, `SURFACE`, `SURFACE_ALT`,
`TEXT_PRIMARY`, `TEXT_SECONDARY`, `BORDER`, `ACCENT`, and `ON_ACCENT` (all color
variables use `#RRGGBB`). Missing values fall back to a complete dark or light palette,
so an independently launched plugin remains usable.
Standalone mode does not install anything by itself; the same safety gates
remain in force.

## Upstream and licenses

- HackBGRT upstream: <https://github.com/Metabolix/HackBGRT>
- Bundled upstream version: 2.6.0
- Official archive SHA-256:
  `6204911D777AC03E514B90126568EF6FAA1D477779B31C536B142DBA92F4A2C0`
- Bundled `setup.exe` SHA-256:
  `EC429816372B68C890371CB22F4A453F79D165701C98579A9528155C2B422A76`

This plugin is MIT-licensed in `LICENSE.txt`. HackBGRT's original MIT license
is retained as `HackBGRT/LICENSE`; shim/MokManager notices are retained as
`HackBGRT/shim-signed/COPYRIGHT`. See `NOTICE` for redistribution details.
