$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path | Resolve-Path
$BuildContext = Join-Path $ScriptDir "../../.." | Resolve-Path
$Dockerfile = Join-Path $ScriptDir "Dockerfile"
$Tag = "docker-registry.qualcomm.com/qqvp/qemu:windows-x86_64-test"

docker build --file $Dockerfile --tag $Tag $BuildContext

