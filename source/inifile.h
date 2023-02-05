/************************************************************************
   T h e   O p e n   W i n d o w s   P r o j e c t
 ------------------------------------------------------------------------
   Filename   : IniFile.h
   Author(s)  : Carsten Breuer
 ------------------------------------------------------------------------
 Copyright (c) 2000 by Carsten Breuer (C.Breuer@openwin.de)
/************************************************************************/

#ifndef INIFILE_H
#define INIFILE_H

#ifndef CCHR_H
#define CCHR_H
typedef const char cchr;
#endif

#ifndef __cplusplus
typedef char bool;
#define true 1
#define TRUE 1
#define false 0
#define FALSE 0
#endif

#define tpNULL 0
#define tpSECTION 1
#define tpKEYVALUE 2
#define tpCOMMENT 3

typedef struct ENTRY
{
   char Type;
   char *Text;
   struct ENTRY *pPrev;
   struct ENTRY *pNext;
} ENTRY;

typedef struct
{
   struct ENTRY *pSec;
   struct ENTRY *pKey;
   char KeyText[128];
   char ValText[4096];
   char Comment[32];
} EFIND;

/* Macros */
#define ArePtrValid(Sec, Key, Val) ((Sec != NULL) && (Key != NULL) && (Val != NULL))

/* Connectors of this file (Prototypes) */

bool OpenIniFile(cchr *FileName);

bool ReadBool(cchr *Section, cchr *Key, bool Default);
int ReadInt(cchr *Section, cchr *Key, int Default);
double ReadDouble(cchr *Section, cchr *Key, double Default);
cchr *ReadString(cchr *Section, cchr *Key, cchr *Default);

void WriteBool(cchr *Section, cchr *Key, bool Value);
void WriteInt(cchr *Section, cchr *Key, int Value);
void WriteDouble(cchr *Section, cchr *Key, double Value);
void WriteString(cchr *Section, cchr *Key, cchr *Value);

bool DeleteKey(cchr *Section, cchr *Key);

void CloseIniFile();
bool WriteIniFile(cchr *FileName);

int GetSectionCount();
void GetSections(char *sections[]);
#endif
