# KSword Plugins

The KSword plugin marketplace catalog. KSword reads [catalog.json](catalog.json)
to list available plugins, then requires users to review and accept each
plugin's license before downloading its archive.

Ordinary plugins are independently packaged under `ARK/`. Their entries provide
an HTTPS archive URL, SHA-256, license URL, and install directory. Upstream
plugins instead declare `distribution.type="upstream-assets"`: the catalog
maintains fixed official release URLs, file hashes, directory mappings, and a
plugin manifest. KSword validates the downloads and installs them on demand.
Command plugins use
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

## Upstream distribution

The Ghidra C decompiler backend uses the generic `upstream-assets` protocol.
Ghidra 12.0.4 and its Eclipse Temurin 21.0.12.1+1 Windows x64 JDK are downloaded
as complete ZIPs directly from the original publishers' GitHub Releases.
This repository stores their distribution metadata only; it does not mirror
Ghidra or JDK binaries. License and notice metadata use SHA-256-pinned raw URLs
at an immutable KSword source revision.

Upstream entries support multiple archives in one installation transaction.
Users explicitly install or update a fixed catalog version; clients do not
follow `latest` or automatically update these entries. Any failed download,
hash, extraction, or manifest check preserves the previous installation.

When updating only a dependency such as the JDK, also increment the plugin's
package `version` so clients can discover the update. Keep `runtime_version`,
`java_version`, archive roots, runtime paths, URL/hash pairs, and license/notice
metadata consistent. Existing ordinary ZIP entries remain supported.

See the [upstream distribution specification](https://github.com/KSwordDEV/KSword/blob/main/docs/插件上游分发协议.md)
for the catalog fields, GitHub repository identity checks, size/path limits,
and the distinction between distribution and the plugin runtime protocol.
