/**
 * ═══════════════════════════════════════════════════════════════
 *  Colorblind Mode — Geode Mod for Geometry Dash
 *  Author:  garesplayzz12
 *  License: MIT
 * ═══════════════════════════════════════════════════════════════
 *
 *  Applies colorblind correction by overlaying a tinted layer
 *  with multiplicative blending (GL_DST_COLOR). This shifts
 *  the per-channel color balance to help distinguish colors.
 *
 *  No render-to-texture, no custom shaders — just a colored
 *  CCLayerColor that multiplies with every pixel underneath.
 */

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCScene.hpp>
#include <Geode/loader/SettingV3.hpp>

using namespace geode::prelude;

// ── Constants ─────────────────────────────────────────────────
static constexpr int FILTER_TAG    = 0xCB01;
static constexpr int INDICATOR_TAG = 0xCB02;
static constexpr int MENU_BTN_TAG  = 0xCB03;

// ═══════════════════════════════════════════════════════════════
//  Color tint values per mode
// ═══════════════════════════════════════════════════════════════
//
//  With GL_DST_COLOR, GL_ZERO blending the result is:
//      output = overlay_color * screen_color  (per-channel)
//
//  Values below 255 reduce that channel, creating a color shift
//  that helps distinguish problem colors.
//
//  These are applied at strength=1.0. At strength=0, all channels
//  are 255 (no change). The slider lerps between them.
// ═══════════════════════════════════════════════════════════════

struct ColorTint {
    uint8_t r, g, b;
};

static std::unordered_map<std::string, ColorTint> const TINT_VALUES = {
    // Protanopia (red-weak): reduce red, keep green/blue
    { "Protanopia",   { 180, 230, 255 } },
    // Deuteranopia (green-weak): reduce green, keep red/blue
    { "Deuteranopia", { 240, 180, 255 } },
    // Tritanopia (blue-weak): reduce blue, keep red/green
    { "Tritanopia",   { 255, 230, 175 } },
};

// ═══════════════════════════════════════════════════════════════
//  Utility — Compute the blended tint color for current settings
// ═══════════════════════════════════════════════════════════════

static ccColor3B computeTintColor() {
    auto mode     = Mod::get()->getSettingValue<std::string>("filter-mode");
    auto strength = static_cast<float>(Mod::get()->getSettingValue<double>("filter-strength"));

    auto it = TINT_VALUES.find(mode);
    if (it == TINT_VALUES.end()) {
        return { 255, 255, 255 };
    }

    auto const& t = it->second;

    // Lerp each channel: 255 (no effect) → target (full effect)
    auto lerp = [&](uint8_t target) -> uint8_t {
        return static_cast<uint8_t>(255.f + (static_cast<float>(target) - 255.f) * strength);
    };

    return { lerp(t.r), lerp(t.g), lerp(t.b) };
}

// ═══════════════════════════════════════════════════════════════
//  Utility — Add or update the filter overlay on a scene
// ═══════════════════════════════════════════════════════════════

static void applyFilter(CCNode* target) {
    if (!target) return;

    bool enabled = Mod::get()->getSettingValue<bool>("enabled");

    // Remove existing filter
    if (auto* old = target->getChildByTag(FILTER_TAG)) {
        old->removeFromParent();
    }

    if (!enabled) return;

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto color   = computeTintColor();

    // Full-screen colored layer with multiplicative blending
    auto* overlay = CCLayerColor::create(ccc4(color.r, color.g, color.b, 255));
    overlay->setContentSize(winSize);
    overlay->setPosition({ 0.f, 0.f });
    overlay->setTag(FILTER_TAG);
    overlay->setID("colorblind-filter"_spr);
    overlay->setZOrder(999);

    // GL_DST_COLOR, GL_ZERO = output = overlay * screen (per-channel)
    overlay->setBlendFunc({ GL_DST_COLOR, GL_ZERO });

    target->addChild(overlay);
}

static void refreshRunningScene() {
    auto* scene = CCDirector::sharedDirector()->getRunningScene();
    if (scene) applyFilter(scene);
}

// ═══════════════════════════════════════════════════════════════
//  Hook: CCScene — Auto-apply filter on every new scene
// ═══════════════════════════════════════════════════════════════

class $modify(ColorblindScene, CCScene) {
    bool init() {
        if (!CCScene::init()) return false;

        // Deferred so the scene is fully built
        this->scheduleOnce(
            schedule_selector(ColorblindScene::onApplyFilter),
            0.f
        );
        return true;
    }

    void onApplyFilter(float) {
        applyFilter(this);
    }
};

// ═══════════════════════════════════════════════════════════════
//  Hook: MenuLayer — Small toggle button, top-right
// ═══════════════════════════════════════════════════════════════

class $modify(ColorblindMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        if (!Mod::get()->getSettingValue<bool>("show-menu-button")) {
            return true;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto* btnSpr = CircleButtonSprite::createWithSpriteFrameName(
            "geode.loader/settings.png", 0.6f,
            CircleBaseColor::DarkPurple, CircleBaseSize::Small
        );

        if (!btnSpr) {
            btnSpr = CircleButtonSprite::createWithSpriteFrameName(
                "GJ_optionsBtn_001.png", 0.45f,
                CircleBaseColor::DarkPurple, CircleBaseSize::Small
            );
        }

        auto* btn = CCMenuItemSpriteExtra::create(
            btnSpr, this,
            menu_selector(ColorblindMenuLayer::onColorblindToggle)
        );
        btn->setTag(MENU_BTN_TAG);
        btn->setID("colorblind-toggle-btn"_spr);

        auto* menu = CCMenu::create();
        menu->setPosition({ winSize.width - 30.f, winSize.height - 30.f });
        menu->setID("colorblind-menu"_spr);
        menu->setZOrder(10);
        this->addChild(menu);
        menu->addChild(btn);

        bool active = Mod::get()->getSettingValue<bool>("enabled");
        btn->setColor(active ? ccColor3B{ 100, 255, 100 } : ccColor3B{ 255, 255, 255 });

        return true;
    }

    void onColorblindToggle(CCObject* sender) {
        bool current = Mod::get()->getSettingValue<bool>("enabled");
        bool next    = !current;

        Mod::get()->setSettingValue<bool>("enabled", next);

        if (auto* btn = static_cast<CCMenuItemSpriteExtra*>(sender)) {
            btn->setColor(next ? ccColor3B{ 100, 255, 100 } : ccColor3B{ 255, 255, 255 });
        }

        // Immediately update the current scene
        refreshRunningScene();

        auto mode = Mod::get()->getSettingValue<std::string>("filter-mode");
        Notification::create(
            next
                ? fmt::format("Colorblind filter ON ({})", mode)
                : "Colorblind filter OFF",
            NotificationIcon::Success,
            1.5f
        )->show();
    }
};

// ═══════════════════════════════════════════════════════════════
//  Hook: PlayLayer — Gameplay HUD indicator
// ═══════════════════════════════════════════════════════════════

class $modify(ColorblindPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        bool enabled = Mod::get()->getSettingValue<bool>("enabled");
        bool showInd = Mod::get()->getSettingValue<bool>("show-indicator");

        if (enabled && showInd) {
            auto winSize = CCDirector::sharedDirector()->getWinSize();

            auto* label = CCLabelBMFont::create("CB", "chatFont.fnt");
            label->setPosition({ winSize.width - 20.f, winSize.height - 15.f });
            label->setScale(0.4f);
            label->setOpacity(120);
            label->setTag(INDICATOR_TAG);
            label->setID("colorblind-indicator"_spr);
            label->setZOrder(1000);
            this->addChild(label);
        }

        return true;
    }
};

// ═══════════════════════════════════════════════════════════════
//  Setting listeners — refresh filter on any change
// ═══════════════════════════════════════════════════════════════

static ListenerHandle* s_enabledListener   = nullptr;
static ListenerHandle* s_filterModeListener = nullptr;
static ListenerHandle* s_strengthListener   = nullptr;

$execute {
    s_enabledListener = listenForSettingChanges<bool>("enabled", [](bool) {
        refreshRunningScene();
    });

    s_filterModeListener = listenForSettingChanges<std::string>("filter-mode", [](std::string) {
        refreshRunningScene();
    });

    s_strengthListener = listenForSettingChanges<double>("filter-strength", [](double) {
        refreshRunningScene();
    });
}