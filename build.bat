@echo off

:: https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_execution_policies?view=powershell-7.6
:: powershell Set-ExecutionPolicy RemoteSigned

powershell scripts\win_test.ps1
powershell scripts\win_test.ps1 -Clean -Configuration Release
powershell scripts\win_build.ps1 -Clean -Configuration Debug

powershell scripts\win_build.ps1 -Clean -Configuration Release -Generator 'Visual Studio 17 2022' -NoTests -BuildDir build_vs22
