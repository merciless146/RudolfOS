# Join the parts of a Rudolf OS disc image and check it (Rudolf OS 1.2.4). GPL-2.0-or-later.
# Put this file in the folder with every rudolf-os-*.iso.part file and the .sha256, then in PowerShell:
#   powershell -ExecutionPolicy Bypass -File .\join.ps1
# (or right-click > Run with PowerShell). Writes rudolf-os-VERSION.iso next to the parts.
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
function Done($msg, $color) { Write-Host ''; Write-Host $msg -ForegroundColor $color; Write-Host ''; Read-Host 'Press Enter to close' | Out-Null; exit }
$sum = Get-ChildItem -Filter 'rudolf-os-*.iso.sha256' | Sort-Object Name | Select-Object -Last 1
if (-not $sum) { Done 'The .sha256 file is not in this folder. Download rudolf-os-VERSION.iso.sha256 too.' Red }
$line = (Get-Content -LiteralPath $sum.FullName -TotalCount 1).Trim()
$want = ($line -split '\s+')[0].ToLower(); $iso = ($line -split '\s+', 2)[1].Trim().TrimStart('*')
$parts = Get-ChildItem -Filter "$iso.part*" | Sort-Object Name
if (-not $parts) { Done "No $iso.part files in this folder ($PSScriptRoot)." Red }
# the parts are numbered 00, 01, ...: a gap means one is missing
for ($i = 0; $i -lt $parts.Count; $i++) {
  $n = '{0}.part{1:D2}' -f $iso, $i
  if ($parts[$i].Name -ne $n) { Done "Missing: $n. Download it from the release page, then run this again." Red }
}
$last = $parts[$parts.Count - 1].Length; $full = $parts[0].Length
for ($i = 0; $i -lt $parts.Count - 1; $i++) {
  if ($parts[$i].Length -ne $full) { Done ("{0} is incomplete ({1:N0} of {2:N0} bytes). Download it again." -f $parts[$i].Name, $parts[$i].Length, $full) Red }
}
Write-Host "Joining $($parts.Count) parts into $iso ..."
$out = [System.IO.File]::Create((Join-Path $PSScriptRoot $iso))
try { foreach ($p in $parts) { $in = [System.IO.File]::OpenRead($p.FullName); try { $in.CopyTo($out) } finally { $in.Close() } } } finally { $out.Close() }
Write-Host 'Checking the checksum ...'
$have = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $iso) -Algorithm SHA256).Hash.ToLower()
if ($have -ne $want) {
  Done "The image does not match its checksum. The last part ($($parts[$parts.Count-1].Name), $last bytes) or another part is damaged: download the parts again." Red
}
Done "OK: $iso is complete and matches its checksum. Write it to a USB stick with Rufus (choose 'DD Image mode'). Keep the .iso on this computer, not on the stick you write to." Green
