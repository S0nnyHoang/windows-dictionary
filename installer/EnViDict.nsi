; EnViDict NSIS Installer Script
; Builds a single Setup.exe that installs EnViDict.exe + dictionary.db

Unicode True

!define APP_NAME      "EnViDict"
!define APP_VERSION   "$%APP_VERSION%"
!define INSTALL_DIR   "$PROGRAMFILES64\${APP_NAME}"
!define REG_KEY       "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\dist\EnViDict-${APP_VERSION}-Setup.exe"
InstallDir "${INSTALL_DIR}"
InstallDirRegKey HKLM "${REG_KEY}" "InstallLocation"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

;-------------------------------------------------
; Pages
;-------------------------------------------------
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;-------------------------------------------------
; Install
;-------------------------------------------------
Section "Install"
    SetOutPath "$INSTDIR"
    File "..\build\Release\EnViDict.exe"
    File "..\data\dictionary.db"

    ; Start Menu shortcut
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\EnViDict.exe"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk" "$INSTDIR\Uninstall.exe"

    ; Desktop shortcut
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\EnViDict.exe"

    ; Write uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Add/Remove Programs entry
    WriteRegStr   HKLM "${REG_KEY}" "DisplayName"          "${APP_NAME}"
    WriteRegStr   HKLM "${REG_KEY}" "DisplayVersion"       "${APP_VERSION}"
    WriteRegStr   HKLM "${REG_KEY}" "Publisher"            "EnViDict"
    WriteRegStr   HKLM "${REG_KEY}" "InstallLocation"      "$INSTDIR"
    WriteRegStr   HKLM "${REG_KEY}" "UninstallString"      '"$INSTDIR\Uninstall.exe"'
    WriteRegStr   HKLM "${REG_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegDWORD HKLM "${REG_KEY}" "NoModify"             1
    WriteRegDWORD HKLM "${REG_KEY}" "NoRepair"             1
SectionEnd

;-------------------------------------------------
; Uninstall
;-------------------------------------------------
Section "Uninstall"
    Delete "$INSTDIR\EnViDict.exe"
    Delete "$INSTDIR\dictionary.db"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir  "$INSTDIR"

    Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk"
    RMDir  "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\${APP_NAME}.lnk"

    DeleteRegKey HKLM "${REG_KEY}"
SectionEnd
