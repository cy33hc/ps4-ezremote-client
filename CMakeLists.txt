cmake_minimum_required(VERSION 3.5)

project(ezremote_client)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDONT_HAVE_STRUPR")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
add_definitions(-DCPPHTTPLIB_THREAD_POOL_COUNT=16)

include_directories(
  source
  source/pugixml
  source/imgui
)

add_executable(ezremote_client
  source/imgui/imgui_draw.cpp
  source/imgui/imgui_impl_sdl.cpp
  source/imgui/imgui_impl_sdlrenderer.cpp
  source/imgui/imgui_tables.cpp
  source/imgui/imgui_widgets.cpp
  source/imgui/imgui.cpp
  source/pugixml/pugixml.cpp
  source/http/httplib.cpp
  source/clients/baseclient.cpp
  source/clients/apache.cpp
  source/clients/archiveorg.cpp
  source/clients/ftpclient.cpp
  source/clients/gdrive.cpp
  source/clients/github.cpp
  source/clients/myrient.cpp
  source/clients/iis.cpp
  source/clients/nginx.cpp
  source/clients/npxserve.cpp
  source/clients/nfsclient.cpp
  source/clients/smbclient.cpp
  source/clients/sftpclient.cpp
  source/clients/rclone.cpp
  source/clients/webdav.cpp
  source/filehost/alldebrid.cpp
  source/filehost/realdebrid.cpp
  source/filehost/directhost.cpp
  source/filehost/gdrive.cpp
  source/filehost/filehost.cpp
  source/filehost/1fichier.cpp
  source/filehost/mediafire.cpp
  source/filehost/pixeldrain.cpp
  source/server/http_server.cpp
  source/actions.cpp
  source/config.cpp
  source/crypt.c
  source/fs.cpp
  source/gui.cpp
  source/getentropy.c
  source/ime_dialog.cpp
  source/inifile.c
  source/installer.cpp
  source/lang.cpp
  source/main.cpp
  source/orbis_jbc.c
  source/system.cpp
  source/sfo.cpp
  source/textures.cpp
  source/windows.cpp
  source/zip_util.cpp
  source/split_file.cpp
  source/mem_file.cpp
)

add_self(ezremote_client)

add_pkg(ezremote_client ${CMAKE_SOURCE_DIR}/data "RMTC00001" "ezRemote Client" "01.37" 32 0)

target_link_libraries(ezremote_client
  c
  c++
  png
  webp
  jpeg
  z
  pthread
  SDL2
  SDL2_image
  samplerate
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
  bz2
  b2
  lzma
  lz4
  archive
  json-c
  ssh2
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
)
