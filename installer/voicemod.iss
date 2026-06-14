; Inno Setup script for VoiceMod Toolbox
; Build the app first (Release), then compile this with ISCC.exe to produce dist\VoiceMod-Toolbox-Setup.exe

#define MyAppName "VoiceMod Toolbox"
#define MyAppVersion "0.1.0"
#define MyAppExeName "VoiceMod.exe"

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=VoiceMod
DefaultDirName={autopf}\VoiceMod Toolbox
DefaultGroupName=VoiceMod Toolbox
DisableProgramGroupPage=yes
; Per-user install -> no admin/UAC needed
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=VoiceMod-Toolbox-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Files]
Source: "..\build\Soundboard_artefacts\Release\Soundboard.exe"; DestDir: "{app}"; DestName: "{#MyAppExeName}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\BUILD.md";  DestDir: "{app}"; Flags: ignoreversion
; Optional: if you drop ffmpeg.exe next to this .iss, it gets bundled (for MP4 support)
Source: "ffmpeg.exe";   DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}";            Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}";  Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";      Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent
