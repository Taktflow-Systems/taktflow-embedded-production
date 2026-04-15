# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
#
# RZC POSIX ECU image — Phase 4 Line B D1
# Build context MUST be the repo root:
#   docker build -f firmware/ecu/rzc/Dockerfile -t taktflow/rzc-posix:latest .

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
RUN make -f firmware/platform/posix/Makefile.posix TARGET=rzc \
    && test -x build/rzc/rzc_posix

FROM ubuntu:24.04 AS runtime
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libc6 \
        iproute2 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /opt/taktflow/rzc
COPY --from=builder /build/build/rzc/rzc_posix /opt/taktflow/rzc/rzc_posix
ENV CAN_INTERFACE=vcan0
ENTRYPOINT ["/opt/taktflow/rzc/rzc_posix"]
