<#
.SYNOPSIS
    Install Manimate on Windows.

.DESCRIPTION
    Downloads the newest build for this machine and installs it. By default it
    runs the real installer; -Portable unpacks the .zip instead, which needs no
    administrator rights and leaves nothing in the registry.

.PARAMETER Channel
    Which builds to install: dev (default) or stable.

.PARAMETER Version
    Install one exact release tag instead of the newest build.

.PARAMETER Portable
    Unpack the .zip to a local folder instead of running the installer.

.PARAMETER Prefix
    Where -Portable installs to. Default: %LOCALAPPDATA%\Programs\Manimate

.PARAMETER Silent
    Run the installer without its interface.

.PARAMETER List
    Print the asset that would be installed, then exit.

.EXAMPLE
    irm https://raw.githubusercontent.com/Hexadecimall/Manimation/main/install.ps1 | iex

.EXAMPLE
    .\install.ps1 -Portable
#>

[CmdletBinding()]
param(
    [ValidateSet('dev', 'stable')]
    [string] $Channel = 'dev',
    [string] $Version,
    [switch] $Portable,
    [string] $Prefix = (Join-Path $env:LOCALAPPDATA 'Programs\Manimate'),
    [switch] $Silent,
    [switch] $List
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Repo = 'Hexadecimall/Manimation'
$Api = "https://api.github.com/repos/$Repo/releases"

function Write-Step { param([string] $Message) Write-Host "==> $Message" -ForegroundColor Cyan }
function Write-Note { param([string] $Message) Write-Host "    $Message" -ForegroundColor DarkGray }
function Write-Done { param([string] $Message) Write-Host "OK  $Message" -ForegroundColor Green }

function Get-Architecture {
    switch ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture) {
        'X64'   { 'x86_64' }
        'Arm64' { 'arm64' }
        default { throw "Unsupported architecture: $_" }
    }
}

function Get-Releases {
    $headers = @{ 'Accept' = 'application/vnd.github+json'; 'User-Agent' = 'manimate-installer' }
    if ($Version) {
        try {
            return @(Invoke-RestMethod -Uri "$Api/tags/$Version" -Headers $headers)
        } catch {
            throw "No release tagged $Version."
        }
    }
    return @(Invoke-RestMethod -Uri "${Api}?per_page=60" -Headers $headers)
}

# Assets are named Manimate-<version>-windows-<arch>.<ext>, so matching on the
# asset name works whatever the release tag happens to be called.
function Select-Asset {
    param([object[]] $Releases, [string] $Arch)

    foreach ($release in $Releases) {
        if ($Channel -eq 'stable' -and $release.prerelease) { continue }

        $matching = @($release.assets | Where-Object { $_.name -like "*-windows-$Arch.*" })
        if (-not $matching) { continue }

        $wanted = if ($Portable) { '.zip' } else { '.exe' }
        $asset = $matching | Where-Object { $_.name.EndsWith($wanted) } | Select-Object -First 1
        if (-not $asset) { $asset = $matching | Select-Object -First 1 }
        if ($asset) { return $asset }
    }
    return $null
}

$arch = Get-Architecture
Write-Step "Looking for a windows $arch build"

$asset = Select-Asset -Releases (Get-Releases) -Arch $arch
if (-not $asset) { throw "No windows $arch build published yet." }

Write-Note $asset.name

if ($List) {
    $asset.browser_download_url
    exit 0
}

$temp = Join-Path ([System.IO.Path]::GetTempPath()) ("manimate-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp -Force | Out-Null
$download = Join-Path $temp $asset.name

try {
    Write-Step 'Downloading'
    $previous = $ProgressPreference
    $ProgressPreference = 'SilentlyContinue'   # the bar makes this several times slower
    try {
        Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $download -UseBasicParsing
    } finally {
        $ProgressPreference = $previous
    }

    if ($download.EndsWith('.zip')) {
        Write-Step "Installing to $Prefix"

        if (Test-Path $Prefix) { Remove-Item $Prefix -Recurse -Force }
        New-Item -ItemType Directory -Path $Prefix -Force | Out-Null
        Expand-Archive -Path $download -DestinationPath $temp -Force

        # The zip holds a single versioned folder; lift its contents out of it.
        $root = Get-ChildItem -Path $temp -Directory | Select-Object -First 1
        if ($root) {
            Copy-Item -Path (Join-Path $root.FullName '*') -Destination $Prefix -Recurse -Force
        } else {
            Copy-Item -Path (Join-Path $temp '*') -Destination $Prefix -Recurse -Force -Exclude $asset.name
        }

        $exe = Get-ChildItem -Path $Prefix -Filter 'Manimate.exe' -Recurse |
               Select-Object -First 1
        if (-not $exe) { throw "No Manimate.exe inside $($asset.name)." }

        $startMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs'
        $shortcut = Join-Path $startMenu 'Manimate.lnk'
        $shell = New-Object -ComObject WScript.Shell
        $link = $shell.CreateShortcut($shortcut)
        $link.TargetPath = $exe.FullName
        $link.WorkingDirectory = Split-Path $exe.FullName -Parent
        $link.IconLocation = $exe.FullName
        $link.Description = 'Design Manim scenes on a canvas and a timeline'
        $link.Save()

        Write-Done "Installed $($exe.FullName)"
        Write-Note 'Added to the Start Menu.'
    } else {
        Write-Step 'Running the installer'
        $arguments = if ($Silent) { '/S' } else { '' }
        $process = Start-Process -FilePath $download -ArgumentList $arguments -Wait -PassThru
        if ($process.ExitCode -ne 0) {
            throw "The installer exited with code $($process.ExitCode)."
        }
        Write-Done 'Installed Manimate'
    }
} finally {
    Remove-Item $temp -Recurse -Force -ErrorAction SilentlyContinue
}
