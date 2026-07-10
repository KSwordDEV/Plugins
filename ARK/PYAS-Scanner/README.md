# PYAS Scanner plugin

This plugin ships a standalone `scanner.exe`. KSword reads `plugin.json`,
starts that executable, and renders its JSON Lines scan events as a progress
view, summary, and result table. No Python installation is required.

`scanner.py` is retained as the corresponding source code. Developers who
want to rebuild the executable can install the source dependencies:

```powershell
py -3 -m pip install -r requirements.txt
py -3 -m PyInstaller --noconfirm --clean --onefile --name scanner scanner.py
```

## Run through KSword

From the repository root, or from a release folder that contains a sibling
`plugin\` directory:

```powershell
.\scanner.exe --ksword-plugin scan -- --target-kind file --path 'C:\Samples\target.exe'
.\scanner.exe --ksword-plugin scan -- --target-kind file --path 'C:\Samples' --model 'C:\Models\custom.onnx'
.\scanner.exe --ksword-plugin scan -- --target-kind process --pid 1234 --path 'C:\Samples\target.exe' --process-name target.exe
```

The plugin emits UTF-8 JSON Lines in host mode. `ready`, `scan_started`,
`file_result`, and `scan_complete` are the normal records. Its normal direct
execution remains supported and keeps the original interactive text UI:

```powershell
py -3 .\scanner.py 'C:\Samples\target.exe'
```

In the KSword GUI, right-click one regular file and use **插件 → PYAS Scanner**.
The process-detail **插件** page exposes the same plugin for a process target.
Both paths launch `scanner.exe`; the bundled Python file is source only.

See `docs/插件系统规范.md` for the host contract. Keep `LICENSE.txt` and this
`NOTICE` with every local copy of this plugin.
