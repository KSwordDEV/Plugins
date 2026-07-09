# PYAS Scanner plugin

This is a standalone Python plugin. KSword never imports its Python code or
links its model; Ksword reads `plugin.json` and starts this program directly
as a child process in this directory.

## Install dependencies

```powershell
py -3 -m pip install -r requirements.txt
```

If Python is not on `PATH`, point the host to it before running a plugin:

```powershell
$env:KSWORD_PLUGIN_PYTHON = 'C:\Python313\python.exe'
```

## Run through KSword

From the repository root, or from a release folder that contains a sibling
`plugin\` directory:

```powershell
py -3 .\scanner.py --ksword-plugin scan -- --target-kind file --path 'C:\Samples\target.exe'
py -3 .\scanner.py --ksword-plugin scan -- --target-kind file --path 'C:\Samples' --model 'C:\Models\custom.onnx'
py -3 .\scanner.py --ksword-plugin scan -- --target-kind process --pid 1234 --path 'C:\Samples\target.exe' --process-name target.exe
```

The plugin emits UTF-8 JSON Lines in host mode. `ready`, `scan_started`,
`file_result`, and `scan_complete` are the normal records. Its normal direct
execution remains supported and keeps the original interactive text UI:

```powershell
py -3 .\scanner.py 'C:\Samples\target.exe'
```

In the KSword GUI, right-click one regular file and use **插件 → PYAS Scanner**.
The process-detail **插件** page exposes the same plugin for a process target.
Both paths launch this plugin entry directly in the background; no Python code
is loaded into the KSword process.

See `docs/插件系统规范.md` for the host contract. Keep `LICENSE.txt` and this
`NOTICE` with every local copy of this plugin.
