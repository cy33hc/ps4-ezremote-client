#include <orbis/SystemService.h>
#include "string.h"
#include "stdio.h"
#include "config.h"
#include "util.h"
#include "lang.h"

char lang_identifiers[LANG_STRINGS_NUM][LANG_ID_SIZE] = {
	FOREACH_STR(GET_STRING)};

// This is properly populated so that emulator won't crash if an user launches it without language INI files.
char lang_strings[LANG_STRINGS_NUM][LANG_STR_SIZE] = {
	"Connection Settings",											  // STR_CONNECTION_SETTINGS
	"Site",															  // STR_SITE
	"Local",														  // STR_LOCAL
	"Remote",														  // STR_REMOTE
	"Messages",														  // STR_MESSAGES
	"Update Software",												  // STR_UPDATE_SOFTWARE
	"Connect",														  // STR_CONNECT
	"Disconnect",													  // STR_DISCONNECT
	"Search",														  // STR_SEARCH
	"Refresh",														  // STR_REFRESH
	"Server",														  // STR_SERVER
	"Username",														  // STR_USERNAME
	"Password",														  // STR_PASSWORD
	"Port",															  // STR_PORT
	"Pasv",															  // STR_PASV
	"Directory",													  // STR_DIRECTORY
	"Filter",														  // STR_FILTER
	"Yes",															  // STR_YES
	"No",															  // STR_NO
	"Cancel",														  // STR_CANCEL
	"Continue",														  // STR_CONTINUE
	"Close",														  // STR_CLOSE
	"Folder",														  // STR_FOLDER
	"File",															  // STR_FILE
	"Type",															  // STR_TYPE
	"Name",															  // STR_NAME
	"Size",															  // STR_SIZE
	"Date",															  // STR_DATE
	"New Folder",													  // STR_NEW_FOLDER
	"Rename",														  // STR_RENAME
	"Delete",														  // STR_DELETE
	"Upload",														  // STR_UPLOAD
	"Download",														  // STR_DOWNLOAD
	"Select All",													  // STR_SELECT_ALL
	"Clear All",													  // STR_CLEAR_ALL
	"Uploading",													  // STR_UPLOADING
	"Downloading",													  // STR_DOWNLOADING
	"Overwrite",													  // STR_OVERWRITE
	"Don't Overwrite",												  // STR_DONT_OVERWRITE
	"Ask for Confirmation",											  // STR_ASK_FOR_CONFIRM
	"Don't Ask for Confirmation",									  // STR_DONT_ASK_CONFIRM
	"Always use this option and don't ask again",					  // STR_ALLWAYS_USE_OPTION
	"Actions",														  // STR_ACTIONS
	"Confirm",														  // STR_CONFIRM
	"Overwrite Options",											  // STR_OVERWRITE_OPTIONS
	"Properties",													  // STR_PROPERTIES
	"Progress",														  // STR_PROGRESS
	"Updates",														  // STR_UPDATES
	"Are you sure you want to delete this file(s)/folder(s)?",		  // STR_DEL_CONFIRM_MSG
	"Canceling. Waiting for last action to complete",				  // STR_CANCEL_ACTION_MSG
	"Failed to upload file",										  // STR_FAIL_UPLOAD_MSG
	"Failed to download file",										  // STR_FAIL_DOWNLOAD_MSG
	"Failed to read contents of directory or folder does not exist.", // STR_FAIL_READ_LOCAL_DIR_MSG
	"426 Connection closed.",										  // STR_CONNECTION_CLOSE_ERR_MSG
	"426 Remote Server has terminated the connection.",				  // STR_REMOTE_TERM_CONN_MSG
	"300 Failed Login. Please check your username or password.",	  // STR_FAIL_LOGIN_MSG
	"426 Failed. Connection timeout.",								  // STR_FAIL_TIMEOUT_MSG
	"Failed to delete directory",									  // STR_FAIL_DEL_DIR_MSG
	"Deleting",														  // STR_DELETING
	"Failed to delete file",										  // STR_FAIL_DEL_FILE_MSG
	"Deleted",														  // STR_DELETED
	"Link",															  // STR_LINK
	"Share",														  // STR_SHARE
	"310 Failed",													  // STR_FAILED
	"310 Failed to create file on local",							  // STR_FAIL_CREATE_LOCAL_FILE_MSG
	"Install",														  // STR_INSTALL
	"Installing",													  // STR_INSTALLING
	"Success",														  // STR_INSTALL_SUCCESS
	"Failed",														  // STR_INSTALL_FAILED
	"Skipped",														  // STR_INSTALL_SKIPPED
	"Checking connection to remote HTTP Server",					  // STR_CHECK_HTTP_MSG
	"Failed connecting to HTTP Server",								  // STR_FAILED_HTTP_CHECK
	"Remote is not a HTTP Server",									  // STR_REMOTE_NOT_HTTP
	"Package not in the /data or /mnt/usbX folder",					  // STR_INSTALL_FROM_DATA_MSG
	"Package is already installed",									  // STR_ALREADY_INSTALLED_MSG
	"Install from URL",												  // STR_INSTALL_FROM_URL
	"Could not read package header info",							  // STR_CANNOT_READ_PKG_HDR_MSG
	"Favorite URLs",												  // STR_FAVORITE_URLS
	"Slot",															  // STR_SLOT
	"Edit",															  // STR_EDIT
	"One Time Url",													  // STR_ONETIME_URL
	"Not a valid Package",											  // STR_NOT_A_VALID_PACKAGE
	"Waiting for Package to finish installing",						  // STR_WAIT_FOR_INSTALL_MSG
	"Failed to install pkg file. Please delete the tmp pkg manually", // STR_FAIL_INSTALL_TMP_PKG_MSG
	"Failed to obtain google download URL",							  // STR_FAIL_TO_OBTAIN_GG_DL_MSG
	"Auto delete temporary downloaded pkg file after install",		  // STR_AUTO_DELETE_TMP_PKG
	"Protocol not supported",										  // STR_PROTOCOL_NOT_SUPPORTED
	"Could not resolve hostname"									  // STR_COULD_NOT_RESOLVE_HOST
};

bool needs_extended_font = false;

namespace Lang
{
	void SetTranslation(int32_t lang_idx)
	{
		char langFile[LANG_STR_SIZE * 2];
		char identifier[LANG_ID_SIZE], buffer[LANG_STR_SIZE];

		std::string lang = std::string(language);
		lang = Util::Trim(lang, " ");
		if (lang.size() > 0)
		{
			sprintf(langFile, "/app0/assets/langs/%s.ini", lang.c_str());
		}
		else
		{
			switch (lang_idx)
			{
			case ORBIS_SYSTEM_PARAM_LANG_ITALIAN:
				sprintf(langFile, "%s", "/app0/assets/langs/Italiano.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_SPANISH:
			case ORBIS_SYSTEM_PARAM_LANG_SPANISH_LA:
				sprintf(langFile, "%s", "/app0/assets/langs/Spanish.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_GERMAN:
				sprintf(langFile, "%s", "/app0/assets/langs/German.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_PORTUGUESE_PT:
			case ORBIS_SYSTEM_PARAM_LANG_PORTUGUESE_BR:
				sprintf(langFile, "%s", "/app0/assets/langs/Portuguese_BR.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_RUSSIAN:
				sprintf(langFile, "%s", "/app0/assets/langs/Russian.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_DUTCH:
				sprintf(langFile, "%s", "/app0/assets/langs/Dutch.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_FRENCH:
			case ORBIS_SYSTEM_PARAM_LANG_FRENCH_CA:
				sprintf(langFile, "%s", "/app0/assets/langs/French.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_POLISH:
				sprintf(langFile, "%s", "/app0/assets/langs/Polish.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_JAPANESE:
				sprintf(langFile, "%s", "/app0/assets/langs/Japanese.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_KOREAN:
				sprintf(langFile, "%s", "/app0/assets/langs/Korean.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_CHINESE_S:
				sprintf(langFile, "%s", "/app0/assets/langs/Simplified Chinese.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_CHINESE_T:
				sprintf(langFile, "%s", "/app0/assets/langs/Traditional Chinese.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_INDONESIAN:
				sprintf(langFile, "%s", "/app0/assets/langs/Indonesian.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_HUNGARIAN:
				sprintf(langFile, "%s", "/app0/assets/langs/Hungarian.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_GREEK:
				sprintf(langFile, "%s", "/app0/assets/langs/Greek.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_VIETNAMESE:
				sprintf(langFile, "%s", "/app0/assets/langs/Vietnamese.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_TURKISH:
				sprintf(langFile, "%s", "/app0/assets/langs/Turkish.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_ARABIC:
				sprintf(langFile, "%s", "/app0/assets/langs/Arabic.ini");
				break;
			case ORBIS_SYSTEM_PARAM_LANG_ROMANIAN:
				sprintf(langFile, "%s", "/app0/assets/langs/Romanian.ini");
				break;
			default:
				sprintf(langFile, "%s", "/app0/assets/langs/English.ini");
				break;
			}
		}

		FILE *config = fopen(langFile, "r");
		if (config)
		{
			while (EOF != fscanf(config, "%[^=]=%[^\n]\n", identifier, buffer))
			{
				for (int i = 0; i < LANG_STRINGS_NUM; i++)
				{
					if (strcmp(lang_identifiers[i], identifier) == 0)
					{
						char *newline = nullptr, *p = buffer;
						while ((newline = strstr(p, "\\n")) != NULL)
						{
							newline[0] = '\n';
							int len = strlen(&newline[2]);
							memmove(&newline[1], &newline[2], len);
							newline[len + 1] = 0;
							p++;
						}
						strcpy(lang_strings[i], buffer);
					}
				}
			}
			fclose(config);
		}

		char buf[12];
		int num;
		sscanf(last_site, "%[^ ] %d", buf, &num);
		sprintf(display_site, "%s %d", lang_strings[STR_SITE], num);
	}
}