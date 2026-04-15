// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// opensovd-can-to-doip-proxy main binary. Reads a TOML config, opens a
// SocketCAN interface (linux-only; errors cleanly elsewhere), starts a
// DoIP TCP server, and wires the two together via a CanProxyHandler.

#![forbid(unsafe_code)]

use std::sync::Arc;

use anyhow::{Context, Result};
use async_trait::async_trait;
use clap::Parser;
use figment::{
    providers::{Env, Format, Toml},
    Figment,
};
use serde::Deserialize;
use tokio::net::TcpListener;
use tracing::{info, warn};

use proxy_can::socket::CanInterface;
use proxy_core::{
    should_respond_to_broadcast, translate_request, DiscoveryMode, RoutingTable,
};
use proxy_doip::server::{serve, DoipHandler};

#[derive(Debug, Parser)]
#[command(name = "opensovd-can-to-doip-proxy", about = "DoIP <-> CAN ISO-TP proxy")]
struct Cli {
    /// Path to the TOML config file.
    #[arg(long, default_value = "/etc/opensovd/proxy.toml")]
    config_file: String,

    /// Override listen address from the config (e.g. 0.0.0.0).
    #[arg(long)]
    listen_address: Option<String>,

    /// Override listen TCP port from the config (default 13400).
    #[arg(long)]
    listen_port: Option<u16>,

    /// Override the CAN interface name (e.g. can0, vcan0).
    #[arg(long)]
    can_interface: Option<String>,
}

#[derive(Debug, Deserialize)]
struct RawConfig {
    #[serde(default = "default_listen_address")]
    listen_address: String,
    #[serde(default = "default_listen_port")]
    listen_port: u16,
    #[serde(default = "default_can_iface")]
    can_interface: String,
    #[serde(default)]
    discovery: DiscoveryConfig,
    // The ECU list is reparsed via RoutingTable::from_toml_str below; this
    // field exists so figment does not complain about unknowns, and so we
    // can sanity-check that the table is non-empty before wiring up.
    #[serde(default)]
    ecu: Vec<toml::Value>,
}

#[derive(Debug, Default, Deserialize)]
struct DiscoveryConfig {
    #[serde(default)]
    mode: DiscoveryMode,
}

fn default_listen_address() -> String {
    "0.0.0.0".to_string()
}
fn default_listen_port() -> u16 {
    13400
}
fn default_can_iface() -> String {
    "can0".to_string()
}

struct CanProxyHandler {
    table: RoutingTable,
    can: Arc<CanInterface>,
}

#[async_trait]
impl DoipHandler for CanProxyHandler {
    async fn on_routing_activation(&self, source_address: u16) -> Option<u16> {
        // Accept any source; claim the first ECU logical address in the
        // table (CDA sends per-target activation, this is just a greeting).
        info!(%source_address, "routing activation accepted");
        self.table.all().next().map(|e| e.doip_logical_address)
    }

    async fn on_diagnostic_message(
        &self,
        source_address: u16,
        target_address: u16,
        uds: &[u8],
    ) -> Option<Vec<u8>> {
        let req = match translate_request(&self.table, source_address, target_address, uds) {
            Ok(req) => req,
            Err(err) => {
                warn!(?err, %target_address, "translate_request failed");
                return None;
            }
        };
        // Send the UDS bytes as ISO-TP on the CAN request ID.
        if let Err(err) = self.can.send_isotp(req.can_request_id, &req.uds_payload).await {
            warn!(?err, can_id = req.can_request_id, "send_isotp failed");
            return None;
        }
        // Wait up to 2s for a response on the CAN response ID.
        match self
            .can
            .recv_isotp(req.can_response_id, std::time::Duration::from_millis(2000))
            .await
        {
            Ok(rsp) => Some(rsp),
            Err(err) => {
                warn!(?err, can_id = req.can_response_id, "recv_isotp failed");
                None
            }
        }
    }
}

fn init_tracing() {
    tracing_subscriber::fmt()
        .json()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("info")),
        )
        .init();
}

#[tokio::main]
async fn main() -> Result<()> {
    init_tracing();

    let cli = Cli::parse();

    info!(config = %cli.config_file, "loading config");
    let raw: RawConfig = Figment::new()
        .merge(Toml::file(&cli.config_file))
        .merge(Env::prefixed("OPENSOVD_PROXY_"))
        .extract()
        .context("failed to parse proxy config")?;

    let config_source =
        std::fs::read_to_string(&cli.config_file).context("failed to read config file")?;
    let table = RoutingTable::from_toml_str(&config_source)
        .context("failed to parse routing table from config")?;
    if table.is_empty() {
        anyhow::bail!("routing table is empty — nothing to proxy");
    }

    let listen_addr = cli
        .listen_address
        .unwrap_or(raw.listen_address);
    let listen_port = cli.listen_port.unwrap_or(raw.listen_port);
    let can_iface = cli.can_interface.unwrap_or(raw.can_interface);
    let _ = raw.ecu; // consume to silence dead_code
    let will_respond_broadcast = should_respond_to_broadcast(raw.discovery.mode);
    info!(will_respond_broadcast, mode = ?raw.discovery.mode, "discovery mode");

    let can = Arc::new(
        CanInterface::open(&can_iface)
            .with_context(|| format!("failed to open CAN interface {can_iface}"))?,
    );
    info!(iface = %can.name(), "CAN interface opened");

    let listener = TcpListener::bind(format!("{listen_addr}:{listen_port}"))
        .await
        .with_context(|| format!("failed to bind DoIP listener on {listen_addr}:{listen_port}"))?;
    info!(listen = %format!("{listen_addr}:{listen_port}"), "DoIP listener bound");

    let handler: Arc<dyn DoipHandler> = Arc::new(CanProxyHandler { table, can });

    // Graceful shutdown on SIGTERM.
    let shutdown = async {
        #[cfg(unix)]
        {
            let mut sigterm = tokio::signal::unix::signal(
                tokio::signal::unix::SignalKind::terminate(),
            )
            .expect("sigterm handler");
            sigterm.recv().await;
        }
        #[cfg(not(unix))]
        {
            let _ = tokio::signal::ctrl_c().await;
        }
        info!("shutdown signal received");
    };

    let serve_fut = serve(listener, handler);
    tokio::select! {
        res = serve_fut => {
            if let Err(err) = res {
                warn!(?err, "server exited with error");
            }
        }
        () = shutdown => {
            info!("stopping proxy");
        }
    }

    Ok(())
}
