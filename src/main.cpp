#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <map>
#include <set>
#include <vector>

using namespace geode::prelude;

namespace {
    constexpr float GRID = 30.f;

    struct Piece {
        int id = 1;
        float rot = 0.f;
        bool flipX = false;
        bool flipY = false;
    };

    using Cell = std::pair<int, int>; // (kolumna, wiersz)

    Cell cellOf(GameObject* o) {
        auto p = o->getPosition();
        return { (int)std::lround(p.x / GRID), (int)std::lround(p.y / GRID) };
    }
}

class $modify(ATEditorUI, EditorUI) {
    struct Fields {
        // szablon [wiersz][kolumna], wiersz 0 = gorny
        std::vector<std::vector<Piece>> tmpl;
    };

    bool init(LevelEditorLayer* layer) {
        if (!EditorUI::init(layer)) return false;

        auto win = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("autotexture-menu"_spr);
        menu->setPosition({ win.width - 45.f, win.height - 150.f });
        menu->setLayout(ColumnLayout::create()->setGap(6.f)->setAxisReverse(true));

        auto setBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Set"), this, menu_selector(ATEditorUI::onATSet));
        auto applyBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Apply"), this, menu_selector(ATEditorUI::onATApply));
        menu->addChild(setBtn);
        menu->addChild(applyBtn);
        menu->updateLayout();
        this->addChild(menu, 100);
        return true;
    }

    void onATSet(CCObject*) {
        auto sel = this->getSelectedObjects();
        if (!sel || sel->count() == 0) {
            FLAlertLayer::create("AutoTexture", "Zaznacz najpierw blok(i) z teksturami, potem kliknij Set.", "OK")->show();
            return;
        }

        std::map<Cell, GameObject*> cells;
        int minC = INT_MAX, maxC = INT_MIN, minR = INT_MAX, maxR = INT_MIN;
        for (auto obj : CCArrayExt<GameObject*>(sel)) {
            auto c = cellOf(obj);
            cells[c] = obj;
            minC = std::min(minC, c.first);  maxC = std::max(maxC, c.first);
            minR = std::min(minR, c.second); maxR = std::max(maxR, c.second);
        }
        int w = maxC - minC + 1;
        int h = maxR - minR + 1;

        auto toPiece = [](GameObject* o) {
            Piece p;
            p.id = o->m_objectID;
            p.rot = o->getRotation();
            p.flipX = o->m_isFlipX;
            p.flipY = o->m_isFlipY;
            return p;
        };
        Piece fallback = toPiece(cells.begin()->second);

        auto& t = m_fields->tmpl;
        t.assign(h, std::vector<Piece>(w, fallback));
        for (auto& [c, o] : cells) {
            int col = c.first - minC;
            int row = maxR - c.second; // gora = 0
            t[row][col] = toPiece(o);
        }

        Notification::create(
            fmt::format("Szablon zapisany: {}x{}", w, h),
            NotificationIcon::Success)->show();
    }

    void onATApply(CCObject*) {
        auto& t = m_fields->tmpl;
        if (t.empty()) {
            FLAlertLayer::create("AutoTexture", "Najpierw ustaw szablon przyciskiem Set.", "OK")->show();
            return;
        }
        auto sel = this->getSelectedObjects();
        if (!sel || sel->count() == 0) {
            FLAlertLayer::create("AutoTexture", "Zaznacz layout (Select All) i kliknij Apply.", "OK")->show();
            return;
        }

        int srcId = (int)Mod::get()->getSettingValue<int64_t>("source-id");
        int h = (int)t.size();
        int w = (int)t[0].size();

        std::vector<GameObject*> targets;
        std::set<Cell> grid;
        for (auto obj : CCArrayExt<GameObject*>(sel)) {
            if (obj->m_objectID == srcId) {
                targets.push_back(obj);
                grid.insert(cellOf(obj));
            }
        }
        if (targets.empty()) {
            Notification::create("Brak blokow do zamiany w zaznaczeniu", NotificationIcon::Warning)->show();
            return;
        }

        auto has = [&](Cell c, int dx, int dy) {
            return grid.count({ c.first + dx, c.second + dy }) > 0;
        };

        auto newObjs = CCArray::create();
        for (auto obj : targets) {
            Cell c = cellOf(obj);
            bool L = has(c, -1, 0), R = has(c, 1, 0);
            bool U = has(c, 0, 1),  D = has(c, 0, -1);

            int col;
            if (w == 1) col = 0;
            else if (!L) col = 0;
            else if (!R) col = w - 1;
            else col = (w >= 3) ? 1 : 0;

            int row;
            if (h == 1) row = 0;
            else if (!U) row = 0;
            else if (!D) row = h - 1;
            else row = (h >= 3) ? 1 : 0;

            const Piece& p = t[row][col];
            auto pos = obj->getPosition();
            auto z = obj->m_zLayer;

            auto n = m_editorLayer->createObject(p.id, pos, true);
            if (!n) continue;
            n->setRotation(p.rot);
            if (p.flipX) n->setFlipX(true);
            if (p.flipY) n->setFlipY(true);
            n->m_zLayer = z;
            newObjs->addObject(n);
        }

        for (auto obj : targets) {
            m_editorLayer->removeObject(obj, true);
        }

        this->selectObjects(newObjs, true);
        Notification::create(
            fmt::format("Zamieniono {} blokow", newObjs->count()),
            NotificationIcon::Success)->show();
    }
};
