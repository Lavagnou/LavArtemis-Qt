#include "displaylayout.h"
#include "streamutils.h"

#include <QCoreApplication>
#include <QStringList>

DisplayLayout DisplayLayout::detect()
{
    DisplayLayout layout;

    SDL_assert(SDL_WasInit(SDL_INIT_VIDEO));

    int displayCount = SDL_GetNumVideoDisplays();
    if (displayCount < 0) {
        layout.m_Problem = QCoreApplication::translate(
            "DisplayLayout", "Unable to enumerate displays: %1").arg(SDL_GetError());
        return layout;
    }

    if (displayCount < 2) {
        // Not an error, just nothing to mirror.
        return layout;
    }

    if (displayCount > k_MaxDisplays) {
        layout.m_Problem = QCoreApplication::translate(
            "DisplayLayout",
            "This PC has %1 displays, which is more than can be emulated on the host (%2).")
            .arg(displayCount).arg(k_MaxDisplays);
        return layout;
    }

    QVector<DisplayLayoutEntry> displays;

    for (int i = 0; i < displayCount; i++) {
        SDL_Rect bounds;
        if (SDL_GetDisplayBounds(i, &bounds) != 0) {
            layout.m_Problem = QCoreApplication::translate(
                "DisplayLayout", "Unable to read the bounds of display %1: %2")
                .arg(i).arg(SDL_GetError());
            return layout;
        }

        SDL_DisplayMode mode;
        SDL_Rect safeArea;
        if (!StreamUtils::getNativeDesktopMode(i, &mode, &safeArea)) {
            layout.m_Problem = QCoreApplication::translate(
                "DisplayLayout", "Unable to read the mode of display %1.").arg(i);
            return layout;
        }

        // Display bounds and the native mode have to agree. If they do not, one of them is
        // in DPI-scaled coordinates and the other in physical pixels, and a layout built
        // from both would take its positions from one space and its sizes from the other --
        // matching neither. There is no safe way to guess which, so refuse the layout and
        // say why rather than emulate a subtly wrong arrangement.
        if (bounds.w != mode.w || bounds.h != mode.h) {
            layout.m_Problem = QCoreApplication::translate(
                "DisplayLayout",
                "Display %1 reports %2x%3 bounds but a %4x%5 native mode. Multi-display "
                "streaming needs these to match; check the display scaling settings.")
                .arg(i).arg(bounds.w).arg(bounds.h).arg(mode.w).arg(mode.h);
            return layout;
        }

        DisplayLayoutEntry entry;
        entry.desktopRect = bounds;
        entry.canvasRect = bounds;  // rebased below, once the bounding box is known
        entry.refreshRate = mode.refresh_rate;

        // Windows puts the primary display at the origin of the desktop, and SDL passes
        // that through. Nothing else in the arrangement identifies it.
        entry.primary = (bounds.x == 0 && bounds.y == 0);

        displays.append(entry);
    }

    int left = displays.first().desktopRect.x;
    int top = displays.first().desktopRect.y;
    int right = left;
    int bottom = top;
    int refreshRate = 0;
    int coveredArea = 0;
    int primaryCount = 0;

    for (const DisplayLayoutEntry& entry : displays) {
        left = qMin(left, entry.desktopRect.x);
        top = qMin(top, entry.desktopRect.y);
        right = qMax(right, entry.desktopRect.x + entry.desktopRect.w);
        bottom = qMax(bottom, entry.desktopRect.y + entry.desktopRect.h);
        refreshRate = qMax(refreshRate, entry.refreshRate);
        coveredArea += entry.desktopRect.w * entry.desktopRect.h;
        primaryCount += entry.primary ? 1 : 0;
    }

    if (primaryCount != 1) {
        // Either no display sits at the origin or several claim to. The host insists on
        // exactly one primary, so pick the first rather than send a layout it will reject.
        for (DisplayLayoutEntry& entry : displays) {
            entry.primary = false;
        }
        displays.first().primary = true;
    }

    layout.m_DesktopBounds.x = left;
    layout.m_DesktopBounds.y = top;
    layout.m_DesktopBounds.w = right - left;
    layout.m_DesktopBounds.h = bottom - top;

    for (DisplayLayoutEntry& entry : displays) {
        entry.canvasRect.x = entry.desktopRect.x - left;
        entry.canvasRect.y = entry.desktopRect.y - top;
    }

    // Video encoders want even dimensions, and a real arrangement's bounding box need not
    // be even -- a 1920x1080 above a 1920x515 panel is 1595 tall. Round the canvas up and
    // leave the extra row or column black; it falls outside every monitor's own region, so
    // nothing displays it.
    layout.m_CanvasWidth = (layout.m_DesktopBounds.w + 1) & ~1;
    layout.m_CanvasHeight = (layout.m_DesktopBounds.h + 1) & ~1;

    // Physical monitors cannot overlap, so covering the bounding box's area is the same as
    // tiling it exactly.
    layout.m_Tiles = (coveredArea == layout.m_DesktopBounds.w * layout.m_DesktopBounds.h);

    layout.m_RefreshRate = refreshRate;
    layout.m_Displays = displays;

    return layout;
}

QString DisplayLayout::toLaunchArgument() const
{
    QStringList parts;

    for (const DisplayLayoutEntry& entry : m_Displays) {
        parts.append(QString("%1,%2,%3,%4,%5")
                         .arg(entry.canvasRect.x)
                         .arg(entry.canvasRect.y)
                         .arg(entry.canvasRect.w)
                         .arg(entry.canvasRect.h)
                         .arg(entry.primary ? 1 : 0));
    }

    return parts.join(';');
}
