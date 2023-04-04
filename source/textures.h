#ifndef LAUNCHER_TEXTURES_H
#define LAUNCHER_TEXTURES_H

#include <string>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

typedef struct {
    SDL_Texture *id;
    int width;
    int height;
} Tex;

namespace Textures {
    bool LoadImageFile(const std::string filename, Tex *texture);
    void Init(SDL_Renderer *renderer);
    void Exit(void);
    void Free(Tex *texture);
}

#endif
