; Minimal Inno Setup script for testing
#define AppName "PrettySumatraPDF"
#define AppVersion "1.0.2"
#define AppPublisher "PrettySumatraPDF"
#define AppURL "https://github.com/JaviLendi/PrettySumatraPDF"
#define AppExeName "SumatraPDF-dll.exe"

[Setup]
AppId={{A4D09F07-2A7C-4A96-B3A4-395D7D1EFD41}
AppName={#AppName}
AppVerName={#AppName} {#AppVersion}
AppVersion={#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
Compression=lzma
SolidCompression=yes
CreateAppDir=yes
OutputDir=dist
OutputBaseFilename=PrettySumatraPDF-Setup-{#AppVersion}

[Files]
Source: "..\\..\\out\\rel64\\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\..\\out\\rel64\\libmupdf.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\\{#AppName}"; Filename: "{app}\\{#AppExeName}"; WorkingDir: "{app}"

[Run]
Filename: "{app}\\{#AppExeName}"; Flags: nowait postinstall skipifsilent
