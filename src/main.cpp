#include <Geode/Geode.hpp>
#include <Geode/modify/CCScene.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace cb {
    // unique tags to disassociate from others
    static constexpr int kFilterTag = 0xCB01;
    static constexpr int kIndicatorTag = 0xCB02;

    struct Tint {
        GLubyte r, g, b;
    };

    // These are hand tuned values that looked decent in-game
    // Not trying to perfectly simulate colorblindness (yet)
    static Tint tintForMode(std::string const& mode) {
        if (mode == "Protanopia")   return { 180, 230, 255 };
        if (mode == "Deuteranopia") return { 240, 180, 255 };
        if (mode == "Tritanopia")   return { 255, 230, 175 };
        return { 255, 255, 255 }; // no change
    }

    // Blend from normal white toward the target tint based on the slider value.
    // 0 = off, 1 = full strength.
    static ccColor3B currentColor() {
        auto mode = Mod::get()->getSettingValue<std::string>("filter-mode");
        auto strength = static_cast<float>(Mod::get()->getSettingValue<double>("filter-strength"));
        auto tint = tintForMode(mode);

        auto mix = [strength](GLubyte target) -> GLubyte {
            return static_cast<GLubyte>(
                255.f + (static_cast<float>(target) - 255.f) * strength
            );
        };

        return { mix(tint.r), mix(tint.g), mix(tint.b) };
    }

    // GD sometimes keeps scenes around longer than expected
    // so always remove the old overlay before adding a new one
    static void removeFilter(CCNode* node) {
        if (!node) return;

        if (auto old = node->getChildByTag(kFilterTag)) {
            old->removeFromParent();
        }
    }

    // This is the effect
    // just a fullscreen color layer with multiply blending
    // Way simpler than messing with shaders and works everywhere
    static void applyFilter(CCNode* node) {
        if (!node) return;

        removeFilter(node);

        // if disabled, do nothing
        if (!Mod::get()->getSettingValue<bool>("enabled"))
            return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto color = currentColor();

        auto overlay = CCLayerColor::create(ccc4(color.r, color.g, color.b, 255));
        overlay->setContentSize(winSize);
        overlay->setPosition({ 0.f, 0.f });
        overlay->setTag(kFilterTag);

        // multiply blend
        overlay->setBlendFunc({ GL_DST_COLOR, GL_ZERO });

        // high z so it sits on top of everything
        overlay->setZOrder(999);

        node->addChild(overlay);
    }

    // reapply to whatever scene is active right now
    // this works fine for now, but it is a bit brute-force
    // ideally I would cache the overlay and just update the color
    static void refresh() {
        if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
            applyFilter(scene);
        }
    }
}

// hook every scene so the filter always comes back automatically
class $modify(CBScene, CCScene) {
    bool init() {
        if (!CCScene::init()) return false;

        // not entirely sure why, but applying immediately here sometimes
        // misses stuff on first load, so delaying by a frame is more reliable
        this->scheduleOnce(schedule_selector(CBScene::apply), 0.f);
        return true;
    }

    void apply(float) {
        cb::applyFilter(this);
    }
};

// small toggle button in the main menu
class $modify(CBMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        // allow disabling of the button entirely
        if (!Mod::get()->getSettingValue<bool>("show-menu-button"))
            return true;

        // get window size directly from gd
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // try to use geode's in-built settings icon first, then
        // default to GD's normal options button
        auto spr = CircleButtonSprite::createWithSpriteFrameName(
            "geode.loader/settings.png",
            0.6f,
            CircleBaseColor::DarkPurple,
            CircleBaseSize::Small
        );
        if (!spr) {
            spr = CircleButtonSprite::createWithSpriteFrameName(
                "GJ_optionsBtn_001.png",
                0.45f,
                CircleBaseColor::DarkPurple,
                CircleBaseSize::Small
            );
        }

        // if it still fails somehow, log the error
        if (!spr) {
            log::warn("Couldn't create colorblind toggle sprite");
            return true;
        }

        auto btn = CCMenuItemSpriteExtra::create(
            spr,
            this,
            menu_selector(CBMenuLayer::onToggle)
        );
        btn->setID("colorblind-toggle-button"_spr);

        // separate menu for this button so other menu's don't get messed up
        auto menu = CCMenu::create();
        menu->setPosition({ winSize.width - 30.f, winSize.height - 30.f });
        this->addChild(menu);
        menu->addChild(btn);

        // green = enabled, white = off
        // quick visual feedback without popup
        bool active = Mod::get()->getSettingValue<bool>("enabled");
        btn->setColor(active ? ccColor3B{ 100, 255, 100 } : ccColor3B{ 255, 255, 255 });

        return true;
    }

    void onToggle(CCObject* sender) {
        bool enabled = Mod::get()->getSettingValue<bool>("enabled");

        // flip setting
        Mod::get()->setSettingValue<bool>("enabled", !enabled);

        // update color immediately
        if (auto btn = static_cast<CCMenuItemSpriteExtra*>(sender)) {
            btn->setColor(!enabled
                ? ccColor3B{ 100, 255, 100 }
                : ccColor3B{ 255, 255, 255 });
        }

        // reapply filter to scene
        // might be slightly overkill since we recreate the overlay each time
        // but it's simple and works (reliably)
        cb::refresh();
    }
};

// tiny "CB" indicator in-game so the person using
// the mod knows it's on
// mainly added because I kept on forgetting if
// the filter was enabled or not
class $modify(CBPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        // only show if both enabled & user actually wants the indicator
        if (Mod::get()->getSettingValue<bool>("enabled") &&
            Mod::get()->getSettingValue<bool>("show-indicator")) {

            auto winSize = CCDirector::sharedDirector()->getWinSize();

            auto label = CCLabelBMFont::create("CB", "chatFont.fnt");

            // top-right corner and small to work with other mods without
            // relying on dependencies (could implement in the future)
            // might need tweaking
            label->setPosition({ winSize.width - 20.f, winSize.height - 15.f });
            label->setScale(0.4f); // small but readable (per testing)
            label->setOpacity(120); // slightly transparent so it's not distracting
            label->setTag(cb::kIndicatorTag);

            this->addChild(label);
        }

        return true;
    }
};

// update live when settings change (no restart needed)
$execute {
    listenForSettingChanges<bool>("enabled", [](bool) {
        cb::refresh();
    });

    listenForSettingChanges<std::string>("filter-mode", [](std::string) {
        cb::refresh();
    });

    listenForSettingChanges<double>("filter-strength", [](double) {
        cb::refresh();
    });
}