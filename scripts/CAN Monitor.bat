@echo off
title Taktflow CAN Bus Monitor
cd /d "%~dp0.."
python -m tools.can-monitor.main %*
