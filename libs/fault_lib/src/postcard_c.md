# postcard_c — covered subset

This file documents the subset of the postcard 1.1.3 wire format
implemented in `postcard_c.c`. It exists to pin the contract between
the embedded C producer and the Rust `fault-sink-unix` consumer.

Source of truth on the Rust side: `postcard = "1.1.3"` in
`opensovd-core/Cargo.lock`.

## Chosen approach

Clean-room — Option B from Phase 3 Line B D2. We do not vendor the
upstream `postcard-c` reference because:

1. Offline execution environment.
2. `WireFaultRecord` uses a tightly bounded subset of postcard
   primitives; implementing just that subset is a few dozen lines of
   code and keeps the Fault Library shim dependency-free.
3. Every extension to the subset is documented here and must land in
   the same commit as the code change.

## Implemented primitives

| Postcard type   | C function                         | Encoding                                     |
|-----------------|------------------------------------|----------------------------------------------|
| `u8`            | `postcard_write_u8`                | 1 raw byte                                   |
| `bool`          | `postcard_write_bool`              | 1 raw byte (0x00 / 0x01)                     |
| `u32` (varint)  | `postcard_write_varint_u32`        | LEB128, 1 - 5 bytes                          |
| `u64` (varint)  | `postcard_write_varint_u64`        | LEB128, 1 - 10 bytes                         |
| `String`        | `postcard_write_string`            | varint(len) + raw UTF-8 bytes                |
| `Option<String>`| `postcard_write_option_string`     | 0x00 (None) / 0x01 + String body             |

## Not implemented (intentional)

- Signed varints (`i8`..`i64`) — `WireFaultRecord` has no signed
  integer fields. Zig-zag wrapping would add complexity that the
  phase does not need.
- `f32` / `f64` — not used by `WireFaultRecord`.
- `Vec<T>` of non-byte types — not used; `meta` is a single
  `Option<String>`.
- Tagged enums with data payloads — not used.
- Maps — not used.
- Nested structs beyond `WireFaultRecord` itself.

If a future `WireFaultRecord` field introduces any of the above, this
table MUST be extended in the same commit as the encoder change.

## LEB128 reference

The LEB128 unsigned encoding used here is:

```
while value >= 0x80:
    emit (value & 0x7F) | 0x80
    value >>= 7
emit value
```

Maximum byte count for a given width is `ceil(width / 7)`:

- `u32`: 5 bytes
- `u64`: 10 bytes

## WireFaultRecord layout

The Rust shadow struct declared in
`opensovd-core/crates/fault-sink-unix/src/codec.rs` is:

```rust
struct WireFaultRecord {
    component: String,
    id: u32,
    severity: u8,
    timestamp_ms: u64,
    meta_json: Option<String>,
}
```

Postcard serializes the fields in declaration order. The C encoder in
`wire_fault_record.c` MUST emit the fields in exactly that order:

1. `component` — `postcard_write_string`
2. `id` — `postcard_write_varint_u32`
3. `severity` — `postcard_write_u8`
4. `timestamp_ms` — `postcard_write_varint_u64`
5. `meta_json` — `postcard_write_option_string`

The entire encoded body is then prefixed with a 4-byte little-endian
`u32` length and sent through the Unix socket. The length prefix is
framing only and is NOT part of the postcard payload.
