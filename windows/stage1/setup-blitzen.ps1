<#
.SYNOPSIS
  Sets up Blitzen Plus (Stage 1) on this PC: the llama.cpp engine, the Qwen2.5-Coder-7B model and the Blitzen
  chat page, then measures its speed and checks it.

.DESCRIPTION
  Everything goes into one folder (C:\Blitzen by default) and nothing is installed system-wide:
    engine\               llama.cpp (CUDA on an NVIDIA card, Vulkan on AMD or Intel, CPU otherwise)
    models\               the model file
    ui\                   the Blitzen chat page
    start-blitzen.cmd     starts Blitzen and opens it in your browser
    stop-blitzen.cmd      stops it and frees the graphics memory
    blitzen-system.txt    Blitzen's system prompt and context packs (for other chat programs)
    blitzen-check.txt     the results of the speed test and checks
  Every download is checked against its published SHA-256 before it is used. Running the script again
  resumes an interrupted download and skips what is already in place.

  The engine listens on 127.0.0.1 only, so no other device on your network can reach it.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File setup-blitzen.ps1
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File setup-blitzen.ps1 -Dir D:\Blitzen -StartWithWindows
#>
[CmdletBinding()]
param(
  [string]$Dir = 'C:\Blitzen',
  [ValidateSet('auto', 'cuda', 'vulkan', 'cpu')][string]$Engine = 'auto',
  [string]$LlamaTag = 'b11559',
  [string]$ModelRepo = 'Qwen/Qwen2.5-Coder-7B-Instruct-GGUF',
  [string]$ModelPattern = 'q4_k_m',
  [int]$Context = 16384,
  [int]$Port = 8099,
  [switch]$SkipBench,
  [switch]$SkipCheck,
  [switch]$StartWithWindows,
  [switch]$NoStart,
  [switch]$AllowUnverifiedEngine
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
trap { Write-Host ''; Write-Host ('Setup stopped: ' + $_.Exception.Message) -ForegroundColor Red; Write-Host 'Fix the problem and run the setup again; it carries on where it stopped.'; exit 1 }
try { [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12 } catch {}
$Here = $PSScriptRoot
$PassTps = 60
$VramLimitMB = 7680

function Say([string]$m) { Write-Host $m }
function Step([string]$m) { Write-Host ''; Write-Host ('== ' + $m) -ForegroundColor Cyan }
function Good([string]$m) { Write-Host ('   OK  ' + $m) -ForegroundColor Green }
function Warn([string]$m) { Write-Host ('   !   ' + $m) -ForegroundColor Yellow }
function Fail([string]$m) { throw $m }
function GB([double]$b) { '{0:N1} GB' -f ($b / 1GB) }

function Get-Json([string]$url) {
  return Invoke-RestMethod -Uri $url -UserAgent 'Blitzen-setup/1' -UseBasicParsing -TimeoutSec 60
}

function Get-Sha256([string]$path) { return (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant() }

# Quote one command-line argument the way Windows programs read it.
function Quote-Arg([string]$a) {
  if ($a -eq '') { return '""' }
  if ($a -notmatch '[\s"]') { return $a }
  return '"' + ($a -replace '(\\*)"', '$1$1\"' -replace '(\\+)$', '$1$1') + '"'
}
function Join-Args([string[]]$argv) { return (($argv | ForEach-Object { Quote-Arg $_ }) -join ' ') }

# Runs a program, waits, and returns its exit code; output goes to two files.
function Run-Native([string]$exe, [string[]]$argv, [string]$outFile, [string]$errFile) {
  $p = Start-Process -FilePath $exe -ArgumentList (Join-Args $argv) -NoNewWindow -Wait -PassThru `
       -RedirectStandardOutput $outFile -RedirectStandardError $errFile
  return $p.ExitCode
}

# Downloads $url to $dest and checks size and SHA-256. Resumes a partial file; skips a complete, correct one.
function Get-VerifiedFile([string]$url, [string]$dest, [long]$size, [string]$sha) {
  $sha = $sha.ToLowerInvariant()
  $name = Split-Path -Leaf $dest
  for ($attempt = 1; $attempt -le 2; $attempt++) {
    if (Test-Path -LiteralPath $dest) {
      $have = (Get-Item -LiteralPath $dest).Length
      if ($size -gt 0 -and $have -gt $size) { Remove-Item -LiteralPath $dest -Force }
      elseif ($size -le 0 -or $have -eq $size) {
        Say ('   checking ' + $name + ' ...')
        if ((Get-Sha256 $dest) -eq $sha) { Good ($name + ' (SHA-256 matches)'); return }
        Warn ($name + ' is damaged or different; downloading it again')
        Remove-Item -LiteralPath $dest -Force
      }
    }
    Say ('   downloading ' + $name + $(if ($size -gt 0) { ' (' + (GB $size) + ')' } else { '' }))
    & curl.exe -L --fail --retry 5 --retry-delay 3 -C - -o $dest $url
    if ($LASTEXITCODE -ne 0) {
      if ($attempt -lt 2) { Warn ('download stopped (curl exit ' + $LASTEXITCODE + '); trying again'); continue }
      Fail ('Could not download ' + $name + ' (curl exit ' + $LASTEXITCODE + '). Run the setup again to resume.')
    }
    break
  }
  if (-not (Test-Path -LiteralPath $dest)) { Fail ('Could not download ' + $name) }
  $got = Get-Sha256 $dest
  if ($got -ne $sha) {
    Remove-Item -LiteralPath $dest -Force
    Fail ($name + ': SHA-256 ' + $got + ' does not match the published ' + $sha + '. The file was deleted; run the setup again.')
  }
  Good ($name + ' (SHA-256 matches)')
}

function Expand-Zip([string]$zip, [string]$to) {
  New-Item -ItemType Directory -Force -Path $to | Out-Null
  $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
  if (Test-Path $tar) {
    & $tar -xf $zip -C $to
    if ($LASTEXITCODE -eq 0) { return }
  }
  Expand-Archive -LiteralPath $zip -DestinationPath $to -Force
}

function Stop-OurEngine {
  $mine = @(Get-Process -Name 'llama-server' -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($Dir, [StringComparison]::OrdinalIgnoreCase) })
  foreach ($p in $mine) { Say ('   stopping the running Blitzen engine (process ' + $p.Id + ')'); Stop-Process -Id $p.Id -Force }
  if ($mine.Count) { Start-Sleep -Seconds 2 }
}

function Write-Utf8([string]$path, [string]$text) {
  [IO.File]::WriteAllText($path, $text, (New-Object Text.UTF8Encoding $false))
}

function Gpu-UsedMB {
  try { $l = & nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits; return [int](@($l)[0].Trim()) } catch { return -1 }
}

# ------------------------------------------------------------------------------------------------------------
Write-Host ''
Write-Host '  Blitzen Plus for Windows - Stage 1 setup' -ForegroundColor Cyan
Write-Host ('  Folder: ' + $Dir)

# ---------- this PC ----------
Step 'This PC'
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
$ram = (Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory
$cores = [int]$cpu.NumberOfCores
Say ('   Processor: ' + $cpu.Name.Trim() + ' (' + $cores + ' cores, ' + $cpu.NumberOfLogicalProcessors + ' threads)')
Say ('   Memory:    ' + (GB $ram))

$nv = $null
if (Get-Command nvidia-smi -ErrorAction SilentlyContinue) {
  try {
    $l = @(& nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv,noheader,nounits)[0]
    $f = $l -split ',\s*'
    $nv = [pscustomobject]@{ Name = $f[0].Trim(); VramMB = [int]$f[1]; Driver = $f[2].Trim() }
  } catch {}
}
$others = @(Get-CimInstance Win32_VideoController | Where-Object { $_.Name -match 'Radeon|AMD|Intel\(R\) Arc|Arc\(TM\)' } | ForEach-Object { $_.Name })
if ($nv) { Say ('   Graphics:  ' + $nv.Name + ', ' + ('{0:N1} GB' -f ($nv.VramMB / 1024)) + ', driver ' + $nv.Driver) }
foreach ($o in $others) { Say ('   Graphics:  ' + $o) }

if ($Engine -eq 'auto') {
  if ($nv -and $nv.VramMB -ge 6000) { $Engine = 'cuda' }
  elseif ($others.Count) { $Engine = 'vulkan' }
  else { $Engine = 'cpu' }
}
if ($Engine -eq 'cuda' -and -not $nv) { Fail 'The CUDA engine needs an NVIDIA card and its driver (nvidia-smi was not found). Use -Engine vulkan or -Engine cpu.' }
$Ngl = $(if ($Engine -eq 'cpu') { 0 } else { 99 })
$Threads = [Math]::Max(1, [Math]::Min($cores, 16))
Good ('engine: ' + $Engine + ', all model layers on ' + $(if ($Ngl) { 'the graphics card' } else { 'the processor' }) + ', ' + $Threads + ' threads')
if ($nv -and $Engine -eq 'cuda' -and $nv.VramMB -lt 9000) { Warn 'Less than 9 GB of graphics memory: if Blitzen fails to start, lower the context with -Context 8192.' }

# ---------- disk space ----------
Step 'Disk space'
$root = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Dir))
$free = (New-Object IO.DriveInfo $root).AvailableFreeSpace
Say ('   ' + $root + ' has ' + (GB $free) + ' free')
$haveModel = Test-Path (Join-Path $Dir 'models')
if ($free -lt 7GB -and -not $haveModel) { Fail ('Blitzen Plus needs about 7 GB free on ' + $root + ' (about 5 GB once set up). Free some space, or put it on another drive: -Dir D:\Blitzen') }
Good 'enough space'
foreach ($d in @($Dir, "$Dir\engine", "$Dir\models", "$Dir\downloads", "$Dir\logs")) { New-Item -ItemType Directory -Force -Path $d | Out-Null }

# ---------- engine ----------
Step ('Engine: llama.cpp ' + $LlamaTag + ' (' + $Engine + ')')
$rel = $null
try { $rel = Get-Json ('https://api.github.com/repos/ggml-org/llama.cpp/releases/tags/' + $LlamaTag) }
catch {
  Warn ('llama.cpp release ' + $LlamaTag + ' was not found (' + $_.Exception.Message + '); using the latest release')
  $rel = Get-Json 'https://api.github.com/repos/ggml-org/llama.cpp/releases/latest'
}
$tag = $rel.tag_name
$assets = @($rel.assets)
function Find-Asset([string]$regex) {
  $m = @($assets | Where-Object { $_.name -match $regex })
  if (-not $m.Count) { return $null }
  return $m[0]
}
$want = @()
if ($Engine -eq 'cuda') {
  $cuda = @($assets | Where-Object { $_.name -match '^llama-.+-bin-win-cuda-(\d+)\.(\d+)-x64\.zip$' } | ForEach-Object {
      $null = $_.name -match '-cuda-(\d+)\.(\d+)-x64'
      [pscustomobject]@{ A = $_; Ver = $Matches[1] + '.' + $Matches[2]; Major = [int]$Matches[1]; Minor = [int]$Matches[2] } })
  # Prefer CUDA 12 (works with every recent driver), the newest 12.x; otherwise the newest build there is.
  $pick = @($cuda | Where-Object { $_.Major -eq 12 } | Sort-Object Minor -Descending)
  if (-not $pick.Count) { $pick = @($cuda | Sort-Object Major, Minor -Descending) }
  if (-not $pick.Count) { Fail ('llama.cpp ' + $tag + ' has no Windows CUDA build. Use -Engine vulkan.') }
  $want += $pick[0].A
  $rt = Find-Asset ('^cudart-llama-bin-win-cuda-' + [regex]::Escape($pick[0].Ver) + '-x64\.zip$')
  if (-not $rt) { Fail ('llama.cpp ' + $tag + ' has no CUDA ' + $pick[0].Ver + ' runtime zip.') }
  $want += $rt
} elseif ($Engine -eq 'vulkan') {
  $a = Find-Asset '^llama-.+-bin-win-vulkan-x64\.zip$'; if (-not $a) { Fail ('llama.cpp ' + $tag + ' has no Windows Vulkan build.') }; $want += $a
} else {
  $a = Find-Asset '^llama-.+-bin-win-cpu-x64\.zip$'
  if (-not $a) { $a = Find-Asset '^llama-.+-bin-win-avx2-x64\.zip$' }
  if (-not $a) { Fail ('llama.cpp ' + $tag + ' has no Windows CPU build.') }
  $want += $a
}
$stamp = Join-Path $Dir 'engine\blitzen-engine.txt'
$stampText = ($tag + ' ' + $Engine + ' ' + (($want | ForEach-Object { $_.name }) -join ' '))
$current = $(if (Test-Path $stamp) { (Get-Content -LiteralPath $stamp -Raw).Trim() } else { '' })
$haveExe = @(Get-ChildItem -LiteralPath "$Dir\engine" -Recurse -Filter 'llama-server.exe' -ErrorAction SilentlyContinue).Count -gt 0
if ($current -ne $stampText -or -not $haveExe) {
  Stop-OurEngine
  foreach ($a in $want) {
    $sha = ''
    if ($a.digest -and ([string]$a.digest).StartsWith('sha256:')) { $sha = ([string]$a.digest).Substring(7) }
    $zip = Join-Path "$Dir\downloads" $a.name
    if ($sha) { Get-VerifiedFile $a.browser_download_url $zip ([long]$a.size) $sha }
    elseif ($AllowUnverifiedEngine) {
      Warn ($a.name + ' has no published checksum; downloading it unverified because of -AllowUnverifiedEngine')
      & curl.exe -L --fail --retry 5 -C - -o $zip $a.browser_download_url
      if ($LASTEXITCODE -ne 0) { Fail ('Could not download ' + $a.name) }
    } else { Fail ($a.name + ' has no published SHA-256 on GitHub, so it cannot be checked. Run again with -AllowUnverifiedEngine to accept it anyway.') }
  }
  if (Test-Path "$Dir\engine") { Remove-Item -Recurse -Force "$Dir\engine" }
  Expand-Zip (Join-Path "$Dir\downloads" $want[0].name) "$Dir\engine"
  $srv = Get-ChildItem -LiteralPath "$Dir\engine" -Recurse -Filter 'llama-server.exe' | Select-Object -First 1
  if (-not $srv) { Fail 'llama-server.exe was not in the engine zip.' }
  for ($i = 1; $i -lt $want.Count; $i++) { Expand-Zip (Join-Path "$Dir\downloads" $want[$i].name) $srv.DirectoryName }
  Write-Utf8 $stamp $stampText
  foreach ($a in $want) { Remove-Item -LiteralPath (Join-Path "$Dir\downloads" $a.name) -Force -ErrorAction SilentlyContinue }
  Good ('llama.cpp ' + $tag + ' installed')
} else { Good ('llama.cpp ' + $tag + ' is already installed') }
$Bin = (Get-ChildItem -LiteralPath "$Dir\engine" -Recurse -Filter 'llama-server.exe' | Select-Object -First 1).DirectoryName
$Server = Join-Path $Bin 'llama-server.exe'
$Bench = Join-Path $Bin 'llama-bench.exe'

# ---------- model ----------
Step ('Model: ' + $ModelRepo + ' (' + $ModelPattern + ')')
$tree = @(Get-Json ('https://huggingface.co/api/models/' + $ModelRepo + '/tree/main'))
$files = @()
foreach ($e in $tree) {
  if ($e.type -eq 'file' -and $e.path -match '\.gguf$' -and $e.path -like ('*' + $ModelPattern + '*')) { $files += $e }
}
if (-not $files.Count) { Fail ('No .gguf file matching ' + $ModelPattern + ' in ' + $ModelRepo) }
# A large model may be split into parts (name-00001-of-00002.gguf): take every part of the first matching set.
$set = ($files | Sort-Object path | Select-Object -First 1).path -replace '-\d{5}-of-\d{5}\.gguf$', ''
$parts = @($files | Where-Object { ($_.path -replace '-\d{5}-of-\d{5}\.gguf$', '') -eq $set -and ($_.path -eq $set -or $_.path -match '-\d{5}-of-\d{5}\.gguf$') } | Sort-Object path)
function Hf-Url([string]$path) { return 'https://huggingface.co/' + $ModelRepo + '/resolve/main/' + (($path -split '/' | ForEach-Object { [Uri]::EscapeDataString($_) }) -join '/') }
$total = 0
$shas = @{}
foreach ($p in $parts) {
  $sha = $(if ($p.lfs -and $p.lfs.oid) { [string]$p.lfs.oid } else { '' })
  if (-not $sha) {
    # Hugging Face also gives a large file's SHA-256 in the x-linked-etag header of its download address.
    $h = @(& curl.exe -sI (Hf-Url $p.path))
    foreach ($line in $h) { if ($line -match '^x-linked-etag:\s*"?([0-9a-fA-F]{64})"?') { $sha = $Matches[1] } }
  }
  if (-not $sha) { Fail ($p.path + ' has no published SHA-256 on Hugging Face, so it cannot be checked.') }
  $shas[$p.path] = $sha.ToLowerInvariant()
  $total += [long]$p.size
}
Say ('   ' + (($parts | ForEach-Object { $_.path }) -join ', '))
Say ('   ' + (GB $total) + ' in all')
foreach ($p in $parts) {
  $leaf = Split-Path -Leaf $p.path
  Get-VerifiedFile (Hf-Url $p.path) (Join-Path "$Dir\models" $leaf) ([long]$p.size) $shas[$p.path]
}
$Model = Join-Path "$Dir\models" (Split-Path -Leaf $parts[0].path)
$modelInfo = "repository: $ModelRepo`r`n" + (($parts | ForEach-Object { 'file: ' + (Split-Path -Leaf $_.path) + '  size: ' + $_.size + '  sha256: ' + $shas[$_.path] }) -join "`r`n") + "`r`nlicence: see https://huggingface.co/$ModelRepo`r`n"
Write-Utf8 (Join-Path "$Dir\models" 'blitzen-model.txt') $modelInfo

# ---------- chat page, prompt, launchers ----------
Step 'Blitzen chat page and launchers'
if (-not (Test-Path (Join-Path $Here 'ui\index.html'))) { Fail ('The ui folder is missing next to this script (' + $Here + '). Extract the whole zip first.') }
Stop-OurEngine
$sameDir = ([IO.Path]::GetFullPath($Here).TrimEnd('\')) -ieq ([IO.Path]::GetFullPath($Dir).TrimEnd('\'))
if (-not $sameDir) {
  if (Test-Path "$Dir\ui") { Remove-Item -Recurse -Force "$Dir\ui" }
  Copy-Item -Recurse -LiteralPath (Join-Path $Here 'ui') -Destination "$Dir\ui"
  foreach ($f in @('blitzen-system.txt', 'README.txt', 'blitzen.ico', 'setup-blitzen.ps1', 'Setup Blitzen.cmd')) {
    if (Test-Path -LiteralPath (Join-Path $Here $f)) { Copy-Item -Force -LiteralPath (Join-Path $Here $f) -Destination (Join-Path $Dir $f) }
  }
}

$EngineArgs = @('-m', $Model, '-ngl', "$Ngl", '-c', "$Context", '-fa', 'on', '--cache-type-k', 'q8_0', '--cache-type-v', 'q8_0',
  '-t', "$Threads", '--host', '127.0.0.1', '--port', "$Port", '--jinja', '--path', "$Dir\ui", '--alias', 'blitzen-plus')
$EngineLine = Join-Args $EngineArgs
$start = @"
@echo off
chcp 65001 >nul
rem Blitzen Plus (Stage 1): starts the engine for this PC only (127.0.0.1) and opens the chat page.
rem   start-blitzen.cmd          start and open the page
rem   start-blitzen.cmd /quiet   start without opening the page (used when starting with Windows)
tasklist /fi "imagename eq llama-server.exe" 2>nul | find /i "llama-server.exe" >nul && goto ready
start "Blitzen engine" /min "$Server" $EngineLine
set /a n=0
:wait
curl.exe -s -f -o nul http://127.0.0.1:$Port/health && goto ready
set /a n+=1
if %n% geq 120 (echo Blitzen did not start; see the "Blitzen engine" window. & pause & exit /b 1)
timeout /t 1 /nobreak >nul
goto wait
:ready
if /i "%~1"=="/quiet" exit /b 0
start "" http://127.0.0.1:$Port/
"@
Write-Utf8 "$Dir\start-blitzen.cmd" ($start -replace "`r?`n", "`r`n")
$stop = "@echo off`r`nrem Stops Blitzen and frees the graphics memory.`r`ntaskkill /im llama-server.exe /f >nul 2>&1 && echo Blitzen stopped. || echo Blitzen was not running.`r`ntimeout /t 2 >nul`r`n"
Write-Utf8 "$Dir\stop-blitzen.cmd" $stop

$sh = New-Object -ComObject WScript.Shell
function New-Shortcut([string]$lnk, [string]$target, [string]$argsLine, [string]$desc, [int]$style) {
  $s = $sh.CreateShortcut($lnk); $s.TargetPath = $target; $s.Arguments = $argsLine; $s.WorkingDirectory = $Dir
  $s.Description = $desc; $s.WindowStyle = $style; if (Test-Path "$Dir\blitzen.ico") { $s.IconLocation = "$Dir\blitzen.ico,0" }; $s.Save()
}
$programs = [Environment]::GetFolderPath('Programs')
New-Shortcut (Join-Path $programs 'Blitzen.lnk') "$Dir\start-blitzen.cmd" '' 'Blitzen, the offline coding assistant' 7
New-Shortcut (Join-Path $programs 'Stop Blitzen.lnk') "$Dir\stop-blitzen.cmd" '' 'Stop Blitzen and free the graphics memory' 7
Good 'Start menu: Blitzen, Stop Blitzen'
$startup = Join-Path ([Environment]::GetFolderPath('Startup')) 'Blitzen.lnk'
if ($StartWithWindows) { New-Shortcut $startup "$Dir\start-blitzen.cmd" '/quiet' 'Start Blitzen with Windows' 7; Good 'starts with Windows' }
elseif (Test-Path $startup) { Remove-Item -Force $startup }

# ---------- speed test ----------
$report = New-Object System.Collections.Generic.List[string]
$report.Add('Blitzen Plus check, ' + (Get-Date -Format 'yyyy-MM-dd HH:mm'))
$report.Add('PC: ' + $cpu.Name.Trim() + ', ' + (GB $ram) + $(if ($nv) { ', ' + $nv.Name + ' (driver ' + $nv.Driver + ')' } else { '' }))
$report.Add('Engine: llama.cpp ' + $tag + ' ' + $Engine + '; model: ' + (Split-Path -Leaf $Model))
$results = @()
function Result([string]$name, [bool]$pass, [string]$detail) {
  $script:results += [pscustomobject]@{ Check = $name; Pass = $pass; Detail = $detail }
  if ($pass) { Good ($name + ': ' + $detail) } else { Warn ($name + ': ' + $detail) }
}

if (-not $SkipBench -and (Test-Path $Bench)) {
  Step 'Speed test (llama-bench, about a minute)'
  Stop-OurEngine
  $bo = Join-Path "$Dir\logs" 'bench.json'; $be = Join-Path "$Dir\logs" 'bench.log'
  $base = @('-m', $Model, '-ngl', "$Ngl", '-t', "$Threads", '-p', '512', '-n', '128', '-r', '3', '-o', 'json')
  $code = Run-Native $Bench ($base + @('-fa', 'on', '-ctk', 'q8_0', '-ctv', 'q8_0')) $bo $be
  if ($code -ne 0) { $code = Run-Native $Bench ($base + @('-fa', '1')) $bo $be }
  if ($code -ne 0) { $code = Run-Native $Bench $base $bo $be }
  if ($code -eq 0) {
    $rows = Get-Content -LiteralPath $bo -Raw | ConvertFrom-Json
    $pp = 0; $tg = 0
    foreach ($r in $rows) { if ([int]$r.n_gen -gt 0) { $tg = [double]$r.avg_ts } elseif ([int]$r.n_prompt -gt 0) { $pp = [double]$r.avg_ts } }
    Say ('   reading (pp512): ' + ('{0:N0}' -f $pp) + ' tokens/s')
    Result 'Writing speed (tg128)' ($tg -ge $PassTps) (('{0:N1}' -f $tg) + ' tokens/s (pass mark ' + $PassTps + ')')
  } else { Result 'Writing speed (tg128)' $false ('llama-bench failed; see ' + $be) }
}

# ---------- checks ----------
if (-not $SkipCheck) {
  Step 'Checks'
  Stop-OurEngine
  $before = Gpu-UsedMB
  $lo = Join-Path "$Dir\logs" 'engine-check.log'; $le = Join-Path "$Dir\logs" 'engine-check.err.log'
  $t0 = Get-Date
  $p = Start-Process -FilePath $Server -ArgumentList $EngineLine -NoNewWindow -PassThru -RedirectStandardOutput $lo -RedirectStandardError $le
  $ready = $false
  for ($i = 0; $i -lt 180 -and -not $p.HasExited; $i++) {
    try { if ((Invoke-WebRequest -UseBasicParsing -TimeoutSec 2 ("http://127.0.0.1:$Port/health")).StatusCode -eq 200) { $ready = $true; break } } catch {}
    Start-Sleep -Milliseconds 500
  }
  $loadS = ((Get-Date) - $t0).TotalSeconds
  if (-not $ready) {
    Result 'Engine starts' $false ('it did not start; the end of ' + $le + ':')
    if (Test-Path $le) { Get-Content -LiteralPath $le -Tail 15 | ForEach-Object { Say ('      ' + $_) } }
  } else {
    Result 'Engine starts' $true ('ready in ' + ('{0:N1}' -f $loadS) + ' s from cold')
    try {
      $warm = @{ messages = @(@{ role = 'user'; content = 'Hi' }); max_tokens = 4 } | ConvertTo-Json -Depth 5
      $null = Invoke-RestMethod -Method Post -Uri ("http://127.0.0.1:$Port/v1/chat/completions") -ContentType 'application/json' -Body $warm -UseBasicParsing -TimeoutSec 120
      $coldS = ((Get-Date) - $t0).TotalSeconds
      $body = @{ messages = @(@{ role = 'user'; content = 'Write a JavaScript function that returns the sum of an array of numbers. Code only.' }); max_tokens = 160; temperature = 0.2 } | ConvertTo-Json -Depth 5
      $r = Invoke-RestMethod -Method Post -Uri ("http://127.0.0.1:$Port/v1/chat/completions") -ContentType 'application/json' -Body $body -UseBasicParsing -TimeoutSec 120
      if (-not $r.timings) { Fail 'the engine did not report its timings' }
      $firstS = ([double]$r.timings.prompt_ms + 1000.0 / [Math]::Max(1.0, [double]$r.timings.predicted_per_second)) / 1000.0
      Result 'First words (model loaded)' ($firstS -le 1.0) (('{0:N2}' -f $firstS) + ' s (limit 1 s)')
      Result 'First words from cold' ($coldS -le 10.0) (('{0:N1}' -f $coldS) + ' s from starting the engine to its first answer (limit 10 s)')
      Result 'Answer speed' ([double]$r.timings.predicted_per_second -ge $PassTps) (('{0:N1}' -f [double]$r.timings.predicted_per_second) + ' tokens/s in a real answer')
      $txt = [string]$r.choices[0].message.content
      Say '   Blitzen answered:'
      ($txt -split "`n" | Select-Object -First 12) | ForEach-Object { Say ('      ' + $_) }
    } catch { Result 'First answer' $false $_.Exception.Message }
    $after = Gpu-UsedMB
    if ($before -ge 0 -and $after -ge 0) {
      $use = $after - $before
      Result 'Graphics memory' ($use -lt $VramLimitMB) (('{0:N1}' -f ($use / 1024)) + ' GB used by Blitzen with the full ' + $Context + '-token context reserved (limit 7.5 GB); ' + ('{0:N1}' -f ($after / 1024)) + ' GB in use in all')
    }
    # Not reachable from the network: it must listen on 127.0.0.1 only, and this PC's own network address must refuse it.
    $listen = @(Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue | ForEach-Object { $_.LocalAddress } | Sort-Object -Unique)
    $ips = @(Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object { $_.IPAddress -ne '127.0.0.1' -and $_.IPAddress -notlike '169.254.*' } | ForEach-Object { $_.IPAddress })
    $open = @()
    foreach ($ip in $ips) {
      $c = New-Object Net.Sockets.TcpClient
      try { $ar = $c.BeginConnect($ip, $Port, $null, $null); if ($ar.AsyncWaitHandle.WaitOne(1500) -and $c.Connected) { $open += $ip } } catch {} finally { $c.Close() }
    }
    $onlyLocal = ($listen.Count -gt 0) -and (@($listen | Where-Object { $_ -ne '127.0.0.1' }).Count -eq 0) -and ($open.Count -eq 0)
    Result 'Not reachable from the network' $onlyLocal ('listens on ' + ($listen -join ', ') + $(if ($ips.Count) { '; refused on ' + (($ips | Where-Object { $open -notcontains $_ }) -join ', ') } else { '' }) + $(if ($open.Count) { '; OPEN on ' + ($open -join ', ') } else { '' }))
  }
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
}

foreach ($x in $results) { $report.Add(('{0}  {1}: {2}' -f $(if ($x.Pass) { 'PASS' } else { 'FAIL' }), $x.Check, $x.Detail)) }
$report.Add('Not checked here: run-and-fix (try a JavaScript or BlockLua answer on the page; Python and C++ come with the Stage 2 app), the 160 Blitzen tasks, unloading for games (Stage 2).')
Write-Utf8 "$Dir\blitzen-check.txt" (($report -join "`r`n") + "`r`n")

Step 'Done'
$failed = @($results | Where-Object { -not $_.Pass })
if ($results.Count -and -not $failed.Count) { Good ('all ' + $results.Count + ' checks passed; results in ' + $Dir + '\blitzen-check.txt') }
elseif ($failed.Count) { Warn ($failed.Count.ToString() + ' of ' + $results.Count + ' checks did not pass; see ' + $Dir + '\blitzen-check.txt') }
Say ('   Start Blitzen any time from the Start menu (Blitzen) or ' + $Dir + '\start-blitzen.cmd')
Say ('   It uses the graphics card while running; Stop Blitzen frees it (for games).')
if (-not $NoStart) { Start-Process -FilePath "$Dir\start-blitzen.cmd" -WorkingDirectory $Dir -WindowStyle Minimized }
