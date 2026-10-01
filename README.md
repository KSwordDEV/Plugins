# KSword Plugins

The KSword plugin marketplace catalog. KSword reads [catalog.json](catalog.json)
to list available plugins, then requires users to review and accept each
plugin's license before downloading its archive.

Each plugin is independently packaged under `ARK/`. A published entry must
provide an HTTPS archive URL, SHA-256, license URL, and install directory.
KSword validates the archive and installs it on demand. Command plugins use
the generic `visualization` contract, while process-isolated Tab plugins expose
only a validated native child window to the host.

Published plugins currently include the PYAS PE scanner and the isolated nDPI
Network Inspector for live application-protocol classification, plus the
guarded Booting Tab plugin for official HackBGRT-based UEFI logo configuration.

The debugger plugins provide a standalone 64-bit Cheat Engine integration
(plugin 2.1.0) and x96dbg 1.1.0, each with a KSword control/log Tab and an acknowledged HVM backend
selector. Their archives include the VM validation report and original component
licenses; both include their corresponding source archives.

R0/HVM features require the matching current KSword driver from
[ce8aa594](https://github.com/KSwordDEV/KSword/commit/ce8aa5944b24b13eccdb2660a9ab32776ea9210d).
Each debugger package directory records the source revision and archive hash in
`publication.json`. x96dbg native forwarding remains usable without the driver.
