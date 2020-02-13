# tgm-server
A C++ server, made for interact with Telegram Bot API

Tested on Windows 10, Linux (Debian 8), may work on other plaftorms

Developed with Qt 5.10.1

Depends on civetweb as embedded C/C++ web server

* mkdir tgm
* cd tgm
* git clone git@github.com:civetweb/civetweb.git
* git clone git@github.com:albertino80/tgm-server.git
* cd tgm-server
* open tgm-server.pro with Qt Creator
* ensure to have OpenSSL binaries in PATH (for HTTPS)
* set ./tgm-server as workdir to debug
