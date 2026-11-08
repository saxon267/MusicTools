@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"

rem ---------------------------------------------------------------- find python
set "PY="
where python >nul 2>nul && set "PY=python"
if not defined PY (
    where py >nul 2>nul && set "PY=py -3"
)
if not defined PY (
    echo.
    echo  [错误] 没有找到 Python 3。
    echo  请先安装 Python 3 并勾选 "Add python.exe to PATH"：
    echo  https://www.python.org/downloads/
    echo.
    pause
    exit /b 1
)

rem ---------------------------------------------------------------- pyserial
%PY% -c "import serial" >nul 2>nul
if errorlevel 1 (
    echo 正在安装 pyserial ...
    %PY% -m pip install --disable-pip-version-check -q pyserial
    if errorlevel 1 (
        echo 直连失败，改用国内镜像 ...
        %PY% -m pip install --disable-pip-version-check -q -i https://mirrors.aliyun.com/pypi/simple/ pyserial
    )
    %PY% -c "import serial" >nul 2>nul
    if errorlevel 1 (
        echo.
        echo  [错误] pyserial 安装失败，请手动执行：
        echo        python -m pip install pyserial
        echo.
        pause
        exit /b 1
    )
)

rem ---------------------------------------------------------------- launch
if "%~1"=="" (
    rem no arguments: start without a console window
    where pythonw >nul 2>nul && start "" pythonw cardputer_sd.py && goto :eof
    start "" %PY% cardputer_sd.py
    goto :eof
)

rem with arguments (for example --mock or --port COM5) keep the console so that
rem any error message stays readable
%PY% cardputer_sd.py %*
if errorlevel 1 pause
