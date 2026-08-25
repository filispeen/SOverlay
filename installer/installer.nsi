Unicode true

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "nsDialogs.nsh"

!ifndef VERSION
  !define VERSION "0.0.0"
!endif

!ifndef PLUGIN_BUILD_DIR
  !define PLUGIN_BUILD_DIR "..\plugin\build_x64\rundir\Release"
!endif

!ifndef OVERLAY_BUILD_DIR
  !define OVERLAY_BUILD_DIR "..\dist-overlay\win-unpacked"
!endif

Name "SOverlay"
OutFile "SOverlay-Setup-${VERSION}.exe"
RequestExecutionLevel admin
InstallDirRegKey HKLM "Software\SOverlay" "InstallDir"

Var ObsDir
Var OverlayInstallDir
Var CandidateDrive

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

!macro TryObsPath _drive _relpath
  ${If} $ObsDir == ""
  ${AndIf} ${FileExists} "${_drive}:\${_relpath}\bin\64bit\obs64.exe"
    StrCpy $ObsDir "${_drive}:\${_relpath}"
  ${EndIf}
!macroend

!macro ScanDriveForObs _drive
  !insertmacro TryObsPath "${_drive}" "Program Files\obs-studio"
  !insertmacro TryObsPath "${_drive}" "Program Files\OBS Studio"
  !insertmacro TryObsPath "${_drive}" "Program Files (x86)\Steam\steamapps\common\OBS Studio"
  !insertmacro TryObsPath "${_drive}" "Program Files\Steam\steamapps\common\OBS Studio"
!macroend

Function .onInit
  StrCpy $ObsDir ""

  ReadRegStr $ObsDir HKLM "SOFTWARE\WOW6432Node\OBS Studio" ""
  ${If} $ObsDir == ""
    ReadRegStr $ObsDir HKLM "SOFTWARE\OBS Studio" ""
  ${EndIf}

  ${If} $ObsDir == ""
    !insertmacro ScanDriveForObs "C"
    !insertmacro ScanDriveForObs "D"
    !insertmacro ScanDriveForObs "E"
    !insertmacro ScanDriveForObs "F"
    !insertmacro ScanDriveForObs "G"
    !insertmacro ScanDriveForObs "H"
  ${EndIf}

  ${If} $ObsDir == ""
    MessageBox MB_OK|MB_ICONINFORMATION "OBS Studio installation was not found automatically.$\r$\nPlease select your OBS Studio installation folder on the next screen."
    nsDialogs::SelectFolderDialog "Select OBS Studio installation folder" ""
    Pop $ObsDir
    ${If} $ObsDir == "error"
      MessageBox MB_OK|MB_ICONSTOP "OBS Studio folder is required to continue."
      Abort
    ${EndIf}
  ${EndIf}

  ${IfNot} ${FileExists} "$ObsDir\bin\64bit\obs64.exe"
    MessageBox MB_YESNO|MB_ICONEXCLAMATION "obs64.exe was not found under:$\r$\n$ObsDir$\r$\n$\r$\nContinue anyway?" IDYES +2
    Abort
  ${EndIf}

  StrCpy $INSTDIR "$ObsDir\obs-plugins\64bit"
  StrCpy $OverlayInstallDir "$ObsDir\obs-plugins\64bit\SOverlay"
FunctionEnd

Section "SOverlay Plugin" SecPlugin
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "${PLUGIN_BUILD_DIR}\soverlay-obs-plugin.dll"
  File "${PLUGIN_BUILD_DIR}\soverlay-obs-plugin.pdb"

  SetOutPath "$OverlayInstallDir"
  File /r "${OVERLAY_BUILD_DIR}\*.*"

  WriteRegStr HKLM "Software\SOverlay" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\SOverlay" "OverlayDir" "$OverlayInstallDir"
  WriteRegStr HKLM "Software\SOverlay" "Version" "${VERSION}"

  WriteUninstaller "$INSTDIR\SOverlay-Uninstall.exe"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SOverlay" "DisplayName" "SOverlay"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SOverlay" "UninstallString" "$INSTDIR\SOverlay-Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SOverlay" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SOverlay" "Publisher" "filispeen"
SectionEnd

Function un.onInit
  ReadRegStr $INSTDIR HKLM "Software\SOverlay" "InstallDir"
  ReadRegStr $OverlayInstallDir HKLM "Software\SOverlay" "OverlayDir"
FunctionEnd

Section "Uninstall"
  ExecWait 'taskkill /IM SOverlay.exe /F'

  Delete "$INSTDIR\soverlay-obs-plugin.dll"
  Delete "$INSTDIR\soverlay-obs-plugin.pdb"
  Delete "$INSTDIR\SOverlay-Uninstall.exe"

  RMDir /r "$OverlayInstallDir"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SOverlay"
  DeleteRegKey HKLM "Software\SOverlay"
SectionEnd
