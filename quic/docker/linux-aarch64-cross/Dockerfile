FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install host-arch build tools
RUN apt-get update && apt-get --assume-yes install \
    bison \
    ca-certificates \
    cmake \
    curl \
    flex \
    git \
    ninja-build \
    pkgconf \
    python3 \
    python3-pip \
    python3-venv \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    libglib2.0-dev \
    && rm --force --recursive /var/lib/apt/lists/*

# Add arm64 foreign architecture and configure ports.ubuntu.com for arm64 packages
COPY "quic/docker/linux-aarch64-cross/arm64.sources" \
    "/etc/apt/sources.list.d/arm64.sources"
RUN dpkg --add-architecture arm64 \
    && sed -i 's/^Types:/Architectures: amd64\nTypes:/' \
        /etc/apt/sources.list.d/ubuntu.sources

# Prevent arm64 Python from replacing the amd64 one (hard dep of libglib2.0-dev:arm64)
RUN apt-mark hold python3 python3-minimal python3.12 python3.12-minimal \
    libpython3.12-minimal libpython3-stdlib libpython3.12-stdlib

RUN apt-get update && apt-get --assume-yes --no-install-recommends install \
    libasound2-dev:arm64 \
    libattr1-dev:arm64 \
    libcap-ng-dev:arm64 \
    libepoxy-dev:arm64 \
    libfdt-dev:arm64 \
    libglib2.0-dev:arm64 \
    libpixman-1-dev:arm64 \
    libpng-dev:arm64 \
    libpulse-dev:arm64 \
    libslirp-dev:arm64 \
    libudev-dev:arm64 \
    libvirglrenderer-dev:arm64 \
    zlib1g-dev:arm64 \
    && rm --force --recursive /var/lib/apt/lists/*

# Install meson via pip
RUN pip3 install --break-system-packages meson

# Allow git operations from any directory
RUN git config --system --add safe.directory "*"

# Install Qualcomm CA certificate
RUN curl --fail --silent --show-error --location \
        --output /usr/local/share/ca-certificates/ssl_v4_cert.crt \
        https://pki.qualcomm.com/ssl_v4_cert.crt \
    && update-ca-certificates
