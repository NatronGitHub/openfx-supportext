# Compile every SupportExt source file with MSVC (cl.exe) against an openfx
# checkout. Run from a Visual Studio developer environment (cl on PATH).
# Usage: compile-msvc.ps1 <openfx dir> [-Std c++17] [-WarningsAsErrors]
# Errors fail; warnings are reported as GitHub annotations, and fail only with
# -WarningsAsErrors.
param(
    [Parameter(Mandatory = $true)][string]$Ofx,
    [string]$Std = "c++17",
    [switch]$WarningsAsErrors
)
$ErrorActionPreference = "Stop"
$Ofx = (Resolve-Path $Ofx).Path
$defs = @("-DOFX_EXTENSIONS_VEGAS", "-DOFX_EXTENSIONS_NUKE", "-DOFX_EXTENSIONS_NATRON",
          "-DOFX_EXTENSIONS_TUTTLE", "-DOFX_SUPPORTS_OPENGLRENDER")
$incs = @("-I.", "-Iglad", "-I$Ofx\include", "-I$Ofx\Support\include", "-I$Ofx\Support\Plugins\include")
$out = Join-Path ([System.IO.Path]::GetTempPath()) "supportext-msvc"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$status = 0
# glad/gladegl.cpp needs EGL/egl.h, which the Windows SDK doesn't provide
$sources = @(Get-ChildItem -Path . -Filter *.cpp) + @(Get-ChildItem -Path glad -Filter *.cpp | Where-Object { $_.Name -ne "gladegl.cpp" })
foreach ($f in $sources) {
    $rel = Resolve-Path -Relative $f.FullName
    $obj = Join-Path $out ($f.BaseName + ".obj")
    $log = & cl.exe -nologo -c -EHsc -W3 "-std:$Std" @defs @incs $f.FullName "-Fo$obj" 2>&1 | Out-String
    $ok = ($LASTEXITCODE -eq 0)
    $warn = $log -match "warning C\d+"
    if (-not $ok) {
        Write-Host "::error file=$rel::compilation failed"
        Write-Host $log
        $status = 1
    } elseif ($warn) {
        Write-Host "::warning file=$rel::compiler warnings"
        Write-Host $log
        if ($WarningsAsErrors) { $status = 1 }
    } else {
        Write-Host "ok $rel"
    }
}
exit $status
