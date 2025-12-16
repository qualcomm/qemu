FROM docker-registry.qualcomm.com/qqvp/windows/base:x86_64

SHELL ["pwsh", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

ENV INSTALL_SCRIPT="install_win_deps.ps1"
ENV PACKAGE_DATABASE="mingw_packages.json"

COPY quic/${INSTALL_SCRIPT} .
COPY quic/${PACKAGE_DATABASE} .

ENV INSTALL_DIR="C:\Windows\System32"
RUN .\install_win_deps.ps1 -InstallDir ${env:INSTALL_DIR}; \
    Remove-Item ${env:INSTALL_SCRIPT}; \
    Remove-Item ${env:PACKAGE_DATABASE}

RUN C:\msys64\usr\bin\bash.exe -lc 'pacman --sync --disable-download-timeout --noconfirm mingw-w64-ucrt-x86_64-python'
