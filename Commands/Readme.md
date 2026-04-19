# Priordium Command Scripts Guide

This folder contains helper scripts for building the project and running automated tests.

## Prerequisites

Before using these scripts, make sure:

1. You are on Windows.
2. Unreal Engine 5.6 is installed at:
	`C:\Program Files\Epic Games\UE_5.6`
3. The project file exists at the repository root:
	`Priordium.uproject`

If your Unreal Engine installation path is different, update the hardcoded engine paths in:

- `Commands/Build.bat`
- `Commands/RunTests.bat`

## Available Scripts

### `Build.bat`

Builds the `PriordiumEditor` target in `Development` configuration for `Win64`.

What it does:

1. Detects the project root from the script location.
2. Calls Unreal Build Tool via UE `Build.bat`.
3. Prints a success/failure summary with exit code.
4. Waits for a keypress before closing.

Run:

```bat
Commands\Build.bat
```

### `Build_Launch.bat`

Opens a persistent `cmd` window and runs `Build.bat` inside it.

This is useful when launching from Explorer, because the terminal stays open after the build.

Run:

```bat
Commands\Build_Launch.bat
```

### `RunTests.bat`

Runs Unreal automation tests for the `Priordium.MapGenerator` test set using `UnrealEditor-Cmd.exe`.

What it does:

1. Executes tests in unattended commandlet mode.
2. Exports Unreal test report data into `Saved\TestResults`.
3. Runs `GenerateReport.ps1` to produce a standalone `report.html`.
4. Opens `report.html` automatically (if generated).
5. Returns the Unreal test process exit code.

Run:

```bat
Commands\RunTests.bat
```

### `GenerateReport.ps1`

Builds a standalone `report.html` from Unreal's generated `index.html` and `index.json`.

Key behavior:

1. Rewrites legacy local dependency paths to CDN links.
2. Removes unsupported local assets.
3. Inlines `index.json` data into the HTML.
4. Makes the final report usable via `file://` (without local web server).

Default input/output directory:

- `Saved\TestResults`

Run manually:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Commands\GenerateReport.ps1
```

Run with custom report directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Commands\GenerateReport.ps1 -ReportDir "C:\Path\To\TestResults"
```

## Typical Workflow

From project root:

1. Build the editor target:

```bat
Commands\Build.bat
```

2. Run automated tests and open report:

```bat
Commands\RunTests.bat
```

3. Review results in:

- `Saved\TestResults\report.html`

## Exit Codes

- `0`: success
- non-zero: failure

`Build.bat` and `RunTests.bat` both print result banners with the final exit code.

## Troubleshooting

### Unreal Engine path errors

Symptom: script cannot find UE `Build.bat` or `UnrealEditor-Cmd.exe`.

Fix: update UE path in:

- `Commands/Build.bat`
- `Commands/RunTests.bat`

### `report.html not found`

Symptom: tests run, but no standalone report is generated.

Check:

1. `Saved\TestResults\index.json` exists.
2. `Saved\TestResults\index.html` exists.
3. PowerShell execution is allowed (`-ExecutionPolicy Bypass` is already used by script).

### Tests fail in CI/headless style mode

`RunTests.bat` uses `-Unattended -NullRHI -NoSplash`. This is expected for non-interactive runs.

If your tests require rendering/GPU behavior, adjust command line flags in `RunTests.bat`.

