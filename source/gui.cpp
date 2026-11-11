#include "imgui.h"
#include <stdio.h>
#include "windows.h"
#include "gui.h"
#include "SDL2/SDL.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "lang.h"
#include "fonts.h"

bool done = false;
int gui_mode = GUI_MODE_BROWSER;

namespace GUI
{
	int RenderLoop(SDL_Renderer *renderer)
	{
		Windows::Init();
		while (!done)
		{
			if (gui_mode == GUI_MODE_BROWSER)
			{
				SDL_Event event;
				while (SDL_PollEvent(&event))
				{
					ImGui_ImplSDL2_ProcessEvent(&event);
				}
				GImGui->GcCompactAll = true;

				// Apply a pending language change before starting the frame:
				// the strings and, when the script differs, the font atlas are
				// rebuilt while no draw data references the old texture.
				if (Lang::ApplyPendingReload())
				{
					ImGui_ImplSDLRenderer_DestroyDeviceObjects();
					Fonts::Load();
					ImGui_ImplSDLRenderer_CreateDeviceObjects();
				}

				ImGui_ImplSDLRenderer_NewFrame();
				ImGui_ImplSDL2_NewFrame();

				ImGui::NewFrame();

				Windows::HandleWindowInput();
				Windows::MainWindow();
				Windows::ExecuteActions();

				ImGui::Render();
				ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
				SDL_RenderPresent(renderer);

			}
			else if (gui_mode == GUI_MODE_IME)
			{
				Windows::HandleImeInput();
			}
		}
		return 0;
	}
}
