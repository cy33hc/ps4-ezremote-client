#ifndef EDITOR_H
#define EDITOR_H
#include "imgui.h"

void Editor_Init();
void Editor_AppendRow(char* String, size_t Length);
void Editor_RenderRows(ImVec2 WindowSize, ImVec2 Pos);
void Editor_Render();

#endif