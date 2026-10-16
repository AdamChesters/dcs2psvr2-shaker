#ifndef AppVersion
  #define AppVersion "0.1.0-alpha"
#endif
#ifndef AppNumericVersion
  #define AppNumericVersion "0.1.0"
#endif
#ifndef AppDisplayVersion
  #define AppDisplayVersion "0.1.0 Alpha"
#endif
#ifndef PackageDir
  #define PackageDir "..\dist\package"
#endif
[Setup]
AppId={{B01E118D-9BC9-48CB-B4A7-44B277628FF1}
AppName=dcs2psvr2-shaker
AppVersion={#AppVersion}
AppVerName=dcs2psvr2-shaker {#AppDisplayVersion}
VersionInfoVersion={#AppNumericVersion}
VersionInfoProductVersion={#AppNumericVersion}
VersionInfoProductTextVersion={#AppVersion}
AppPublisher=Adam Chesters
AppPublisherURL=https://github.com/AdamChesters/dcs2psvr2-shaker
AppSupportURL=https://github.com/AdamChesters/dcs2psvr2-shaker/issues
DefaultDirName={localappdata}\Programs\dcs2psvr2-shaker
DefaultGroupName=dcs2psvr2-shaker
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
DisableProgramGroupPage=yes
WizardStyle=modern
LicenseFile=..\LICENSE
OutputDir=..\dist
OutputBaseFilename=dcs2psvr2-shaker-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\dcs2psvr2-shaker.exe
SetupIconFile=..\assets\dcs2psvr2-shaker.ico
CloseApplications=yes
RestartApplications=no
SetupLogging=yes

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\dcs2psvr2-shaker"; Filename: "{app}\dcs2psvr2-shaker.exe"
Name: "{autodesktop}\dcs2psvr2-shaker"; Filename: "{app}\dcs2psvr2-shaker.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\dcs2psvr2-shaker.exe"; Description: "Open dcs2psvr2-shaker"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\dcs2psvr2-shaker.exe"; Parameters: "--uninstall-integration"; Flags: runhidden waituntilterminated; RunOnceId: "RemoveDcsHooks"

; Personal settings are deliberately retained for reinstalls.
