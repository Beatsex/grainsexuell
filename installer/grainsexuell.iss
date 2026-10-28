; Windows installer for grainsexuell (Inno Setup 6). Build with:
;   ISCC.exe installer\grainsexuell.iss
; after a Release build. Installs the VST3 to the standard VST3 folder and the standalone app with a Start menu entry.

#define AppVersion "0.3.0"
#define Artefacts "..\build\grainsexuell_artefacts\Release"

[Setup]
; the AppId identifies the app for upgrades and uninstall: never change it
AppId={{8db66198-2466-4457-b0f0-16ac1af46a9e}
AppName=grainsexuell
AppVersion={#AppVersion}
AppVerName=grainsexuell {#AppVersion} beta
AppPublisher=beatsexuell
DefaultDirName={autopf}\beatsexuell\grainsexuell
DefaultGroupName=grainsexuell
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; the VST3 folder under Common Files needs admin rights
PrivilegesRequired=admin
OutputDir=..\build\installer
OutputBaseFilename=grainsexuell-beta-{#AppVersion}-win64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=grainsexuell {#AppVersion} beta
InfoBeforeFile=README.txt

[Types]
Name: "full"; Description: "VST3 plugin and standalone app"
Name: "custom"; Description: "Choose"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (Common Files\VST3)"; Types: full custom
Name: "standalone"; Description: "Standalone app"; Types: full custom

[Files]
Source: "{#Artefacts}\VST3\grainsexuell.vst3\*"; DestDir: "{commoncf64}\VST3\grainsexuell.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\grainsexuell.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "USER_MANUAL.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "USER_MANUAL.html"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\grainsexuell-tutorial.mp4"; DestName: "Tutorial.mp4"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\grainsexuell"; Filename: "{app}\grainsexuell.exe"; Components: standalone
Name: "{group}\grainsexuell User Manual"; Filename: "{app}\USER_MANUAL.html"
Name: "{group}\grainsexuell Tutorial (video)"; Filename: "{app}\Tutorial.mp4"
Name: "{group}\Uninstall grainsexuell"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\grainsexuell.exe"; Description: "Start grainsexuell now"; Components: standalone; Flags: nowait postinstall skipifsilent

; user presets in %APPDATA%\grainsexuell\Presets are left alone on uninstall on purpose
