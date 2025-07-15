; Copyright Dolphin Emulator Project / Azahar Emulator Project / Team Cemu
; Licensed under MPL 2.0

; Require /DPRODUCT_VERSION=<release-name> to makensis.
!ifndef PRODUCT_VERSION
  !error "PRODUCT_VERSION must be defined"
!endif

!define PRODUCT_NAME "Cemu"
!define PRODUCT_PUBLISHER "Team Cemu"
!define PRODUCT_WEB_SITE "https://cemu.info/"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\${PRODUCT_NAME}.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

!define BINARY_SOURCE_DIR "..\..\bin"

Name "${PRODUCT_NAME}"
OutFile "cemu-${PRODUCT_VERSION}-windows-x64-installer.exe"
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUnInstDetails show

; Setup MultiUser support:
; If launched without ability to elevate, user will not see any extra options.
; If user has ability to elevate, they can choose to install system-wide, with default to CurrentUser.
!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_INSTALLMODE_INSTDIR "${PRODUCT_NAME}"
!define MULTIUSER_MUI
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_USE_PROGRAMFILES64
!include "MultiUser.nsh"

!include "MUI2.nsh"
; Custom page plugin
!include "nsDialogs.nsh"

; MUI Settings
!define MUI_ICON "logo_icon.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; License page
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE.txt"
; All/Current user selection page
!insertmacro MULTIUSER_PAGE_INSTALLMODE
; Desktop Shortcut page
Page custom desktopShortcutPageCreate desktopShortcutPageLeave
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Variables
Var DisplayName
Var DesktopShortcutPageDialog
Var DesktopShortcutCheckbox
Var DesktopShortcut

; Language files
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "TradChinese"
!insertmacro MUI_LANGUAGE "Danish"
!insertmacro MUI_LANGUAGE "Dutch"
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "Hungarian"
!insertmacro MUI_LANGUAGE "Italian"
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "Korean"
!insertmacro MUI_LANGUAGE "Lithuanian"
!insertmacro MUI_LANGUAGE "Norwegian"
!insertmacro MUI_LANGUAGE "Polish"
!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "Romanian"
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "Swedish"
!insertmacro MUI_LANGUAGE "Turkish"
!insertmacro MUI_LANGUAGE "Vietnamese"

; MUI end ------

!include "WinVer.nsh"
; Declare the installer itself as win10/win11 compatible, so WinVer.nsh works correctly.
ManifestSupportedOS {8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}

Function .onInit
  StrCpy $DesktopShortcut 1
  !insertmacro MULTIUSER_INIT

  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Function un.onInit
  !insertmacro MULTIUSER_UNINIT
FunctionEnd

!macro UPDATE_DISPLAYNAME
  ${If} $MultiUser.InstallMode == "CurrentUser"
    StrCpy $DisplayName "$(^Name) (User)"
  ${Else}
    StrCpy $DisplayName "$(^Name)"
  ${EndIf}
!macroend

Function desktopShortcutPageCreate
  !insertmacro MUI_HEADER_TEXT "Create Desktop Shortcut" "Would you like to create a desktop shortcut?"
  nsDialogs::Create 1018
  Pop $DesktopShortcutPageDialog
  ${If} $DesktopShortcutPageDialog == error
    Abort
  ${EndIf}

  ${NSD_CreateCheckbox} 0u 0u 100% 12u "Create a desktop shortcut"
  Pop $DesktopShortcutCheckbox
  ${NSD_SetState} $DesktopShortcutCheckbox $DesktopShortcut

  nsDialogs::Show
FunctionEnd

Function desktopShortcutPageLeave
  ${NSD_GetState} $DesktopShortcutCheckbox $DesktopShortcut
FunctionEnd

Section "Base"
  ExecWait '"$INSTDIR\uninst.exe" /S _?=$INSTDIR'

  SectionIn RO

  SetOutPath "$INSTDIR"

  ; The binplaced build output will be included verbatim.
  File /r "${BINARY_SOURCE_DIR}\*"

  !insertmacro UPDATE_DISPLAYNAME

  ; Create start menu and desktop shortcuts
  CreateShortCut "$SMPROGRAMS\$DisplayName.lnk" "$INSTDIR\Cemu.exe"
  ${If} $DesktopShortcut == 1
    CreateShortCut "$DESKTOP\$DisplayName.lnk" "$INSTDIR\Cemu.exe"
  ${EndIf}

  ; ??
  SetOutPath "$TEMP"
SectionEnd

!include "FileFunc.nsh"

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"

  WriteRegStr SHCTX "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\Cemu.exe"

  ; Write metadata for add/remove programs applet
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "DisplayName" "$DisplayName"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe /$MultiUser.InstallMode"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\Cemu.exe"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "InstallLocation" "$INSTDIR"
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD SHCTX "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"

  WriteRegStr HKCR ".wud" "" "Cemu"
  WriteRegStr HKCR ".wux" "" "Cemu"
  WriteRegStr HKCR ".wua" "" "Cemu"
  WriteRegStr HKCR "Cemu\DefaultIcon" "" "$INSTDIR\Cemu.exe,0"
  WriteRegStr HKCR "Cemu\Shell\open\command" "" '"$INSTDIR\Cemu.exe" %1'
SectionEnd

Section Uninstall
  !insertmacro UPDATE_DISPLAYNAME

  Delete "$DESKTOP\$DisplayName.lnk"
  Delete "$SMPROGRAMS\$DisplayName.lnk"

  ; Be a bit careful to not delete files a user may have put into the install directory.
  Delete "$INSTDIR\Cemu.exe"
  Delete "$INSTDIR\uninst.exe"
  RMDir /r "$INSTDIR\gameProfiles"
  RMDir /r "$INSTDIR\resources"
  RMDir "$INSTDIR"

  ; Delete both system wide and user defined file associations.
  DeleteRegKey HKCR ".wud"
  DeleteRegKey HKCR ".wux"
  DeleteRegKey HKCR ".wua"
  DeleteRegKey HKCR "Cemu"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.wud"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.wux"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.wua"

  DeleteRegKey SHCTX "${PRODUCT_UNINST_KEY}"
  DeleteRegKey SHCTX "${PRODUCT_DIR_REGKEY}"
  DeleteRegKey HKCU "Software\Classes\discord-460807638964371468"

  SetAutoClose true
SectionEnd
