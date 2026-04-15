# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
#
# CVC POSIX ECU image — Phase 4 Line B D1
#
# Two-stage build:
#   1. builder: gcc + make + git on ubuntu:24.04, builds build/cvc/cvc_posix
#      via firmware/platform/posix/Makefile.posix.
#   2. runtime: ubuntu:24.04 slim, copies the POSIX binary and the VIN
#      config file, runs it against CAN_INTERFACE (default vcan0 — the
#      compose network provides the vcan bridge).
#
# Build context MUST be the repo root:
#   docker build -f firmware/ecu/cvc/Dockerfile -t taktflow/cvc-posix:latest .

FROM ubuntu:24.04 AS builder
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        gcc \
        make \
        git \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /build
COPY . .
# Build via the existing POSIX makefile — single entrypoint for all ECUs.
RUN make -f firmware/platform/posix/Makefile.posix TARGET=cvc \
    && test -x build/cvc/cvc_posix

FROM ubuntu:24.04 AS runtime
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libc6 \
        iproute2 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /opt/taktflow/cvc
# Copy the built binary and the VIN config (config comes from the
# repo via the build context — no inline VIN literals anywhere).
COPY --from=builder /build/build/cvc/cvc_posix /opt/taktflow/cvc/cvc_posix
COPY --from=builder /build/firmware/ecu/cvc/cfg/cvc_identity.toml \
     /opt/taktflow/cvc/cfg/cvc_identity.toml

# CAN_INTERFACE is read by Can_Posix.c at runtime; vcan0 default matches
# the compose-managed vcan bridge. Override with `-e CAN_INTERFACE=...`.
ENV CAN_INTERFACE=vcan0
ENV CVC_IDENTITY_CONFIG=/opt/taktflow/cvc/cfg/cvc_identity.toml
ENTRYPOINT ["/opt/taktflow/cvc/cvc_posix"]
