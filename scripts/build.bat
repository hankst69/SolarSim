@echo off
cd "%~dp0.."
cd

:: https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_execution_policies?view=powershell-7.6
:: powershell Set-ExecutionPolicy RemoteSigned

rem powershell scripts\test.ps1 
rem powershell scripts\test.ps1 -Clean -Configuration Release
rem powershell scripts\build.ps1 -Clean -Configuration Debug

powershell .\scripts\test.ps1  -BuildDir out/build_RelTests -Clean -Configuration Release
powershell .\scripts\build.ps1 -BuildDir out/build_VS22            -Configuration Release -Generator 'Visual Studio 17 2022' -NoTests
