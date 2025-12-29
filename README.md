# ZedLink - Unreal Engine Integration for Zed Editor

A comprehensive integration between Unreal Engine 5.6+ and Zed editor, inspired by JetBrains RiderLink/UnrealLink.

## Overview

This project consists of two components:

1. **ZedLink** - A C++ Unreal Engine Editor plugin that runs a TCP server
2. **zed-unreal** - A Zed extension with a Rust helper binary that connects to ZedLink

## Features

- **Code Navigation** - clangd LSP with UE compile_commands.json support
- **Blueprint Integration** - Navigate between C++ and Blueprints
- **Live Logging** - Stream UE logs to Zed with filtering
- **Play Control** - Start/Stop/Pause PIE sessions from Zed
- **Build Integration** - Trigger Live Coding from Zed

## Installation

### ZedLink (Unreal Engine Plugin)

1. Copy the `ZedLink` folder to your project's `Plugins` directory:
   ```
   YourProject/
   └── Plugins/
       └── ZedLink/
   ```

2. Regenerate project files and rebuild

3. Enable the plugin in Edit > Plugins > ZedLink

### zed-unreal (Zed Extension)

The extension will be available via Zed's extension registry once published.

For development:
1. Build the extension: `cd zed-unreal && cargo build --release`
2. Install as dev extension in Zed

## Usage

### Helper Binary Commands

```bash
# Check connection status
zed-unreal-helper status

# Play control
zed-unreal-helper play start
zed-unreal-helper play stop
zed-unreal-helper play pause
zed-unreal-helper play resume

# Build
zed-unreal-helper build live

# Logs
zed-unreal-helper logs --follow

# Blueprints
zed-unreal-helper blueprint list
zed-unreal-helper blueprint open "/Game/Blueprints/BP_Player"
zed-unreal-helper blueprint details "/Game/Blueprints/BP_Player"
```

## Architecture

```
┌─────────────────┐         TCP/JSON-RPC        ┌──────────────────┐
│  Unreal Editor  │◄──────────────────────────►│   Zed Editor     │
│                 │         Port 21567          │                  │
│  ┌───────────┐  │                             │  ┌────────────┐  │
│  │  ZedLink  │  │                             │  │ zed-unreal │  │
│  │  Plugin   │  │                             │  │ extension  │  │
│  └───────────┘  │                             │  └─────┬──────┘  │
│        │        │                             │        │         │
│  ┌─────┴─────┐  │                             │  ┌─────▼──────┐  │
│  │ Services  │  │                             │  │   Helper   │  │
│  │-Blueprint │  │                             │  │   Binary   │  │
│  │-Logging   │  │                             │  │   (Rust)   │  │
│  │-Play      │  │                             │  └────────────┘  │
│  │-Build     │  │                             │                  │
│  └───────────┘  │                             │                  │
└─────────────────┘                             └──────────────────┘
```

## Protocol

Communication uses JSON-RPC 2.0 over TCP with 4-byte length-prefixed messages.

### Port Discovery

ZedLink writes the server port to `{ProjectDir}/Intermediate/ZedLink.port`

### Message Types

```json
// Play Control
{ "method": "play/start", "params": { "mode": "PIE" } }
{ "method": "play/stop", "params": {} }

// Build
{ "method": "build/liveCoding", "params": {} }

// Logging
{ "method": "logging/subscribe", "params": { "minVerbosity": "Warning" } }

// Blueprint
{ "method": "blueprint/openAsset", "params": { "assetPath": "/Game/BP_Player" } }
```

## Requirements

- Unreal Engine 5.6+
- Zed Editor
- Rust toolchain (for building the extension)
- clangd (for C++ language server support)

## License

MIT License - see [LICENSE](LICENSE)

## Author

Krishna Teja Mekala ([@kasaiarashi](https://github.com/kasaiarashi))
