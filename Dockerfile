# Stage 1: Build C++ server
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    zlib1g-dev \
    libhiredis-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Run CMake and compile
RUN mkdir build && cd build && cmake .. && make -j$(nproc)

# Stage 2: Run C++ server
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    zlib1g \
    libhiredis0.14 \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/codepair_server /app/codepair_server

EXPOSE 9001
CMD ["./codepair_server"]
