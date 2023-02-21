cmake_minimum_required(VERSION 3.0)

project(ezremote_client)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDONT_HAVE_STRUPR")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)

include_directories(
  source
  source/pugixml
)

add_executable(ezremote_client
  source/pugixml/pugixml.cpp
  source/web/callback.cpp
  source/web/fsinfo.cpp
  source/web/header.cpp
  source/web/request.cpp
  source/web/urn.cpp
  source/webdav/client.cpp
  source/http/httplib.cpp
  source/http/baseclient.cpp
  source/http/apache.cpp
  source/http/iis.cpp
  source/http/npxserve.cpp
  source/actions.cpp
  source/config.cpp
  source/fs.cpp
  source/ftpclient.cpp
  source/gui.cpp
  source/getentropy.c
  source/ime_dialog.cpp
  source/inifile.c
  source/installer.cpp
  source/lang.cpp
  source/main.cpp
  source/orbis_jbc.c
  source/rtc.cpp
  source/smbclient.cpp
  source/windows.cpp
  source/webdavclient.cpp
  source/zip_util.cpp
  source/imgui_draw.cpp
  source/imgui_impl_sdl.cpp
  source/imgui_impl_sdlrenderer.cpp
  source/imgui_tables.cpp
  source/imgui_widgets.cpp
  source/imgui.cpp
)

add_self(ezremote_client)

add_pkg(ezremote_client ${CMAKE_SOURCE_DIR}/data "RMTC00001" "ezRemote Client" "01.01" 32 0)

target_link_libraries(ezremote_client
  dbglogger
  c
  c++
  png
  z
  pthread
  SDL2
  samplerate
  jbc
  crypto
  ssl
  curl
  lexbor
  smb2
  minizip
  un7zip
  unrar
  kernel
  SceShellCoreUtil
  SceSysmodule
  SceSystemService
  ScePigletv2VSH
  ScePrecompiledShaders
  SceRtc
  SceUserService
  ScePad
  SceAudioOut
  SceSysUtil
  SceImeDialog
  SceNet
  SceBgft
  SceAppInstUtil
  SceLncUtil
)
