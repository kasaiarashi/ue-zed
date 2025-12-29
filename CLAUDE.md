# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ZedLink is a comprehensive integration between Unreal Engine 5.6+ and Zed editor, inspired by JetBrains RiderLink. The project consists of two components that communicate via TCP using JSON-RPC 2.0:

- **ZedLink** - C++ Unreal Engine Editor plugin (TCP server)
- **zed-unreal** - Zed extension with Rust helper binary (TCP client)

## Build Commands

### Building the Zed Extension

**Windows:**
```powershell
.\build.ps1
```

**Unix/Mac/Linux:**
```bash
./build.sh
```

This builds:
1. Helper binary (`zed-unreal-helper`) as native executable
2. Extension as WASM module (`wasm32-wasip1` target)

**Manual steps:**
```bash
# Build helper binary
cd zed-unreal/zed-unreal-helper
cargo build --release

# Build WASM extension
cd ../
rustup target add wasm32-wasip1
cargo build --release --target wasm32-wasip1
```

### Building the UE Plugin

The ZedLink plugin is built using Unreal Build Tool (UBT) as part of the host project:

1. Copy `ZedLink/` to `{UEProject}/Plugins/ZedLink/`
2. Regenerate project files (right-click `.uproject` → "Generate Visual Studio project files")
3. Build the project in your IDE or via Unreal Editor

The plugin module loads at `PostEngineInit` phase and is editor-only (not packaged with game builds).

## Architecture

### High-Level Communication Flow

```
┌─────────────────┐    TCP Port 21567      ┌──────────────────┐
│  Unreal Editor  │◄───────────────────────►│   Zed Editor     │
│                 │  JSON-RPC 2.0 messages  │                  │
│  ZedLink Plugin │  4-byte length prefix   │  zed-unreal ext  │
│  (C++ Server)   │                         │  (Rust Client)   │
└─────────────────┘                         └──────────────────┘
```

### ZedLink Plugin Architecture (C++)

**Module Entry Point:** `FZedLinkModule` (implements `IModuleInterface`)
- Manages server lifecycle and all services
- Writes port to `{ProjectDir}/Intermediate/ZedLink.port` for discovery
- Default port: 21567

**Server:** `FZedLinkServer` (implements `FRunnable`)
- Multi-threaded TCP server with single-client model
- Thread-safe message queue for outgoing notifications
- Delegates: `OnMessageReceived`, `OnClientConnected`, `OnClientDisconnected`
- Automatic handshake via `connection/initialize` method

**Services:** All services follow a consistent pattern:
- Hold `TWeakPtr` to server to prevent circular references
- Subscribe to server's `OnMessageReceived` delegate
- Handle their namespace of JSON-RPC methods
- Send notifications back to client for events

1. **BlueprintService** (`blueprint/*` methods)
   - Queries Blueprint assets via Asset Registry
   - Extracts metadata (functions, variables, parent class)
   - Finds C++ parent source files using `FSourceCodeNavigation`
   - Opens Blueprints via `UAssetEditorSubsystem`
   - Sends `blueprint/changed` notifications on compile

2. **LogService** (`logging/*` methods)
   - Extends `FOutputDevice` to hook UE logging system
   - Filters by category and verbosity
   - Streams logs as `logging/message` notifications
   - Thread-safe with `FCriticalSection`

3. **PlayService** (`play/*` methods)
   - Controls PIE/SIE sessions
   - Monitors state changes via `FEditorDelegates::PostPIEStarted` and `FEditorDelegates::EndPIE`
   - Sends `play/stateChanged` notifications

4. **BuildService** (`build/*` methods)
   - Triggers Live Coding compilation
   - Only available when `WITH_LIVE_CODING=1` (UE 5.0+)
   - Sends `build/status` notifications

### Zed Extension Architecture (Rust)

**Extension** (`zed-unreal/src/lib.rs`)
- Configures clangd LSP for C++ navigation
- Searches for `compile_commands.json` in multiple locations
- Discovers ZedLink port from `{ProjectDir}/Intermediate/ZedLink.port`

**Helper Binary** (`zed-unreal-helper/`)
- Standalone CLI tool built with tokio async runtime
- Commands: `daemon`, `play`, `build`, `logs`, `blueprint`, `status`
- Reads port from `ZedLink.port` file or defaults to 21567

**Connection Handler** (`connection.rs`)
- Async TCP client using tokio
- Implements JSON-RPC 2.0 with 4-byte big-endian length prefixes
- Handles requests (with ID) and notifications (no ID)
- Maintains persistent connection in daemon mode

### JSON-RPC Protocol

**Wire Format:**
```
[4 bytes: message length (big-endian)][N bytes: UTF-8 JSON]
```

**Method Namespaces:**
- `connection/*` - handshake and lifecycle
- `blueprint/*` - Blueprint queries and navigation
- `logging/*` - log streaming and filtering
- `play/*` - PIE session control
- `build/*` - compilation and Live Coding

**Communication Patterns:**
- **Request-Response:** Client sends request, server responds with matching ID
- **Server Push:** Server sends notifications (no ID) for events
- **Threading:** C++ server broadcasts to game thread via `AsyncTask(ENamedThreads::GameThread)`, Rust client uses tokio async/await

## Key Patterns and Conventions

### Thread Safety
- C++ services use `FCriticalSection` for shared state
- Message queues for cross-thread communication
- All UE API calls must occur on game thread

### Weak References
- Services hold `TWeakPtr<FZedLinkServer>` to prevent circular references
- Always check `IsValid()` before dereferencing weak pointers

### Error Handling
- **C++:** Extensive logging via `UE_LOG(LogZedLink, ...)`, graceful degradation
- **Rust:** Result types with `anyhow` for error context, retry logic in daemon mode

### Module Dependencies

**ZedLink Plugin (`ZedLink.Build.cs`):**
- Public: Core, CoreUObject, Engine, Sockets, Networking, Json, JsonUtilities
- Private: UnrealEd, Slate, SlateCore, EditorStyle, LevelEditor, Kismet, BlueprintGraph, AssetRegistry, ToolMenus, EditorSubsystem, Projects
- Conditional: LiveCoding (when `Target.bWithLiveCoding` is true)

**Rust Workspace:**
- Extension (WASM): `zed_extension_api`, `serde`, `serde_json`
- Helper (native): `tokio`, `serde`, `anyhow`, `thiserror`, `tracing`, `clap`

## Development Workflow

### Testing the Integration

1. Build and install ZedLink plugin in your UE project
2. Build the Zed extension using `build.ps1` or `build.sh`
3. Install extension in Zed: `Cmd+Shift+P` → "zed: install dev extension"
4. Open UE project in Zed
5. Launch Unreal Editor (ZedLink starts server automatically)
6. Test connection: `zed-unreal-helper status`
7. Test features:
   ```bash
   # Stream logs
   zed-unreal-helper logs --follow

   # Control PIE
   zed-unreal-helper play start
   zed-unreal-helper play stop

   # List Blueprints
   zed-unreal-helper blueprint list

   # Trigger Live Coding
   zed-unreal-helper build live
   ```

### Port Discovery
1. ZedLink writes port to `{ProjectDir}/Intermediate/ZedLink.port` on startup
2. Helper binary reads this file when `--project-dir` is specified
3. Falls back to default port 21567
4. File is deleted on shutdown

### Debugging

**C++ Plugin:**
- Attach debugger to `UnrealEditor.exe` process
- Set breakpoints in ZedLink source files
- Monitor UE Output Log for `LogZedLink` category

**Rust Helper:**
- Run with `RUST_LOG=debug` environment variable for verbose logging
- Use `--verbose` flag for detailed output
- Check connection status with `status` command

## Common Modifications

### Adding a New Service

1. Create header in `ZedLink/Source/ZedLink/Public/Services/YourService.h`
2. Implement in `ZedLink/Source/ZedLink/Private/Services/YourService.cpp`
3. Hold `TWeakPtr<FZedLinkServer>` and subscribe to `OnMessageReceived`
4. Handle methods in your namespace (e.g., `yourservice/method`)
5. Instantiate service in `FZedLinkModule::StartupModule()`
6. Add corresponding Rust types in `zed-unreal-helper/src/protocol.rs`
7. Add CLI commands in `zed-unreal-helper/src/main.rs`

### Adding a New JSON-RPC Method

**C++ (Server):**
```cpp
void FYourService::HandleMessage(const FString& Method, const TSharedPtr<FJsonObject>& Params, const FString& Id)
{
    if (Method == TEXT("yourservice/yourmethod"))
    {
        // Extract params
        FString Input = Params->GetStringField(TEXT("input"));

        // Process on game thread if needed
        AsyncTask(ENamedThreads::GameThread, [this, Input, Id]()
        {
            // Do work...

            // Send response
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("output"), Output);
            ServerPtr.Pin()->SendResponse(Id, Result);
        });
    }
}
```

**Rust (Client):**
```rust
// In protocol.rs
#[derive(Serialize, Deserialize)]
pub struct YourMethodParams {
    pub input: String,
}

// In connection.rs
pub async fn your_method(&mut self, input: String) -> Result<String> {
    let params = YourMethodParams { input };
    let response = self.send_request("yourservice/yourmethod", params).await?;
    Ok(response["output"].as_str().unwrap().to_string())
}
```

### Modifying clangd Configuration

Edit `zed-unreal/src/lib.rs` in the `language_server_command` method to add clangd arguments or change `compile_commands.json` search paths.
