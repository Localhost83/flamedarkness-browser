@echo off
REM Initialize MSVC environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

cd /d %~dp0..

REM Compile WebView2 browser
REM Use the prerelease WebView2 SDK in the repo so newer environment options and APIs are available
cd program-builds
rem Compile the browser source in the repository root but emit outputs into program-builds
rem Current directory is scripts\ (script calls cd /d %~dp0.. earlier then cd program-builds)
rem Produce the EXE in this directory and copy the matching WebView2Loader.dll here.

rem Remove any stale EXE in repo root (we want EXE to live only in program-builds)
if exist "%~dp0..\flamedarkness_browser.exe" del "%~dp0..\flamedarkness_browser.exe"

cl.exe /nologo /W4 /EHsc /std:c++17 /DUNICODE /D_UNICODE /MT "..\flamedarkness_browser.cpp" /I "%~dp0..\microsoft.web.webview2.1.0.3650-prerelease\build\native\include" /Fe:"flamedarkness_browser.exe" /link user32.lib gdi32.lib comctl32.lib shell32.lib WebView2Loader.dll.lib /LIBPATH:"%~dp0..\microsoft.web.webview2.1.0.3650-prerelease\build\native\x64" /MANIFEST:EMBED /SUBSYSTEM:WINDOWS

rem If build succeeded, ensure WebView2Loader.dll is present in program-builds
if %errorlevel% equ 0 (
	rem Prefer build\native\x64, then runtimes\win-x64\native
	if exist "%~dp0..\microsoft.web.webview2.1.0.3650-prerelease\build\native\x64\WebView2Loader.dll" (
		copy /Y "%~dp0..\microsoft.web.webview2.1.0.3650-prerelease\build\native\x64\WebView2Loader.dll" "%CD%\WebView2Loader.dll" >nul
		echo Copied WebView2Loader.dll to program-builds
	) else if exist "%~dp0..\microsoft.web.webview2.1.0.3650-prerelease\runtimes\win-x64\native\WebView2Loader.dll" (
		copy /Y "%~dp0..\microsoft.web.webview2.1.0.3650-prerelease\runtimes\win-x64\native\WebView2Loader.dll" "%CD%\WebView2Loader.dll" >nul
		echo Copied WebView2Loader.dll to program-builds
	) else (
		echo WARNING: WebView2Loader.dll not found in SDK paths; ensure WebView2 runtime is available at runtime
	)
	echo Build succeeded: %CD%\flamedarkness_browser.exe
)

if %errorlevel% neq 0 (
	echo Build failed with errorlevel %errorlevel%
	pause
)
