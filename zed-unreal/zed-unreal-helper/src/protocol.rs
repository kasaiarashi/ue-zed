// Copyright Krishna Teja Mekala. All Rights Reserved.
// Protocol types for ZedLink JSON-RPC communication

#![allow(dead_code)]

use serde::{Deserialize, Serialize};

/// JSON-RPC notification (no response expected)
#[derive(Debug, Clone)]
pub struct Notification {
    pub method: String,
    pub params: Option<serde_json::Value>,
}

/// Blueprint information from ZedLink
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct BlueprintInfo {
    pub asset_path: String,
    pub class_name: String,
    pub parent_class_name: String,
    #[serde(default)]
    pub parent_source_file: Option<String>,
    #[serde(default)]
    pub parent_source_line: Option<i32>,
    #[serde(default)]
    pub functions: Vec<String>,
    #[serde(default)]
    pub variables: Vec<String>,
    #[serde(default)]
    pub event_graphs: Vec<String>,
}

/// Log message from ZedLink
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct LogMessage {
    pub category: String,
    pub verbosity: String,
    pub message: String,
    pub timestamp: String,
    pub frame: u64,
    #[serde(default)]
    pub source_file: Option<String>,
    #[serde(default)]
    pub source_line: Option<i32>,
}

/// Play state from ZedLink
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum PlayState {
    Stopped,
    Playing,
    Paused,
    Simulating,
}

impl std::fmt::Display for PlayState {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            PlayState::Stopped => write!(f, "Stopped"),
            PlayState::Playing => write!(f, "Playing"),
            PlayState::Paused => write!(f, "Paused"),
            PlayState::Simulating => write!(f, "Simulating"),
        }
    }
}

/// Build status from ZedLink
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum BuildStatus {
    Idle,
    Compiling,
    Success,
    Failed,
}

impl std::fmt::Display for BuildStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            BuildStatus::Idle => write!(f, "Idle"),
            BuildStatus::Compiling => write!(f, "Compiling"),
            BuildStatus::Success => write!(f, "Success"),
            BuildStatus::Failed => write!(f, "Failed"),
        }
    }
}

/// Build error from ZedLink
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct BuildError {
    pub file: String,
    pub line: i32,
    pub column: i32,
    pub message: String,
    pub severity: String,
}
