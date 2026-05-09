; Inno Setup script for PrettySumatraPDF
; Requires Inno Setup (ISCC.exe) to compile

#define AppName "PrettySumatraPDF"
#define AppVersion "1.0.0"
#define AppPublisher "PrettySumatraPDF"
#define AppURL "https://github.com/JaviLendi/PrettySumatraPDF"
#define AppExeName "SumatraPDF-dll.exe"

[Setup]
AppId={{A4D09F07-2A7C-4A96-B3A4-395D7D1EFD41}
AppName={#AppName}
AppVerName={#AppName} {#AppVersion}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma
SolidCompression=yes
CreateAppDir=yes
OutputDir=dist
OutputBaseFilename=PrettySumatraPDF-Setup-{#AppVersion}
SetupIconFile=..\..\gfx\SumatraPDF.ico
UninstallDisplayIcon={app}\SumatraPDF.ico
LicenseFile=..\..\COPYING
WizardStyle=modern
DisableDirPage=no
DisableProgramGroupPage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Paths are relative to this script file (scripts\installer)
Source: "..\\..\\out\\rel64\\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\..\\out\\rel64\\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\..\\out\\rel64\\*.dat"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\..\\out\\rel64\\*.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\..\\prettysumatra\\webui\\*"; DestDir: "{app}\\prettysumatra\\webui"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\\..\\gfx\\SumatraPDF.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\..\\COPYING"; DestDir: "{app}"; DestName: "COPYING.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\\{#AppName}"; Filename: "{app}\\{#AppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\\SumatraPDF.ico"
Name: "{commondesktop}\\{#AppName}"; Filename: "{app}\\{#AppExeName}"; Tasks: desktopicon; IconFilename: "{app}\\SumatraPDF.ico"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Run]
Filename: "{app}\\{#AppExeName}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
