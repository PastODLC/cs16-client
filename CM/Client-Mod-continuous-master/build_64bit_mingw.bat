@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Client-Mod 64-bit Build Script (MinGW)
echo ========================================
echo.

REM Проверка наличия CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake не найден в PATH!
    echo Пожалуйста, установите CMake и добавьте его в PATH.
    pause
    exit /b 1
)

REM Проверка наличия MinGW (gcc или x86_64-w64-mingw32-gcc)
set MINGW_FOUND=0
where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set MINGW_FOUND=1
    echo [INFO] Найден GCC: 
    gcc --version | findstr /i "gcc"
)

where x86_64-w64-mingw32-gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set MINGW_FOUND=1
    echo [INFO] Найден MinGW-w64: 
    x86_64-w64-mingw32-gcc --version | findstr /i "gcc"
)

if %MINGW_FOUND% EQU 0 (
    echo [WARNING] MinGW GCC не найден в PATH!
    echo Пожалуйста, убедитесь, что MinGW-w64 установлен и добавлен в PATH.
    echo.
    echo Попытка продолжить сборку...
    echo.
)

REM Проверка архитектуры компилятора (если gcc доступен)
where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    gcc -dumpmachine | findstr /i "x86_64" >nul 2>&1
    if %ERRORLEVEL% NEQ 0 (
        gcc -dumpmachine | findstr /i "amd64" >nul 2>&1
        if %ERRORLEVEL% NEQ 0 (
            echo [WARNING] Компилятор GCC не является 64-битным!
            echo Для 64-битной сборки необходим MinGW-w64 x86_64.
            echo.
        )
    )
)

REM Настройка переменных окружения для MinGW
REM Можно задать путь к MinGW вручную, если он не в PATH
REM set PATH=C:\mingw64\bin;%PATH%

REM Создание директории для сборки
set BUILD_DIR=build_mingw64
if not exist "%BUILD_DIR%" (
    echo [INFO] Создание директории сборки: %BUILD_DIR%
    mkdir "%BUILD_DIR%"
)

echo.
echo ========================================
echo Конфигурация CMake для 64-bit MinGW
echo ========================================
echo.

REM Конфигурация CMake с MinGW Makefiles генератором для 64-bit
cmake -G "MinGW Makefiles" ^
    -S . ^
    -B "%BUILD_DIR%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -D64BIT=ON ^
    -DUSE_IMGUI=ON ^
    -DUSE_DISCORD_RPC=OFF ^
    -DUSE_VGUI=OFF

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Ошибка конфигурации CMake!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Сборка проекта
echo ========================================
echo.

REM Сборка проекта
REM Для MinGW Makefiles используется параллельная сборка (CMake 3.12+)
echo [INFO] Запуск сборки с использованием всех доступных ядер CPU...
cmake --build "%BUILD_DIR%" --parallel %NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Ошибка сборки!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Сборка завершена успешно!
echo ========================================
echo.
echo Результаты сборки находятся в: %BUILD_DIR%\cl_dll\
echo.

REM Поиск скомпилированной библиотеки
REM Для 64-bit Windows библиотека будет называться client_amd64.dll
echo [INFO] Поиск скомпилированной библиотеки...
set "DLL_FOUND=0"

if exist "%BUILD_DIR%\cl_dll\client_amd64.dll" (
    echo [SUCCESS] Найден client_amd64.dll
    dir "%BUILD_DIR%\cl_dll\client_amd64.dll"
    set "DLL_FOUND=1"
)

if exist "%BUILD_DIR%\cl_dll\client.dll" (
    echo [SUCCESS] Найден client.dll
    dir "%BUILD_DIR%\cl_dll\client.dll"
    set "DLL_FOUND=1"
)

if exist "%BUILD_DIR%\cl_dll\libclient.dll" (
    echo [SUCCESS] Найден libclient.dll
    dir "%BUILD_DIR%\cl_dll\libclient.dll"
    set "DLL_FOUND=1"
)

if "!DLL_FOUND!" EQU "0" (
    echo [WARNING] DLL файл не найден в ожидаемом месте.
    echo Попробуйте найти его в: %BUILD_DIR%\cl_dll\
    echo.
    echo Список всех DLL файлов в директории сборки:
    dir /b "%BUILD_DIR%\cl_dll\*.dll" 2>nul
    if errorlevel 1 (
        echo (DLL файлы не найдены)
    )
)

echo.
pause

