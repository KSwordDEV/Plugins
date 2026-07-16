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
