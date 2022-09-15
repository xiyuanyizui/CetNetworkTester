; �ű��� Inno Setup �ű��� ���ɣ�
; �йش��� Inno Setup �ű��ļ�����ϸ��������İ����ĵ���

#define MyAppName "CetNetworkTester"
#define MyAppVersion "1.2.4"
#define MyAppPublisher "cetxiyuan"
#define MyAppURL "https://blog.csdn.net/xiyuan255"
#define MyAppExeName "CetNetworkTester.exe"
#define MyAppIcon  "F:\sharefolder\cetqtlearn\CetNetworkTester\CetNetworkTester-setup\favorite.ico"
 ;ע�� Release-��ʽ��  Beta-���԰�
#define MyVersionTip "Release"

[Setup]
; ע: AppId��ֵΪ������ʶ��Ӧ�ó���
; ��ҪΪ������װ����ʹ����ͬ��AppIdֵ��
; (��Ҫ�����µ� GUID�����ڲ˵��е�� "����|���� GUID"��)
AppId={{B1DFA05B-6489-494D-AE3A-A23F39EDAD0F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
; ������ȡ��ע�ͣ����ڷǹ���װģʽ�����У���Ϊ��ǰ�û���װ����
;PrivilegesRequired=lowest
OutputBaseFilename=CetNetworkTester-Setup-{#MyVersionTip}-V{#MyAppVersion}
SetupIconFile={#MyAppIcon}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={#MyAppIcon}

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Default.isl"
Name: "english"; MessagesFile: "compiler:Languages\English.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "F:\sharefolder\cetqtlearn\CetNetworkTester\CetNetworkTester-release\CetNetworkTester.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "F:\sharefolder\cetqtlearn\CetNetworkTester\CetNetworkTester-release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; ע��: ��Ҫ���κι���ϵͳ�ļ���ʹ�á�Flags: ignoreversion��

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

