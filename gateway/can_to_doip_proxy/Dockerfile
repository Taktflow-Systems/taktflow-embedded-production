# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
#
# Multi-arch build (linux/arm64 for the Raspberry Pi, linux/amd64 for
# developer laptops). Two-stage: rust 1.88 builder -> debian slim runtime.
# Invoke with:
#   docker buildx build --platform linux/arm64,linux/amd64 \
#     -t taktflow/can-to-doip-proxy:latest .

ARG RUST_VERSION=1.88
FROM rust:${RUST_VERSION}-bookworm AS builder
WORKDIR /build
COPY . .
RUN cargo build --release --bin opensovd-can-to-doip-proxy

FROM debian:bookworm-slim AS runtime
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /build/target/release/opensovd-can-to-doip-proxy \
     /usr/local/bin/opensovd-can-to-doip-proxy
COPY deploy/opensovd-proxy.toml /etc/opensovd/proxy.toml
EXPOSE 13400/tcp
EXPOSE 13400/udp
ENTRYPOINT ["/usr/local/bin/opensovd-can-to-doip-proxy"]
CMD ["--config-file", "/etc/opensovd/proxy.toml"]
