; =============================================================================
; Viss Language Inno Setup Script
; Version: 0.2.2 "Lemongrab & Lemonhope"
; =============================================================================

#define MyAppName "Viss"
#define MyAppVersion "0.2.2"
#define MyAppPublisher "Halva"
#define MyAppURL "https://github.com/Halva-developer/Viss"
#define MyAppExeName "viss.exe"

[Setup]
AppId={{D37F8A21-4B96-4F91-9543-A128B46C9D10}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={userpf}\{#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=..\..\LICENSE
OutputDir=..\..\dist
OutputBaseFilename=Setup_Viss_{#MyAppVersion}_x64
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ChangesEnvironment=yes
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "envPath"; Description: "Add Viss to User PATH"; GroupDescription: "System Integration:"; Flags: checkedonce
Name: "fileAssoc"; Description: "Associate .viss files with Viss Engine"; GroupDescription: "File Associations:"; Flags: checkedonce

[Files]
Source: "..\..\viss.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\viss.cmd"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\..\libs\*"; DestDir: "{app}\libs"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\README.md"; DestDir: "{app}"; Flags: isreadme
Source: "..\..\extensions\viss-vscode\*.vsix"; DestDir: "{app}\extensions"; Flags: ignoreversion skipifsourcedoesntexist

[Registry]
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Tasks: envPath; Check: NeedsAddPath(ExpandConstant('{app}'))
Root: HKCU; Subkey: "Software\Classes\.viss"; ValueType: string; ValueName: ""; ValueData: "VissSourceFile"; Tasks: fileAssoc; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\VissSourceFile"; ValueType: string; ValueName: ""; ValueData: "Viss Source File"; Tasks: fileAssoc; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\VissSourceFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\viss.exe"" run ""%1"""; Tasks: fileAssoc
Root: HKCU; Subkey: "Software\Classes\VissSourceFile\shell\Run with Viss\command"; ValueType: string; ValueName: ""; ValueData: """{app}\viss.exe"" run ""%1"""; Tasks: fileAssoc

[Code]
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + Param + ';', ';' + OrigPath + ';') = 0;
end;
