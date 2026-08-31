@echo off
rem agentd 构建脚本：临时把 D:\Tools\go 加入 PATH，不污染系统环境
setlocal
set "GOBIN=D:\Tools\go\bin"
if exist "%GOBIN%\go.exe" (
    set "PATH=%GOBIN%;%PATH%"
) else (
    echo [err] 未找到 D:\Tools\go\bin\go.exe，请先安装 Go 到 D:\Tools\go
    exit /b 1
)
cd /d "%~dp0"
go vet .\...
if errorlevel 1 exit /b 1
go build -ldflags "-s -w" -o bin\agentd.exe .
if errorlevel 1 exit /b 1
echo [ok] bin\agentd.exe