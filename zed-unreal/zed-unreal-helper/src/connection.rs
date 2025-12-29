// Copyright Krishna Teja Mekala. All Rights Reserved.
// TCP connection handler for ZedLink communication

use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};
use std::sync::atomic::{AtomicU64, Ordering};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;

use crate::protocol::Notification;

static REQUEST_ID: AtomicU64 = AtomicU64::new(1);

/// JSON-RPC 2.0 request
#[derive(Debug, Serialize)]
struct JsonRpcRequest {
    jsonrpc: &'static str,
    id: String,
    method: String,
    params: serde_json::Value,
}

/// JSON-RPC 2.0 response
#[derive(Debug, Deserialize)]
pub struct JsonRpcResponse {
    pub jsonrpc: String,
    pub id: Option<String>,
    pub result: Option<serde_json::Value>,
    pub error: Option<JsonRpcError>,
    pub method: Option<String>,
    pub params: Option<serde_json::Value>,
}

/// JSON-RPC 2.0 error
#[derive(Debug, Deserialize)]
pub struct JsonRpcError {
    pub code: i32,
    pub message: String,
}

/// TCP connection to ZedLink server
pub struct ZedLinkConnection {
    stream: TcpStream,
}

impl ZedLinkConnection {
    /// Connect to ZedLink server and perform handshake
    pub async fn connect(port: u16) -> Result<Self> {
        let addr = format!("127.0.0.1:{}", port);
        let stream = TcpStream::connect(&addr).await?;

        let mut conn = Self { stream };

        // Perform handshake
        let response = conn.send_request("connection/initialize", serde_json::json!({
            "clientVersion": env!("CARGO_PKG_VERSION"),
            "clientName": "zed-unreal-helper",
            "capabilities": ["blueprint", "logging", "play", "build"]
        })).await?;

        tracing::debug!("Handshake response: {:?}", response);

        Ok(conn)
    }

    /// Send a JSON-RPC request and wait for response
    pub async fn send_request(&mut self, method: &str, params: serde_json::Value) -> Result<serde_json::Value> {
        let id = REQUEST_ID.fetch_add(1, Ordering::SeqCst).to_string();

        let request = JsonRpcRequest {
            jsonrpc: "2.0",
            id: id.clone(),
            method: method.to_string(),
            params,
        };

        // Serialize and send
        let json = serde_json::to_string(&request)?;
        self.send_message(&json).await?;

        // Read response
        loop {
            let response = self.read_message().await?;

            // Check if this is our response
            if let Some(response_id) = &response.id {
                if response_id == &id {
                    if let Some(error) = response.error {
                        return Err(anyhow!("RPC error {}: {}", error.code, error.message));
                    }
                    return Ok(response.result.unwrap_or(serde_json::Value::Null));
                }
            }

            // If not our response, it might be a notification - ignore for now
            tracing::debug!("Received notification while waiting for response");
        }
    }

    /// Send a raw message with length prefix
    async fn send_message(&mut self, message: &str) -> Result<()> {
        let bytes = message.as_bytes();
        let len = bytes.len() as u32;

        // Write 4-byte big-endian length prefix
        self.stream.write_all(&len.to_be_bytes()).await?;
        self.stream.write_all(bytes).await?;
        self.stream.flush().await?;

        Ok(())
    }

    /// Read a message with length prefix
    async fn read_message(&mut self) -> Result<JsonRpcResponse> {
        // Read 4-byte length prefix
        let mut len_buf = [0u8; 4];
        self.stream.read_exact(&mut len_buf).await?;
        let len = u32::from_be_bytes(len_buf) as usize;

        if len == 0 || len > 10 * 1024 * 1024 {
            return Err(anyhow!("Invalid message length: {}", len));
        }

        // Read message body
        let mut msg_buf = vec![0u8; len];
        self.stream.read_exact(&mut msg_buf).await?;

        let response: JsonRpcResponse = serde_json::from_slice(&msg_buf)?;
        Ok(response)
    }

    /// Run message loop, handling notifications
    pub async fn run_message_loop(&mut self) -> Result<()> {
        loop {
            let msg = self.read_message().await?;

            // Handle notifications (no id)
            if msg.id.is_none() {
                if let Some(method) = &msg.method {
                    self.handle_notification(method, msg.params.as_ref());
                }
            }
        }
    }

    /// Handle incoming notification
    fn handle_notification(&self, method: &str, params: Option<&serde_json::Value>) {
        match method {
            "logging/message" => {
                if let Some(p) = params {
                    let category = p.get("category").and_then(|v| v.as_str()).unwrap_or("Unknown");
                    let verbosity = p.get("verbosity").and_then(|v| v.as_str()).unwrap_or("Log");
                    let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("");

                    tracing::info!("[{}] {}: {}", verbosity, category, message);
                }
            }
            "play/stateChanged" => {
                if let Some(p) = params {
                    let state = p.get("state").and_then(|v| v.as_str()).unwrap_or("Unknown");
                    tracing::info!("Play state changed: {}", state);
                }
            }
            "build/status" => {
                if let Some(p) = params {
                    let status = p.get("status").and_then(|v| v.as_str()).unwrap_or("Unknown");
                    tracing::info!("Build status: {}", status);
                }
            }
            "blueprint/changed" => {
                if let Some(p) = params {
                    let asset_path = p.get("assetPath").and_then(|v| v.as_str()).unwrap_or("Unknown");
                    tracing::info!("Blueprint changed: {}", asset_path);
                }
            }
            _ => {
                tracing::debug!("Unknown notification: {}", method);
            }
        }
    }

    /// Stream notifications with a custom handler
    pub async fn stream_notifications<F>(&mut self, mut handler: F) -> Result<()>
    where
        F: FnMut(Notification),
    {
        loop {
            let msg = self.read_message().await?;

            if msg.id.is_none() {
                if let Some(method) = msg.method {
                    handler(Notification {
                        method,
                        params: msg.params,
                    });
                }
            }
        }
    }
}
