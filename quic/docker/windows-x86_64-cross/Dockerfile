FROM ubuntu:24.04

ARG INSTALL_SCRIPT="install.sh"
ARG PACKAGE_DB="mingw_packages.json"

ENV PKG_CONFIG_PATH="/mingw64/lib/pkgconfig"

COPY "quic/docker/windows-x86_64-cross/${INSTALL_SCRIPT}" .
COPY "quic/${PACKAGE_DB}" .

RUN "./${INSTALL_SCRIPT}" "${PACKAGE_DB}" \
    && rm --force "${INSTALL_SCRIPT}" \
    && apt-get clean \
    && rm --force --recursive /var/lib/apt/lists/*

RUN git config --system --add safe.directory "*" \
    && update-alternatives --set x86_64-w64-mingw32-gcc \
        /usr/bin/x86_64-w64-mingw32-gcc-posix \
    && update-alternatives --set x86_64-w64-mingw32-g++ \
        /usr/bin/x86_64-w64-mingw32-g++-posix

RUN curl --fail --silent --show-error --location \
        --output /usr/local/share/ca-certificates/ssl_v4_cert.crt \
        https://pki.qualcomm.com/ssl_v4_cert.crt \
    && update-ca-certificates
