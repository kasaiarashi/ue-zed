// Copyright Krishna Teja Mekala. All Rights Reserved.
// ZedLink Helper Binary - TCP client for communication with Unreal Engine

use anyhow::Result;
use clap::{Parser, Subcommand};
use std::path::PathBuf;

mod connection;
mod protocol;

use connection::ZedLinkConnection;

#[derive(Parser)]
#[command(name = "zed-unreal-helper")]
#[command(author = "Krishna Teja Mekala")]
#[command(version = "1.0.0")]
#[command(about = "Helper binary for Zed Unreal Engine integration")]
struct Cli {
    #[command(subcommand)]
    command: Commands,

    /// ZedLink server port (default: 21567, or read from port file)
    #[arg(short, long, default_value = "21567")]
    port: u16,

    /// Unreal project directory (for port file discovery)
    #[arg(short = 'd', long)]
    project_dir: Option<PathBuf>,
}

#[derive(Subcommand)]
enum Commands {
    /// Run as daemon, maintaining persistent connection to ZedLink
    Daemon,

    /// Play control commands
    Play {
        #[command(subcommand)]
        action: PlayAction,
    },

    /// Build commands
    Build {
        #[command(subcommand)]
        action: BuildAction,
    },

    /// Log streaming commands
    Logs {
        /// Follow log output continuously
        #[arg(short, long)]
        follow: bool,

        /// Filter by log category
        #[arg(short, long)]
        category: Option<String>,

        /// Minimum verbosity level
        #[arg(short, long, default_value = "Log")]
        verbosity: String,
    },

    /// Blueprint commands
    Blueprint {
        #[command(subcommand)]
        action: BlueprintAction,
    },

    /// Connection status
    Status,
}

#[derive(Subcommand)]
enum PlayAction {
    /// Start Play-In-Editor session
    Start {
        /// Play mode: PIE, Simulate, or Standalone
        #[arg(short, long, default_value = "PIE")]
        mode: String,
    },
    /// Stop current PIE session
    Stop,
    /// Pause PIE session
    Pause,
    /// Resume paused PIE session
    Resume,
    /// Get current play state
    State,
}

#[derive(Subcommand)]
enum BuildAction {
    /// Trigger Live Coding compilation
    Live,
    /// Get build status
    Status,
}

#[derive(Subcommand)]
enum BlueprintAction {
    /// List all Blueprint assets
    List {
        /// Filter by name pattern
        #[arg(short, long)]
        filter: Option<String>,
    },
    /// Open Blueprint in Unreal Editor
    Open {
        /// Blueprint asset path
        asset_path: String,
    },
    /// Get Blueprint details
    Details {
        /// Blueprint asset path
        asset_path: String,
    },
    /// Find Blueprints referencing a C++ class
    References {
        /// C++ class name
        class_name: String,
    },
}

#[tokio::main]
async fn main() -> Result<()> {
    // Initialize logging
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::from_default_env()
                .add_directive("zed_unreal_helper=info".parse()?)
        )
        .init();

    let cli = Cli::parse();

    // Resolve port from port file if project directory specified
    let port = resolve_port(cli.port, cli.project_dir.as_deref());

    match cli.command {
        Commands::Daemon => run_daemon(port).await,
        Commands::Play { action } => handle_play(port, action).await,
        Commands::Build { action } => handle_build(port, action).await,
        Commands::Logs { follow, category, verbosity } => {
            handle_logs(port, follow, category, verbosity).await
        }
        Commands::Blueprint { action } => handle_blueprint(port, action).await,
        Commands::Status => handle_status(port).await,
    }
}

/// Try to read port from ZedLink.port file, fall back to specified port
fn resolve_port(default_port: u16, project_dir: Option<&PathBuf>) -> u16 {
    if let Some(dir) = project_dir {
        let port_file = dir.join("Intermediate").join("ZedLink.port");
        if let Ok(contents) = std::fs::read_to_string(&port_file) {
            if let Ok(port) = contents.trim().parse::<u16>() {
                tracing::info!("Using port {} from port file", port);
                return port;
            }
        }
    }
    default_port
}

async fn run_daemon(port: u16) -> Result<()> {
    tracing::info!("Starting zed-unreal-helper daemon, connecting to port {}", port);

    loop {
        match ZedLinkConnection::connect(port).await {
            Ok(mut conn) => {
                tracing::info!("Connected to ZedLink");

                // Run message loop until disconnected
                if let Err(e) = conn.run_message_loop().await {
                    tracing::warn!("Connection error: {}", e);
                }

                tracing::info!("Disconnected from ZedLink");
            }
            Err(e) => {
                tracing::debug!("Failed to connect: {}", e);
            }
        }

        // Wait before retry
        tokio::time::sleep(std::time::Duration::from_secs(5)).await;
    }
}

async fn handle_play(port: u16, action: PlayAction) -> Result<()> {
    let mut conn = ZedLinkConnection::connect(port).await?;

    match action {
        PlayAction::Start { mode } => {
            let response = conn.send_request("play/start", serde_json::json!({
                "mode": mode,
                "playerCount": 1,
                "dedicated": false
            })).await?;

            if response.get("success").and_then(|v| v.as_bool()).unwrap_or(false) {
                println!("Play session started");
            } else {
                println!("Failed to start play session");
            }
        }
        PlayAction::Stop => {
            let response = conn.send_request("play/stop", serde_json::json!({})).await?;

            if response.get("success").and_then(|v| v.as_bool()).unwrap_or(false) {
                println!("Play session stopped");
            } else {
                println!("No active play session");
            }
        }
        PlayAction::Pause => {
            let response = conn.send_request("play/pause", serde_json::json!({
                "paused": true
            })).await?;

            if response.get("success").and_then(|v| v.as_bool()).unwrap_or(false) {
                println!("Play session paused");
            } else {
                println!("Failed to pause");
            }
        }
        PlayAction::Resume => {
            let response = conn.send_request("play/pause", serde_json::json!({
                "paused": false
            })).await?;

            if response.get("success").and_then(|v| v.as_bool()).unwrap_or(false) {
                println!("Play session resumed");
            } else {
                println!("Failed to resume");
            }
        }
        PlayAction::State => {
            let response = conn.send_request("play/getState", serde_json::json!({})).await?;
            println!("Play State: {}", serde_json::to_string_pretty(&response)?);
        }
    }

    Ok(())
}

async fn handle_build(port: u16, action: BuildAction) -> Result<()> {
    let mut conn = ZedLinkConnection::connect(port).await?;

    match action {
        BuildAction::Live => {
            let response = conn.send_request("build/liveCoding", serde_json::json!({})).await?;

            if response.get("success").and_then(|v| v.as_bool()).unwrap_or(false) {
                println!("Live Coding triggered");
            } else {
                let enabled = response.get("liveCodingEnabled").and_then(|v| v.as_bool()).unwrap_or(false);
                if !enabled {
                    println!("Live Coding is not enabled for this session");
                } else {
                    println!("Failed to trigger Live Coding");
                }
            }
        }
        BuildAction::Status => {
            let response = conn.send_request("build/getStatus", serde_json::json!({})).await?;
            println!("Build Status: {}", serde_json::to_string_pretty(&response)?);
        }
    }

    Ok(())
}

async fn handle_logs(port: u16, follow: bool, category: Option<String>, verbosity: String) -> Result<()> {
    let mut conn = ZedLinkConnection::connect(port).await?;

    // Subscribe to logs
    let categories: Vec<String> = category.into_iter().collect();
    conn.send_request("logging/subscribe", serde_json::json!({
        "categories": categories,
        "minVerbosity": verbosity
    })).await?;

    if follow {
        // Stream logs continuously
        conn.stream_notifications(|notification| {
            if notification.method == "logging/message" {
                if let Some(params) = &notification.params {
                    let category = params.get("category").and_then(|v| v.as_str()).unwrap_or("Unknown");
                    let verbosity = params.get("verbosity").and_then(|v| v.as_str()).unwrap_or("Log");
                    let message = params.get("message").and_then(|v| v.as_str()).unwrap_or("");

                    // Color code by verbosity
                    let color = match verbosity {
                        "Fatal" | "Error" => "\x1b[31m",  // Red
                        "Warning" => "\x1b[33m",          // Yellow
                        "Display" => "\x1b[36m",          // Cyan
                        _ => "\x1b[0m",                   // Default
                    };

                    println!("{}[{}] {}: {}\x1b[0m", color, verbosity, category, message);
                }
            }
        }).await?;
    } else {
        println!("Subscribed to logs. Use --follow to stream continuously.");
    }

    Ok(())
}

async fn handle_blueprint(port: u16, action: BlueprintAction) -> Result<()> {
    let mut conn = ZedLinkConnection::connect(port).await?;

    match action {
        BlueprintAction::List { filter } => {
            let response = conn.send_request("blueprint/listClasses", serde_json::json!({
                "filter": filter.unwrap_or_default()
            })).await?;

            if let Some(blueprints) = response.as_array() {
                for bp in blueprints {
                    if let Some(path) = bp.as_str() {
                        println!("{}", path);
                    }
                }
                println!("\nTotal: {} blueprints", blueprints.len());
            }
        }
        BlueprintAction::Open { asset_path } => {
            let response = conn.send_request("blueprint/openAsset", serde_json::json!({
                "assetPath": asset_path
            })).await?;

            if response.get("success").and_then(|v| v.as_bool()).unwrap_or(false) {
                println!("Opened Blueprint: {}", asset_path);
            } else {
                println!("Failed to open Blueprint");
            }
        }
        BlueprintAction::Details { asset_path } => {
            let response = conn.send_request("blueprint/getDetails", serde_json::json!({
                "assetPath": asset_path
            })).await?;

            println!("{}", serde_json::to_string_pretty(&response)?);
        }
        BlueprintAction::References { class_name } => {
            let response = conn.send_request("blueprint/findReferences", serde_json::json!({
                "className": class_name
            })).await?;

            if let Some(refs) = response.as_array() {
                println!("Blueprints referencing {}:", class_name);
                for r in refs {
                    if let Some(path) = r.as_str() {
                        println!("  {}", path);
                    }
                }
                println!("\nTotal: {} references", refs.len());
            }
        }
    }

    Ok(())
}

async fn handle_status(port: u16) -> Result<()> {
    match ZedLinkConnection::connect(port).await {
        Ok(_) => {
            println!("Connected to ZedLink on port {}", port);
            Ok(())
        }
        Err(e) => {
            println!("Not connected: {}", e);
            std::process::exit(1);
        }
    }
}
