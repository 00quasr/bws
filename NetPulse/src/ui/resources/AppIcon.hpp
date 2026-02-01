#pragma once

#include <QIcon>
#include <QPixmap>

namespace netpulse::ui {

/**
 * @brief Provides application branding icons.
 *
 * Generates NetPulse icons programmatically using Qt's painting system.
 * This avoids external dependencies on SVG libraries while maintaining
 * consistent branding across all icon sizes.
 */
class AppIcon {
public:
    /**
     * @brief Get the main application icon.
     *
     * Returns a QIcon with the NetPulse logo rendered at multiple sizes
     * for use as window icon and taskbar icon.
     *
     * @return QIcon containing the application icon
     */
    static QIcon applicationIcon();

    /**
     * @brief Get the system tray icon.
     *
     * Returns a simplified version of the logo optimized for small sizes
     * typically used in system tray.
     *
     * @return QIcon containing the tray icon
     */
    static QIcon trayIcon();

    /**
     * @brief Generate icon pixmap at specific size.
     *
     * @param size Icon size in pixels (width and height)
     * @param simplified Use simplified version for small sizes
     * @return QPixmap with the icon
     */
    static QPixmap createPixmap(int size, bool simplified = false);

private:
    static void drawIcon(QPainter& painter, int size, bool simplified);
    static void drawPulseLine(QPainter& painter, int size, bool simplified);
};

} // namespace netpulse::ui
