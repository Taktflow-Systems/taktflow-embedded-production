// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// SocketCAN async wrapper. Linux-only for real I/O; stubbed on other
// targets so the workspace still compiles cleanly on Windows and macOS
// developer machines. The real integration test lives in
// tests/integration_vcan.rs and is skipped on non-Linux hosts.

use core::time::Duration;

#[cfg(target_os = "linux")]
use crate::isotp::{CAN_DLC, Reassembler, encode};

#[derive(Debug, thiserror::Error)]
pub enum CanError {
    #[error("platform not supported (socketcan is linux-only)")]
    UnsupportedPlatform,
    #[error("io: {0}")]
    Io(String),
    #[error("timeout waiting for ISO-TP response on id 0x{id:x}")]
    Timeout { id: u32 },
    #[error("isotp: {0}")]
    IsoTp(#[from] crate::isotp::IsoTpError),
}

/// Opaque CAN interface handle. Real impl on linux; stub elsewhere.
#[cfg(target_os = "linux")]
pub struct CanInterface {
    name: String,
    sock: tokio::sync::Mutex<socketcan::CanSocket>,
}

#[cfg(not(target_os = "linux"))]
pub struct CanInterface {
    name: String,
}

impl CanInterface {
    /// Open a SocketCAN interface by name (e.g. `"can0"` or `"vcan0"`).
    ///
    /// # Errors
    /// Returns [`CanError::UnsupportedPlatform`] on non-Linux hosts, or
    /// [`CanError::Io`] if the interface is missing or cannot be opened.
    #[cfg(target_os = "linux")]
    pub fn open(name: &str) -> Result<Self, CanError> {
        use socketcan::Socket;
        let sock = socketcan::CanSocket::open(name).map_err(|e| CanError::Io(e.to_string()))?;
        sock.set_nonblocking(true)
            .map_err(|e| CanError::Io(e.to_string()))?;
        Ok(Self {
            name: name.to_string(),
            sock: tokio::sync::Mutex::new(sock),
        })
    }

    #[cfg(not(target_os = "linux"))]
    pub fn open(name: &str) -> Result<Self, CanError> {
        tracing::warn!(
            iface = name,
            "CanInterface::open called on non-linux host (stub)"
        );
        Err(CanError::UnsupportedPlatform)
    }

    #[must_use]
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Send an ISO-TP PDU on the given CAN ID. Blocks (async) until every
    /// frame is written.
    ///
    /// # Errors
    /// Returns [`CanError::Io`] or [`CanError::IsoTp`] on wire or encoder
    /// failures; [`CanError::UnsupportedPlatform`] off-Linux.
    #[cfg(target_os = "linux")]
    pub async fn send_isotp(&self, can_id: u32, payload: &[u8]) -> Result<(), CanError> {
        use socketcan::{EmbeddedFrame, StandardId};
        let frames = encode(payload)?;
        let id = StandardId::new(can_id as u16).ok_or_else(|| {
            CanError::Io(format!("can_id 0x{can_id:x} out of 11-bit standard range"))
        })?;
        let guard = self.sock.lock().await;
        for f in frames {
            let frame = socketcan::CanDataFrame::new(id, &f)
                .ok_or_else(|| CanError::Io("failed to build can data frame".into()))?;
            socketcan::Socket::write_frame(&*guard, &frame)
                .map_err(|e| CanError::Io(e.to_string()))?;
        }
        Ok(())
    }

    #[cfg(not(target_os = "linux"))]
    pub async fn send_isotp(&self, _can_id: u32, _payload: &[u8]) -> Result<(), CanError> {
        Err(CanError::UnsupportedPlatform)
    }

    /// Receive an ISO-TP PDU, filtering by `can_id`, up to `timeout`.
    ///
    /// # Errors
    /// Returns [`CanError::Timeout`] on deadline, [`CanError::Io`] on
    /// socket error, or [`CanError::IsoTp`] on reassembly failure.
    #[cfg(target_os = "linux")]
    pub async fn recv_isotp(&self, can_id: u32, timeout: Duration) -> Result<Vec<u8>, CanError> {
        use socketcan::{EmbeddedFrame, Frame};
        let deadline = tokio::time::Instant::now() + timeout;
        let mut r = Reassembler::new();
        loop {
            if tokio::time::Instant::now() >= deadline {
                return Err(CanError::Timeout { id: can_id });
            }
            let guard = self.sock.lock().await;
            let step = match socketcan::Socket::read_frame(&*guard) {
                Ok(frame) => {
                    if frame.raw_id() == can_id {
                        let data = frame.data();
                        if data.len() >= CAN_DLC {
                            Some(r.push(&data[..CAN_DLC])?)
                        } else {
                            let mut padded = [0u8; CAN_DLC];
                            let (head, _) = padded.split_at_mut(data.len());
                            head.copy_from_slice(data);
                            Some(r.push(&padded)?)
                        }
                    } else {
                        None
                    }
                }
                Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => None,
                Err(e) => return Err(CanError::Io(e.to_string())),
            };
            drop(guard);
            if let Some(Some(pdu)) = step {
                return Ok(pdu);
            }
            tokio::time::sleep(Duration::from_millis(2)).await;
        }
    }

    #[cfg(not(target_os = "linux"))]
    pub async fn recv_isotp(&self, can_id: u32, _timeout: Duration) -> Result<Vec<u8>, CanError> {
        Err(CanError::Timeout { id: can_id })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[cfg(not(target_os = "linux"))]
    fn open_on_non_linux_returns_unsupported() {
        match CanInterface::open("vcan0") {
            Err(CanError::UnsupportedPlatform) => {}
            Err(e) => panic!("expected UnsupportedPlatform, got error {e}"),
            Ok(_iface) => panic!("expected UnsupportedPlatform, got Ok"),
        }
    }
}
