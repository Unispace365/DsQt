<#
.SYNOPSIS
    Build and run DsQt tests, then generate an HTML report.
.DESCRIPTION
    Configures the library with tests enabled, builds in Debug mode,
    runs ctest, and produces a local HTML report you can open in a browser.
.PARAMETER BuildDir
    Build directory (default: Library/build/test)
.PARAMETER Open
    Automatically open the HTML report in the default browser.
.EXAMPLE
    .\run-tests.ps1
    .\run-tests.ps1 -Open
#>
param(
    [string]$BuildDir = "$PSScriptRoot\build\test",
    [switch]$Open
)

$ErrorActionPreference = "Stop"

Write-Host "`n=== DsQt Test Runner ===" -ForegroundColor Cyan

# --- Setup MSVC environment if not already active ---
if (-not $env:VSINSTALLDIR) {
    Write-Host "`n--- Setting up MSVC environment ---" -ForegroundColor Yellow
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -property installationPath
        $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcvars) {
            # Run vcvars64.bat and capture the resulting environment
            $cmd = "`"$vcvars`" >nul 2>&1 && set"
            $envVars = cmd /c $cmd
            foreach ($line in $envVars) {
                if ($line -match '^([^=]+)=(.*)$') {
                    [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
                }
            }
            Write-Host "MSVC environment loaded from: $vcvars" -ForegroundColor DarkGray
        }
        else {
            Write-Error "vcvars64.bat not found at: $vcvars"
            exit 1
        }
    }
    else {
        Write-Error "vswhere.exe not found. Install Visual Studio or run from Developer Command Prompt."
        exit 1
    }
}

# --- Configure ---
Write-Host "`n--- Configure ---" -ForegroundColor Yellow
cmake -S $PSScriptRoot -B $BuildDir `
    -G Ninja `
    -DCMAKE_BUILD_TYPE=Debug `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
    -DDSQT_BUILD_TESTS=ON

if ($LASTEXITCODE -ne 0) { Write-Error "Configure failed"; exit 1 }

# --- Build ---
Write-Host "`n--- Build ---" -ForegroundColor Yellow
cmake --build $BuildDir

if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

# --- Ensure Qt DLLs are on PATH ---
# The test executable needs Qt's bin directory to find DLLs at runtime.
$qtBinDirs = Get-ChildItem -Path "C:\Qt\*\msvc2022_64\bin" -Directory -ErrorAction SilentlyContinue | Sort-Object -Descending
if ($qtBinDirs) {
    $qtBin = $qtBinDirs[0].FullName
    if ($env:PATH -notlike "*$qtBin*") {
        $env:PATH = "$qtBin;$env:PATH"
        Write-Host "Added Qt to PATH: $qtBin" -ForegroundColor DarkGray
    }
}

# --- Run Tests ---
Write-Host "`n--- Running Tests ---" -ForegroundColor Yellow
ctest --test-dir $BuildDir --output-on-failure --output-junit "$BuildDir\test-results.xml"
$testExitCode = $LASTEXITCODE

# --- Generate HTML Report ---
# Collect all per-test JUnit XML files from the Tests subdirectories.
# Each test registers output as <test_name>.xml in its own build directory.
$junitFiles = @(Get-ChildItem -Path "$BuildDir\Tests" -Filter "*.xml" -Recurse -File -ErrorAction SilentlyContinue)

# Fall back to the ctest-level results if no per-test files found
if ($junitFiles.Count -eq 0) {
    $fallback = "$BuildDir\test-results.xml"
    if (Test-Path $fallback) {
        $junitFiles = @(Get-Item $fallback)
    }
}

$htmlReport = "$BuildDir\test-report.html"

if ($junitFiles) {
    Write-Host "`n--- Generating HTML Report ---" -ForegroundColor Yellow
    Write-Host "Found $($junitFiles.Count) test result file(s):" -ForegroundColor DarkGray
    foreach ($f in $junitFiles) { Write-Host "  $($f.FullName)" -ForegroundColor DarkGray }

    # Generate a self-contained HTML report from all JUnit XML files
    $totalTests = 0
    $totalFailures = 0
    $totalTime = 0.0
    $sections = ""

    foreach ($junitFile in $junitFiles) {
        $xml = [xml](Get-Content $junitFile.FullName)
        foreach ($suite in $xml.SelectNodes("//testsuite")) {
            $suiteName = $suite.name
            $suiteTests = 0
            $suiteFailures = 0
            $suiteTime = 0.0
            $rows = ""

            foreach ($tc in $suite.SelectNodes("testcase")) {
                $totalTests++
                $suiteTests++
                $name = $tc.name
                $time = $tc.time
                $totalTime += [double]$time
                $suiteTime += [double]$time
                $failure = $tc.SelectSingleNode("failure")
                if ($failure) {
                    $totalFailures++
                    $suiteFailures++
                    $status = "<span style='color:#e74c3c;font-weight:bold'>FAIL</span>"
                    $msg = [System.Web.HttpUtility]::HtmlEncode($failure.message) -replace "`n","<br>"
                }
                else {
                    $status = "<span style='color:#2ecc71;font-weight:bold'>PASS</span>"
                    $msg = ""
                }
                $rows += "<tr><td>$name</td><td>$status</td><td>${time}s</td><td style='font-size:0.85em'>$msg</td></tr>`n"
            }

            $suitePassed = $suiteTests - $suiteFailures
            $suiteIcon = if ($suiteFailures -eq 0) { "<span style='color:#2ecc71'>&#x2714;</span>" } else { "<span style='color:#e74c3c'>&#x2718;</span>" }
            $openAttr = if ($suiteFailures -gt 0) { " open" } else { " open" }
            $sections += @"
<details class="suite"$openAttr>
  <summary>$suiteIcon $suiteName <span class="suite-stats">$suitePassed / $suiteTests passed &mdash; $([math]::Round($suiteTime, 3))s</span></summary>
  <table>
  <tr><th>Test</th><th>Status</th><th>Time</th><th>Details</th></tr>
  $rows
  </table>
</details>

"@
        }
    }

    $passed = $totalTests - $totalFailures
    $summaryColor = if ($totalFailures -eq 0) { "#2ecc71" } else { "#e74c3c" }

    $html = @"
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>DsQt Test Report</title>
<style>
  body { font-family: 'Segoe UI', sans-serif; margin: 2em; background: #1a1a2e; color: #eee; }
  h1 { color: #16a085; }
  h2 { color: #e0e0e0; font-size: 1.1em; margin: 1.5em 0 0.5em; }
  .summary { font-size: 1.2em; margin: 1em 0; padding: 1em; background: #16213e; border-radius: 8px; }
  .summary .count { font-weight: bold; color: $summaryColor; font-size: 1.4em; }
  .suite { margin-bottom: 1.5em; padding: 1em; background: #16213e; border-radius: 8px; }
  .suite summary { cursor: pointer; font-size: 1.1em; font-weight: bold; color: #e0e0e0; list-style: none; }
  .suite summary::-webkit-details-marker { display: none; }
  .suite summary::before { content: '\25B6'; display: inline-block; margin-right: 0.5em; transition: transform 0.2s; font-size: 0.7em; }
  .suite[open] summary::before { transform: rotate(90deg); }
  .suite table { margin-top: 0.75em; }
  .suite-stats { font-size: 0.8em; color: #888; font-weight: normal; margin-left: 0.5em; }
  table { border-collapse: collapse; width: 100%; }
  th { background: #0f3460; padding: 10px 12px; text-align: left; }
  td { padding: 8px 12px; border-bottom: 1px solid #333; }
  tr:hover { background: #1a1a2e; }
</style>
</head>
<body>
<h1>DsQt Test Report</h1>
<div class="summary">
  <span class="count">$passed / $totalTests passed</span>
  &mdash; $totalFailures failures &mdash; $([math]::Round($totalTime, 2))s total
</div>
$sections
<p style="color:#666;margin-top:2em;font-size:0.85em">
Generated $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
</p>
</body>
</html>
"@
    $html | Out-File -Encoding utf8 $htmlReport

    Write-Host "`nReport: $htmlReport" -ForegroundColor Green

    if ($Open) {
        Start-Process $htmlReport
    }
}
else {
    Write-Host "`nNo JUnit XML found - skipping HTML report." -ForegroundColor DarkYellow
}

# --- Summary ---
Write-Host ""
if ($testExitCode -eq 0) {
    Write-Host "All tests passed!" -ForegroundColor Green
}
else {
    Write-Host "Some tests failed (exit code $testExitCode)" -ForegroundColor Red
}

exit $testExitCode
