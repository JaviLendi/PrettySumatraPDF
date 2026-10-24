# Build and Installer Guide

This guide builds PrettySumatraPDF in Release x64 and creates the installer with Inno Setup. Run all commands from the repository root, not from `scripts` or `scripts\installer`.

## Requirements

- Visual Studio 2022 or Build Tools 2022 with MSBuild and the x64 C++ tools.
- Inno Setup 6.
- PowerShell.

## Build the Application

From the repository root, locate the VS 2022 installation with `vswhere` and build the `SumatraPDF-dll` target:

```powershell
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -version '[17.0,18.0)' -property installationPath
if (-not $vsRoot) { throw 'Visual Studio 2022 installation not found.' }

$msbuild = Join-Path $vsRoot 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild '.\vs2022\SumatraPDF.sln' /m /t:SumatraPDF-dll /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw 'Release x64 build failed.' }

Test-Path '.\out\rel64\SumatraPDF-dll.exe'
```

The executable should be created at `out\rel64\SumatraPDF-dll.exe`. The repository's `scripts\build-vs2022.cmd` specifically looks for VS 2022 Build Tools; the command above also supports the Community, Professional, and Enterprise editions.

## Build the Installer

Find `ISCC.exe` in the usual Inno Setup locations and compile the script:

```powershell
$isccCandidates = @(
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
)
$iscc = $isccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw 'ISCC.exe not found. Install Inno Setup 6.' }

& $iscc '.\scripts\installer\PrettySumatraPDF.iss'
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
```

The installer is created at `scripts\installer\dist\PrettySumatraPDF-Setup-1.0.2.exe`. The `.iss` script packages the executable, DLLs, and data files from `out\rel64`, as well as `prettysumatra\webui`.

## Run the Installer

To launch the generated setup:

```powershell
& '.\scripts\installer\dist\PrettySumatraPDF-Setup-1.0.2.exe'
```

The installer requests administrator privileges. To build a different version, update `AppVersion` in `PrettySumatraPDF.iss` and adjust the setup filename shown above.

## Troubleshooting

- `Visual Studio 2022 installation not found`: verify that `vswhere.exe` and the MSBuild/C++ components are installed. Open a new terminal after installing Visual Studio.
- `LNK1201` while writing a PDB: close processes that may be using the build artifacts. If the repository is in a synchronized folder such as OneDrive, try building from a local, non-synchronized copy.
- Missing installer files: confirm that the Release build completed and that `out\rel64` and `prettysumatra\webui` contain the required files.