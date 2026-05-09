PrettySumatraPDF Installer — Build Instructions

This folder contains an Inno Setup script to create a Windows installer for PrettySumatraPDF.

Requirements
- Inno Setup (https://jrsoftware.org/isinfo.php) — `ISCC.exe` must be on PATH
- A built application executable (release build) located at `out\rel64\SumatraPDF.exe` (adjust paths if necessary)

Quick build steps

1. Build the application (release):

```powershell
# Example using provided tasks (Windows):
# Build Release x64 SumatraPDF (SumatraPDF-dll or appropriate target)
cmd /c "scripts\\build-vs2022.cmd Release x64 SumatraPDF-dll"
```

2. Copy the produced `SumatraPDF.exe` and any required runtime files into the repository root or adjust `PrettySumatraPDF.iss` `Source` paths accordingly.

3. Compile the installer with Inno Setup:

```powershell
ISCC.exe scripts\\installer\\PrettySumatraPDF.iss
```

4. The installer will be produced under the `dist/` directory with filename `PrettySumatraPDF-Setup-1.0.0.exe`.

Notes
- Customize the `[Files]` section in `PrettySumatraPDF.iss` to include any extra files (DLLs, license file, README, etc.).
- If you prefer NSIS, I can generate an NSIS script instead.
