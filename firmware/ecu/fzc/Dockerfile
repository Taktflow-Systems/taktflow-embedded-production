# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
#
# FZC POSIX ECU image — Phase 4 Line B D1
# Build context MUST be the repo root:
#   docker build -f firmware/ecu/fzc/Dockerfile -t taktflow/fzc-posix:latest .

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
RUN make -f firmware/platform/posix/Makefile.posix TARGET=fzc \
    && test -x build/fzc/fzc_posix

FROM ubuntu:24.04 AS runtime
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libc6 \
        iproute2 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /opt/taktflow/fzc
COPY --from=builder /build/build/fzc/fzc_posix /opt/taktflow/fzc/fzc_posix
ENV CAN_INTERFACE=vcan0
ENTRYPOINT ["/opt/taktflow/fzc/fzc_posix"]
