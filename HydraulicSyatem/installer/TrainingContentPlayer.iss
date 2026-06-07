; ============================================================================
; TrainingContentPlayer - Inno Setup 설치 스크립트
;
; 사용 방법:
;   1. Release | x64 로 빌드하여 Bin 폴더에 실행 파일을 준비합니다.
;   2. Inno Setup 6 (https://jrsoftware.org/isinfo.php) 을 설치합니다.
;   3. 이 파일을 Inno Setup Compiler로 열고 Compile(F9) 합니다.
;   4. installer\Output\TrainingContentPlayer_Setup.exe 가 생성됩니다.
;
; 배포 대상: 개발 PC가 아닌 새 컴퓨터
;   - 필요한 파일은 Setup.exe 안에 모두 포함됩니다.
;   - 설치 시 사용자가 원하는 폴더를 직접 지정할 수 있습니다.
;   - 프로그램은 실행 파일 위치 기준으로 Data/Pdf/Images/Video를 찾습니다.
;
; 사전 요구 사항 (설치 대상 PC):
;   - Windows 10/11 x64
;   - Microsoft Edge WebView2 Runtime (Windows 11 / 최신 Windows 10에는 기본 포함)
; ============================================================================

#define MyAppName        "스마트 강의 플레이어"
#define MyAppNameEn      "Training Content Player"
#define MyAppVersion     "1.0.0"
#define MyAppPublisher   "Easytech"
#define MyAppExeName     "TrainingContentPlayer.exe"

; 이 스크립트(installer 폴더) 기준 Bin 폴더 절대 경로
#define SourceDir        AddBackslash(SourcePath) + "..\..\Bin"

#ifnexist "..\..\Bin\TrainingContentPlayer.exe"
  #error "Bin\TrainingContentPlayer.exe 가 없습니다. Visual Studio에서 Release | x64 로 먼저 빌드하세요."
#endif
; 빌드 결과 설치 파일 출력 폴더
#define OutputDir        "Output"

[Setup]
AppId={{A3F8C2E1-9B4D-4F6A-8C1E-2D5F7A9B3C4E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
; 기본 설치 위치 (설치 중 [찾아보기]로 임의 경로 변경 가능)
DefaultDirName={autopf}\{#MyAppNameEn}
DefaultGroupName={#MyAppName}
DisableDirPage=no
DisableProgramGroupPage=yes
UsePreviousAppDir=no
OutputDir={#OutputDir}
OutputBaseFilename=TrainingContentPlayer_Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
; 관리자 권한 없이도 사용자 지정 폴더 설치 가능 (필요 시 설치 중 권한 상승 선택)
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupLogging=yes
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} 설치 프로그램
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "바탕 화면에 바로 가기 만들기"; GroupDescription: "추가 작업:"; Flags: unchecked

[Files]
; 실행 파일 및 런타임 DLL (PDB 등 디버그 파일은 제외)
Source: "{#SourceDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\pdfium.dll"; DestDir: "{app}"; Flags: ignoreversion

; 교육 콘텐츠 데이터
Source: "{#SourceDir}\Data\*"; DestDir: "{app}\Data"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\Images\*"; DestDir: "{app}\Images"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\Pdf\*"; DestDir: "{app}\Pdf"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\Video\*"; DestDir: "{app}\Video"; Flags: ignoreversion recursesubdirs createallsubdirs

; 학습 진행 상태: 최초 설치 시만 복사, 업그레이드·삭제 시 사용자 데이터 보존
Source: "{#SourceDir}\Progress\Progress.json"; DestDir: "{app}\Progress"; Flags: onlyifdoesntexist uninsneveruninstall

[Dirs]
Name: "{app}\Progress"; Permissions: users-modify

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{#MyAppName} 제거"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Messages]
korean.BeveledLabel=설치 정보
korean.SetupAppTitle={#MyAppName} 설치
korean.SetupWindowTitle={#MyAppName} 설치
korean.WelcomeLabel2=컴퓨터에 [name/ver]을(를) 설치합니다.%n%nYouTube·PDF·이미지·로컬 동영상 교육 콘텐츠를 순서대로 학습할 수 있는 프로그램입니다.
