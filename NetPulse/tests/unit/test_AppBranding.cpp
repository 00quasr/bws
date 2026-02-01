#include <catch2/catch_test_macros.hpp>

#include "ui/resources/AppIcon.hpp"

#include <QApplication>
#include <QIcon>
#include <QPixmap>

using namespace netpulse::ui;

namespace {

// Ensure QApplication is initialized for Qt GUI tests
struct QtAppInitializer {
    QtAppInitializer() {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {const_cast<char*>("test"), nullptr};
            app = new QApplication(argc, argv);
        }
    }
    QApplication* app{nullptr};
};

// Static initialization to ensure QApplication exists
static QtAppInitializer qtInit;

} // namespace

TEST_CASE("AppIcon generates valid application icon", "[branding]") {
    SECTION("Application icon is not null") {
        QIcon icon = AppIcon::applicationIcon();
        REQUIRE(!icon.isNull());
    }

    SECTION("Application icon provides multiple sizes") {
        QIcon icon = AppIcon::applicationIcon();

        // Test standard icon sizes
        QPixmap px16 = icon.pixmap(16, 16);
        QPixmap px32 = icon.pixmap(32, 32);
        QPixmap px64 = icon.pixmap(64, 64);
        QPixmap px128 = icon.pixmap(128, 128);

        REQUIRE(!px16.isNull());
        REQUIRE(!px32.isNull());
        REQUIRE(!px64.isNull());
        REQUIRE(!px128.isNull());

        REQUIRE(px16.width() == 16);
        REQUIRE(px16.height() == 16);
        REQUIRE(px32.width() == 32);
        REQUIRE(px32.height() == 32);
        REQUIRE(px64.width() == 64);
        REQUIRE(px64.height() == 64);
        REQUIRE(px128.width() == 128);
        REQUIRE(px128.height() == 128);
    }
}

TEST_CASE("AppIcon generates valid tray icon", "[branding]") {
    SECTION("Tray icon is not null") {
        QIcon icon = AppIcon::trayIcon();
        REQUIRE(!icon.isNull());
    }

    SECTION("Tray icon works at small sizes") {
        QIcon icon = AppIcon::trayIcon();

        // System tray typically uses these sizes
        QPixmap px16 = icon.pixmap(16, 16);
        QPixmap px22 = icon.pixmap(22, 22);
        QPixmap px24 = icon.pixmap(24, 24);
        QPixmap px32 = icon.pixmap(32, 32);

        REQUIRE(!px16.isNull());
        REQUIRE(!px22.isNull());
        REQUIRE(!px24.isNull());
        REQUIRE(!px32.isNull());
    }
}

TEST_CASE("AppIcon::createPixmap generates valid pixmaps", "[branding]") {
    SECTION("Creates pixmaps at requested sizes") {
        QPixmap px16 = AppIcon::createPixmap(16);
        QPixmap px32 = AppIcon::createPixmap(32);
        QPixmap px64 = AppIcon::createPixmap(64);

        REQUIRE(px16.width() == 16);
        REQUIRE(px32.width() == 32);
        REQUIRE(px64.width() == 64);
    }

    SECTION("Simplified mode produces valid pixmaps") {
        QPixmap pxSimple = AppIcon::createPixmap(16, true);
        QPixmap pxDetailed = AppIcon::createPixmap(64, false);

        REQUIRE(!pxSimple.isNull());
        REQUIRE(!pxDetailed.isNull());
    }

    SECTION("Pixmaps have transparency") {
        QPixmap px = AppIcon::createPixmap(64);
        REQUIRE(px.hasAlpha());
    }
}

TEST_CASE("AppIcon handles edge cases", "[branding]") {
    SECTION("Very small sizes work") {
        QPixmap px8 = AppIcon::createPixmap(8);
        REQUIRE(!px8.isNull());
        REQUIRE(px8.width() == 8);
    }

    SECTION("Large sizes work") {
        QPixmap px512 = AppIcon::createPixmap(512);
        REQUIRE(!px512.isNull());
        REQUIRE(px512.width() == 512);
    }

    SECTION("Odd sizes work") {
        QPixmap px17 = AppIcon::createPixmap(17);
        QPixmap px37 = AppIcon::createPixmap(37);

        REQUIRE(!px17.isNull());
        REQUIRE(!px37.isNull());
        REQUIRE(px17.width() == 17);
        REQUIRE(px37.width() == 37);
    }
}
