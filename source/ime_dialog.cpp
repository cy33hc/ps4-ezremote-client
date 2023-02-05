#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdarg.h>
#include <orbis/UserService.h>
#include "ime_dialog.h"

static int ime_dialog_running = 0;
static uint16_t inputTextBuffer[512+1];
static uint8_t storebuffer[512];
static char initial_ime_text[512];
static int max_text_length;

static void utf16_to_utf8(const uint16_t *src, uint8_t *dst)
{
  int i;
  for (i = 0; src[i]; i++)
  {
    if ((src[i] & 0xFF80) == 0)
    {
      *(dst++) = src[i] & 0xFF;
    }
    else if ((src[i] & 0xF800) == 0)
    {
      *(dst++) = ((src[i] >> 6) & 0xFF) | 0xC0;
      *(dst++) = (src[i] & 0x3F) | 0x80;
    }
    else if ((src[i] & 0xFC00) == 0xD800 && (src[i + 1] & 0xFC00) == 0xDC00)
    {
      *(dst++) = (((src[i] + 64) >> 8) & 0x3) | 0xF0;
      *(dst++) = (((src[i] >> 2) + 16) & 0x3F) | 0x80;
      *(dst++) = ((src[i] >> 4) & 0x30) | 0x80 | ((src[i + 1] << 2) & 0xF);
      *(dst++) = (src[i + 1] & 0x3F) | 0x80;
      i += 1;
    }
    else
    {
      *(dst++) = ((src[i] >> 12) & 0xF) | 0xE0;
      *(dst++) = ((src[i] >> 6) & 0x3F) | 0x80;
      *(dst++) = (src[i] & 0x3F) | 0x80;
    }
  }

  *dst = '\0';
}

static void utf8_to_utf16(const uint8_t *src, uint16_t *dst)
{
  int i;
  for (i = 0; src[i];)
  {
    if ((src[i] & 0xE0) == 0xE0)
    {
      *(dst++) = ((src[i] & 0x0F) << 12) | ((src[i + 1] & 0x3F) << 6) | (src[i + 2] & 0x3F);
      i += 3;
    }
    else if ((src[i] & 0xC0) == 0xC0)
    {
      *(dst++) = ((src[i] & 0x1F) << 6) | (src[i + 1] & 0x3F);
      i += 2;
    }
    else
    {
      *(dst++) = src[i];
      i += 1;
    }
  }

  *dst = '\0';
}

namespace Dialog
{

  int initImeDialog(const char *Title, const char *initialTextBuffer, int max_text_length, OrbisImeType type, float posx, float posy)
  {
    if (ime_dialog_running)
      return IME_DIALOG_ALREADY_RUNNING;

    uint16_t title[100];

    if ((initialTextBuffer && strlen(initialTextBuffer) > 511) || (Title && strlen(Title) > 99))
    {
      ime_dialog_running = 0;
      return -1;
    }

    memset(&inputTextBuffer[0], 0, sizeof(inputTextBuffer));
    memset(&storebuffer[0], 0, sizeof(storebuffer));
    memset(&initial_ime_text[0], 0, sizeof(initial_ime_text));

    if (initialTextBuffer)
    {
      snprintf(initial_ime_text, 511, "%s", initialTextBuffer);
    }

    // converts the multibyte string src to a wide-character string starting at dest.
    utf8_to_utf16((uint8_t *)initialTextBuffer, inputTextBuffer);
    utf8_to_utf16((uint8_t *)Title, title);

    OrbisImeDialogSetting param;
    memset(&param, 0, sizeof(OrbisImeDialogSetting));

    int UserID = 0;
    sceUserServiceGetInitialUser(&UserID);
    param.supportedLanguages = 0;
    param.maxTextLength = max_text_length;
    param.inputTextBuffer = reinterpret_cast<wchar_t*>(inputTextBuffer);
    param.title = reinterpret_cast<wchar_t*>(title);
    param.userId = UserID;
    param.type = type;
    param.posx = posx;
    param.posy = posy;
    param.enterLabel = ORBIS_BUTTON_LABEL_DEFAULT;

    int res = sceImeDialogInit(&param, NULL);
    if (res >= 0)
    {
      ime_dialog_running = 1;
    }

    return res;
  }

  int isImeDialogRunning()
  {
    return ime_dialog_running;
  }

  uint8_t *getImeDialogInputText()
  {
    return storebuffer;
  }

  uint16_t *getImeDialogInputText16()
  {
    return inputTextBuffer;
  }

  const char *getImeDialogInitialText()
  {
    return initial_ime_text;
  }

  int updateImeDialog()
  {
    if (!ime_dialog_running)
      return IME_DIALOG_RESULT_NONE;

    int status;
    while (1)
    {
      status = sceImeDialogGetStatus();

      if (status == ORBIS_DIALOG_STATUS_STOPPED)
      {
        OrbisDialogResult result;
        memset(&result, 0, sizeof(OrbisDialogResult));
        sceImeDialogGetResult(&result);

        if (result.endstatus == ORBIS_DIALOG_CANCEL)
        {
          status = IME_DIALOG_RESULT_CANCELED;
          goto Finished;
        }

        if (result.endstatus == ORBIS_DIALOG_OK)
        {
          utf16_to_utf8(inputTextBuffer, storebuffer);
          status = IME_DIALOG_RESULT_FINISHED;
          goto Finished;
        }
      }

      if (status == ORBIS_DIALOG_STATUS_NONE)
      {
        status = IME_DIALOG_RESULT_NONE;
        goto Finished;
      }
    }
  Finished:
    sceImeDialogTerm();
    ime_dialog_running = 0;
    return status;
  }

} // namespace Dialog