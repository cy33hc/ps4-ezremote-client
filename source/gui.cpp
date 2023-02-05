#include "imgui.h"
#include <stdio.h>
#include "windows.h"
#include "gui.h"
#include "SDL2/SDL.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

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
