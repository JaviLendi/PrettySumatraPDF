; Inno Setup script for PrettySumatraPDF
; Requires Inno Setup (ISCC.exe) to compile

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

; Allow using a shortened webui source directory set via environment variable
; Set SHORT_WEBUI_SRC to a short path (e.g. C:\\ps_webui_temp) before running ISCC
#define WEBUI_SRC GetEnv('SHORT_WEBUI_SRC')
#if WEBUI_SRC == ""
#define WEBUI_SRC "..\\..\\prettysumatra\\webui"
#endif
Source: "{#WEBUI_SRC}\\home.html"; DestDir: "{app}\\prettysumatra\\webui"; Flags: ignoreversion
Source: "{#WEBUI_SRC}\\toolbar.html"; DestDir: "{app}\\prettysumatra\\webui"; Flags: ignoreversion
Source: "{#WEBUI_SRC}\\toolbar.css"; DestDir: "{app}\\prettysumatra\\webui"; Flags: ignoreversion
Source: "{#WEBUI_SRC}\\assets\\*"; DestDir: "{app}\\prettysumatra\\webui\\assets"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#WEBUI_SRC}\\vendor\\inter\\inter.css"; DestDir: "{app}\\prettysumatra\\webui\\vendor\\inter"; Flags: ignoreversion
Source: "{#WEBUI_SRC}\\vendor\\inter\\fonts\\UcCO3FwrK3iLTeHuS_nVMrMxCp50SjIw2boKoduKmMEVuLyfMZg.woff2"; DestDir: "{app}\\prettysumatra\\webui\\vendor\\inter\\fonts"; Flags: ignoreversion
Source: "{#WEBUI_SRC}\\vendor\\inter\\fonts\\UcCO3FwrK3iLTeHuS_nVMrMxCp50SjIw2boKoduKmMEVuI6fMZg.woff2"; DestDir: "{app}\\prettysumatra\\webui\\vendor\\inter\\fonts"; Flags: ignoreversion
Source: "{#WEBUI_SRC}\\vendor\\inter\\fonts\\UcCO3FwrK3iLTeHuS_nVMrMxCp50SjIw2boKoduKmMEVuGKYMZg.woff2"; DestDir: "{app}\\prettysumatra\\webui\\vendor\\inter\\fonts"; Flags: ignoreversion
Source: "{#WEBUI_SRC}\\vendor\\inter\\fonts\\UcCO3FwrK3iLTeHuS_nVMrMxCp50SjIw2boKoduKmMEVuFuYMZg.woff2"; DestDir: "{app}\\prettysumatra\\webui\\vendor\\inter\\fonts"; Flags: ignoreversion
Source: "{#WEBUI_SRC}\\vendor\\inter\\fonts\\UcCO3FwrK3iLTeHuS_nVMrMxCp50SjIw2boKoduKmMEVuDyYMZg.woff2"; DestDir: "{app}\\prettysumatra\\webui\\vendor\\inter\\fonts"; Flags: ignoreversion
Source: "{#WEBUI_SRC}\\vendor\\material-icons\\material-icons.css"; DestDir: "{app}\\prettysumatra\\webui\\vendor\\material-icons"; Flags: ignoreversion
Source: "{#WEBUI_SRC}\\vendor\\material-icons\\fonts\\flUhRq6tzZclQEJ-Vdg-IuiaDsNZ.ttf"; DestDir: "{app}\\prettysumatra\\webui\\vendor\\material-icons\\fonts"; Flags: ignoreversion
Source: "{#WEBUI_SRC}\\vendor\\material-icons\\fonts\\material-symbols-rounded-subset.woff2"; DestDir: "{app}\\prettysumatra\\webui\\vendor\\material-icons\\fonts"; Flags: ignoreversion

Source: "..\\..\\gfx\\SumatraPDF.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\..\\COPYING"; DestDir: "{app}"; DestName: "COPYING.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\\{#AppName}"; Filename: "{app}\\{#AppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\\SumatraPDF.ico"
Name: "{commondesktop}\\{#AppName}"; Filename: "{app}\\{#AppExeName}"; Tasks: desktopicon; IconFilename: "{app}\\SumatraPDF.ico"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Run]
Filename: "{app}\\{#AppExeName}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
