// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// taktflow-xtask — Phase 5 Line B operator flashing tool.
//
// Current scope: `flash-cvc` (CVC STM32G474 via ST-LINK + STM32_Programmer_CLI).
//
// DEFAULT MODE IS DRY-RUN. This is a hard requirement of the Phase 5 Line B
// subset prompt: the autonomous worker must NOT flash real hardware — it
// only scaffolds and prints the exact command the operator would run.
// Passing `--execute` upgrades to real invocation, but only if the
// STM32_Programmer_CLI binary can be located. When STM32_Programmer_CLI is
// not installed (e.g. autonomous CI env) the operator workflow is:
//
//   1. Autonomous agent runs    : cargo xtask flash-cvc --dry-run
//   2. Operator inspects output : matches expected serial / elf / port
//   3. Operator runs manually   : cargo xtask flash-cvc --execute
//
// The ST-LINK serial is resolved at runtime from
// tools/bench/hardware-map.toml. No serial is hard-coded in this file;
// that is enforced by tests/phase5/test_xtask_flash_cvc_dry_run.py.
//
// ADR cross-refs:
//   ADR-0006 (fork + track upstream + extras)
//   ADR-0018 (never hard fail) — this tool prefers "stop and report" to
//            destructive fallbacks (no --erase all, no --force).

use std::path::{Path, PathBuf};
use std::process::Command;

use anyhow::{bail, Context, Result};
use clap::{Parser, Subcommand};
use serde::Deserialize;

// ---------------------------------------------------------------------------
// hardware-map.toml schema (minimal, only the fields xtask needs)
// ---------------------------------------------------------------------------

#[derive(Debug, Deserialize)]
struct HardwareMap {
    #[serde(default)]
    stlink: Vec<StLinkProbe>,
}

#[derive(Debug, Deserialize)]
struct StLinkProbe {
    logical_ecu: String,
    stlink_serial: String,
    #[allow(dead_code)]
    #[serde(default)]
    com_port: Option<String>,
    #[allow(dead_code)]
    #[serde(default)]
    target_mcu: Option<String>,
}

fn find_repo_root(start: &Path) -> Result<PathBuf> {
    let mut cur = start.to_path_buf();
    loop {
        if cur.join("tools/bench/hardware-map.toml").exists() {
            return Ok(cur);
        }
        if !cur.pop() {
            bail!("could not find repo root (looking for tools/bench/hardware-map.toml)");
        }
    }
}

fn load_hardware_map(repo_root: &Path) -> Result<HardwareMap> {
    let path = repo_root.join("tools/bench/hardware-map.toml");
    let raw = std::fs::read_to_string(&path)
        .with_context(|| format!("reading {}", path.display()))?;
    let map: HardwareMap = toml::from_str(&raw)
        .with_context(|| format!("parsing {}", path.display()))?;
    Ok(map)
}

fn resolve_stlink_serial(map: &HardwareMap, logical_ecu: &str) -> Result<String> {
    for probe in &map.stlink {
        if probe.logical_ecu == logical_ecu {
            return Ok(probe.stlink_serial.clone());
        }
    }
    bail!(
        "no [[stlink]] row with logical_ecu='{}' in tools/bench/hardware-map.toml",
        logical_ecu
    );
}

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------

#[derive(Debug, Parser)]
#[command(
    name = "xtask",
    about = "Taktflow operator tooling — Phase 5 Line B scaffolding",
    version
)]
struct Cli {
    #[command(subcommand)]
    cmd: Cmd,
}

#[derive(Debug, Subcommand)]
enum Cmd {
    /// Flash the CVC firmware ELF via ST-LINK + STM32_Programmer_CLI.
    ///
    /// Default mode is --dry-run. Pass --execute to actually invoke the
    /// STM32_Programmer_CLI binary. In dry-run mode the command is
    /// printed to stdout exactly as it would be run, then xtask exits 0.
    FlashCvc {
        /// Dry-run (default). Prints the command without invoking it.
        #[arg(long, default_value_t = true)]
        dry_run: bool,
        /// Actually invoke STM32_Programmer_CLI. Overrides --dry-run.
        #[arg(long, default_value_t = false)]
        execute: bool,
        /// Override the ELF path. Defaults to build/cvc-arm/cvc_firmware.elf.
        #[arg(long)]
        elf: Option<PathBuf>,
        /// Override the hardware-map.toml path (test hook).
        #[arg(long, hide = true)]
        hardware_map: Option<PathBuf>,
    },
}

// ---------------------------------------------------------------------------
// Flashing
// ---------------------------------------------------------------------------

struct FlashPlan {
    programmer: String,
    args: Vec<String>,
}

impl FlashPlan {
    fn format(&self) -> String {
        let mut s = self.programmer.clone();
        for a in &self.args {
            s.push(' ');
            s.push_str(a);
        }
        s
    }
}

fn build_flash_plan(serial: &str, elf: &Path) -> FlashPlan {
    // Matches the STM32_Programmer_CLI invocation pattern from the Phase 5
    // Line B prompt:
    //   STM32_Programmer_CLI -c port=SWD sn=<serial> --connect-under-reset
    //                        -w <elf> -v --rst
    //
    // --connect-under-reset is per the Phase 5 rule "use under-reset if SWD
    // connection is flaky". We keep it on by default — it is safe and makes
    // cold-boot flashing reliable on unprogrammed G474 boards.
    //
    // -v = verify, --rst = reset target after write. We deliberately do NOT
    // pass --erase all: the Phase 5 prompt warns that full erase without
    // fallback bootloader can brick the board.
    let args = vec![
        "-c".to_string(),
        "port=SWD".to_string(),
        format!("sn={}", serial),
        "--connect-under-reset".to_string(),
        "-w".to_string(),
        elf.display().to_string(),
        "-v".to_string(),
        "--rst".to_string(),
    ];
    FlashPlan {
        programmer: "STM32_Programmer_CLI".to_string(),
        args,
    }
}

fn run_flash_cvc(
    dry_run: bool,
    execute: bool,
    elf_override: Option<PathBuf>,
    hardware_map_override: Option<PathBuf>,
) -> Result<()> {
    // --execute wins over the default --dry-run.
    let effective_dry_run = !execute;

    let cwd = std::env::current_dir().context("getting cwd")?;
    let repo_root = match &hardware_map_override {
        Some(p) => p.parent().and_then(|p| p.parent()).and_then(|p| p.parent())
            .map(|p| p.to_path_buf())
            .unwrap_or_else(|| cwd.clone()),
        None => find_repo_root(&cwd)?,
    };

    let map = if let Some(p) = hardware_map_override.as_ref() {
        let raw = std::fs::read_to_string(p)
            .with_context(|| format!("reading {}", p.display()))?;
        toml::from_str::<HardwareMap>(&raw)?
    } else {
        load_hardware_map(&repo_root)?
    };

    let serial = resolve_stlink_serial(&map, "cvc")?;

    let elf_path = elf_override
        .unwrap_or_else(|| repo_root.join("build/cvc-arm/cvc_firmware.elf"));

    let plan = build_flash_plan(&serial, &elf_path);
    let printed = plan.format();

    if effective_dry_run {
        eprintln!("[xtask flash-cvc] DRY-RUN (no hardware contacted)");
        eprintln!("[xtask flash-cvc] resolved ST-LINK serial: {}", serial);
        eprintln!("[xtask flash-cvc] resolved ELF path     : {}", elf_path.display());
        eprintln!(
            "[xtask flash-cvc] resolved via              : {}",
            repo_root.join("tools/bench/hardware-map.toml").display()
        );
        println!("{}", printed);
        eprintln!(
            "[xtask flash-cvc] To actually flash, rerun with --execute \
             after confirming the ELF path, serial, and target MCU family \
             (STM32G474) match the board under test. \
             Do NOT run --execute from an unattended agent."
        );
        // Suppress the _ warning variant by explicitly noting dry_run was set.
        let _ = dry_run;
        return Ok(());
    }

    if !elf_path.exists() {
        bail!(
            "ELF {} does not exist. Run `make -f firmware/platform/arm/Makefile.arm TARGET=cvc` first.",
            elf_path.display()
        );
    }

    eprintln!("[xtask flash-cvc] EXECUTE mode — invoking {}", plan.programmer);
    eprintln!("[xtask flash-cvc] {}", printed);
    let status = Command::new(&plan.programmer)
        .args(&plan.args)
        .status()
        .with_context(|| format!("spawning {}", plan.programmer))?;
    if !status.success() {
        bail!("{} exited with {}", plan.programmer, status);
    }
    Ok(())
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

fn main() -> Result<()> {
    let cli = Cli::parse();
    match cli.cmd {
        Cmd::FlashCvc {
            dry_run,
            execute,
            elf,
            hardware_map,
        } => run_flash_cvc(dry_run, execute, elf, hardware_map),
    }
}
