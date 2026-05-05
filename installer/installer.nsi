; SnapCapture NSIS 安装脚本
; 使用方法：安装 NSIS 后运行 makensis installer.nsi

!include "MUI2.nsh"

; --- 基本信息 ---
Name "SnapCapture 截屏工具"
OutFile "..\dist\SnapCapture_Setup.exe"
InstallDir "$PROGRAMFILES\SnapCapture"
InstallDirRegKey HKCU "Software\SnapCapture" "InstallDir"
RequestExecutionLevel user

; --- 界面设置 ---
!define MUI_ABORTWARNING
!define MUI_ICON "..\src\resources\icons\app.ico"
!define MUI_UNICON "..\src\resources\icons\app.ico"

; --- 页面 ---
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; --- 语言 ---
!insertmacro MUI_LANGUAGE "SimpChinese"

; --- 安装区段 ---
Section "安装文件" SecMain
    SetOutPath "$INSTDIR"

    ; 复制所有文件
    File /r "..\dist\SnapCapture\*.*"

    ; 写入注册表
    WriteRegStr HKCU "Software\SnapCapture" "InstallDir" "$INSTDIR"

    ; 创建卸载程序
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; 写入卸载信息到注册表
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SnapCapture" \
        "DisplayName" "SnapCapture 截屏工具"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SnapCapture" \
        "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SnapCapture" \
        "DisplayIcon" "$INSTDIR\SnapCapture.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SnapCapture" \
        "Publisher" "SnapCapture"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SnapCapture" \
        "DisplayVersion" "1.0.0"

    ; 创建开始菜单快捷方式
    CreateDirectory "$SMPROGRAMS\SnapCapture"
    CreateShortCut "$SMPROGRAMS\SnapCapture\SnapCapture.lnk" "$INSTDIR\SnapCapture.exe"
    CreateShortCut "$SMPROGRAMS\SnapCapture\卸载 SnapCapture.lnk" "$INSTDIR\Uninstall.exe"

    ; 创建桌面快捷方式
    CreateShortCut "$DESKTOP\SnapCapture.lnk" "$INSTDIR\SnapCapture.exe"
SectionEnd

; --- 卸载区段 ---
Section "Uninstall"
    ; 删除文件
    RMDir /r "$INSTDIR"

    ; 删除快捷方式
    Delete "$DESKTOP\SnapCapture.lnk"
    RMDir /r "$SMPROGRAMS\SnapCapture"

    ; 删除注册表项
    DeleteRegKey HKCU "Software\SnapCapture"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SnapCapture"

    ; 删除自启动注册表项
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "SnapCapture"
SectionEnd
