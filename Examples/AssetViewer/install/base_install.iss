
; Base Inno Setup script for Qt/DsQt applications
; Included by project-specific .iss files generated from inno.iss.in
;
; Required defines (set by inno.iss.in before #include):
;   BASEDIR          - Absolute path to the CMake DEPLOY directory
;   APP_DISPLAY_NAME - Display name for start menu and shortcuts
;   APP_NAME         - Short name used for paths and filenames
;   APP_EXE          - Executable filename (e.g. appClonerSource.exe)
;   APP_VERS         - Version string extracted from the executable
;
; Optional defines:
;   IS_PRODUCTION    - Production mode: auto-start, WER suppression, env vars
;   USE_APPHOST      - Include DSAppHost and its configuration
;   USE_BRIDGESYNC   - Include BridgeSync component
;   REPLACE_SHELL    - Replace Windows Explorer with DSAppHost (requires IS_PRODUCTION + USE_APPHOST)
;   SKIP_APP_ICON    - Do not create a desktop shortcut for the main app
;   CMS_URL          - Set DS_BASEURL environment variable to this URL

#define SYSTEMF GetEnv('SYSTEMROOT')
#define TOOLS_DIR "{pf}\Downstream\tools"

#define PROD_NAME ""
#ifndef IS_PRODUCTION
; NPI = Not Production Installer
#define PROD_NAME "-NPI"
#endif

[Setup]
AppName={#APP_DISPLAY_NAME}
AppVersion={#APP_VERS}
AppPublisher=Downstream
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
DefaultDirName={pf}\Downstream\{#APP_NAME}
DefaultGroupName={#APP_DISPLAY_NAME}
OutputDir=install/build
OutputBaseFilename={#APP_NAME}{#PROD_NAME}-v{#APP_VERS}
SourceDir=../
UninstallDisplayIcon={app}\bin\{#APP_EXE}
; OutputManifestFile={#APP_NAME}_log.txt

; For a lighter-weight install, disable all the wizard pages
DisableDirPage=yes
DisableFinishedPage=yes
DisableProgramGroupPage=yes
DisableReadyMemo=yes
DisableReadyPage=yes
DisableStartupPrompt=yes
DisableWelcomePage=yes

[Dirs]
; This will allow us to modify the settings after installation
Name: "{app}/bin/settings"; Permissions: users-modify;

[Files]
; Copy the entire DEPLOY directory contents (bin/, qml/, etc.)
Source: "{#BASEDIR}\*"; Excludes:"*.iobj,*.ipdb,*.pdb,*.map"; DestDir: "{app}"; Flags: recursesubdirs

#ifdef USE_APPHOST
Source: "install/DSAppHost/*"; DestDir: "{#TOOLS_DIR}\DSAppHost\"; Flags: recursesubdirs
Source: "install/apphost.json"; DestDir: "{userdocs}\downstream\common\dsapphost\config"
#endif

#ifdef USE_BRIDGESYNC
Source: "install/bridgesync/*"; DestDir: "{#TOOLS_DIR}\bridgesync\"; Flags: recursesubdirs
#endif

[Icons]
Name: "{group}\{#APP_DISPLAY_NAME}"; Filename: "{app}\bin\{#APP_EXE}"

#ifndef SKIP_APP_ICON
Name: "{commondesktop}\{#APP_DISPLAY_NAME}"; Filename: "{app}\bin\{#APP_EXE}"
#endif

#ifdef USE_APPHOST
Name: "{commondesktop}\{#APP_DISPLAY_NAME} DSAppHost"; Filename: "{#TOOLS_DIR}\DSAppHost\DSAppHost.exe"
#endif

; In production will launch the app on system boot
#ifdef IS_PRODUCTION

  ; If we're using apphost, apphost will launch everything itself, so just launch apphost on startup
  #ifdef USE_APPHOST
    ; Only if we're not replacing the shell, see below.
    #ifndef REPLACE_SHELL
    Name: "{commonstartup}\{#APP_NAME}-DSAppHost"; Filename: "{#TOOLS_DIR}\DSAppHost\DSAppHost.exe";
    #endif
  #else
    ; No apphost, but start the main app on system boot
    Name: "{commonstartup}\{#APP_NAME}"; Filename: "{app}\bin\{#APP_EXE}"
  #endif

#endif

[Registry]
; Sets the environment variable for pango text to look goodly
Root: HKCU; Subkey: "Environment"; ValueType: string; ValueName: "PANGOCAIRO_BACKEND"; ValueData: "fontconfig"; Flags: preservestringtype

#ifdef IS_PRODUCTION

; Disable the "program not responding" if this app crashed
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\Windows Error Reporting"; ValueType: dword; ValueName: "DontShowUI"; ValueData: "1"

#ifdef CMS_URL
; Set DS_BASEURL if the cms url has been defined
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: string; ValueName: "DS_BASEURL"; ValueData: "{#CMS_URL}"
#endif

; Instead of launching apphost on startup, we can replace the shell and not launch Windows Explorer at all.
#ifdef REPLACE_SHELL
  #ifdef USE_APPHOST
    Root: HKCU; Subkey: "Software\Microsoft\Windows NT\CurrentVersion\Winlogon"; ValueName: "Shell"; ValueType: string; ValueData: "{#TOOLS_DIR}\DSAppHost\DSAppHost.exe"; Flags: createvalueifdoesntexist uninsdeletevalue;
  #endif
#endif

#endif

[InstallDelete]
Type: filesandordirs; Name: "{app}\*"

; Also delete the shortcuts on the desktop and in the startup folder
Type: files; Name: "{commondesktop}\{#APP_DISPLAY_NAME}";
Type: files; Name: "{commondesktop}\{#APP_DISPLAY_NAME} DSAppHost";
Type: files; Name: "{commonstartup}\{#APP_NAME}-DSAppHost";
Type: files; Name: "{commonstartup}\{#APP_NAME}";

; Check if DS_BASEURL environment variable is already set. If not, request a reboot
; Only checked if IS_PRODUCTION & CMS_URL are both set
#ifdef IS_PRODUCTION
#ifdef CMS_URL
[Code]
var
  CmsUrl: String;

function InitializeSetup(): Boolean;
begin
  RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'DS_BASEURL', CmsUrl)

  Result := True;
end;

[Files]
; ^ This is a hack so that comments in the project installer after the include
;   don't break the code block :eyeroll:
#endif
#endif
