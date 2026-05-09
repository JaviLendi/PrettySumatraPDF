; Inno Setup script for PrettySumatraPDF
; Requires Inno Setup (ISCC.exe) to compile

#define AppName "PrettySumatraPDF"
#define AppVersion "1.0.0"
#define AppPublisher "PrettySumatraPDF"
#define AppExeName "SumatraPDF.exe"

[Setup]
AppName={#AppName}
AppVersion={#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
Compression=lzma
SolidCompression=yes
CreateAppDir=yes
OutputDir=dist
OutputBaseFilename=PrettySumatraPDF-Setup-{#AppVersion}
DisableDirPage=no
DisableProgramGroupPage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Adjust source paths if your build output differs
Source: "out\\rel64\\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "out\\rel64\\*"; DestDir: "{app}"; Flags: recursesubdirs
Source: "gfx\\SumatraPDF.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\\{#AppName}"; Filename: "{app}\\{#AppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\\SumatraPDF.ico"
Name: "{commondesktop}\\{#AppName}"; Filename: "{app}\\{#AppExeName}"; Tasks: desktopicon; IconFilename: "{app}\\SumatraPDF.ico"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Run]
Filename: "{app}\\{#AppExeName}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
