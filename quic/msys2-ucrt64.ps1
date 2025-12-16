# This script is a wrapper around msys2 bash to simplify running scripts in msys2

$env:CHERE_INVOKING = "yes"
$env:MSYSTEM = "UCRT64"

& C:\msys64\usr\bin\bash -lc "$args"

exit $LASTEXITCODE
