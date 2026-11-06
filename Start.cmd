@echo off
rem Uncharted: The Nathan Drake Collection PC Port - Quick Launcher
cd /d "%~dp0"

where pythonw >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    start "" pythonw "launcher\uncharted_launcher.pyw" %*
) else (
    where python >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        start "" python "launcher\uncharted_launcher.pyw" %*
    ) else (
        echo [ERROR] Python is required to run the Uncharted launcher.
        echo Please install Python 3.10+ from python.org and add it to your PATH.
        pause
    )
)
