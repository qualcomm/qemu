FROM docker-registry.qualcomm.com/qqvp/msys2:x86_64

SHELL ["pwsh", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

ENV CHERE_INVOKING=yes
ENV INSTALL_SCRIPT=install.sh
ENV MSYSTEM=UCRT64

COPY ${INSTALL_SCRIPT} .

RUN C:\msys64\usr\bin\bash.exe -lc "./${env:INSTALL_SCRIPT}"; \
    Remove-Item ${env:INSTALL_SCRIPT}
