@echo off
title SC Serial Monitor (COM11)
cd /d "%~dp0.."
python scripts/serial-monitor.py COM11 115200
pause
