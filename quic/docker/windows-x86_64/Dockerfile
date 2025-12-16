FROM docker-registry.qualcomm.com/qqvp/windows/base:x86_64

SHELL ["pwsh", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

ENV INSTALL_SCRIPT=install.sh

COPY ${INSTALL_SCRIPT} .

RUN C:\msys64\usr\bin\bash.exe -lc "./${env:INSTALL_SCRIPT}"; \
    Remove-Item ${env:INSTALL_SCRIPT}
