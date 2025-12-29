# zed-unreal-helper Command Reference

Complete reference for the `zed-unreal-helper` binary, which provides command-line access to ZedLink integration features.

## Table of Contents

- [Global Options](#global-options)
- [Connection Commands](#connection-commands)
- [Play Control Commands](#play-control-commands)
- [Build Commands](#build-commands)
- [Logging Commands](#logging-commands)
- [Blueprint Commands](#blueprint-commands)
- [Daemon Mode](#daemon-mode)

---

## Global Options

These options must come **before** the subcommand:

```bash
zed-unreal-helper [OPTIONS] <COMMAND>
```

### Options

- `-p, --port <PORT>` - ZedLink server port (default: 21567)
- `-d, --project-dir <PATH>` - Unreal project directory (reads port from `ZedLink.port` file)

### Examples

```bash
# Use default port
zed-unreal-helper status

# Specify custom port
zed-unreal-helper --port 21567 status

# Auto-detect port from project directory
zed-unreal-helper -d "W:\Projects\MyUEProject" status
```

---

## Connection Commands

### `status`

Check connection to ZedLink server.

```bash
zed-unreal-helper status
```

**Output:**
```
Connected to ZedLink on port 21567
```

**Exit code:** 0 if connected, 1 if not connected

---

## Play Control Commands

Control Play-In-Editor (PIE) sessions from the command line.

### `play start`

Start a PIE session.

```bash
zed-unreal-helper play start [OPTIONS]
```

**Options:**
- `-m, --mode <MODE>` - Play mode: `PIE`, `Simulate`, or `Standalone` (default: `PIE`)

**Examples:**
```bash
# Start normal PIE
zed-unreal-helper play start

# Start in Simulate mode
zed-unreal-helper play start --mode Simulate
```

**Output:**
```
Play session started
```

### `play stop`

Stop the current PIE session.

```bash
zed-unreal-helper play stop
```

**Output:**
```
Play session stopped
```

### `play pause`

Pause the current PIE session.

```bash
zed-unreal-helper play pause
```

**Output:**
```
Play session paused
```

### `play resume`

Resume a paused PIE session.

```bash
zed-unreal-helper play resume
```

**Output:**
```
Play session resumed
```

### `play state`

Get the current play state.

```bash
zed-unreal-helper play state
```

**Output:**
```json
{
  "state": "Playing",
  "isPlaying": true,
  "isPaused": false,
  "isSimulating": false
}
```

**Possible states:**
- `Stopped` - No PIE session running
- `Playing` - PIE session active
- `Paused` - PIE session paused
- `Simulating` - Simulate-In-Editor active

---

## Build Commands

Trigger compilation and check build status.

### `build live`

Trigger Live Coding compilation.

```bash
zed-unreal-helper build live
```

**Output:**
```
Live Coding triggered
```

**Note:** Requires Live Coding to be enabled in Unreal Editor (Edit → Editor Preferences → Live Coding → Enable Live Coding).

### `build status`

Get current build/compilation status.

```bash
zed-unreal-helper build status
```

**Output:**
```json
{
  "isCompiling": false,
  "liveCodingEnabled": true
}
```

---

## Logging Commands

Stream Unreal Engine logs to the terminal.

### `logs`

Subscribe to log streaming.

```bash
zed-unreal-helper logs [OPTIONS]
```

**Options:**
- `-f, --follow` - Stream logs continuously (required for actual streaming)
- `-c, --category <CATEGORY>` - Filter by log category (e.g., `LogTemp`, `LogBlueprintUserMessages`)
- `-v, --verbosity <LEVEL>` - Minimum verbosity level (default: `Log`)

**Verbosity levels:**
- `Fatal` - Only fatal errors
- `Error` - Errors and above
- `Warning` - Warnings and above
- `Display` - Display messages and above
- `Log` - Standard logs and above
- `Verbose` - Verbose logs and above
- `VeryVerbose` - All logs

**Examples:**

```bash
# Stream all logs
zed-unreal-helper logs --follow

# Stream only warnings and errors
zed-unreal-helper logs --follow --verbosity Warning

# Stream specific category
zed-unreal-helper logs --follow --category LogTemp

# Stream errors from specific category
zed-unreal-helper logs -f -c LogBlueprintUserMessages -v Error
```

**Output:**
```
[Log] LogTemp: This is a log message
[Warning] LogScript: Warning message here
[Error] LogBlueprintUserMessages: Error occurred
```

Logs are color-coded:
- **Red** - Fatal/Error
- **Yellow** - Warning
- **Cyan** - Display
- **White** - Log/Verbose

**Press Ctrl+C to stop streaming.**

---

## Blueprint Commands

Query and interact with Blueprint assets.

### `blueprint list`

List all Blueprint assets in the project.

```bash
zed-unreal-helper blueprint list [OPTIONS]
```

**Options:**
- `-f, --filter <PATTERN>` - Filter Blueprints by name pattern

**Examples:**

```bash
# List all Blueprints
zed-unreal-helper blueprint list

# Filter by name
zed-unreal-helper blueprint list --filter Character
zed-unreal-helper blueprint list -f "BP_Player"
```

**Output:**
```
/Game/Blueprints/BP_PlayerCharacter
/Game/Blueprints/BP_GameMode
/Game/UI/BP_MainMenu

Total: 3 blueprints
```

### `blueprint details`

Get detailed information about a specific Blueprint.

```bash
zed-unreal-helper blueprint details <ASSET_PATH>
```

**Example:**
```bash
zed-unreal-helper blueprint details "/Game/Blueprints/BP_PlayerCharacter"
```

**Output:**
```json
{
  "assetPath": "/Game/Blueprints/BP_PlayerCharacter",
  "className": "BP_PlayerCharacter_C",
  "parentClassName": "Character",
  "parentSourceFile": "C:/UE/Engine/Source/Runtime/Engine/Classes/GameFramework/Character.h",
  "parentSourceLine": 1,
  "functions": [
    "ReceiveBeginPlay",
    "OnJump"
  ],
  "variables": [
    "MaxHealth",
    "CurrentHealth"
  ]
}
```

### `blueprint open`

Open a Blueprint in the Unreal Editor.

```bash
zed-unreal-helper blueprint open <ASSET_PATH>
```

**Example:**
```bash
zed-unreal-helper blueprint open "/Game/Blueprints/BP_PlayerCharacter"
```

**Output:**
```
Opened Blueprint: /Game/Blueprints/BP_PlayerCharacter
```

The Blueprint editor will open in Unreal Engine.

### `blueprint references`

Find all Blueprints that inherit from a specific C++ class.

```bash
zed-unreal-helper blueprint references <CLASS_NAME>
```

**Example:**
```bash
zed-unreal-helper blueprint references "Character"
zed-unreal-helper blueprint references "AActor"
```

**Output:**
```
Blueprints referencing Character:
  /Game/Blueprints/BP_PlayerCharacter
  /Game/Blueprints/BP_EnemyCharacter

Total: 2 references
```

---

## Daemon Mode

Maintain a persistent connection to ZedLink and stream all events in real-time.

### `daemon`

Run as a background daemon, logging all notifications.

```bash
zed-unreal-helper daemon
```

**What it does:**
- Connects to ZedLink server
- Subscribes to all event notifications
- Streams events to terminal
- Auto-reconnects if connection drops (retries every 5 seconds)

**Events displayed:**
- Log messages (color-coded by severity)
- Play state changes (PIE start/stop/pause)
- Build status updates
- Blueprint compilation events

**Output:**
```
INFO Starting zed-unreal-helper daemon, connecting to port 21567
INFO Connected to ZedLink
[Log] LogTemp: Game started
INFO Play state changed: Playing
[Warning] LogScript: Something needs attention
INFO Play state changed: Stopped
```

**Press Ctrl+C to stop the daemon.**

**Use case:** Leave this running in a terminal while developing to monitor all Unreal Engine activity.

---

## Common Workflows

### Quick PIE Testing

```bash
# Start PIE
zed-unreal-helper play start

# Wait for testing...
sleep 5

# Stop PIE
zed-unreal-helper play stop
```

### Development Monitoring

```bash
# Terminal 1: Run daemon to monitor all activity
zed-unreal-helper daemon

# Terminal 2: Execute commands
zed-unreal-helper play start
zed-unreal-helper build live
```

### Blueprint Workflow

```bash
# Find all character Blueprints
zed-unreal-helper blueprint list --filter Character

# Get details about one
zed-unreal-helper blueprint details "/Game/Blueprints/BP_Player"

# Open it for editing
zed-unreal-helper blueprint open "/Game/Blueprints/BP_Player"
```

### Live Coding Workflow

```bash
# Make C++ code changes in Zed

# Trigger compilation
zed-unreal-helper build live

# Check status
zed-unreal-helper build status

# Test in PIE
zed-unreal-helper play start
```

---

## Troubleshooting

### Connection Failed

**Problem:** `Not connected: Connection refused`

**Solutions:**
1. Verify Unreal Editor is running
2. Check ZedLink plugin is enabled (Edit → Plugins → ZedLink)
3. Check Output Log for `LogZedLink: Server started on port 21567`
4. Verify port file exists: `{Project}/Intermediate/ZedLink.port`

### Port Already in Use

**Problem:** Server won't start on port 21567

**Solution:** Specify a different port in both places:
- Unreal Editor: Modify `ZedLinkModule.cpp` to use different port
- Helper: Use `--port` option

### Live Coding Not Available

**Problem:** `Live Coding is not enabled for this session`

**Solution:** Enable Live Coding:
1. Edit → Editor Preferences
2. Search for "Live Coding"
3. Check "Enable Live Coding"
4. Restart Unreal Editor

### No Logs Appearing

**Problem:** Logs command shows nothing

**Solution:**
1. Use `--follow` flag to stream continuously
2. Generate activity in Unreal (compile Blueprint, start PIE, etc.)
3. Lower verbosity: `--verbosity Log`
4. Check Output Log in Unreal to verify logs are being generated

---

## Environment Variables

### `RUST_LOG`

Control helper binary's internal logging verbosity.

```bash
# Windows PowerShell
$env:RUST_LOG="debug"
zed-unreal-helper status

# Windows CMD
set RUST_LOG=debug
zed-unreal-helper status

# Linux/Mac
RUST_LOG=debug zed-unreal-helper status
```

**Levels:** `error`, `warn`, `info`, `debug`, `trace`

---

## Exit Codes

- `0` - Success
- `1` - Connection failed, command failed, or error occurred

---

## Version

Check version:
```bash
zed-unreal-helper --version
```

Get help:
```bash
zed-unreal-helper --help
zed-unreal-helper <command> --help
```
