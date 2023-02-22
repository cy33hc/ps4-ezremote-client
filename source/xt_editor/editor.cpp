#include "xt_editor/types.h"
#include "xt_editor/editor.h"
struct EditorRow {
    char* Chars;
    size_t Size;
};

struct EditorState {
    int CPosX;
    int CPosY;

    int Rows;
    int Columns;

    EditorRow* Row;
    int RowCount;
    int CharCount;

    bool IsFileDirty;
    char* FileName;
};

enum CursorStyle_ {
    CursorStyle_Block,
    CursorStyle_Block_Outline,
    CursorStyle_Line,
    CursorStyle_Underline,
};

typedef int CursorStyle;

struct EditorConfig {
    CursorStyle Style;
    bool LineBlink;
};

static EditorState State;
static EditorConfig Config;
static bool IsInitialized = false;
static int TextStart = 7;
static char LeftBuffer[16];
static float BlinkStart = 0;
static float BlinkEnd = 0;

#ifdef BUILD_WIN32
#include "editor_input_win32.cpp"
#else
void Editor_HandleInput()
{};
#endif


void Editor_OpenFile(char* Filename) {
    State.FileName = Filename;
    State.IsFileDirty = false;

    FILE* File = fopen(Filename, "r");
    if (!File)
        return;

    char* Line = 0;
    size_t LineCapacity = 0;
    ssize_t LineLength;
    while ((LineLength = getline(&Line, &LineCapacity, File)) != -1) {
        while (LineLength > 0 && (Line[LineLength - 1] == '\n' || Line[LineLength - 1] == '\r')) {
            LineLength--;
            Editor_AppendRow(Line, LineLength);
            State.CharCount += LineLength;
        }
    }
    free(Line);
    fclose(File);
}

void Editor_Init() {
    State.CPosX = 0;
    State.CPosY = 0;

    ImVec2 WindowSize = ImGui::GetWindowContentRegionMax();
    float FontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
    ImVec2 CharAdvance = ImVec2(FontSize, ImGui::GetTextLineHeightWithSpacing() * 1.0f);

    State.Rows = WindowSize.y / CharAdvance.y;
    State.Columns = WindowSize.x / CharAdvance.x;

    State.Row = (EditorRow*)malloc(sizeof(EditorRow));

    Editor_OpenFile("../src/editor.cpp");

    IsInitialized = true;

    Config.Style = CursorStyle_Block;
    Config.LineBlink = true;
}
// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int UTF8CharLength(char c) {
	if ((c & 0xFE) == 0xFC)
		return 6;
	if ((c & 0xFC) == 0xF8)
		return 5;
	if ((c & 0xF8) == 0xF0)
		return 4;
	else if ((c & 0xF0) == 0xE0)
		return 3;
	else if ((c & 0xE0) == 0xC0)
		return 2;
	return 1;
}

int Editor_GetCharacterIndexByCursor(int X, int Y) {
    int Index = 0;
    int Column = 0;
    EditorRow* Line = &State.Row[Y];
    if (Line == 0)
        return 0;

    for (; Index < Line->Size && Column < X;) {
        Index += UTF8CharLength(Line->Chars[Index]);
        ++Column;
    }

    return Index;
}

void Editor_RenderRows(ImVec2 WindowSize, ImVec2 Pos) {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
    //ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("Editor", WindowSize, true);
    ImGui::PushAllowKeyboardFocus(true);

    // Handle input here or else we can't grab the childs input
    Editor_HandleInput();

    if (State.CPosX < 0)
        State.CPosX = 0;

    if (State.CPosY < 0)
        State.CPosY = 0;

    static float FontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x; // Get the size of the tallest char
    ImVec2 CharAdvance = ImVec2(FontSize, ImGui::GetTextLineHeightWithSpacing() * 1.0f);

    bool Focused = ImGui::IsWindowFocused();
    float ScrollX = ImGui::GetScrollX();
	float ScrollY = ImGui::GetScrollY();

    int LineNum = (int)floor(ScrollY / CharAdvance.y);
    int LineMax = Maximum(0, Minimum(State.RowCount - 1, LineNum + (int)floor((ScrollY + WindowSize.y) / CharAdvance.y)));

    int ActualTextStart = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, LeftBuffer, nullptr, nullptr).x + TextStart;

    ImDrawList* Draw = ImGui::GetWindowDrawList();

    while (LineNum <= LineMax) {
        ImVec2 LineStartPos = ImVec2(Pos.x, Pos.y + LineNum * CharAdvance.y);
        ImVec2 TextPos = ImVec2(LineStartPos.x + ActualTextStart, LineStartPos.y);

        snprintf(LeftBuffer, 16, "%*d ", (TextStart - 1), LineNum + 1);
        int LineNumWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, LeftBuffer, nullptr, nullptr).x;
        Draw->AddText(ImVec2(LineStartPos.x, LineStartPos.y), IM_COL32(255, 255, 255, 255), LeftBuffer);

        ImVec2 Start = ImVec2(LineStartPos.x + ScrollX, LineStartPos.y);
        ImVec2 End = ImVec2(Start.x + ScrollX + WindowSize.x, Start.y + CharAdvance.y);
        Draw->AddRectFilled(Start, End, 0x141414);
        Draw->AddRect(Start, End, 0x40808080, 1.0f);

        EditorRow* Row = &State.Row[LineNum];

        size_t Len = Row->Size;
        if (Len > State.Columns) {
            Len = State.Columns;
        }

        if (Config.Style != (CursorStyle_Line))
            Draw->AddText(TextPos, IM_COL32(255, 255, 255, 255), Row->Chars);

        // Draw the cursor
        if (State.CPosY == LineNum && Focused) {
            float CursorWidth = CharAdvance.x;
            if (Config.Style == CursorStyle_Line || Config.Style == CursorStyle_Underline)
                CursorWidth = 1.f;

            int Index = Editor_GetCharacterIndexByCursor(State.CPosX, State.CPosY);
            int ScaledCurX = (Index * CharAdvance.x);
            int ScaledCurY = (State.CPosY * CharAdvance.y);
            ImVec2 TextStartPos = ImVec2(Pos.x + ActualTextStart, Pos.y);

            ImVec2 CursorStart, CursorEnd;
            if (Config.Style == CursorStyle_Underline) { // We are doing underline style
                CursorStart = ImVec2(TextStartPos.x + ScaledCurX, ((ScaledCurY + TextStartPos.y + CharAdvance.y) - CursorWidth) - 1);
                CursorEnd = ImVec2(TextStartPos.x + ScaledCurX + CharAdvance.x, (ScaledCurY + TextStartPos.y + CharAdvance.y) - 1);
            } else {
                CursorStart = ImVec2(TextStartPos.x + ScaledCurX, ScaledCurY + TextStartPos.y);
                CursorEnd = ImVec2(TextStartPos.x + ScaledCurX + CursorWidth, ScaledCurY + TextStartPos.y + CharAdvance.y);
            }

            BlinkEnd++;
            float Elapsed = (BlinkEnd - BlinkStart);

            static int OldCPosX = 0;
            static int OldCPosY = 0;
            if ((OldCPosX != State.CPosX || OldCPosY != State.CPosY) || Config.LineBlink == false) {
                // Constantly render the cursor if we're in motion
                (Config.Style == CursorStyle_Block_Outline) ? Draw->AddRect(CursorStart, CursorEnd, 0xffffffff, 1.0f) : Draw->AddRectFilled(CursorStart, CursorEnd, 0xffffffff);
                
                // Draw the char of text at the cursors location in the opposite color
                char* Char = (char*)malloc(sizeof(char) * 1);
                Char[0] = Row->Chars[Index];
                Draw->AddText(CursorStart, IM_COL32(0, 0, 0, 255), Char);
            } else {
                // Blink the cursor rendering
                static float InitStart = 108;
                if (Elapsed > InitStart) {
                    (Config.Style == CursorStyle_Block_Outline) ? Draw->AddRect(CursorStart, CursorEnd, 0xffffffff, 1.0f) : Draw->AddRectFilled(CursorStart, CursorEnd, 0xffffffff);

                    // Draw the char of text at the cursors location in the opposite color
                    char* Char = (char*)malloc(sizeof(char) * 1);
                    Char[0] = Row->Chars[Index];
                    Draw->AddText(CursorStart, IM_COL32(0, 0, 0, 255), Char);

                    if (Elapsed > (InitStart + 40))
                        BlinkStart = BlinkEnd;
                }
            }

            OldCPosX = State.CPosX;
            OldCPosY = State.CPosY;
        }

        // AddRect doesn't allow for a transparent rectangle so we need to write the character again :/
        // When we use CursorStyle_Line the original text gets overwritten in a similar way.
        if (Config.Style == CursorStyle_Block_Outline || Config.Style == CursorStyle_Line)
            Draw->AddText(TextPos, IM_COL32(255, 255, 255, 255), Row->Chars);

        {
            // DEBUG DRAW: Cursor position and index
            int Index = Editor_GetCharacterIndexByCursor(State.CPosX, State.CPosY);
            char Temp[64];
            sprintf(Temp, "%d, %d, %d", State.CPosX, State.CPosY, Index);
            Draw->AddText(ImVec2(Pos.x + 550, Pos.y), IM_COL32(255, 255, 255, 255), Temp);
        }

        LineNum++;
    }

    while (LineNum >= LineMax) {
        ImVec2 LineStartPos = ImVec2(Pos.x, Pos.y + LineNum * CharAdvance.y);
        ImVec2 TextPos = ImVec2(LineStartPos.x + CharAdvance.x * (TextStart + 1), LineStartPos.y);

        snprintf(LeftBuffer, 16, "%*s ", (TextStart - 1), "~");
        int LineNumWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, LeftBuffer, nullptr, nullptr).x;
        Draw->AddText(ImVec2(LineStartPos.x, LineStartPos.y), IM_COL32(255, 255, 255, 255), LeftBuffer);

        ImVec2 Start = ImVec2(LineStartPos.x + ScrollX, LineStartPos.y);
        ImVec2 End = ImVec2(Start.x + ScrollX + WindowSize.x, Start.y + CharAdvance.y);
        Draw->AddRectFilled(Start, End, 0x141414);
        Draw->AddRect(Start, End, 0x40808080, 1.0f);

        LineNum++;

        if (LineNum >= (LineMax)) break; // Impose some limit so that we don't render infinitly
    }

    ImGui::PopAllowKeyboardFocus();
    ImGui::EndChild();
    //ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::Text("xt editor -- \"%s\"%s %2s %dL %dC %8s (%d, %d)", State.FileName, (State.IsFileDirty) ? "*" : "", "", State.RowCount, State.CharCount, "", State.CPosX, State.CPosY);
}

void Editor_AppendRow(char* String, size_t Length) {
    State.Row = (EditorRow*)realloc(State.Row, sizeof(EditorRow) * (State.RowCount + 1));

    int At = State.RowCount;
    State.Row[At].Size = Length;
    State.Row[At].Chars = (char*)malloc(Length + 1);
    memcpy(State.Row[At].Chars, String, Length);
    State.Row[At].Chars[Length] = 0;
    State.RowCount++;
}

bool StringsMatch(char* A, char* B) {
    while (*A && *B) {
        if (*A != *B){
            return false;
        }

        ++A;
        ++B;
    }

    if (*A != *B){
        return false;
    } else {
        return true;
    }
}

int StringLength(const char* String) {
    int Count = 0;
    while (*String++) {
        ++Count;
    }
    return Count;
}

char* StringCopy(const char* String) {
    char* Result = (char*)malloc(sizeof(char) * (StringLength(String) + 1));
    memcpy(Result, String, sizeof(char) * (StringLength(String) + 1));

    return Result;
}

void Editor_Render() {
    ImGui::Begin("xt Demo Panel", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar);
    ImGui::SetWindowSize(ImVec2(1280, 720), ImGuiCond_FirstUseEver);

    if (!IsInitialized) {
        Editor_Init();
    }
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit", "Alt-F4, CTRL-Q")) {
                exit(0);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Cursor Blink", NULL, &Config.LineBlink)) {
                //ImGui::PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);
                //ImGui::PopItemFlag();
            }

            {
                const char* Items[] = { "Block", "Block Outline", "Line", "Underline" };
                static char* CurrentItem = NULL;
                ImGuiComboFlags flags = ImGuiComboFlags_NoArrowButton;

                ImGuiStyle& Style = ImGui::GetStyle();

                ImGui::Text("Cursor Style");
                ImGui::SameLine(0, Style.ItemInnerSpacing.x);
                ImGui::PushItemWidth(ImGui::CalcItemWidth() - Style.ItemInnerSpacing.x- ImGui::GetFrameHeight());
                if (ImGui::BeginCombo("##custom combo", CurrentItem, ImGuiComboFlags_NoArrowButton)) {
                    for (int n = 0; n < IM_ARRAYSIZE(Items); n++) {
                        bool is_selected = (CurrentItem == Items[n]);
                        if (ImGui::Selectable(Items[n], is_selected)) {
                            CurrentItem = StringCopy(Items[n]);
                            if (StringsMatch(CurrentItem, "Block")) {
                                Config.Style = CursorStyle_Block;
                            }
                            else if (StringsMatch(CurrentItem, "Block Outline")) {
                                Config.Style = CursorStyle_Block_Outline;
                            }
                            else if (StringsMatch(CurrentItem, "Line")) {
                                Config.Style = CursorStyle_Line;
                            }
                            else if (StringsMatch(CurrentItem, "Underline")) {
                                Config.Style = CursorStyle_Underline;
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
            }

            ImGui::EndMenu();
        }

	    ImGui::EndMenuBar();
    }

    ImVec2 WindowSize = ImGui::GetWindowContentRegionMax();
    WindowSize.y -= 215;
    ImVec2 Pos = ImGui::GetCursorScreenPos();

    Editor_RenderRows(WindowSize, Pos);

    ImGui::End();
}