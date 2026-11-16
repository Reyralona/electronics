Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!ifndef PRODUCT_KEY
  !define PRODUCT_KEY "Bancada"
!endif
!ifndef PAYLOAD
  !error "Pass /DPAYLOAD=... /DOUTPUT=... /DVERSION=..."
!endif
Name "Bancada ${VERSION}"
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\Bancada"
InstallDirRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "InstallLocation"
RequestExecutionLevel user
SetCompressor /SOLID lzma
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "Bancada requires Windows x64."
    Abort
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_ICONSTOP "Bancada requires Windows 10 or newer."
    Abort
  ${EndIf}
  SetShellVarContext current
FunctionEnd

Section "Bancada"
  SetOutPath "$INSTDIR"
  File /r "${PAYLOAD}\*"
  ; The ZIP keeps portable.mode; installed copies use the user's data directory.
  Delete "$INSTDIR\portable.mode"
  FileOpen $0 "$INSTDIR\installed.mode" w
  FileClose $0
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\${PRODUCT_KEY}"
  CreateShortcut "$SMPROGRAMS\${PRODUCT_KEY}\Bancada.lnk" "$INSTDIR\bancada.exe"
  CreateShortcut "$SMPROGRAMS\${PRODUCT_KEY}\Desinstalar.lnk" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "DisplayName" "Bancada"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "URLInfoAbout" "https://github.com/Reyralona/electronics"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  ; Only remove shipped files. Never recursively remove the user's chosen directory or saved data.
  !include "${DELETE_MANIFEST}"
  Delete "$INSTDIR\installed.mode"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${PRODUCT_KEY}\Bancada.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_KEY}\Desinstalar.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT_KEY}"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}"
SectionEnd
