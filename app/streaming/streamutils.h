#pragma once

#include "SDL_compat.h"

class StreamUtils
{
public:
    static
    Uint32 getPlatformWindowFlags();

    static
    SDL_Window* createTestWindow();

    static
    void scaleSourceToDestinationSurface(SDL_Rect* src, SDL_Rect* dst);

    // scaleSourceToDestinationSurface() followed by the client-side zoom and
    // pan. Use this wherever the result is the video the user looks at or
    // clicks on -- the renderers and the input coordinate mapping -- so that a
    // zoomed picture and the pointer inside it cannot disagree.
    //
    // Not for laying out the window itself: Session::getWindowDimensions()
    // reuses the aspect fit to size the window, and a zoom applied there would
    // resize the window instead of magnifying the video.
    static
    void scaleSourceToDestinationSurfaceWithPanZoom(SDL_Rect* src, SDL_Rect* dst);

    static
    void screenSpaceToNormalizedDeviceCoords(SDL_FRect* rect, int viewportWidth, int viewportHeight);

    static
    void screenSpaceToNormalizedDeviceCoords(SDL_Rect* src, SDL_FRect* dst, int viewportWidth, int viewportHeight);

    static
    bool getNativeDesktopMode(int displayIndex, SDL_DisplayMode* mode, SDL_Rect* safeArea);

    static
    int getDisplayRefreshRate(SDL_Window* window);

    static
    bool hasFastAes();

    static
    int getDrmFdForWindow(SDL_Window* window, bool* needsClose);

    static
    int getDrmFd(bool preferRenderNode);

    static
    void enterAsyncLoggingMode();

    static
    void exitAsyncLoggingMode();
};
