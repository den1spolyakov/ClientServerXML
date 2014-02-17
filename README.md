<h2>ClientServerXML</h2>
===============

This repository contains server, which is designed as Windows Service, and client program for this server.

To register server navigate to build folder and run register.cmd. After that navigate to 
Control Panel->Administrative Tools->Services and run ServerXML service. After that you can 
use client to connect and work with server.

To delete service run unregister.cmd

Pairs.xml is stored somewhere in Windows folder (C:\Windows\SysWOW64 for Windows 7 x64).
