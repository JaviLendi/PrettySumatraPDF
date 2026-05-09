PrettySumatraPDF Installer — Build Instructions

This folder contains an Inno Setup script to create a Windows installer for PrettySumatraPDF.

Requirements
- Inno Setup (https://jrsoftware.org/isinfo.php)
- A built application executable (release build) located at `out\rel64\SumatraPDF-dll.exe` (adjust paths if necessary)

Quick build steps

1. Build the application (release):

```powershell
# Example using provided tasks (Windows):
# Build Release x64 SumatraPDF (SumatraPDF-dll or appropriate target)
cmd /c "scripts\\build-vs2022.cmd Release x64 SumatraPDF-dll"
```

2. Ensure the produced `SumatraPDF-dll.exe` and any required runtime files are present under `out\rel64\`, or adjust `PrettySumatraPDF.iss` `Source` paths accordingly. The script resolves paths relative to `scripts\installer`.

3. Compile the installer with Inno Setup:

```powershell
& 'C:\Users\javil\AppData\Local\Programs\Inno Setup 6\ISCC.exe' scripts\\installer\\PrettySumatraPDF.iss
```

4. The installer will be produced under `scripts\installer\dist\` with filename `PrettySumatraPDF-Setup-1.0.0.exe`.

Notes
- The current script packages runtime artifacts from `out\\rel64` (`*.exe`, `*.dll`, `*.dat`, `*.txt`) to keep installer size reasonable.
- The script also packages `prettysumatra\\webui` (homepage + toolbar HTML and vendor assets). If this folder is missing in the install dir, homepage/toolbar will not render.
- Customize the `[Files]` section in `PrettySumatraPDF.iss` to include extra runtime assets if needed.
- If you prefer NSIS, I can generate an NSIS script instead.
