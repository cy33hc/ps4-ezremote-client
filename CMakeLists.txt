cmake_minimum_required(VERSION 3.5)

project(ezremote_client)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDONT_HAVE_STRUPR")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
set(APP_VERSION "02.12")

# Includes and definitions are target-scoped so the payload build can also add
# ps4-ezremote-server without leaking this project's headers into its sources.

# Shared by both builds. The payload build compiles the same code with
# EZREMOTE_PAYLOAD set and without the native UI sources.
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
  source/fonts.cpp
  source/gui.cpp
  source/ime_dialog.cpp
  source/orbis_jbc.c
  source/textures.cpp
  source/windows.cpp
)

if(DEFINED PS4_PAYLOAD_SDK)
  # Payload SDK build: a headless ELF with no native UI, loadable with elfldr.
  # The app/openorbis SDK is still needed for the orbis/ headers and the
  # libSceBgft.so stub, which the payload SDK does not ship.
  set(EZREMOTE_ORBIS_SDK "" CACHE PATH "OpenOrbis SDK used for headers and app stubs in the payload build")
  if(NOT EZREMOTE_ORBIS_SDK)
    if(DEFINED ENV{OPENORBIS})
      set(EZREMOTE_ORBIS_SDK "$ENV{OPENORBIS}")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/.sdk/pacbrew/ps4/openorbis/include/orbis")
      set(EZREMOTE_ORBIS_SDK "${CMAKE_SOURCE_DIR}/.sdk/pacbrew/ps4/openorbis")
    else()
      set(EZREMOTE_ORBIS_SDK "/opt/pacbrew/ps4/openorbis")
    endif()
  endif()
  if(NOT EXISTS "${EZREMOTE_ORBIS_SDK}/include/orbis")
    message(FATAL_ERROR "OpenOrbis SDK headers not found at ${EZREMOTE_ORBIS_SDK}; set -DEZREMOTE_ORBIS_SDK=<openorbis sdk>")
  endif()

  # libSceBgft/libSceAppInstUtil are app libraries that a payload loader cannot
  # resolve. They are loaded at runtime by source/payload_modules.cpp instead.
  add_executable(ezremote_client.elf ${EZREMOTE_CLIENT_COMMON_SOURCES} source/payload_modules.cpp)
  target_include_directories(ezremote_client.elf PRIVATE source source/pugixml)
  target_compile_definitions(ezremote_client.elf PRIVATE
    EZREMOTE_PAYLOAD=1
    __BSD_VISIBLE
    _BSD_SOURCE
    CPPHTTPLIB_OPENSSL_SUPPORT
    CPPHTTPLIB_THREAD_POOL_COUNT=16)
  # This payload SDK installation ships minizip headers that include "ints.h"
  # without shipping the file itself. Provide the one typedef they use.
  set(EZREMOTE_COMPAT_INCLUDE "${CMAKE_BINARY_DIR}/payload-compat-include")
  file(MAKE_DIRECTORY "${EZREMOTE_COMPAT_INCLUDE}")
  file(WRITE "${EZREMOTE_COMPAT_INCLUDE}/ints.h"
    "#ifndef INTS_H\n#define INTS_H\ntypedef unsigned long long ui64_t;\n#endif\n")
  target_include_directories(ezremote_client.elf PRIVATE "${EZREMOTE_COMPAT_INCLUDE}")
  # orbis/ headers only; -idirafter keeps the payload SDK's libc headers first.
  # The payload SDK's ifaddrs.h assumes <sys/types.h> was included first, and
  # its inifile translation unit needs <ctype.h> for islower/toupper.
  target_compile_options(ezremote_client.elf PRIVATE
    "-idirafter" "${EZREMOTE_ORBIS_SDK}/include"
    "-includesys/types.h"
    "-includectype.h"
    # Keep the payload small enough for ScePartyDaemon's loader.
    -Oz -ffunction-sections -fdata-sections)
  target_link_options(ezremote_client.elf PRIVATE
    "-Wl,--gc-sections"
    "-Wl,--icf=all")
  target_link_libraries(ezremote_client.elf
    z
    crypto
    ssl
    json-c
    smb2
    ssh2
    nfs
    archive
    zstd
    deflate
    bz2
    lzma
    minizip
    psl
    curl
    lexbor_static
    kernel_sys
    SceSysmodule
    SceSystemService
    SceUserService
    SceNet
  )

  set_target_properties(ezremote_client.elf PROPERTIES SKIP_BUILD_RPATH TRUE)

  # Stage a stripped copy for the pkg's data/daemon folder. The server payload
  # (data/daemon/ezremote-server.elf) is produced by the subdirectory below and
  # is not overwritten here.
  set(EZREMOTE_DAEMON_DIR "${CMAKE_SOURCE_DIR}/data/daemon")
  add_custom_command(TARGET ezremote_client.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${EZREMOTE_DAEMON_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ezremote_client.elf>"
            "${EZREMOTE_DAEMON_DIR}/ezremote_client.elf"
    COMMAND "${PS4_PAYLOAD_SDK}/bin/orbis-strip" "${EZREMOTE_DAEMON_DIR}/ezremote_client.elf"
    COMMENT "Staging stripped payload ELF in data/daemon"
    VERBATIM)

  add_custom_target(elf DEPENDS ezremote_client.elf)

  # Copy the console web assets next to the build so they can be uploaded to
  # /data/ezremote-client/assets on the PS4.
  add_custom_target(payload-data
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/payload-data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data/assets" "${CMAKE_BINARY_DIR}/payload-data/assets"
    COMMENT "Staging data/assets for /data/ezremote-client/assets")

  # The ezRemote Server payload uses the same PS4_PAYLOAD_SDK/toolchain. Its
  # `package` target strips the server ELF and copies it to
  # data/daemon/ezremote-server.elf, so building the client ELF also refreshes
  # the bundled server payload.
  if(EXISTS "${CMAKE_SOURCE_DIR}/ps4-ezremote-server/CMakeLists.txt")
    add_subdirectory(ps4-ezremote-server EXCLUDE_FROM_ALL)
    add_dependencies(ezremote_client.elf package)
  else()
    message(WARNING "ps4-ezremote-server submodule is missing; building the client ELF without the server payload")
  endif()
else()
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
  file(WRITE "${CMAKE_SOURCE_DIR}/data/version.txt" "${APP_VERSION}\n")

  add_self(ezremote_client)
  add_pkg(ezremote_client ${CMAKE_SOURCE_DIR}/data "RMTC00001" "ezRemote Client" "${APP_VERSION}" 32 0)

  set(PS4_FTP_URL "ftp://192.168.100.70:2121/data/pkg" CACHE STRING "PS4 FTP folder the package is uploaded to")
  set(PS4_FTP_DATA_DIR "/data/ezremote-client" CACHE STRING "PS4 folder for the web assets and daemon payloads")
  add_custom_target(upload
    DEPENDS ezremote_client_pkg
    COMMAND cd ${CMAKE_BINARY_DIR} && for pkg in IV0001-*.pkg \; do curl -sS --fail --ftp-create-dirs -T \"$$pkg\" \"${PS4_FTP_URL}/$$pkg\" || exit 1 \; done
    COMMAND python3 -u "${CMAKE_SOURCE_DIR}/scripts/upload_ftp.py"
            "${PS4_FTP_URL}" "${PS4_FTP_DATA_DIR}"
            "${CMAKE_SOURCE_DIR}/data/assets" "${CMAKE_SOURCE_DIR}/data/daemon"
    COMMENT "Uploading package, assets and daemon payloads to ${PS4_FTP_DATA_DIR}"
  )
endif()
