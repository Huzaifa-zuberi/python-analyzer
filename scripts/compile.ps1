param(
    [switch]$Run
)

$Src = Join-Path $PSScriptRoot "..\src\python_analyzer.cpp"
$OutDir = Join-Path $PSScriptRoot "..\bin"
$Out = Join-Path $OutDir "python_analyzer.exe"

if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    Write-Host "[ERROR] g++ not found. Install MinGW or use WSL." -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

Write-Host "[BUILD] Compiling $Src ..." -ForegroundColor Cyan
g++ -std=c++11 -Wall -O2 -o $Out $Src

if ($LASTEXITCODE -eq 0) {
    Write-Host "[OK] Compiled to $Out" -ForegroundColor Green
    if ($Run) {
        Write-Host "[RUN] Starting analyzer..." -ForegroundColor Yellow
        & $Out
    }
} else {
    Write-Host "[FAIL] Compilation failed" -ForegroundColor Red
    exit 1
}
