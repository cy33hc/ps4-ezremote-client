#include "textures.h"

static SDL_Renderer *renderer;

namespace Textures {
	
	bool LoadImageFile(const std::string filename, Tex *texture)
	{
		// Load from file
		SDL_Surface *image = IMG_Load(filename.c_str());
		if (image == nullptr)
			return false;
		image = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_RGBA8888, 0);
		SDL_Texture *sdl_texture = SDL_CreateTextureFromSurface(renderer, image);
		if (sdl_texture == nullptr)
		{
			SDL_FreeSurface(image);
			return false;
		}
		texture->id = sdl_texture;
		texture->height = image->h;
		texture->width = image->w;
		SDL_FreeSurface(image);

		return true;
	}
	
	void Init(SDL_Renderer *p_renderer) {
		renderer = p_renderer;
		IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP);
	}

	void Exit(void) {
		IMG_Quit();
	}

	void Free(Tex *texture) {
		if (texture->id != nullptr)
		{
			SDL_DestroyTexture(texture->id);
			texture->id = nullptr;
		}
	}
	
}
