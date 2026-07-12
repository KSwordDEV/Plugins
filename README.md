# KSword Plugins

The KSword plugin marketplace catalog. KSword reads [catalog.json](catalog.json)
to list available plugins, then requires users to review and accept each
plugin's license before downloading its archive.

Each plugin is independently packaged under `ARK/`. A published entry must
provide an HTTPS archive URL, SHA-256, license URL, and install directory.
KSword validates the archive, installs it on demand, and renders scan plugins
from the generic `visualization` contract in each plugin manifest.

Published plugins currently include the PYAS PE scanner and the isolated nDPI
Network Inspector for live application-protocol classification.
