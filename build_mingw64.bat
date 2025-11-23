@echo off
REM ========================================
REM CS16-Client MinGW-w64 Build Script
REM ========================================
REM 
REM Этот скрипт собирает CS16-Client для 64-битной Windows используя MinGW-w64
REM 
REM Требования:
REM   - MinGW-w64 (64-bit) в PATH или переменная MINGW_PATH
REM   - CMake 3.10 или выше
REM   - Python 3
REM   - Git
REM 
REM Использование:
REM   build_mingw64.bat
REM 
REM Или с указанием пути к MinGW:
REM   set MINGW_PATH=C:\mingw64
REM   build_mingw64.bat
REM ========================================

setlocal enabledelayedexpansion

echo ========================================
echo CS16-Client MinGW-w64 Build Script
echo ========================================
echo.

REM Проверка наличия MinGW-w64
where gcc >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] MinGW-w64 не найден в PATH!
    echo Пожалуйста, установите MinGW-w64 и добавьте его в PATH
    echo Или укажите путь к MinGW-w64 в переменной MINGW_PATH
    if defined MINGW_PATH (
        set PATH=%MINGW_PATH%\bin;%PATH%
        echo Используется MINGW_PATH: %MINGW_PATH%
    ) else (
        echo Пример: set MINGW_PATH=C:\mingw64
        exit /b 1
    )
)

REM Проверка версии GCC
echo Проверка компилятора...
gcc -v 2>&1 | findstr /i "x86_64" >nul
if %ERRORLEVEL% NEQ 0 (
    echo [WARNING] Возможно, используется не 64-битный компилятор
)

REM Проверка наличия CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake не найден в PATH!
    echo Пожалуйста, установите CMake и добавьте его в PATH
    exit /b 1
)

REM Проверка наличия Python
where python >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Python не найден в PATH!
    echo Пожалуйста, установите Python и добавьте его в PATH
    exit /b 1
)

echo [OK] Все необходимые инструменты найдены
echo.

REM Получение пути к скрипту
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

REM Проверка наличия Git
where git >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Git не найден в PATH!
    echo Пожалуйста, установите Git и добавьте его в PATH
    exit /b 1
)

REM Проверка и клонирование подмодулей
echo ========================================
echo Проверка подмодулей 3rdparty...
echo ========================================

REM Попытка использовать git submodule, если это git репозиторий
if exist ".git" (
    echo Обновление подмодулей через git submodule...
    git submodule update --init --recursive --depth 1
    if %ERRORLEVEL% EQU 0 (
        echo [OK] Подмодули обновлены через git submodule
        goto :submodules_done
    ) else (
        echo [WARNING] Не удалось обновить подмодули через git submodule, попробуем вручную...
    )
)

REM Ручное клонирование подмодулей, если git submodule не сработал
if not exist "3rdparty\yapb\.git" (
    echo Клонирование yapb...
    if exist "3rdparty\yapb" rmdir /s /q "3rdparty\yapb"
    git clone --depth 1 https://github.com/yapb/yapb.git "3rdparty\yapb"
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Не удалось клонировать yapb
        exit /b 1
    )
) else (
    echo [OK] yapb уже существует
)

if not exist "3rdparty\mainui_cpp\.git" (
    echo Клонирование mainui_cpp...
    if exist "3rdparty\mainui_cpp" rmdir /s /q "3rdparty\mainui_cpp"
    git clone --depth 1 https://github.com/Velaron/mainui_cpp.git "3rdparty\mainui_cpp"
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Не удалось клонировать mainui_cpp
        exit /b 1
    )
) else (
    echo [OK] mainui_cpp уже существует
)

if not exist "3rdparty\ReGameDLL_CS\.git" (
    echo Клонирование ReGameDLL_CS...
    if exist "3rdparty\ReGameDLL_CS" rmdir /s /q "3rdparty\ReGameDLL_CS"
    git clone --depth 1 https://github.com/Velaron/ReGameDLL_CS.git "3rdparty\ReGameDLL_CS"
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Не удалось клонировать ReGameDLL_CS
        exit /b 1
    )
) else (
    echo [OK] ReGameDLL_CS уже существует
)

if not exist "3rdparty\miniutl\.git" (
    echo Клонирование miniutl...
    if exist "3rdparty\miniutl" rmdir /s /q "3rdparty\miniutl"
    git clone --depth 1 https://github.com/FWGS/MiniUTL.git "3rdparty\miniutl"
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Не удалось клонировать miniutl
        exit /b 1
    )
) else (
    echo [OK] miniutl уже существует
)

:submodules_done

REM Исправление устаревшей версии CMake в mainui_cpp (если требуется)
if exist "3rdparty\mainui_cpp\CMakeLists.txt" (
    echo Проверка версии CMake в mainui_cpp...
    findstr /C:"cmake_minimum_required(VERSION  2.8.0)" "3rdparty\mainui_cpp\CMakeLists.txt" >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        echo Исправление устаревшей версии CMake в mainui_cpp...
        REM Используем временный файл для замены
        (
            for /f "usebackq delims=" %%a in ("3rdparty\mainui_cpp\CMakeLists.txt") do (
                set "line=%%a"
                set "line=!line:cmake_minimum_required(VERSION  2.8.0)=cmake_minimum_required(VERSION 3.10)!"
                echo(!line!
            )
        ) > "3rdparty\mainui_cpp\CMakeLists.txt.tmp"
        move /y "3rdparty\mainui_cpp\CMakeLists.txt.tmp" "3rdparty\mainui_cpp\CMakeLists.txt" >nul
        if !ERRORLEVEL! EQU 0 (
            echo [OK] Версия CMake обновлена в mainui_cpp
        )
    )
)

echo.
echo ========================================
echo Настройка CMake для MinGW-w64 (64-bit)
echo ========================================

REM Обработка существующей директории build
if exist "build" (
    echo Директория build уже существует.
    set /p CLEAN_BUILD="Очистить её перед сборкой? (y/n): "
    if /i "!CLEAN_BUILD!"=="y" (
        echo Очистка директории build...
        rmdir /s /q build
        mkdir build
    )
) else (
    mkdir build
)
cd build

REM Настройка CMake для MinGW-w64
echo Запуск CMake...
cmake .. -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER=gcc ^
    -DCMAKE_CXX_COMPILER=g++ ^
    -DCMAKE_C_FLAGS="-m64 -DWIN32" ^
    -DCMAKE_CXX_FLAGS="-m64 -DWIN32" ^
    -DBUILD_CLIENT=ON ^
    -DBUILD_MAINUI=ON ^
    -DBUILD_SERVER=OFF

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Ошибка настройки CMake
    cd ..
    exit /b 1
)

echo.
echo ========================================
echo Сборка проекта
echo ========================================

REM Сборка проекта
cmake --build . --parallel %NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Ошибка сборки
    cd ..
    exit /b 1
)

echo.
echo ========================================
echo Сборка завершена успешно!
echo ========================================
echo Результаты сборки находятся в папке: build
echo.

cd ..
pause

