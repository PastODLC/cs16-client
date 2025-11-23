@echo off
REM ========================================
REM Client-Mod 64-bit Build Script для Windows
REM ========================================
REM
REM Требования:
REM   - CMake 3.9 или выше
REM   - MinGW-w64 (GCC) или Clang компилятор
REM   - make или ninja (опционально, для ускорения)
REM
REM Установка MinGW-w64:
REM   1. Скачайте MSYS2: https://www.msys2.org/
REM   2. Установите и запустите MSYS2
REM   3. Выполните: pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
REM   4. Добавьте C:\msys64\mingw64\bin в PATH
REM
REM Или используйте TDM-GCC: https://jmeubank.github.io/tdm-gcc/
REM
setlocal enabledelayedexpansion

echo ========================================
echo Client-Mod 64-bit Build Script
echo ========================================
echo.

REM Проверка наличия CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake не найден в PATH!
    echo Пожалуйста, установите CMake и добавьте его в PATH
    echo Скачать: https://cmake.org/download/
    pause
    exit /b 1
)

echo [OK] CMake найден
cmake --version
echo.

REM Проверка наличия компилятора
set COMPILER_FOUND=0
set CMAKE_C_COMPILER=
set CMAKE_CXX_COMPILER=

REM Проверка GCC (MinGW-w64, TDM-GCC, MSYS2)
where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [OK] GCC найден
    gcc --version | findstr /i "gcc"
    set COMPILER_FOUND=1
    echo.
)

REM Проверка Clang
if %COMPILER_FOUND% EQU 0 (
    where clang >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo [OK] Clang найден
        clang --version | findstr /i "clang"
        set COMPILER_FOUND=1
        set CMAKE_C_COMPILER=clang
        set CMAKE_CXX_COMPILER=clang++
        echo.
    )
)

if %COMPILER_FOUND% EQU 0 (
    echo [WARNING] Компилятор не найден в PATH!
    echo Будет использован компилятор по умолчанию CMake.
    echo.
)

REM Определение генератора
set GENERATOR=
set CMAKE_GENERATOR=

REM Проверка наличия ninja (предпочтительный вариант)
where ninja >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [INFO] Ninja найден, будет использован для ускорения сборки
    set GENERATOR=Ninja
    set CMAKE_GENERATOR=-G "Ninja"
)

REM Если Ninja не найден, пробуем MinGW Makefiles
if "%GENERATOR%"=="" (
    where mingw32-make >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo [INFO] MinGW Make найден
        set GENERATOR=MinGW Makefiles
        set CMAKE_GENERATOR=-G "MinGW Makefiles"
    ) else (
        REM Пробуем обычный make (MSYS2, Git Bash)
        where make >nul 2>&1
        if %ERRORLEVEL% EQU 0 (
            echo [INFO] Make найден, используем MinGW Makefiles
            set GENERATOR=MinGW Makefiles
            set CMAKE_GENERATOR=-G "MinGW Makefiles"
        ) else (
            echo [WARNING] Make не найден!
            echo Пытаемся использовать MinGW Makefiles (может потребоваться make)
            set GENERATOR=MinGW Makefiles
            set CMAKE_GENERATOR=-G "MinGW Makefiles"
        )
    )
)

echo [INFO] Используется генератор: %GENERATOR%
echo.

REM Создание директории для сборки
set BUILD_DIR=build64
if exist %BUILD_DIR% (
    echo [INFO] Директория сборки уже существует: %BUILD_DIR%
) else (
    echo [INFO] Создание директории сборки: %BUILD_DIR%
    mkdir %BUILD_DIR%
)

echo.
echo ========================================
echo Настройка CMake для 64-bit сборки
echo ========================================
echo.

REM Конфигурация CMake для 64-бит
set CMAKE_ARGS=%CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=Release -D64BIT=ON -DUSE_IMGUI=ON -DUSE_DISCORD_RPC=OFF -DUSE_VGUI=OFF -DGOLDSOURCE_SUPPORT=OFF -S . -B %BUILD_DIR%

REM Добавляем специфичные компиляторы если они найдены
if not "%CMAKE_C_COMPILER%"=="" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_C_COMPILER=%CMAKE_C_COMPILER%
)
if not "%CMAKE_CXX_COMPILER%"=="" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_CXX_COMPILER=%CMAKE_CXX_COMPILER%
)

echo Выполняется: cmake %CMAKE_ARGS%
echo.

cmake %CMAKE_ARGS%

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Ошибка при настройке CMake!
    echo.
    echo Возможные решения:
    echo 1. Установите MinGW-w64: https://www.mingw-w64.org/downloads/
    echo    или используйте MSYS2: https://www.msys2.org/
    echo 2. Убедитесь, что компилятор добавлен в PATH
    echo 3. Установите Ninja для ускорения: https://ninja-build.org/
    echo 4. Установите make (mingw32-make или через MSYS2)
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo Запуск сборки
echo ========================================
echo.

REM Определение количества ядер для параллельной сборки
if "%NUMBER_OF_PROCESSORS%"=="" (
    set JOBS=4
) else (
    set JOBS=%NUMBER_OF_PROCESSORS%
)

REM Запуск сборки
echo Используется параллельная сборка с %JOBS% потоками
if "%GENERATOR%"=="Ninja" (
    cmake --build %BUILD_DIR% -j %JOBS%
) else (
    cmake --build %BUILD_DIR% --parallel %JOBS%
)

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Ошибка при сборке!
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
set DLL_FOUND=0
if exist "%BUILD_DIR%\cl_dll\client_amd64.dll" (
    echo [OK] Найден файл: client_amd64.dll
    echo Полный путь: %CD%\%BUILD_DIR%\cl_dll\client_amd64.dll
    set DLL_FOUND=1
)
if exist "%BUILD_DIR%\cl_dll\client.dll" (
    if %DLL_FOUND% EQU 0 (
        echo [OK] Найден файл: client.dll
        echo Полный путь: %CD%\%BUILD_DIR%\cl_dll\client.dll
        set DLL_FOUND=1
    )
)
if %DLL_FOUND% EQU 0 (
    echo [INFO] Ищем скомпилированные файлы в %BUILD_DIR%\cl_dll\...
    if exist "%BUILD_DIR%\cl_dll\" (
        dir /b "%BUILD_DIR%\cl_dll\*.dll" 2>nul
        if %ERRORLEVEL% NEQ 0 (
            echo [WARNING] DLL файлы не найдены в %BUILD_DIR%\cl_dll\
        )
    ) else (
        echo [WARNING] Директория %BUILD_DIR%\cl_dll\ не существует
    )
)

echo.
echo ========================================
echo Готово!
echo ========================================
echo.
pause
