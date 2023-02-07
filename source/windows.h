#ifndef LAUNCHER_WINDOWS_H
#define LAUNCHER_WINDOWS_H

#define IMGUI_DEFINE_MATH_OPERATORS
#include <set>
#include "imgui.h"
#include "imgui_internal.h"
#include "common.h"
#include "actions.h"
#include "SDL2/SDL.h"

#define LOCAL_BROWSER 1
#define REMOTE_BROWSER 2

extern int view_mode;
extern bool handle_updates;
extern int64_t bytes_transfered;
extern int64_t bytes_to_download;
extern std::vector<DirEntry> local_files;
extern std::vector<DirEntry> remote_files;
extern std::set<DirEntry> multi_selected_local_files;
extern std::set<DirEntry> multi_selected_remote_files;
extern DirEntry selected_local_file;
extern DirEntry selected_remote_file;
extern ACTIONS selected_action;
extern char status_message[];
extern char local_file_to_select[];
extern char remote_file_to_select[];
extern char local_filter[];
extern char remote_filter[];
extern char activity_message[];
extern char confirm_message[];
extern bool activity_inprogess;
extern bool stop_activity;
extern int confirm_state;
extern int overwrite_type;
extern ACTIONS action_to_take;
extern bool file_transfering;
extern char extract_zip_folder[];

static ImVector<ImRect> s_GroupPanelLabelStack;

namespace Windows
{

    inline void SetupWindow(void)
    {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y), ImGuiCond_Once);
    };

    inline void SetNavFocusHere()
    {
        GImGui->NavId = GImGui->LastItemData.ID;
    }

    inline void ClearNavFocus()
    {
        GImGui->NavId = 0;
    }

    inline void BeginGroupPanel(const char *name, const ImVec2 &size)
    {
        ImGui::BeginGroup();

        auto cursorPos = ImGui::GetCursorScreenPos();
        auto itemSpacing = ImGui::GetStyle().ItemSpacing;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

        auto frameHeight = ImGui::GetFrameHeight();
        ImGui::BeginGroup();

        ImVec2 effectiveSize = size;
        if (size.x < 0.0f)
            effectiveSize.x = ImGui::GetContentRegionAvail().x;
        else
            effectiveSize.x = size.x;
        ImGui::Dummy(ImVec2(effectiveSize.x, 0.0f));

        ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted(name);
        auto labelMin = ImGui::GetItemRectMin();
        auto labelMax = ImGui::GetItemRectMax();
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::Dummy(ImVec2(0.0, frameHeight + itemSpacing.y));
        ImGui::BeginGroup();

        ImGui::PopStyleVar(2);

#if IMGUI_VERSION_NUM >= 17301
        ImGui::GetCurrentWindow()->ContentRegionRect.Max.x -= frameHeight * 0.5f;
        ImGui::GetCurrentWindow()->WorkRect.Max.x -= frameHeight * 0.5f;
        ImGui::GetCurrentWindow()->InnerRect.Max.x -= frameHeight * 0.5f;
#else
        ImGui::GetCurrentWindow()->ContentsRegionRect.Max.x -= frameHeight * 0.5f;
#endif
        ImGui::GetCurrentWindow()->Size.x -= frameHeight;

        auto itemWidth = ImGui::CalcItemWidth();
        ImGui::PushItemWidth(ImMax(0.0f, itemWidth - frameHeight));

        s_GroupPanelLabelStack.push_back(ImRect(labelMin, labelMax));
    }

    inline void EndGroupPanel()
    {
        ImGui::PopItemWidth();

        auto itemSpacing = ImGui::GetStyle().ItemSpacing;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

        auto frameHeight = ImGui::GetFrameHeight();

        ImGui::EndGroup();
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
        ImGui::Dummy(ImVec2(0.0, frameHeight - frameHeight * 0.5f - itemSpacing.y));

        ImGui::EndGroup();

        auto itemMin = ImGui::GetItemRectMin();
        auto itemMax = ImGui::GetItemRectMax();

        auto labelRect = s_GroupPanelLabelStack.back();
        s_GroupPanelLabelStack.pop_back();

        ImVec2 halfFrame = ImVec2(frameHeight * 0.25f, frameHeight) * 0.5f;
        ImRect frameRect = ImRect(itemMin + halfFrame, itemMax - ImVec2(halfFrame.x, 0.0f));
        labelRect.Min.x -= itemSpacing.x;
        labelRect.Max.x += itemSpacing.x;
        for (int i = 0; i < 4; ++i)
        {
            switch (i)
            {
            // left half-plane
            case 0:
                ImGui::PushClipRect(ImVec2(-FLT_MAX, -FLT_MAX), ImVec2(labelRect.Min.x, FLT_MAX), true);
                break;
            // right half-plane
            case 1:
                ImGui::PushClipRect(ImVec2(labelRect.Max.x, -FLT_MAX), ImVec2(FLT_MAX, FLT_MAX), true);
                break;
            // top
            case 2:
                ImGui::PushClipRect(ImVec2(labelRect.Min.x, -FLT_MAX), ImVec2(labelRect.Max.x, labelRect.Min.y), true);
                break;
            // bottom
            case 3:
                ImGui::PushClipRect(ImVec2(labelRect.Min.x, labelRect.Max.y), ImVec2(labelRect.Max.x, FLT_MAX), true);
                break;
            }

            ImGui::GetWindowDrawList()->AddRect(
                frameRect.Min, frameRect.Max,
                ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Button)),
                halfFrame.x, 0, 2.0f);

            ImGui::PopClipRect();
        }

        ImGui::PopStyleVar(2);

#if IMGUI_VERSION_NUM >= 17301
        ImGui::GetCurrentWindow()->ContentRegionRect.Max.x += frameHeight * 0.5f;
        ImGui::GetCurrentWindow()->WorkRect.Max.x += frameHeight * 0.5f;
        ImGui::GetCurrentWindow()->InnerRect.Max.x += frameHeight * 0.5f;
#else
        ImGui::GetCurrentWindow()->ContentsRegionRect.Max.x += frameHeight * 0.5f;
#endif
        ImGui::GetCurrentWindow()->Size.x += frameHeight;

        ImGui::Dummy(ImVec2(0.0f, 0.0f));

        ImGui::EndGroup();
    }

    void Init();
    void HandleWindowInput();
    void MainWindow();
    void HandleImeInput();
    void ExecuteActions();
    void ResetImeCallbacks();
    void SetModalMode(bool modal);

    void SingleValueImeCallback(int ime_result);
    void MultiValueImeCallback(int ime_result);
    void NullAfterValueChangeCallback(int ime_result);
    void AfterLocalFileChangesCallback(int ime_result);
    void AfterRemoteFileChangesCallback(int ime_result);
    void AfterFolderNameCallback(int ime_result);
    void CancelActionCallBack(int ime_result);
    void AfterPackageUrlCallback(int ime_result);
    void AfterFavoriteUrlCallback(int ime_result);
    void AfterExtractFolderCallback(int ime_result);
}

#endif
