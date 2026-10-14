[CmdletBinding()]
param(
    [ValidateSet('validate','pilot','collect')][string]$Mode = 'validate',
    [ValidateRange(0,1000)][int]$Blocks = 0,
    [ValidateRange(0,63)][int]$Cpu = 0,
    [uint32]$Seed = 20261012,
    [ValidateRange(1,1000)][int]$TargetMs = 20,
    [ValidateRange(1,100)][int]$WarmRsa = 3,
    [ValidateRange(1,4096)][int]$WarmFast = 1000,
    [string]$Batches = '',
    [switch]$ComputadorLivre
)
$ErrorActionPreference = 'Stop'
if ($Mode -ne 'validate' -and -not $ComputadorLivre) {
    throw 'Para medições, confirme que o computador está livre usando -ComputadorLivre. A validação funciona sem essa opção.'
}
if ($Mode -eq 'collect' -and [string]::IsNullOrWhiteSpace($Batches)) {
    throw 'A coleta exige lotes fixados após a análise do piloto (-Batches).'
}
$taskStamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
$taskId = [Guid]::NewGuid().ToString('N').Substring(0,8)
$taskRoot = Join-Path $PSScriptRoot 'execucoes'
New-Item -ItemType Directory -Path $taskRoot -Force | Out-Null
$taskOutput = Join-Path $taskRoot "$Mode-$taskStamp-$taskId"
$taskExe = Join-Path $PSScriptRoot 'build\benchmark.exe'
$taskLibrary = Join-Path $PSScriptRoot 'dependencias\openssl-msvc-3.5.5\bin'
$taskArguments = @('--mode',$Mode,'--out',$taskOutput,'--cpu',"$Cpu",'--seed',"$Seed",'--target-ms',"$TargetMs",'--warm-rsa',"$WarmRsa",'--warm-fast',"$WarmFast")
if ($Blocks -gt 0) { $taskArguments += @('--blocks',"$Blocks") }
if (-not [string]::IsNullOrWhiteSpace($Batches)) { $taskArguments += @('--batches',$Batches) }
$taskOriginalPath = $env:PATH
$taskStartUTC = [DateTime]::UtcNow.ToString('o')
$taskPowerBefore = (& powercfg /getactivescheme | Out-String).Trim()
$taskProcessesBefore = @(Get-Process | Select-Object ProcessName,Id,CPU,WorkingSet64)
try {
    $env:PATH = "$taskLibrary;$taskOriginalPath"
    & $taskExe @taskArguments
    $taskExit = $LASTEXITCODE
} finally {
    $env:PATH = $taskOriginalPath
}
$taskHashes = @('benchmark.c','compilar_benchmark.cmd','build\benchmark.exe','dependencias\openssl-msvc-3.5.5\bin\libcrypto-3-x64.dll') | ForEach-Object {
    $taskFile=Join-Path $PSScriptRoot $_
    [pscustomobject]@{Arquivo=$_;SHA256=(Get-FileHash -LiteralPath $taskFile).Hash}
}
$taskRun=[pscustomobject]@{
    Mode=$Mode;Args=$taskArguments;ExitCode=$taskExit;DataInicioUTC=$taskStartUTC;DataFinalUTC=[DateTime]::UtcNow.ToString('o')
    ComputadorLivreConfirmado=[bool]$ComputadorLivre
    PlanoEnergiaInicio=$taskPowerBefore;PlanoEnergiaFinal=((& powercfg /getactivescheme | Out-String).Trim());Hashes=@($taskHashes)
    ProcessosAntes=@($taskProcessesBefore)
    ProcessosDepois=@(Get-Process | Select-Object ProcessName,Id,CPU,WorkingSet64)
    AmbienteInicial='../../logs/ambiente-inicial.json'
    Observacao='Condições de fundo e térmicas devem ser registradas na sessão de medição; não foram inferidas pelo programa.'
}
$taskRecord = if (Test-Path -LiteralPath $taskOutput -PathType Container) {
    Join-Path $taskOutput 'run.json'
} else {
    Join-Path $taskRoot "$Mode-$taskStamp-$taskId-error.json"
}
$taskRun | ConvertTo-Json -Depth 5 | Set-Content $taskRecord -Encoding utf8
if ($taskExit -ne 0) { throw "Execução interrompida (código $taskExit). Preserve as saídas em $taskOutput." }
Write-Output "Saídas: $taskOutput"
