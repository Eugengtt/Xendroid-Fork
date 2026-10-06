@echo off
title Project Cleaner

echo.
echo ============================
echo      PROJECT CLEANER
echo ============================
echo.

if exist ".cxx" (
    echo Removing .cxx...
    rmdir /s /q ".cxx"
)

if exist "build" (
    echo Removing build...
    rmdir /s /q "build"
)

if exist ".gradle" (
    echo Removing .gradle...
    rmdir /s /q ".gradle"
)

if exist "app\build" (
    echo Removing app\build...
    rmdir /s /q "app\build"
)

if exist "emulator-core\.cxx" (
    echo Removing emulator-core\.cxx...
    rmdir /s /q "emulator-core\.cxx"
)

if exist "emulator-core\build" (
    echo Removing emulator-core\build...
    rmdir /s /q "emulator-core\build"
)

if exist ".git" (
    echo Removing .git...
    rmdir /s /q ".git"
)

if exist "xendroid-fork.jks" (
    echo Removing xendroid-fork.jks...
    del /f /q "xendroid-fork.jks"
)

if exist "Xendroid-red.jpg" (
    echo Removing Xendroid-red.jpg...
    del /f /q "Xendroid-red.jpg"
)

echo.
echo ============================
echo Cleanup complete.
echo ============================
echo.
pause