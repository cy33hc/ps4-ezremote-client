cmake_minimum_required(VERSION 3.0)

project(ezremote_client)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDONT_HAVE_STRUPR")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
add_definitions(-DCPPHTTPLIB_THREAD_POOL_COUNT=16)
add_definitions(-DDAEMON=Y)

include_directories(
  ../source
  ../source/pugixml
)

add_executable(ezremote_client
  ../source/pugixml/pugixml.cpp
  ../source/web/callback.cpp
  ../source/web/fsinfo.cpp
  ../source/web/header.cpp
  ../source/web/request.cpp
  ../source/web/urn.cpp
  ../source/webdav/client.cpp
  ../source/http/httplib.cpp
  ../source/clients/baseclient.cpp
  ../source/clients/apache.cpp
  ../source/clients/ftpclient.cpp
  ../source/clients/gdrive.cpp
  ../source/clients/iis.cpp
  ../source/clients/nginx.cpp
  ../source/clients/npxserve.cpp
  ../source/clients/nfsclient.cpp
  ../source/clients/smbclient.cpp
  ../source/clients/webdavclient.cpp
  ../source/clients/sftpclient.cpp
  ../source/server/http_server.cpp
  ../source/config.cpp
  ../source/crypt.c
  ../source/fs.cpp
  ../source/getentropy.c
  ../source/inifile.c
  ../source/installer.cpp
  ../source/lang.cpp
  ../source/daemon/main.cpp
  ../source/orbis_jbc.c
  ../source/system.cpp
  ../source/sfo.cpp
  ../source/zip_util.cpp
)

add_self(ezremote_client)

add_pkg(ezremote_client ${CMAKE_SOURCE_DIR}/data "RMTC00002" "ezRemote Client Daemon" "01.00")

target_link_libraries(ezremote_client
  dbglogger
  c
  c++
  z
  pthread
  jbc
  crypto
  ssl
  curl
  lexbor
  smb2
  nfs
  minizip
  un7zip
  unrar
  json-c
  ssh2
  kernel
  SceShellCoreUtil
  SceSysmodule
  SceSystemService
  SceRtc
  SceUserService
  SceSysUtil
  SceNet
  SceBgft
  SceAppInstUtil
)
