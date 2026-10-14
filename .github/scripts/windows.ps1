param([ValidateSet('bootstrap', 'runtime')][string]$Task)
$ErrorActionPreference = 'Stop'

switch ($Task) {
  'bootstrap' {
    Remove-Item "$env:FLUTTER_ROOT\bin\cache\engine-dart-sdk.stamp" -ErrorAction SilentlyContinue
    & "$env:FLUTTER_ROOT\bin\internal\update_dart_sdk.ps1"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ((dart --version 2>&1 | Out-String) -notmatch 'windows_arm64') { throw 'Expected native ARM64 Dart SDK' }
  }
  'runtime' {
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $install = & $vswhere -latest -products '*' -property installationPath
    $runtime = Get-ChildItem "$install/VC/Redist/MSVC/*/$env:ARCH/Microsoft.VC*.CRT" -Directory |
      Sort-Object { [version]$_.Parent.Parent.Name } -Descending | Select-Object -First 1
    if (!$runtime) { throw 'Visual C++ runtime was not found' }
    $bundle = "$env:APP_DIR/build/windows/$env:ARCH/runner/Release"
    Copy-Item "$($runtime.FullName)/*.dll" $bundle
    foreach ($dll in @('msvcp140.dll', 'vcruntime140.dll')) {
      if (!(Test-Path "$bundle/$dll")) { throw "Missing $dll" }
    }
  }
}
