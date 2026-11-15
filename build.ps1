param(
    [Parameter(Mandatory=$true)][string]$KenshiLib,
    [Parameter(Mandatory=$true)][string]$Boost,
    [string]$MsvcRoot = "${env:ProgramFiles(x86)}\Microsoft Visual Studio 10.0\VC",
    [string]$WindowsSdk = "$env:ProgramFiles\Microsoft SDKs\Windows\v7.1"
)
$ErrorActionPreference = 'Stop'
$KenshiLib = (Resolve-Path -LiteralPath $KenshiLib).Path
$Boost = (Resolve-Path -LiteralPath $Boost).Path
$oldInclude = $env:INCLUDE
$oldLib = $env:LIB
Push-Location $PSScriptRoot
try {
    $env:INCLUDE = "$MsvcRoot\include;$WindowsSdk\Include;$KenshiLib\Include;$Boost"
    $env:LIB = "$MsvcRoot\lib\amd64;$WindowsSdk\Lib\x64;$KenshiLib\Libraries;$Boost\stage\lib"
    New-Item -ItemType Directory -Force development | Out-Null
    & "$MsvcRoot\bin\amd64\lib.exe" /nologo /def:MyGUIImports.def /machine:x64 /out:development/MyGUIImports.lib
    if ($LASTEXITCODE -ne 0) { throw 'MyGUI import library generation failed' }
    & "$MsvcRoot\bin\amd64\cl.exe" /nologo /LD /MD /EHsc /O2 /DNDEBUG /wd4819 BowSkills.cpp BowRuntime.cpp BowLocalization.cpp /Fodevelopment/ /link /OUT:development/BowSkills.dll KenshiLib.lib OgreMain_x64.lib development/MyGUIImports.lib user32.lib
    if ($LASTEXITCODE -ne 0) { throw 'BowSkills build failed' }
} finally {
    Pop-Location
    $env:INCLUDE = $oldInclude
    $env:LIB = $oldLib
}
