@echo off
:: ============================================================================
::  NucleOS - Dependency Installer for Windows
::  Supports: MSYS2, Git Bash, WSL, Chocolatey, winget
:: ============================================================================
setlocal enabledelayedexpansion
chcp 65001 >nul 2>&1

:: ── Colors via ANSI escape codes (Windows 10+) ─────────────────────────────
for /f "tokens=2 delims=[]" %%a in ('ver') do set "WINVER=%%a"
set "ESC="
set "R=%ESC%[0m"
set "GR=%ESC%[0;32m"
set "RE=%ESC%[0;31m"
set "Y=%ESC%[0;33m"
set "BL=%ESC%[0;34m"
set "M=%ESC%[0;35m"
set "C=%ESC%[0;36m"
set "W=%ESC%[1;37m"
set "DIM=%ESC%[2m"
set "BOLD=%ESC%[1m"

:: ── Title ──────────────────────────────────────────────────────────────────
echo.
echo %C%%BOLD%
echo     _   _ _____ _____     _____         _
echo    ^| ^\ ^| ^|_   _^| ____^|   ^|  ___^|_ _ __^| ^| ___  _ __
echo    ^|  ^\^| ^| ^| ^| ^|  _^|     ^| ^|_ / _` / __^| ^|/ _ \^| '_ \
echo    ^| ^|\  ^| ^| ^| ^| ^|___    ^|  ^|_ ^(_^| \__ \ ^| ^(_^) ^| ^| ^| ^|
echo    ^|_^| ^\_^| ^|_^| ^|_____^|   ^|_^|  ^\__^,_^|___/_^|\___/^|_^| ^|_|
echo %R%
echo %W%    Dependency Installer%R%
echo %DIM%    Windows ^- MSYS2 / Chocolatey / winget%R%
echo.
echo %DIM%$(printf '%.0s─' {1..56})%R%
echo.

:: ── Helpers ────────────────────────────────────────────────────────────────
goto :main

:print_ok
    echo     %GR%✓%R%  %GR%%~1%R%
    goto :eof

:print_fail
    echo     %RE%✗%R%  %RE%%~1%R%
    goto :eof

:print_info
    echo     %C%*%R%  %BL%%~1%R%
    goto :eof

:print_warn
    echo     %Y%~%R%  %Y%%~1%R%
    goto :eof

:print_section
    echo.
    echo     %C%%BOLD%%~1%R%
    echo     %DIM%$(printf '%.0s─' {1..50})%R%
    goto :eof

:check_cmd
    where "%~1" >nul 2>&1
    goto :eof

:ask_choice
    set /p "_answer=    %M%?%R%  %W%%~1%R% %DIM%[Y/n]%R% "
    if /i "!_answer!"=="N" exit /b 1
    if /i "!_answer!"=="NO" exit /b 1
    exit /b 0

:: ── Detect environment ────────────────────────────────────────────────────
:detect_env
    set "ENV=unknown"
    set "HAS_MSYS2=0"
    set "HAS_WSL=0"
    set "HAS_CHOCO=0"
    set "HAS_WINGET=0"
    set "HAS_GITBASH=0"

    :: Check for MSYS2
    if defined MSYSTEM (
        set "ENV=msys2"
        set "HAS_MSYS2=1"
    )
    where msys2 >nul 2>&1 && set "HAS_MSYS2=1"
    if exist "C:\msys64\usr\bin\bash.exe" set "HAS_MSYS2=1"

    :: Check for WSL
    where wsl >nul 2>&1 && set "HAS_WSL=1"
    wsl --list --verbose >nul 2>&1 && set "HAS_WSL=1"

    :: Check for Chocolatey
    where choco >nul 2>&1 && set "HAS_CHOCO=1"

    :: Check for winget
    where winget >nul 2>&1 && set "HAS_WINGET=1"

    :: Check for Git Bash
    where bash >nul 2>&1 && set "HAS_GITBASH=1"

    goto :eof

:: ── MSYS2 installer ───────────────────────────────────────────────────────
:install_msys2
    call :print_section "Installing via MSYS2"

    :: Open MSYS2 terminal and run pacman
    echo     %C%*%R%  Launching MSYS2 package installation...
    echo.
    echo     %DIM%The following will be installed:%R%
    echo       gcc ^| binutils ^| make ^| nasm ^| xorriso ^| mtools ^| python ^| qemu
    echo.

    :: Use MSYS2's pacman directly if available
    if exist "C:\msys64\usr\bin\bash.exe" (
        echo     %C%*%R%  Running pacman in MSYS2...
        "C:\msys64\usr\bin\bash.exe" -lc "pacman -S --noconfirm --needed gcc binutils make nasm xorriso mtools python qemu-system-x86 qemu-full"
        if !errorlevel! equ 0 (
            call :print_ok "MSYS2 packages installed"
        ) else (
            call :print_fail "MSYS2 package installation failed"
        )
    ) else (
        echo     %Y%~%R%  MSYS2 found but pacman path not located.
        echo     %C%*%R%  Open MSYS2 terminal manually and run:
        echo       %W%pacman -S --needed gcc binutils make nasm xorriso mtools python qemu-system-x86%R%
    )
    goto :eof

:: ── Chocolatey installer ──────────────────────────────────────────────────
:install_choco
    call :print_section "Installing via Chocolatey"

    echo     %C%*%R%  Using Chocolatey package manager...
    echo.

    :: Install via choco
    echo     %C%*%R%  Installing make...
    choco install -y make 2>&1 | findstr /V "Refreshing" >nul
    if !errorlevel! equ 0 (call :print_ok "make") else (call :print_fail "make")

    echo     %C%*%R%  Installing python3...
    choco install -y python3 2>&1 | findstr /V "Refreshing" >nul
    if !errorlevel! equ 0 (call :print_ok "python3") else (call :print_fail "python3")

    echo     %C%*%R%  Installing qemu...
    choco install -y qemu 2>&1 | findstr /V "Refreshing" >nul
    if !errorlevel! equ 0 (call :print_ok "qemu") else (call :print_fail "qemu")

    echo.
    echo     %Y%~%R%  Chocolatey does not provide gcc/binutils/nasm/xorriso/mtools.
    echo     %C%*%R%  For full build support, install MSYS2 from:
    echo       %W%https://www.msys2.org/%R%
    echo.
    echo     %C%*%R%  Or use the install-deps.sh script inside MSYS2/WSL.
    goto :eof

:: ── winget installer ──────────────────────────────────────────────────────
:install_winget
    call :print_section "Installing via winget"

    echo     %C%*%R%  Using winget...
    echo.

    echo     %C%*%R%  Installing Python...
    winget install -e --id Python.Python.3.12 --accept-source-agreements --accept-package-agreements 2>&1 >nul
    if !errorlevel! equ 0 (call :print_ok "Python") else (call :print_fail "Python")

    echo     %C%*%R%  Installing QEMU...
    winget install -e --id SoftwareFreedomConservancy.QEMU --accept-source-agreements --accept-package-agreements 2>&1 >nul
    if !errorlevel! equ 0 (call :print_ok "QEMU") else (call :print_fail "QEMU")

    echo.
    echo     %Y%~%R%  winget has limited coverage for dev tools.
    echo     %C%*%R%  For gcc/binutils/nasm, install MSYS2 or use WSL.
    goto :eof

:: ── WSL installer ─────────────────────────────────────────────────────────
:install_wsl
    call :print_section "Installing via WSL (Windows Subsystem for Linux)"

    echo     %C%*%R%  Detecting WSL distro...
    echo.

    :: Check for a running WSL distro
    where wsl >nul 2>&1
    if !errorlevel! neq 0 (
        echo     %Y%~%R%  WSL not found. Install it first:
        echo       %W%wsl --install%R%
        echo       %DIM%Requires Windows 10 1903+ or Windows 11%R%
        goto :eof
    )

    :: Try Ubuntu first, then any available distro
    set "WSL_DISTRO=Ubuntu"
    wsl --list --quiet 2>nul | findstr /i "Ubuntu" >nul || (
        for /f "tokens=*" %%d in ('wsl --list --quiet 2^>nul') do (
            if not "%%d"=="" (
                set "WSL_DISTRO=%%d"
                goto :wsl_found
            )
        )
        echo     %RE%✗%R%  No WSL distro found.
        echo     %C%*%R%  Install one: %W%wsl --install -d Ubuntu%R%
        goto :eof
    )
    :wsl_found

    echo     %C%*%R%  Using distro: %W%!WSL_DISTRO!%R%
    echo     %C%*%R%  Running install-deps.sh inside WSL...
    echo.

    :: Copy script to WSL and run it
    wsl -d !WSL_DISTRO! -- bash -c "curl -sL https://raw.githubusercontent.com/NucleOS/cris-os-v2/main/install-deps.sh | bash" 2>&1
    if !errorlevel! equ 0 (
        call :print_ok "WSL dependencies installed"
    ) else (
        echo     %Y%~%R%  Direct download failed. Trying local copy...
        :: Convert Windows path to WSL path
        set "SCRIPT_DIR=%~dp0"
        set "WSL_DIR=!SCRIPT_DIR:\=/!"
        set "WSL_DIR=!WSL_DIR::=:!"
        wsl -d !WSL_DISTRO! -- bash -c "cd '/mnt/!WSL_DIR:~1!' && bash install-deps.sh -y" 2>&1
        if !errorlevel! equ 0 (
            call :print_ok "WSL dependencies installed"
        ) else (
            call :print_fail "WSL installation failed"
        )
    )
    goto :eof

:: ════════════════════════════════════════════════════════════════════════════
:main

:: ── Detect environment ────────────────────────────────────────────────────
call :detect_env

echo     %C%*%R%  Platform:    %W%Windows%R%
echo     %C%*%R%  MSYS2:       %W%!HAS_MSYS2!%R%
echo     %C%*%R%  WSL:         %W%!HAS_WSL!%R%
echo     %C%*%R%  Chocolatey:  %W%!HAS_CHOCO!%R%
echo     %C%*%R%  winget:      %W%!HAS_WINGET!%R%
echo     %C%*%R%  Git Bash:    %W%!HAS_GITBASH!%R%

:: ── Check existing tools ──────────────────────────────────────────────────
call :print_section "Checking Dependencies"

:: GCC
call :check_cmd gcc
if !errorlevel! equ 0 (call :print_ok "GNU C Compiler (gcc)") else (call :print_fail "GNU C Compiler (gcc)  (not found)")

:: ld / as
call :check_cmd ld
if !errorlevel! equ 0 (call :print_ok "GNU Binutils (ld, as)") else (call :print_fail "GNU Binutils (ld, as)  (not found)")

:: make
call :check_cmd make
if !errorlevel! equ 0 (
    call :print_ok "GNU Make"
) else (
    call :check_cmd mingw32-make
    if !errorlevel! equ 0 (
        call :print_ok "GNU Make (mingw32-make)"
    ) else (
        call :print_fail "GNU Make  (not found)"
    )
)

:: nasm
call :check_cmd nasm
if !errorlevel! equ 0 (call :print_ok "Netwide Assembler (nasm)") else (call :print_fail "Netwide Assembler (nasm)  (not found)")

:: xorriso
call :check_cmd xorriso
if !errorlevel! equ 0 (call :print_ok "xorriso") else (call :print_fail "xorriso  (not found)")

:: mformat (mtools)
call :check_cmd mformat
if !errorlevel! equ 0 (call :print_ok "mtools (mformat)") else (call :print_fail "mtools (mformat)  (not found)")

:: python
call :check_cmd python
if !errorlevel! equ 0 (
    call :print_ok "Python"
) else (
    call :check_cmd python3
    if !errorlevel! equ 0 (
        call :print_ok "Python (python3)"
    ) else (
        call :print_fail "Python  (not found)"
    )
)

:: QEMU
call :check_cmd qemu-system-x86_64
if !errorlevel! equ 0 (
    call :print_ok "QEMU x86_64 System Emulator"
) else (
    call :print_fail "QEMU x86_64 System Emulator  (not found)"
)

:: ── Summary ────────────────────────────────────────────────────────────────
echo.
echo     %GR%✓%R%  %GR%%BOLD%Check complete!%R%
echo.

:: ── Ask which method to use ────────────────────────────────────────────────
call :print_section "Install Method"

if "!HAS_MSYS2!"=="1" (
    echo     %C%1%R%  MSYS2          %DIM%(Recommended - full support)%R%
)
if "!HAS_WSL!"=="1" (
    echo     %C%2%R%  WSL            %DIM%(Linux inside Windows)%R%
)
if "!HAS_CHOCO!"=="1" (
    echo     %C%3%R%  Chocolatey     %DIM%(Partial - no gcc/nasm)%R%
)
if "!HAS_WINGET!"=="1" (
    echo     %C%4%R%  winget         %DIM%(Partial - no gcc/nasm)%R%
)
echo     %C%5%R%  Open MSYS2     %DIM%(Download and install MSYS2 first)%R%
echo     %C%6%R%  Manual         %DIM%(Show instructions)%R%
echo.

set /p "_method=    %M%?%R%  %W%Choose method [1-6]:%R% "

if "!_method!"=="1" if "!HAS_MSYS2!"=="1" (
    call :install_msys2
    goto :done
)
if "!_method!"=="2" if "!HAS_WSL!"=="1" (
    call :install_wsl
    goto :done
)
if "!_method!"=="3" if "!HAS_CHOCO!"=="1" (
    call :install_choco
    goto :done
)
if "!_method!"=="4" if "!HAS_WINGET!"=="1" (
    call :install_winget
    goto :done
)
if "!_method!"=="5" (
    echo.
    echo     %C%*%R%  Opening MSYS2 download page...
    start https://www.msys2.org/
    echo.
    echo     %C%*%R%  After installing MSYS2, run this script again.
    goto :done
)
if "!_method!"=="6" (
    goto :manual
)
echo     %RE%✗%R%  Invalid choice.
goto :done

:: ── Manual instructions ───────────────────────────────────────────────────
:manual
call :print_section "Manual Installation"

echo.
echo     %W%Option A: MSYS2 (Recommended)%R%
echo     %DIM%1.  Download from https://www.msys2.org/%R%
echo     %DIM%2.  Install and open MSYS2 UCRT64 terminal%R%
echo     %DIM%3.  Run:%R%
echo         %C%pacman -S --needed gcc binutils make nasm xorriso mtools python qemu-system-x86 qemu-full%R%
echo.
echo     %W%Option B: WSL%R%
echo     %DIM%1.  Install WSL:%R%
echo         %C%wsl --install -d Ubuntu%R%
echo     %DIM%2.  Inside WSL, run:%R%
echo         %C%sudo apt update ^&^& sudo apt install -y gcc binutils make nasm xorriso mtools python3 qemu-system-x86%R%
echo.
echo     %W%Option C: Cross-compile with i686-elf-gcc%R%
echo     %DIM%Build a freestanding GCC cross-compiler for i686-elf:%R%
echo         %C%https://wiki.osdev.org/GCC_Cross-Compiler%R%
echo.
echo     %DIM%Note: Native Windows (MinGW) has limited support for%R%
echo     %DIM%kernel development. MSYS2 or WSL is strongly recommended.%R%

:done
echo.
echo     %GR%✓%R%  %GR%%BOLD%Done!%R%
echo     %C%*%R%  Build NucleOS:
echo         %W%make clean ^&^& make%R%
echo         %W%make iso%R%
echo         %W%make run%R%
echo.

endlocal
