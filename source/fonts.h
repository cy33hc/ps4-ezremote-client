#ifndef EZREMOTE_FONTS_H
#define EZREMOTE_FONTS_H

// (Re)builds the ImGui font atlas for the current language. The caller must
// destroy and recreate the backend device objects around it.
namespace Fonts
{
    void Load();
}

#endif
