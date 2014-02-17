@echo off

CD /D %~dp0

sc create "ServerXML" binPath= "%~dp0Server.exe"
