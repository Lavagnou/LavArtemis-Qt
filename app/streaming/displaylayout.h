#pragma once

#include "SDL_compat.h"

#include <QString>
#include <QVector>

/**
 * @brief One of the client's physical monitors, placed inside the canvas.
 */
struct DisplayLayoutEntry
{
    /// Position and size within the canvas, with the layout's top-left corner at (0,0).
    SDL_Rect canvasRect;

    /// Where this monitor actually sits on the client desktop, in SDL's coordinate space.
    SDL_Rect desktopRect;

    int refreshRate;
    bool primary;
};

/**
 * @brief The client's monitor arrangement, in the form a host needs to reproduce it.
 *
 * The "canvas" is the arrangement's bounding box, and it is what the video stream carries:
 * one stream, holding every emulated display side by side exactly where the client has
 * them. Gaps in a non-tiling arrangement are simply black.
 *
 * Keeping the canvas geometrically identical to the emulated host desktop is the whole
 * trick -- it is what lets absolute mouse, touch and pen coordinates map straight through
 * without touching the protocol, because the host derives its coordinate plane from the
 * captured display's own offset and size.
 */
class DisplayLayout
{
public:
    /// Same ceiling the host enforces on `displayLayout=`; a bigger arrangement is refused
    /// on both sides rather than silently truncated on one.
    static const int k_MaxDisplays = 4;

    /**
     * @brief Describe the monitor arrangement of the machine we are running on.
     *
     * Requires SDL video to be initialised. A single-monitor machine yields a layout that
     * reports false from isMultiDisplay() and is not an error.
     */
    static DisplayLayout detect();

    bool isMultiDisplay() const
    {
        return m_Displays.count() >= 2;
    }

    const QVector<DisplayLayoutEntry>& displays() const
    {
        return m_Displays;
    }

    /// Stream size to request. Always even: encoders reject odd dimensions, and the
    /// bounding box of a real arrangement can easily be odd.
    int canvasWidth() const
    {
        return m_CanvasWidth;
    }

    int canvasHeight() const
    {
        return m_CanvasHeight;
    }

    /// Highest refresh rate among the monitors. One stream carries one frame rate, so the
    /// slower monitors drop frames rather than the faster ones being held back.
    int refreshRate() const
    {
        return m_RefreshRate;
    }

    /// The arrangement's bounding box on the client desktop: where to put a single window
    /// covering every monitor. Up to one pixel smaller than the canvas per axis.
    SDL_Rect desktopBounds() const
    {
        return m_DesktopBounds;
    }

    /**
     * @brief True when the monitors leave no gap in their bounding box.
     *
     * When they tile it, one window spanning the lot shows each monitor exactly its own
     * region, and no per-monitor rendering is needed at all. When they do not -- a 1440p
     * beside a 1080p, say -- a spanning window would paint the gap onto real screen space,
     * so each monitor needs its own window sampling its own part of the canvas.
     */
    bool tilesBoundingBox() const
    {
        return m_Tiles;
    }

    /// Value for the `displayLayout=` launch argument: `x,y,w,h,primary` per display,
    /// semicolon separated.
    QString toLaunchArgument() const;

    /// Why no usable layout could be described, suitable for a launch warning. Empty when
    /// there is no problem, including on an ordinary single-monitor machine.
    QString problem() const
    {
        return m_Problem;
    }

private:
    QVector<DisplayLayoutEntry> m_Displays;
    SDL_Rect m_DesktopBounds {0, 0, 0, 0};
    int m_CanvasWidth = 0;
    int m_CanvasHeight = 0;
    int m_RefreshRate = 0;
    bool m_Tiles = false;
    QString m_Problem;
};
