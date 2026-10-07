# syntax=docker/dockerfile:1

ARG UBUNTU_VERSION=26.04
ARG LLVM_VERSION=21

FROM ubuntu:${UBUNTU_VERSION} AS base
ARG LLVM_VERSION
ENV DEBIAN_FRONTEND=noninteractive
# genmc invokes clang at runtime to compile its inputs, so it stays in the final image
RUN apt-get update \
 && apt-get install -y --no-install-recommends clang-${LLVM_VERSION} llvm-${LLVM_VERSION} \
 && rm -rf /var/lib/apt/lists/*

FROM base AS build
ARG LLVM_VERSION
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      cmake make g++ git llvm-${LLVM_VERSION}-dev libffi-dev zlib1g-dev \
      libedit-dev libzstd-dev libhwloc-dev libgoogle-perftools-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B /build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/usr/lib/llvm-${LLVM_VERSION} \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DGENMC_TCMALLOC=ON \
 && cmake --build /build -j"$(nproc)" \
 && DESTDIR=/install cmake --install /build

FROM base
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      libffi8 libhwloc15 libtcmalloc-minimal4t64 \
 && rm -rf /var/lib/apt/lists/*
COPY --from=build /install/usr/local/ /usr/local/
RUN genmc --version
WORKDIR /work
ENTRYPOINT ["genmc"]
