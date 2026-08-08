#pragma once

#include <SDL.h>

// Client-side zoom and pan over the streamed video.
//
// Ported from the Android client's PanZoomHandler, which drives this with pinch
// and drag gestures. There is no pinch on a desktop, and the scroll wheel and
// mouse drag are both forwarded to the host, so the controls here are the
// Ctrl+Alt+Shift combos that keyboard.cpp already reserves for the client.
//
// This magnifies what the host sent; it does not ask for a sharper stream. It
// earns its place when the stream is larger than the window (4K host on a 1080p
// display) or when something on screen is too small to read.
class PanZoom
{
public:
    // Applies the current zoom and pan to an already aspect-fitted rectangle.
    // `viewport` is the area the video was fitted into, and bounds the result:
    // the video is centred while it is smaller than the viewport, and clamped
    // to its edges once it is larger, so no gap can open along an edge.
    //
    // A no-op at the default 1x with no offset, which is the common case and
    // costs one comparison per frame.
    static void apply(SDL_Rect* dst, const SDL_Rect& viewport);

    // Multiplies the zoom by `factor`, keeping the result within [1x, 10x].
    // Panning is re-clamped on the next apply(), so zooming out always walks
    // back to a centred image rather than leaving it stuck against an edge.
    static void zoomBy(float factor);

    // Shifts the image by a fraction of the viewport, so a keypress moves the
    // same visible distance whatever the window size. Ignored at 1x, where
    // there is nothing hidden to pan towards.
    static void panBy(float fractionX, float fractionY);

    // Back to 1x, centred.
    static void reset();

    static bool isActive();
    static float scale();
};
