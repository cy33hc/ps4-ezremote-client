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


option(EZREMOTE_HEADLESS_APP "Build the headless client as an OpenOrbis eboot" OFF)
set(EZREMOTE_DAEMON_AUTHINFO "010000000010003800000000001c004000ff00000000008000000000000000000000000000000000000000c000400040000000000000008000000000000000f00040ffff000000f000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" CACHE STRING "create-fself authinfo for the gdd daemon eboot")

if(EZREMOTE_HEADLESS_APP)
  # OpenOrbis headless app: the payload code paths without SDL/ImGui, plus jbc
  # so it can still reach /data when launched as a gdd daemon.
  add_executable(ezremote_client.elf
    ${EZREMOTE_CLIENT_COMMON_SOURCES}
    source/daemon.cpp
    source/orbis_jbc.c)
  target_include_directories(ezremote_client.elf PRIVATE source source/pugixml)
  target_compile_definitions(ezremote_client.elf PRIVATE
    EZREMOTE_HEADLESS_APP=1
    CPPHTTPLIB_OPENSSL_SUPPORT
    CPPHTTPLIB_THREAD_POOL_COUNT=32)
  target_link_libraries(ezremote_client.elf
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
    SceLncUtil
  )
  set_target_properties(ezremote_client.elf PROPERTIES SKIP_BUILD_RPATH TRUE)

  if(NOT DEFINED OPENORBIS)
    set(OPENORBIS "$ENV{OPENORBIS}")
  endif()
  set(EZREMOTE_SELF_OUT "${CMAKE_BINARY_DIR}/ezremote_client.oelf")
  set(EZREMOTE_EBOOT "${CMAKE_BINARY_DIR}/eboot.bin")
  add_custom_command(
    OUTPUT "${EZREMOTE_EBOOT}"
    COMMAND ${CMAKE_COMMAND} -E env "OO_PS4_TOOLCHAIN=${OPENORBIS}"
            "${OPENORBIS}/bin/create-fself"
            "-in=$<TARGET_FILE:ezremote_client.elf>"
            "-out=${EZREMOTE_SELF_OUT}"
            "--eboot" "${EZREMOTE_EBOOT}"
            "--paid" "0x3800000000000035"
            "--authinfo" "${EZREMOTE_DAEMON_AUTHINFO}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/data/daemon"
    COMMAND ${CMAKE_COMMAND} -E copy "${EZREMOTE_EBOOT}" "${CMAKE_SOURCE_DIR}/data/daemon/ezremote_client.self"
    COMMAND sh -c "md5sum '${CMAKE_SOURCE_DIR}/data/daemon/ezremote_client.self' | cut -d' ' -f1 > '${CMAKE_SOURCE_DIR}/data/daemon/ezremote_client.md5'"
    DEPENDS ezremote_client.elf
    COMMENT "Creating gdd daemon eboot.bin with custom authinfo"
    VERBATIM)
  add_custom_target(eboot ALL DEPENDS "${EZREMOTE_EBOOT}")
  add_custom_target(elf DEPENDS ezremote_client.elf)
else()
  add_executable(ezremote_client ${EZREMOTE_CLIENT_COMMON_SOURCES} ${EZREMOTE_CLIENT_NATIVE_SOURCES})
  target_include_directories(ezremote_client PRIVATE source source/pugixml source/imgui)
  target_compile_definitions(ezremote_client PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    CPPHTTPLIB_THREAD_POOL_COUNT=32)

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
    COMMENT "Uploading package and daemon payloads to ${PS4_FTP_DATA_DIR}"
  )
endif()
