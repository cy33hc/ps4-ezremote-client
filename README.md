# ezRemote Client

ezRemote Client is an application that allows you to connect the PS4 to remote FTP, SMB, WebDAV and HTTP servers to transfer files. The interface is inspired by Filezilla client which provides a commander like GUI.
![Preview](/screenshot.jpg)
## Usage
To distinguish between FTP, SMB, WebDAV or HTTP, the URL must be prefix with **ftp://**, **smb://**, **dav://**, **davs://**, **http://** and **https://**

 - The url format for FTP is
   ```
   ftp://hostname[:port]

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 21 if not provided
   ```

 - The url format for SMB is
   ```
   smb://hostname[:port]/sharename

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 445 if not provided
     - sharename is required
   ```

 - The url format for WebDAV is
   ```
   webdav://hostname[:port]/[url_path]
   webdavs://hostname[:port]/[url_path]

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 80(webdav) and 443(webdavs) if not provided
     - url_path is optional based on your WebDAV hosting requiremets
   ```

- The url format for HTTP Server is
   ```
   http://hostname[:port]/[url_path]
   https://hostname[:port]/[url_path]
     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 80(http) and 443(https) if not provided
     - url_path is optional based on your HTTP Server hosting requiremets
   ```

Tested with following WebDAV server:
 - **(Recommeded)** [Dufs](https://github.com/sigoden/dufs) - For hosting your own WebDAV server. (Recommended since this allow anonymous access which is required for Remote Package Install)
 - [SFTPgo](https://github.com/drakkan/sftpgo) - For local hosted WebDAV server. Can also be used as a webdav frontend for Cloud Storage like AWS S3, Azure Blob or Google Storage.
 - box.com (Note: delete folder does not work. This is an issue with box.com and not the app)
 - mega.nz (via the [megacmd tool](https://mega.io/cmd))
 - 4shared.com
 - drivehq.com

## Remote Package Installer Feature
Remote Package Installation only works if the WebDAV server allow anonymous access. It's a limitation of the PS4 Installer not able to access protected links. As suggested, use the [Dufs](https://github.com/sigoden/dufs) app for WebDAV.

## Features ##
 - Transfer files back and forth between PS4 and FTP/SMB/WebDAV server
 - Support for connecting to Http Servers like (Apache/Nginx,Microsoft IIS, Serve) with directory listings to download or install pkg. 
 - Install Remote Packages from connected WebDAV server
 - Install Remote Packages for FTP/SMB if HTTP server setup on same host sharing same folder as FTP/SMB
   - If Remote Package Install is not possible, optionally the user can choose to download package to PS4 local drive and install
 - Install packages from PS4 local drive **/data** folder or usb drive **/mnt/usbX**
 - Install package from Direct Links. Direct links are links that can be reached without being redirected to a webpage where it requires capthas or timers. Example of direct links are github release artifacts. Google shared links is the only exception since I could indirectly parse the webpage to obtain the direct links
 - Create Zip files on PS4 local drive or usb drive
 - Extract from zip, 7zip and rar files
 - File management function include cut/copy/paste/rename/delete/new folder for files on PS4 local drive or usb or WebDAV Server.

## Installation
Copy the **ezremote_client.pkg** in to a FAT32 format usb drive then install from package installer

## Controls
```
Triangle - Menu (after a file(s)/folder(s) is selected)
Cross - Select Button/TextBox
Circle - Un-Select the file list to navigate to other widgets
Square - Mark file(s)/folder(s) for Delete/Rename/Upload/Download
R1 - Navigate to the Remote list of files
L1 - Navigate to the Local list of files
TouchPad Button - Exit Application
```

## Multi Language Support
The appplication support following languages.

**Note:** Due to new strings added, there are about 31 missing translations for all the languagess. Please help by downloading this [Template](https://github.com/cy33hc/ps4-ezremote-client/blob/master/data/assets/langs/English.ini), make your changes and submit an issue with the file attached for the language.

The following languages are auto detected.
```
Dutch
English
French
German
Italiano
Japanese
Korean
Polish
Portuguese_BR
Russian
Spanish
Simplified Chinese
Traditional Chinese
```

The following aren't standard languages supported by the PS4, therefore requires a config file update.
```
Arabic
Catalan
Croatian
Euskera
Galego
Greek
Hungarian
Indonesian
Romanian
Ryukyuan
Thai
Turkish
```
User must modify the file **/data/ezremote-client/config.ini** located in the ps4 hard drive and update the **language** setting to with the **exact** values from the list above.

**HELP:** There are no language translations for the following languages, therefore not support yet. Please help expand the list by submitting translation for the following languages. If you would like to help, please download this [Template](https://github.com/cy33hc/ps4-ezremote-client/blob/master/data/assets/langs/English.ini), make your changes and submit an issue with the file attached.
```
Finnish
Swedish
Danish
Norwegian
Czech
Vietnamese
```
or any other language that you have a traslation for.

## Building
Before build the app, you need to build the dependencies first.
Clone the following Git repos and build them in order

Download the PS4SDK Toolchain
```
1. Download the pacbrew-pacman from following location and install.
   https://github.com/PacBrew/pacbrew-pacman/releases
2. Run following cmds
   pacbrew-pacman -Sy
   pacbrew-pacman -S ps4-openorbis ps4-openorbis-portlibs
   chmod guo+x /opt/pacbrew/ps4/openorbis/ps4vars.sh
```

Build and install openssl - https://github.com/cy33hc/ps4-openssl/blob/OpenSSL_1_1_1-ps4/README_PS4.md

Build and install libcurl
```
1. download libcurl https://curl.haxx.se/download/curl-7.80.0.tar.xz and extract to a folder
2. source /opt/pacbrew/ps4/openorbis/ps4vars.sh
3. autoreconf -fi
4. CFLAGS="${CFLAGS} -DSOL_IP=0" LIBS="${LIBS} -lSceNet" \
  ./configure --prefix="${OPENORBIS}/usr" --host=x86_64 \
    --disable-shared --enable-static \
    --with-openssl --disable-manual
5. sed -i 's|#include <osreldate.h>|//#include <osreldate.h>|g' include/curl/curl.h
6. make -C lib install
```

Build and install libsmb2 - https://github.com/cy33hc/libsmb2/blob/ps4/README_PS4.md

Build and install lexbor - https://github.com/lexbor/lexbor.git

Build libjbc - https://github.com/cy33hc/ps4-libjbc/blob/master/README_PS4.md

Build libunrar - https://github.com/cy33hc/libunrar-ps3

Build libun7zip - https://github.com/cy33hc/libun7zip

Finally build the app
```
   source /opt/pacbrew/ps4/openorbis/ps4vars.sh
   mkdir build; cd build
   openorbis-cmake ..
   make
```

## Credits
The color theme was borrowed from NX-Shell on the switch.
