// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// D3 integration test: proves the proxy's FC-aware receiver drives a
// real multi-frame ISO-TP round-trip on vcan0.
//
// Unlike `integration_vcan.rs`, this test deliberately simulates a
// spec-compliant ECU: after sending the FirstFrame, the mock ECU
// BLOCKS until it observes a FlowControl (0x30 ...) on the request CAN
// ID before releasing its ConsecutiveFrames. A broken proxy would
// never send the FC and the mock ECU would stall — exactly the bug
// Phase 4 Line B D4 saw live on the Linux laptop.
//
// Gated on:
//   * Linux target (vcan0 only exists on Linux)
//   * PROXY_CAN_VCAN_TEST=1 env var, since vcan0 must be up already.
//     The D4 live runbook brings it up before invoking cargo test.
//
// On any other host (or when the gate is off) the test is a skip with
// a clear print message rather than a failure.

#![cfg(target_os = "linux")]

use std::time::Duration;

use proxy_can::isotp::{
    CAN_DLC, FC_BLOCK_SIZE, FC_PAD_BYTE, FC_PCI_CTS, FC_STMIN, encode as isotp_encode,
};
use proxy_can::socket::CanInterface;

fn vcan_gate() -> Option<&'static str> {
    if std::env::var("PROXY_CAN_VCAN_TEST").as_deref() != Ok("1") {
        return Some("PROXY_CAN_VCAN_TEST=1 not set");
    }
    None
}

#[tokio::test]
async fn vcan_multiframe_round_trip_requires_flow_control() {
    if let Some(reason) = vcan_gate() {
        eprintln!("SKIP vcan0 multi-frame test: {reason}");
        return;
    }

    // Three independent socket handles on vcan0:
    //   * `tester` plays the role of the proxy's DoIP-facing client:
    //     it calls recv_isotp_with_flow_control on 0x7E0/0x7E8.
    //   * `ecu_writer` sends the FirstFrame and, after observing the
    //     FlowControl, sends the ConsecutiveFrames.
    //   * `fc_observer` reads the raw CAN bus on 0x7E0 to prove the
    //     proxy actually emitted a FlowControl frame (not just
    //     accumulated bytes internally).
    let tester = CanInterface::open("vcan0").expect("tester open");
    let ecu_writer = CanInterface::open("vcan0").expect("ecu writer open");
    let fc_observer =
        open_raw_socketcan("vcan0").expect("fc observer open (needs raw socketcan handle)");

    // 20-byte positive UDS VIN response: 0x62 F1 90 + 17-byte ASCII.
    let mut payload = vec![0x62u8, 0xF1, 0x90];
    payload.extend_from_slice(b"TAKTFLWCVC0000017");
    let frames = isotp_encode(&payload).expect("isotp encode");
    assert!(frames.len() >= 3, "need multi-frame payload for this test");

    // ECU writer task: send FF only, then wait for FC from the proxy,
    // then send the CFs. Mirrors what a spec-compliant ECU does.
    let ecu_task = tokio::spawn(async move {
        use socketcan::{EmbeddedFrame, Frame, Socket, StandardId};
        // Use a raw handle under the hood via send_isotp_raw_frame.
        let ecu_raw = open_raw_socketcan("vcan0").expect("ecu raw");
        let resp_id = StandardId::new(0x7E8).expect("resp id");
        // FF.
        let ff = &frames[0];
        let ff_frame = socketcan::CanDataFrame::new(resp_id, ff).expect("ff build");
        ecu_raw.write_frame(&ff_frame).expect("ff tx");

        // Block until we observe a FlowControl (PCI 0x30) on 0x7E0.
        let deadline = std::time::Instant::now() + Duration::from_secs(3);
        loop {
            if std::time::Instant::now() >= deadline {
                panic!("ECU never saw FlowControl from proxy on 0x7E0");
            }
            match ecu_raw.read_frame() {
                Ok(frame) => {
                    if frame.raw_id() == 0x7E0 {
                        let data = frame.data();
                        if !data.is_empty() && (data[0] & 0xF0) == 0x30 {
                            break;
                        }
                    }
                }
                Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                    std::thread::sleep(Duration::from_millis(5));
                }
                Err(e) => panic!("ecu raw read: {e}"),
            }
        }

        // Drain the CFs at nominal cadence.
        for cf in &frames[1..] {
            let cf_frame = socketcan::CanDataFrame::new(resp_id, cf).expect("cf build");
            ecu_raw.write_frame(&cf_frame).expect("cf tx");
            std::thread::sleep(Duration::from_millis(1));
        }
        let _ = ecu_writer; // keep alive
    });

    // Small delay so the ECU task is listening before the tester
    // begins its receive loop.
    tokio::time::sleep(Duration::from_millis(30)).await;

    // Run the fc_observer loop in a background thread that captures
    // the first FC frame the proxy sends on 0x7E0.
    let (fc_tx, fc_rx) = std::sync::mpsc::channel::<[u8; CAN_DLC]>();
    std::thread::spawn(move || {
        use socketcan::{EmbeddedFrame, Frame, Socket};
        let deadline = std::time::Instant::now() + Duration::from_secs(5);
        loop {
            if std::time::Instant::now() >= deadline {
                return;
            }
            match fc_observer.read_frame() {
                Ok(frame) => {
                    if frame.raw_id() == 0x7E0 {
                        let data = frame.data();
                        if !data.is_empty() && (data[0] & 0xF0) == 0x30 {
                            let mut out = [0u8; CAN_DLC];
                            let take = core::cmp::min(data.len(), CAN_DLC);
                            out[..take].copy_from_slice(&data[..take]);
                            let _ = fc_tx.send(out);
                            return;
                        }
                    }
                }
                Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                    std::thread::sleep(Duration::from_millis(5));
                }
                Err(_) => return,
            }
        }
    });

    // The proxy receives on 0x7E8 and must emit FC on 0x7E0.
    let rx = tester
        .recv_isotp_with_flow_control(0x7E0, 0x7E8, Duration::from_secs(4))
        .await
        .expect("recv_isotp_with_flow_control");
    assert_eq!(rx, payload, "reassembled PDU mismatch");

    // Verify the observer saw a well-formed FC on the wire.
    let fc = fc_rx
        .recv_timeout(Duration::from_secs(1))
        .expect("no FC frame observed on 0x7E0");
    assert_eq!(fc[0], FC_PCI_CTS, "FC PCI should be 0x30 CTS");
    assert_eq!(fc[1], FC_BLOCK_SIZE, "FC BS should be 0");
    assert_eq!(fc[2], FC_STMIN, "FC STmin should be 0");
    for b in &fc[3..] {
        assert_eq!(*b, FC_PAD_BYTE);
    }

    ecu_task.await.expect("ecu task");
}

// Small helper to open a raw socketcan handle. The public CanInterface
// type owns a socket under a Mutex, which we cannot easily reuse from
// a `std::thread`-bound observer. The raw handle goes straight to the
// underlying crate.
fn open_raw_socketcan(iface: &str) -> std::io::Result<socketcan::CanSocket> {
    use socketcan::Socket;
    let sock = socketcan::CanSocket::open(iface)
        .map_err(|e| std::io::Error::other(e.to_string()))?;
    sock.set_nonblocking(true)?;
    Ok(sock)
}
