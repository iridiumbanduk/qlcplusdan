;--------------------------------
;Include Modern UI
  !include "MUI2.nsh"

;--------------------------------
;Defines
!define QLCPLUS_HOME "c:\projects\qlcplus"
!define MUI_ICON "${QLCPLUS_HOME}\resources\icons\qlcplus.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\nsis3-uninstall.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Header\nsis3-vintage.bmp"
!define MUI_HEADERIMAGE_LEFT
!define MUI_PAGE_HEADER_TEXT "QLC+DAN"

;--------------------------------
;General
Name "QLC+DAN"
OutFile "QLC+DAN_4.14.5.exe"
InstallDir C:\QLC+DAN
InstallDirRegKey HKCU "Software\qlcplusdan" "Install_Dir"
RequestExecutionLevel user

!define MUI_LICENSEPAGE_TEXT_TOP "Do you accept the following statement of the Apache 2.0 license?"

!insertmacro MUI_PAGE_LICENSE "${QLCPLUS_HOME}\platforms\windows\apache_2.0.txt"

;--------------------------------
;Languages
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Italian"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "SpanishInternational"
!insertmacro MUI_LANGUAGE "Czech"
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "Finnish"
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "Catalan"

;--------------------------------
; Pages
Page directory
Page custom StartMenuGroupSelect "" ": Start Menu Folder"
Page instfiles

Function StartMenuGroupSelect
	Push $R1

	StartMenu::Select /checknoshortcuts "Don't create a start menu folder" /autoadd /lastused $R0 "QLC+DAN"
	Pop $R1

	StrCmp $R1 "success" success
	StrCmp $R1 "cancel" done
		; error
		MessageBox MB_OK $R1
		StrCpy $R0 "QLC+DAN" # use default
		Return
	success:
	Pop $R0

	done:
	Pop $R1
FunctionEnd

Section
	SetOutPath $INSTDIR

	# this part is only necessary if you used /checknoshortcuts
	StrCpy $R1 $R0 1
	StrCmp $R1 ">" skip
		CreateDirectory $SMPROGRAMS\$R0
		CreateShortCut '$SMPROGRAMS\$R0\QLC+DAN.lnk' $INSTDIR\qlcplus.exe

		CreateDirectory $SMPROGRAMS\$R0
		CreateShortCut '$SMPROGRAMS\$R0\QLC+DAN Fixture Editor.lnk' $INSTDIR\qlcplus-fixtureeditor.exe

		CreateDirectory $SMPROGRAMS\$R0
		CreateShortCut '$SMPROGRAMS\$R0\Uninstall.lnk' $INSTDIR\uninstall.exe

	skip:
SectionEnd

Section
	File qlcplus.exe
	File qlcplus-fixtureeditor.exe
	File *.dll
	File /r platforms
	File /r imageformats
	File /r multimedia
	File /r styles
	File Sample.qxw
	File *.qm
	File /r Fixtures
	File /r Gobos
	File /r InputProfiles
	File /r MidiTemplates
	File /r ModifiersTemplates
	File /r Plugins
	File /r RGBScripts
	File /r translations
	File /r Web

	; QLC+DAN: file associations are left to the official QLC+ install

	WriteRegStr HKCU "SOFTWARE\qlcplusdan" "Install_Dir" "$INSTDIR"

	WriteUninstaller $INSTDIR\uninstall.exe
SectionEnd

;--------------------------------
; Uninstallation

UninstPage uninstConfirm
UninstPage instfiles
Section "Uninstall"
	Delete $INSTDIR\uninstall.exe
	Delete $INSTDIR\qlcplus.exe
	Delete $INSTDIR\qlcplus-fixtureeditor.exe
	Delete $INSTDIR\*.dll
	RMDir /r $INSTDIR\platforms
	RMDir /r $INSTDIR\imageformats
	RMDir /r $INSTDIR\multimedia
	RMDir /r $INSTDIR\styles
	Delete $INSTDIR\Sample.qxw
	Delete $INSTDIR\*.qm
	RMDir /r $INSTDIR\Fixtures
	RMDir /r $INSTDIR\Gobos
	RMDir /r $INSTDIR\InputProfiles
	RMDir /r $INSTDIR\MidiTemplates
	RMDir /r $INSTDIR\ModifiersTemplates
	RMDir /r $INSTDIR\Plugins
	RMDir /r $INSTDIR\RGBScripts
	RMDir /r $INSTDIR\translations
	RMDir /r $INSTDIR\Web

	RMDir $INSTDIR

	; QLC+DAN: leave file associations and the official QLC+ settings alone
	DeleteRegKey HKCU "Software\qlcplusdan"
	DeleteRegKey HKCU "Software\qlcplus\QLC+DAN"
SectionEnd
