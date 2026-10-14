[CmdletBinding()]
param([switch]$ComputadorLivre)
$ErrorActionPreference = 'Stop'
if (-not $ComputadorLivre) {
    throw 'A coleta necessita de confirmação de computador livre. Use -ComputadorLivre após preparar as condições.'
}
$taskConfig = Get-Content (Join-Path $PSScriptRoot 'config_coleta.json') -Raw -Encoding utf8 | ConvertFrom-Json
foreach ($taskItem in $taskConfig.sha256.PSObject.Properties) {
    $taskFile = Join-Path $PSScriptRoot $taskItem.Name
    $taskHash = (Get-FileHash -LiteralPath $taskFile -Algorithm SHA256).Hash
    if ($taskHash -ne $taskItem.Value) { throw "O arquivo $($taskItem.Name) mudou após os pilotos. Revise a configuração antes de medir." }
}
& (Join-Path $PSScriptRoot 'executar_benchmark.ps1') -Mode collect -Blocks $taskConfig.blocks `
    -Cpu $taskConfig.logical_cpu -Seed $taskConfig.seed_order -WarmRsa $taskConfig.warmup_rsa_keygen `
    -WarmFast $taskConfig.warmup_other_operations -Batches ($taskConfig.batches -join ',') -ComputadorLivre
