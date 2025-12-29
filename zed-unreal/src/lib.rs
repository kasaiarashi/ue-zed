// Copyright Krishna Teja Mekala. All Rights Reserved.
// Unreal Engine Integration for Zed Editor

#![allow(dead_code)]

use zed_extension_api::{self as zed, Result};
use std::fs;
use std::path::Path;

/// Main extension struct for Unreal Engine integration
struct UnrealExtension {
    /// Cached path to the helper binary (for future use)
    _helper_path: Option<String>,
}

impl zed::Extension for UnrealExtension {
    fn new() -> Self {
        Self { _helper_path: None }
    }

    fn language_server_command(
        &mut self,
        _language_server_id: &zed::LanguageServerId,
        worktree: &zed::Worktree,
    ) -> Result<zed::Command> {
        // Use clangd for C++ LSP support
        // ZedLink plugin generates compile_commands.json in the Intermediate folder

        let clangd_path = self.find_clangd()?;
        let project_root = worktree.root_path();

        // Look for compile_commands.json in common UE locations
        let compile_commands_dir = self.find_compile_commands_dir(&project_root);

        let mut args = vec![
            "--background-index".to_string(),
            "--clang-tidy".to_string(),
            "--header-insertion=iwyu".to_string(),
            "--completion-style=detailed".to_string(),
        ];

        if let Some(dir) = compile_commands_dir {
            args.push(format!("--compile-commands-dir={}", dir));
        }

        Ok(zed::Command {
            command: clangd_path,
            args,
            env: Default::default(),
        })
    }
}

impl UnrealExtension {
    /// Find clangd binary in system PATH
    fn find_clangd(&self) -> Result<String> {
        // Check common locations
        let common_paths = [
            "/usr/bin/clangd",
            "/usr/local/bin/clangd",
            "/opt/homebrew/bin/clangd",
            // Windows paths would be handled differently
        ];

        for path in common_paths {
            if Path::new(path).exists() {
                return Ok(path.to_string());
            }
        }

        // Fall back to assuming it's in PATH
        Ok("clangd".to_string())
    }

    /// Find compile_commands.json directory for Unreal project
    fn find_compile_commands_dir(&self, project_root: &str) -> Option<String> {
        // UE generates compile_commands.json in Intermediate folder
        let intermediate_path = format!("{}/Intermediate", project_root);
        let compile_commands_path = format!("{}/compile_commands.json", intermediate_path);

        if Path::new(&compile_commands_path).exists() {
            return Some(intermediate_path);
        }

        // Check .vscode folder (some setups put it there)
        let vscode_path = format!("{}/.vscode", project_root);
        let vscode_compile_commands = format!("{}/compile_commands.json", vscode_path);

        if Path::new(&vscode_compile_commands).exists() {
            return Some(vscode_path);
        }

        // Check project root
        let root_compile_commands = format!("{}/compile_commands.json", project_root);
        if Path::new(&root_compile_commands).exists() {
            return Some(project_root.to_string());
        }

        None
    }

    /// Find ZedLink port file and read the port number
    #[allow(dead_code)]
    fn find_zedlink_port(&self, project_root: &str) -> Option<u16> {
        let port_file_path = format!("{}/Intermediate/ZedLink.port", project_root);

        if let Ok(contents) = fs::read_to_string(&port_file_path) {
            if let Ok(port) = contents.trim().parse::<u16>() {
                return Some(port);
            }
        }

        // Default ZedLink port
        Some(21567)
    }

    /// Get the path to the helper binary for the current platform
    #[allow(dead_code)]
    fn get_helper_binary_name(&self) -> &'static str {
        #[cfg(target_os = "macos")]
        {
            #[cfg(target_arch = "aarch64")]
            return "zed-unreal-helper-darwin-aarch64";
            #[cfg(target_arch = "x86_64")]
            return "zed-unreal-helper-darwin-x86_64";
        }

        #[cfg(target_os = "linux")]
        return "zed-unreal-helper-linux-x86_64";

        #[cfg(target_os = "windows")]
        return "zed-unreal-helper-windows-x86_64.exe";

        #[cfg(not(any(target_os = "macos", target_os = "linux", target_os = "windows")))]
        return "zed-unreal-helper";
    }
}

zed::register_extension!(UnrealExtension);
