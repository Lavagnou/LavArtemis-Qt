#include "panzoom.h"

#include <QtGlobal>

namespace
{
    // Matches the Android handler's ceiling. Past this the picture is mostly
    // interpolation artefacts.
    const float k_MinScale = 1.0f;
    const float k_MaxScale = 10.0f;

    // Written from the SDL event thread and read from whichever thread the
    // renderer presents on. Both are single machine words; a frame rendered
    // with a half-applied pan would only differ from its neighbour by one
    // keypress worth of offset, and the next frame corrects it. Locking the
    // present path for that is not a trade worth making.
    float s_Scale = 1.0f;
    float s_OffsetX = 0.0f;
    float s_OffsetY = 0.0f;
}

void PanZoom::apply(SDL_Rect* dst, const SDL_Rect& viewport)
{
    const float scale = s_Scale;

    if (scale == 1.0f && s_OffsetX == 0.0f && s_OffsetY == 0.0f) {
        return;
    }

    const int scaledW = (int)(dst->w * scale);
    const int scaledH = (int)(dst->h * scale);

    // Zoom about the centre of the fitted rectangle, then pan away from it.
    const int centreX = dst->x + dst->w / 2;
    const int centreY = dst->y + dst->h / 2;

    int x = centreX - scaledW / 2 + (int)s_OffsetX;
    int y = centreY - scaledH / 2 + (int)s_OffsetY;

    // Containment, same rule as the Android handler: centre while it fits,
    // clamp to the edges once it does not. Without this, panning could drag
    // the picture off the viewport and leave a black band.
    if (scaledW <= viewport.w) {
        x = viewport.x + (viewport.w - scaledW) / 2;
    }
    else {
        x = qBound(viewport.x + viewport.w - scaledW, x, viewport.x);
    }

    if (scaledH <= viewport.h) {
        y = viewport.y + (viewport.h - scaledH) / 2;
    }
    else {
        y = qBound(viewport.y + viewport.h - scaledH, y, viewport.y);
    }

    dst->x = x;
    dst->y = y;
    dst->w = scaledW;
    dst->h = scaledH;
}

void PanZoom::zoomBy(float factor)
{
    const float previous = s_Scale;
    s_Scale = qBound(k_MinScale, s_Scale * factor, k_MaxScale);

    // Keep the pan proportional to the zoom, so the same part of the picture
    // stays under the cursor instead of sliding as the image grows.
    if (previous != 0.0f) {
        const float ratio = s_Scale / previous;
        s_OffsetX *= ratio;
        s_OffsetY *= ratio;
    }

    if (s_Scale == k_MinScale) {
        // Fully zoomed out: nothing is hidden, so any residual offset would
        // only be re-centred by apply() anyway. Clear it so zooming back in
        // starts from the middle.
        s_OffsetX = s_OffsetY = 0.0f;
    }
}

void PanZoom::panBy(float fractionX, float fractionY)
{
    if (s_Scale == k_MinScale) {
        return;
    }

    // Expressed against a nominal 1080p viewport rather than the real one,
    // which is not known here. apply() clamps the result, so an overshoot on a
    // small window costs nothing beyond hitting the edge sooner.
    s_OffsetX += fractionX * 1920.0f;
    s_OffsetY += fractionY * 1080.0f;
}

void PanZoom::reset()
{
    s_Scale = 1.0f;
    s_OffsetX = s_OffsetY = 0.0f;
}

bool PanZoom::isActive()
{
    return s_Scale != 1.0f;
}

float PanZoom::scale()
{
    return s_Scale;
}
