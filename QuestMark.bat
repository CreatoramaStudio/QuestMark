@echo off
rem QuestMark benchmark launcher (double-click). Finds Git for Windows' bash.exe and runs the one-command benchmark:
rem   QuestMark.bat [--quick|--full] [godot] [stride] [ezengine] [--no-setup] [--rid] ...   (see benchmark\tools\run-benchmark.sh)
rem Default: set up everything that is missing, then run the quick subset (about 25 min). Close other programs first.
setlocal
cd /d "%~dp0"
set "QM_BASH="
for %%P in ("%ProgramFiles%\Git\bin\bash.exe" "%ProgramFiles(x86)%\Git\bin\bash.exe" "%LocalAppData%\Programs\Git\bin\bash.exe" "%ProgramW6432%\Git\bin\bash.exe") do (
  if not defined QM_BASH if exist %%P set "QM_BASH=%%~P"
)
if not defined QM_BASH (
  rem fall back to bash on PATH, but skip the WSL launcher in System32 (it cannot run these scripts)
  for /f "delims=" %%B in ('where bash.exe 2^>nul') do (
    if not defined QM_BASH echo %%B | findstr /i /c:"\System32\" >nul || if not defined QM_BASH set "QM_BASH=%%B"
  )
)
if not defined QM_BASH (
  echo Git for Windows was not found. Install it first:  winget install --id Git.Git -e
  echo or download it from https://git-scm.com/download/win , then double-click this file again.
  pause
  exit /b 1
)
echo Using %QM_BASH%
set CHERE_INVOKING=1
"%QM_BASH%" --login "%~dp0benchmark\tools\run-benchmark.sh" %*
set "QM_RC=%ERRORLEVEL%"
echo.
echo Finished with exit code %QM_RC%. Results are in .tmp\results (SUMMARY.md); logs are in .tmp\logs.
pause
exit /b %QM_RC%
