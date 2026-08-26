; Build the Windows installer from the portable package staging directory.
; Run package.bat first so that the source directory contains only release files.

#ifndef MyAppVersion
  #error MyAppVersion is required. Pass /DMyAppVersion=1.2.3 to ISCC.exe.
#endif

#define MyAppName "Claudaga"
#define MyAppPublisher "ZoidCTF"
#define MyAppURL "https://github.com/ZoidCTF/claudaga"
#define MyAppExeName "claudaga.exe"
#define MyAppSource "..\dist\claudaga-" + MyAppVersion

[Setup]
AppId={{7C1817F4-15C8-4F14-BC95-35D2139ADAB4}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=claudaga-{#MyAppVersion}-setup
; The same icon the executable carries, so the installer, the Start menu entry
; and Add/Remove Programs all show it rather than the generic one.
SetupIconFile=..\res\claudaga.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
; Copy the executable, runtime DLLs, documentation, and all audio assets.
Source: "{#MyAppSource}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Start {#MyAppName}"; Flags: nowait postinstall skipifsilent
