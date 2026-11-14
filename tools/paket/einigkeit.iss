; Installer für Windows (seit M13b5, docs/technik/vertical_slice.md 7). Inno Setup 6:
;   iscc /DVersion=0.13.0 /DQuelle=<Paketordner> /DAusgabe=<Zielordner> tools\paket\einigkeit.iss
; Der Paketordner ist der von paket.py gebaute Ordner einigkeit-<version>-windows-x86_64 (einigkeit.exe,
; einigkeit.pck, uhrwerk_bridge.dll, data\, DRITTANBIETER.txt, LIESMICH.txt). Installiert wird für den
; angemeldeten Benutzer (keine Administratorrechte); Spielstände bleiben beim Deinstallieren erhalten.

#ifndef Version
  #define Version "0.0.0"
#endif
#ifndef Quelle
  #error "Quelle (Paketordner) fehlt: iscc /DQuelle=..."
#endif
#ifndef Ausgabe
  #define Ausgabe "."
#endif

[Setup]
AppId={{6E1B5A3C-1848-4F2D-9B71-EB1A0C1871DE}
AppName=EINIGKEIT
AppVersion={#Version}
AppVerName=EINIGKEIT {#Version}
AppPublisher=EINIGKEIT
DefaultDirName={autopf}\EINIGKEIT
DefaultGroupName=EINIGKEIT
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Ausgabe}
OutputBaseFilename=einigkeit-{#Version}-windows-x86_64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\einigkeit.exe
InfoBeforeFile={#Quelle}\LIESMICH.txt

[Languages]
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#Quelle}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\EINIGKEIT"; Filename: "{app}\einigkeit.exe"
Name: "{group}\{cm:UninstallProgram,EINIGKEIT}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\EINIGKEIT"; Filename: "{app}\einigkeit.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\einigkeit.exe"; Description: "{cm:LaunchProgram,EINIGKEIT}"; Flags: nowait postinstall skipifsilent
