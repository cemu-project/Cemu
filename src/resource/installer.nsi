; Copyright Dolphin Emulator Project / Azahar Emulator Project / Team Cemu
; Licensed under MPL 2.0 with permission from authors

; Usage:
;   get the latest nsis: https://nsis.sourceforge.io/Download
;   probably also want vscode extension: https://marketplace.visualstudio.com/items?itemName=idleberg.nsis

; Require /DPRODUCT_VERSION for makensis.
!ifndef PRODUCT_VERSION
  !error "PRODUCT_VERSION must be defined"
!endif

ManifestDPIAware true

!define PRODUCT_NAME "Cemu"
!define PRODUCT_PUBLISHER "Team Cemu"
!define PRODUCT_WEB_SITE "https://cemu.info/"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\${PRODUCT_NAME}.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

!define BINARY_SOURCE_DIR "..\..\bin"

Name "${PRODUCT_NAME}"
OutFile "cemu-${PRODUCT_VERSION}-windows-x64-installer.exe"
SetCompressor /SOLID lzma
InstallDir "$LOCALAPPDATA\Cemu" 
ShowInstDetails show
ShowUnInstDetails show

!include "MUI2.nsh"
; Custom page plugin
!include "nsDialogs.nsh"

; MUI Settings
!define MUI_ICON "logo_icon.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; License page
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE.txt"
; Desktop Shortcut page
Page custom desktopShortcutPageCreate desktopShortcutPageLeave
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\Cemu.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Variables
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

Function .onInit
  StrCpy $DesktopShortcut 1

  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

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

  ; Create start menu and desktop shortcuts
  CreateShortCut "$SMPROGRAMS\$(^Name).lnk" "$INSTDIR\Cemu.exe"
  ${If} $DesktopShortcut == 1
    CreateShortCut "$DESKTOP\$(^Name).lnk" "$INSTDIR\Cemu.exe"
  ${EndIf}
SectionEnd

!include "FileFunc.nsh"

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"

  WriteRegStr HKCU "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\Cemu.exe"

  ; Write metadata for add/remove programs applet
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\Cemu.exe"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "InstallLocation" "$INSTDIR"
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"

  WriteRegStr HKCU "Software\Classes\.wud" "" "$(^Name)"
  WriteRegStr HKCU "Software\Classes\.wux" "" "$(^Name)"
  WriteRegStr HKCU "Software\Classes\.wua" "" "$(^Name)"
  WriteRegStr HKCU "Software\Classes\$(^Name)\DefaultIcon" "" "$INSTDIR\Cemu.exe,0"
  WriteRegStr HKCU "Software\Classes\$(^Name)\Shell\open\command" "" '"$INSTDIR\Cemu.exe" %1'
SectionEnd

Section Uninstall
  Delete "$DESKTOP\$(^Name).lnk"
  Delete "$SMPROGRAMS\$(^Name).lnk"

; Be a bit careful to not delete files a user may have put into the install directory
  Delete "$INSTDIR\Cemu.exe"
  Delete "$INSTDIR\uninst.exe"
  RMDir /r "$INSTDIR\gameProfiles"
  RMDir /r "$INSTDIR\resources"
  RMDir "$INSTDIR"

  DeleteRegKey HKCU "Software\Classes\.wud"
  DeleteRegKey HKCU "Software\Classes\.wux"
  DeleteRegKey HKCU "Software\Classes\.wua"
  DeleteRegKey HKCU "Software\Classes\$(^Name)"

  DeleteRegKey HKCU "Software\Classes\discord-460807638964371468"

  DeleteRegKey HKCU "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKCU "${PRODUCT_DIR_REGKEY}"

  SetAutoClose true
SectionEnd
