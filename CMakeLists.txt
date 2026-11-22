cmake_minimum_required(VERSION 3.5)

project(ezremote_client)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDONT_HAVE_STRUPR")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
set(APP_VERSION "02.12")

# Shared by both builds. The headless build compiles the same code with
# EZREMOTE_HEADLESS_APP set and without the native UI sources.
set(EZREMOTE_CLIENT_COMMON_SOURCES
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
  source/activity.cpp
  source/config.cpp
  source/crypt.c
  source/fs.cpp
  source/getentropy.c
  source/inifile.c
  source/installer.cpp
  source/payload_loader.cpp
  source/lang.cpp
  source/main.cpp
  source/system.cpp
  source/sfo.cpp
  source/zip_util.cpp
  source/split_file.cpp
  source/mem_file.cpp
  source/memccpy.c
  source/file_stream.cpp
  source/remote_pool.cpp
  source/ui_state.cpp
)

# Everything that needs SDL, ImGui or the app-only SDK libraries.
set(EZREMOTE_CLIENT_NATIVE_SOURCES
  source/imgui/imgui_draw.cpp
  source/imgui/imgui_impl_sdl.cpp
  source/imgui/imgui_impl_sdlrenderer.cpp
  source/imgui/imgui_tables.cpp
  source/imgui/imgui_widgets.cpp
  source/imgui/imgui.cpp
  source/daemon.cpp
  source/fonts.cpp
  source/gui.cpp
  source/ime_dialog.cpp
  source/orbis_jbc.c
  source/textures.cpp
  source/windows.cpp
)

set(EZREMOTE_DAEMON_AUTHINFO "010000000010003800000000001c004000ff00000000008000000000000000000000000000000000000000c000400040000000000000008000000000000000f00040ffff000000f000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" CACHE STRING "create-fself authinfo for the gdd daemon eboot")

if(NOT DEFINED OPENORBIS)
  set(OPENORBIS "$ENV{OPENORBIS}")
endif()

# ---------------------------------------------------------------------------
# Native app + package
# ---------------------------------------------------------------------------
add_executable(ezremote_client ${EZREMOTE_CLIENT_COMMON_SOURCES} ${EZREMOTE_CLIENT_NATIVE_SOURCES})
target_include_directories(ezremote_client PRIVATE source source/pugixml source/imgui)
target_compile_definitions(ezremote_client PRIVATE
  CPPHTTPLIB_OPENSSL_SUPPORT
  CPPHTTPLIB_THREAD_POOL_COUNT=16)

target_link_libraries(ezremote_client
  dbglogger
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
  bz2
  b2
  lzma
  # Only libarchive's lz4 filter references it; nothing in source/ uses LZ4_*.
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
  SceImeDialog
  SceNet
  SceBgft
  SceAppInstUtil
  SceLncUtil
)

# Ship the client version inside the package. The server's launcher reads it
# to know which client build is running.
file(WRITE "${CMAKE_SOURCE_DIR}/data/version.txt" "2.11\n")

# The native UI app is shipped as a self payload for the loader package.
set(EZREMOTE_NATIVEUI_AUTHINFO "000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" CACHE STRING "create-fself authinfo for the native UI self")
set(EZREMOTE_NATIVEUI_SELF "${CMAKE_SOURCE_DIR}/data/daemon/ezremote_client_nativeui.self")
set(EZREMOTE_NATIVEUI_MD5 "${CMAKE_SOURCE_DIR}/data/daemon/ezremote_client_nativeui.md5")
set(EZREMOTE_NATIVEUI_SELF_OUT "${CMAKE_BINARY_DIR}/ezremote_client_nativeui.oelf")
set(EZREMOTE_NATIVEUI_EBOOT "${CMAKE_BINARY_DIR}/nativeui_eboot.bin")
add_custom_command(
  OUTPUT "${EZREMOTE_NATIVEUI_SELF}"
  COMMAND ${CMAKE_COMMAND} -E env "OO_PS4_TOOLCHAIN=${OPENORBIS}"
          "${OPENORBIS}/bin/create-fself"
          "-in=$<TARGET_FILE:ezremote_client>"
          "-out=${EZREMOTE_NATIVEUI_SELF_OUT}"
          "--eboot" "${EZREMOTE_NATIVEUI_EBOOT}"
          "--paid" "0x3800000000000035"
          "--authinfo" "${EZREMOTE_NATIVEUI_AUTHINFO}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/data/daemon"
  COMMAND ${CMAKE_COMMAND} -E copy "${EZREMOTE_NATIVEUI_EBOOT}" "${EZREMOTE_NATIVEUI_SELF}"
  COMMAND sh -c "md5sum '${EZREMOTE_NATIVEUI_SELF}' | cut -d' ' -f1 > '${EZREMOTE_NATIVEUI_MD5}'"
  DEPENDS ezremote_client
  COMMENT "Staging native UI self payload"
  VERBATIM)
add_custom_target(ezremote_client_nativeui_self ALL DEPENDS "${EZREMOTE_NATIVEUI_SELF}")

# ---------------------------------------------------------------------------
# Headless daemon self
# ---------------------------------------------------------------------------
add_executable(ezremote_client_headless
  ${EZREMOTE_CLIENT_COMMON_SOURCES}
  source/orbis_jbc.c)
target_include_directories(ezremote_client_headless PRIVATE source source/pugixml)
target_compile_definitions(ezremote_client_headless PRIVATE
  EZREMOTE_HEADLESS_APP=1
  CPPHTTPLIB_OPENSSL_SUPPORT
  CPPHTTPLIB_THREAD_POOL_COUNT=16)
target_link_libraries(ezremote_client_headless
  dbglogger
  c
  c++
  pthread
  z
  crypto
  ssl
  curl
  lexbor
  smb2
  nfs
  minizip
  bz2
  b2
  lzma
  lz4
  archive
  json-c
  ssh2
  jbc
  kernel
  SceSysmodule
  SceSystemService
  SceRtc
  SceUserService
  SceNet
  SceBgft
  SceAppInstUtil
)
set_target_properties(ezremote_client_headless PROPERTIES SKIP_BUILD_RPATH TRUE)

set(EZREMOTE_DAEMON_SELF_OUT "${CMAKE_BINARY_DIR}/ezremote_client_daemon.oelf")
set(EZREMOTE_DAEMON_EBOOT "${CMAKE_BINARY_DIR}/daemon_eboot.bin")
add_custom_command(
  OUTPUT "${EZREMOTE_DAEMON_EBOOT}"
  COMMAND ${CMAKE_COMMAND} -E env "OO_PS4_TOOLCHAIN=${OPENORBIS}"
          "${OPENORBIS}/bin/create-fself"
          "-in=$<TARGET_FILE:ezremote_client_headless>"
          "-out=${EZREMOTE_DAEMON_SELF_OUT}"
          "--eboot" "${EZREMOTE_DAEMON_EBOOT}"
          "--paid" "0x3800000000000035"
          "--authinfo" "${EZREMOTE_DAEMON_AUTHINFO}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/data/daemon"
  COMMAND ${CMAKE_COMMAND} -E copy "${EZREMOTE_DAEMON_EBOOT}" "${CMAKE_SOURCE_DIR}/data/daemon/ezremote_client_webui.self"
  COMMAND sh -c "md5sum '${CMAKE_SOURCE_DIR}/data/daemon/ezremote_client_webui.self' | cut -d' ' -f1 > '${CMAKE_SOURCE_DIR}/data/daemon/ezremote_client_webui.md5'"
  DEPENDS ezremote_client_headless
  COMMENT "Creating gdd daemon eboot.bin with custom authinfo"
  VERBATIM)
add_custom_target(ezremote_client_daemon_self ALL DEPENDS "${EZREMOTE_DAEMON_EBOOT}")

# ---------------------------------------------------------------------------
# Loader app + package
# ---------------------------------------------------------------------------
add_executable(ezremote_loader
  ps4-ezremote-loader/main.c
  ps4-ezremote-loader/daemon.c
  ps4-ezremote-loader/payload_loader.c
  source/orbis_jbc.c)
target_include_directories(ezremote_loader PRIVATE source ps4-ezremote-loader)
target_compile_definitions(ezremote_loader PRIVATE EZREMOTE_LOADER=1)
target_link_libraries(ezremote_loader
  c
  jbc
  kernel
  SceSysmodule
  SceSystemService
  SceLncUtil
  SceNet
)
set_target_properties(ezremote_loader PROPERTIES SKIP_BUILD_RPATH TRUE)

add_self(ezremote_loader)
add_pkg(ezremote_loader "${CMAKE_SOURCE_DIR}/data" "RMTC00001" "ezRemote Client" "${APP_VERSION}" 32 0)

# The loader package carries both client self payloads.
add_dependencies(ezremote_loader_pkg ezremote_client_nativeui_self ezremote_client_daemon_self)

# ---------------------------------------------------------------------------
# Optional upload target
# ---------------------------------------------------------------------------
set(PS4_FTP_URL "ftp://192.168.100.70:2121/data/pkg" CACHE STRING "PS4 FTP folder the package is uploaded to")
set(PS4_FTP_DATA_DIR "/data/ezremote-client" CACHE STRING "PS4 folder for the web assets and daemon payloads")
add_custom_target(upload
  DEPENDS ezremote_loader_pkg
  COMMAND cd ${CMAKE_BINARY_DIR} && for pkg in IV0001-*.pkg \; do curl -sS --fail --ftp-create-dirs -T \"$$pkg\" \"${PS4_FTP_URL}/$$pkg\" || exit 1 \; done
  COMMENT "Uploading package to ${PS4_FTP_URL}"
)
