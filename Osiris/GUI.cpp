#include <algorithm>
#include <array>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <ShlObj.h>
#include <Windows.h>
#endif

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_stdlib.h"

#include "imguiCustom.h"

#include "GUI.h"
#include "Config.h"
#include "ConfigStructs.h"
#include "Hacks/Misc.h"
#include "Hacks/InventoryChanger.h"
#include "Helpers.h"
#include "Hooks.h"
#include "Interfaces.h"
#include "SDK/InputSystem.h"
#include "Hacks/Visuals.h"
#include "Hacks/Glow.h"
#include "Hacks/AntiAim.h"
#include "Hacks/Backtrack.h"
#include "Hacks/Sound.h"

constexpr auto windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

static ImFont* addFontFromVFONT(const std::string& path, float size, const ImWchar* glyphRanges, bool merge) noexcept
{
    auto file = Helpers::loadBinaryFile(path);
    if (!Helpers::decodeVFONT(file))
        return nullptr;

    ImFontConfig cfg;
    cfg.FontData = file.data();
    cfg.FontDataSize = file.size();
    cfg.FontDataOwnedByAtlas = false;
    cfg.MergeMode = merge;
    cfg.GlyphRanges = glyphRanges;
    cfg.SizePixels = size;

    return ImGui::GetIO().Fonts->AddFont(&cfg);
}

GUI::GUI() noexcept
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.ScrollbarSize = 9.0f;

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImFontConfig cfg;
    cfg.SizePixels = 15.0f;

#ifdef _WIN32
    if (PWSTR pathToFonts; SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &pathToFonts))) {
        const std::filesystem::path path{ pathToFonts };
        CoTaskMemFree(pathToFonts);

        fonts.msyh = io.Fonts->AddFontFromFileTTF((path / "msyh.ttc").string().c_str(), 15.0f, &cfg, Helpers::getFontGlyphRanges());
        if (!fonts.msyh)
            io.Fonts->AddFontDefault(&cfg);

        cfg.MergeMode = true;
        static constexpr ImWchar symbol[]{
            0x2605, 0x2605, // ★
            0
        };
        io.Fonts->AddFontFromFileTTF((path / "msyh.ttc").string().c_str(), 15.0f, &cfg, symbol);
        cfg.MergeMode = false;
    }
#else
    fonts.normal15px = addFontFromVFONT("csgo/panorama/fonts/notosans-regular.vfont", 15.0f, Helpers::getFontGlyphRanges(), false);
#endif
    if (!fonts.msyh)
        io.Fonts->AddFontDefault(&cfg);
    addFontFromVFONT("csgo/panorama/fonts/notosanskr-regular.vfont", 15.0f, io.Fonts->GetGlyphRangesKorean(), true);
    addFontFromVFONT("csgo/panorama/fonts/notosanssc-regular.vfont", 17.0f, io.Fonts->GetGlyphRangesChineseFull(), true);
}

void GUI::render() noexcept
{
    if (!config->style.menuStyle) {
        renderMenuBar();
        renderAimbotWindow();
        AntiAim::drawGUI(false);
        renderTriggerbotWindow();
        Backtrack::drawGUI(false);
        Glow::drawGUI(false);
        renderChamsWindow();
        renderStreamProofESPWindow();
        Visuals::drawGUI(false);
        InventoryChanger::drawGUI(false);
        Sound::drawGUI(false);
        renderStyleWindow();
        renderMiscWindow();
        renderConfigWindow();
    } else {
        renderGuiStyle2();
    }
}

void GUI::updateColors() const noexcept
{
    switch (config->style.menuColors) {
    case 0: ImGui::StyleColorsDark(); break;
    case 1: ImGui::StyleColorsLight(); break;
    case 2: ImGui::StyleColorsClassic(); break;
    }
}

void GUI::handleToggle() noexcept
{
    if (config->misc.menuKey.isPressed()) {
        open = !open;
        if (!open)
            interfaces->inputSystem->resetInputState();
#ifndef _WIN32
        ImGui::GetIO().MouseDrawCursor = gui->open;
#endif
    }
}

static void menuBarItem(const char* name, bool& enabled) noexcept
{
    if (ImGui::MenuItem(name)) {
        enabled = true;
        ImGui::SetWindowFocus(name);
        ImGui::SetWindowPos(name, { 100.0f, 100.0f });
    }
}

void GUI::renderMenuBar() noexcept
{
    if (ImGui::BeginMainMenuBar()) {
        menuBarItem("自瞄", window.aimbot);
        AntiAim::menuBarItem();
        menuBarItem("自动开火", window.triggerbot);
        Backtrack::menuBarItem();
        Glow::menuBarItem();
        menuBarItem("实体", window.chams);
        menuBarItem("ESP", window.streamProofESP);
        Visuals::menuBarItem();
        InventoryChanger::menuBarItem();
        Sound::menuBarItem();
        menuBarItem("样式", window.style);
        menuBarItem("杂项", window.misc);
        menuBarItem("配置", window.config);
        ImGui::EndMainMenuBar();   
    }
}

void GUI::renderAimbotWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.aimbot)
            return;
        ImGui::SetNextWindowSize({ 600.0f, 0.0f });
        ImGui::Begin("自瞄", &window.aimbot, windowFlags);
    }
    ImGui::Checkbox("绑定按键", &config->aimbotOnKey);
    ImGui::SameLine();
    ImGui::PushID("Aimbot Key");
    ImGui::hotkey("", config->aimbotKey);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::PushID(2);
    ImGui::PushItemWidth(70.0f);
    ImGui::Combo("", &config->aimbotKeyMode, "保持\0切换\0");
    ImGui::PopItemWidth();
    ImGui::PopID();
    ImGui::Separator();
    static int currentCategory{ 0 };
    ImGui::PushItemWidth(110.0f);
    ImGui::PushID(0);
    ImGui::Combo("", &currentCategory, "全部\0手枪\0重型武器\0微型冲锋枪\0步枪\0");
    ImGui::PopID();
    ImGui::SameLine();
    static int currentWeapon{ 0 };
    ImGui::PushID(1);

    switch (currentCategory) {
    case 0:
        currentWeapon = 0;
        ImGui::NewLine();
        break;
    case 1: {
        static int currentPistol{ 0 };
        static constexpr const char* pistols[]{ "全部", "格洛克 18 型", "P2000", "USP 消音型", "双持贝瑞塔", "P250", "Tec-9", "FN57", "CZ75", "沙漠之鹰", "R8 左轮手枪" };

        ImGui::Combo("", &currentPistol, [](void* data, int idx, const char** out_text) {
            if (config->aimbot[idx ? idx : 35].enabled) {
                static std::string name;
                name = pistols[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = pistols[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(pistols));

        currentWeapon = currentPistol ? currentPistol : 35;
        break;
    }
    case 2: {
        static int currentHeavy{ 0 };
        static constexpr const char* heavies[]{ "全部", "新星", "XM1014", "截短霰弹枪", "MAG-7", "M249", "内格夫" };

        ImGui::Combo("", &currentHeavy, [](void* data, int idx, const char** out_text) {
            if (config->aimbot[idx ? idx + 10 : 36].enabled) {
                static std::string name;
                name = heavies[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = heavies[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(heavies));

        currentWeapon = currentHeavy ? currentHeavy + 10 : 36;
        break;
    }
    case 3: {
        static int currentSmg{ 0 };
        static constexpr const char* smgs[]{ "全部", "MAC-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP-野牛" };

        ImGui::Combo("", &currentSmg, [](void* data, int idx, const char** out_text) {
            if (config->aimbot[idx ? idx + 16 : 37].enabled) {
                static std::string name;
                name = smgs[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = smgs[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(smgs));

        currentWeapon = currentSmg ? currentSmg + 16 : 37;
        break;
    }
    case 4: {
        static int currentRifle{ 0 };
        static constexpr const char* rifles[]{ "全部", "加利尔 AR", "法玛斯", "AK-47", "M4A4", "M4A1 消音型", "SSG 08", "SG 553", "AUG", "AWP", "G3SG1", "SCAR-20" };

        ImGui::Combo("", &currentRifle, [](void* data, int idx, const char** out_text) {
            if (config->aimbot[idx ? idx + 23 : 38].enabled) {
                static std::string name;
                name = rifles[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = rifles[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(rifles));

        currentWeapon = currentRifle ? currentRifle + 23 : 38;
        break;
    }
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Checkbox("启用", &config->aimbot[currentWeapon].enabled);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 220.0f);
    ImGui::Checkbox("瞄准锁定", &config->aimbot[currentWeapon].aimlock);
    ImGui::Checkbox("静默", &config->aimbot[currentWeapon].silent);
    ImGui::Checkbox("无视队友", &config->aimbot[currentWeapon].friendlyFire);
    ImGui::Checkbox("仅可见时", &config->aimbot[currentWeapon].visibleOnly);
    ImGui::Checkbox("仅开镜时", &config->aimbot[currentWeapon].scopedOnly);
    ImGui::Checkbox("无视闪光", &config->aimbot[currentWeapon].ignoreFlash);
    ImGui::Checkbox("无视烟雾", &config->aimbot[currentWeapon].ignoreSmoke);
    ImGui::Checkbox("自动开火", &config->aimbot[currentWeapon].autoShot);
    ImGui::Checkbox("自动开镜", &config->aimbot[currentWeapon].autoScope);
    ImGui::Combo("部位", &config->aimbot[currentWeapon].bone, "最近\0最佳伤害\0头部\0脖子\0肩膀\0胸部\0胃部\0腹部\0");
    ImGui::NextColumn();
    ImGui::PushItemWidth(240.0f);
    ImGui::SliderFloat("范围", &config->aimbot[currentWeapon].fov, 0.0f, 255.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("平滑", &config->aimbot[currentWeapon].smooth, 1.0f, 100.0f, "%.2f");
    ImGui::SliderFloat("最大瞄准误差", &config->aimbot[currentWeapon].maxAimInaccuracy, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("最大射击误差", &config->aimbot[currentWeapon].maxShotInaccuracy, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic);
    ImGui::InputInt("最小伤害", &config->aimbot[currentWeapon].minDamage);
    config->aimbot[currentWeapon].minDamage = std::clamp(config->aimbot[currentWeapon].minDamage, 0, 250);
    ImGui::Checkbox("致命射击", &config->aimbot[currentWeapon].killshot);
    ImGui::Checkbox("两次射击之间", &config->aimbot[currentWeapon].betweenShots);
    ImGui::Columns(1);
    if (!contentOnly)
        ImGui::End();
}

void GUI::renderTriggerbotWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.triggerbot)
            return;
        ImGui::SetNextWindowSize({ 0.0f, 0.0f });
        ImGui::Begin("自动开火", &window.triggerbot, windowFlags);
    }
    static int currentCategory{ 0 };
    ImGui::PushItemWidth(110.0f);
    ImGui::PushID(0);
    ImGui::Combo("", &currentCategory, "全部\0手枪\0重型武器\0微型冲锋枪\0步枪\0宙斯 X27 电击枪\0");
    ImGui::PopID();
    ImGui::SameLine();
    static int currentWeapon{ 0 };
    ImGui::PushID(1);
    switch (currentCategory) {
    case 0:
        currentWeapon = 0;
        ImGui::NewLine();
        break;
    case 5:
        currentWeapon = 39;
        ImGui::NewLine();
        break;

    case 1: {
        static int currentPistol{ 0 };
        static constexpr const char* pistols[]{ "全部", "格洛克 18 型", "P2000", "USP 消音型", "双持贝瑞塔", "P250", "Tec-9", "FN57", "CZ75", "沙漠之鹰", "R8 左轮手枪" };

        ImGui::Combo("", &currentPistol, [](void* data, int idx, const char** out_text) {
            if (config->triggerbot[idx ? idx : 35].enabled) {
                static std::string name;
                name = pistols[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = pistols[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(pistols));

        currentWeapon = currentPistol ? currentPistol : 35;
        break;
    }
    case 2: {
        static int currentHeavy{ 0 };
        static constexpr const char* heavies[]{ "全部", "新星", "XM1014", "截短霰弹枪", "MAG-7", "M249", "内格夫" };

        ImGui::Combo("", &currentHeavy, [](void* data, int idx, const char** out_text) {
            if (config->triggerbot[idx ? idx + 10 : 36].enabled) {
                static std::string name;
                name = heavies[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = heavies[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(heavies));

        currentWeapon = currentHeavy ? currentHeavy + 10 : 36;
        break;
    }
    case 3: {
        static int currentSmg{ 0 };
        static constexpr const char* smgs[]{ "全部", "MAC-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP-野牛" };

        ImGui::Combo("", &currentSmg, [](void* data, int idx, const char** out_text) {
            if (config->triggerbot[idx ? idx + 16 : 37].enabled) {
                static std::string name;
                name = smgs[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = smgs[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(smgs));

        currentWeapon = currentSmg ? currentSmg + 16 : 37;
        break;
    }
    case 4: {
        static int currentRifle{ 0 };
        static constexpr const char* rifles[]{ "全部", "加利尔 AR", "法玛斯", "AK-47", "M4A4", "M4A1 消音型", "SSG 08", "SG 553", "AUG", "AWP", "G3SG1", "SCAR-20" };

        ImGui::Combo("", &currentRifle, [](void* data, int idx, const char** out_text) {
            if (config->triggerbot[idx ? idx + 23 : 38].enabled) {
                static std::string name;
                name = rifles[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = rifles[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(rifles));

        currentWeapon = currentRifle ? currentRifle + 23 : 38;
        break;
    }
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Checkbox("启用", &config->triggerbot[currentWeapon].enabled);
    ImGui::Separator();
    ImGui::hotkey("保持按键", config->triggerbotHoldKey);
    ImGui::Checkbox("无视队友", &config->triggerbot[currentWeapon].friendlyFire);
    ImGui::Checkbox("仅开镜时", &config->triggerbot[currentWeapon].scopedOnly);
    ImGui::Checkbox("无视闪光", &config->triggerbot[currentWeapon].ignoreFlash);
    ImGui::Checkbox("无视烟雾", &config->triggerbot[currentWeapon].ignoreSmoke);
    ImGui::SetNextItemWidth(85.0f);
    ImGui::Combo("命中组", &config->triggerbot[currentWeapon].hitgroup, "全部\0头部\0胸部\0胃部\0左肩\0右肩\0左腿\0右腿\0");
    ImGui::PushItemWidth(220.0f);
    ImGui::SliderInt("射击延迟", &config->triggerbot[currentWeapon].shotDelay, 0, 250, "%d 毫秒");
    ImGui::InputInt("最小伤害", &config->triggerbot[currentWeapon].minDamage);
    config->triggerbot[currentWeapon].minDamage = std::clamp(config->triggerbot[currentWeapon].minDamage, 0, 250);
    ImGui::Checkbox("致命射击", &config->triggerbot[currentWeapon].killshot);
    ImGui::SliderFloat("爆发时间", &config->triggerbot[currentWeapon].burstTime, 0.0f, 0.5f, "%.3f 秒");

    if (!contentOnly)
        ImGui::End();
}

void GUI::renderChamsWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.chams)
            return;
        ImGui::SetNextWindowSize({ 0.0f, 0.0f });
        ImGui::Begin("实体", &window.chams, windowFlags);
    }

    ImGui::hotkey("切换按键", config->chamsToggleKey, 80.0f);
    ImGui::hotkey("保持按键", config->chamsHoldKey, 80.0f);
    ImGui::Separator();

    static int currentCategory{ 0 };
    ImGui::PushItemWidth(110.0f);
    ImGui::PushID(0);

    static int material = 1;

    if (ImGui::Combo("", &currentCategory, "队友\0敌人\0已安装\0拆卸中\0自己\0武器\0手部\0回溯\0袖子\0"))
        material = 1;

    ImGui::PopID();

    ImGui::SameLine();

    if (material <= 1)
        ImGuiCustom::arrowButtonDisabled("##left", ImGuiDir_Left);
    else if (ImGui::ArrowButton("##left", ImGuiDir_Left))
        --material;

    ImGui::SameLine();
    ImGui::Text("%d", material);

    constexpr std::array categories{ "Allies", "Enemies", "Planting", "Defusing", "Local player", "Weapons", "Hands", "Backtrack", "Sleeves" };

    ImGui::SameLine();

    if (material >= int(config->chams[categories[currentCategory]].materials.size()))
        ImGuiCustom::arrowButtonDisabled("##right", ImGuiDir_Right);
    else if (ImGui::ArrowButton("##right", ImGuiDir_Right))
        ++material;

    ImGui::SameLine();

    auto& chams{ config->chams[categories[currentCategory]].materials[material - 1] };

    ImGui::Checkbox("启用", &chams.enabled);
    ImGui::Separator();
    ImGui::Checkbox("根据生命值", &chams.healthBased);
    ImGui::Checkbox("闪烁", &chams.blinking);
    ImGui::Combo("材质", &chams.material, "普通\0扁平\0变换\0铂金\0玻璃\0铬合金\0水晶\0银色\0金色\0塑料\0辉光\0珠光\0金属\0");
    ImGui::Checkbox("线框", &chams.wireframe);
    ImGui::Checkbox("覆盖", &chams.cover);
    ImGui::Checkbox("忽略Z值", &chams.ignorez);
    ImGuiCustom::colorPicker("颜色", chams);

    if (!contentOnly) {
        ImGui::End();
    }
}

void GUI::renderStreamProofESPWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.streamProofESP)
            return;
        ImGui::SetNextWindowSize({ 0.0f, 0.0f });
        ImGui::Begin("ESP", &window.streamProofESP, windowFlags);
    }

    ImGui::hotkey("切换按键", config->streamProofESP.toggleKey, 80.0f);
    ImGui::hotkey("保持按键", config->streamProofESP.holdKey, 80.0f);
    ImGui::Separator();

    static std::size_t currentCategory;
    static auto currentItem = "All";

    constexpr auto getConfigShared = [](std::size_t category, const char* item) noexcept -> Shared& {
        switch (category) {
        case 0: default: return config->streamProofESP.enemies[item];
        case 1: return config->streamProofESP.allies[item];
        case 2: return config->streamProofESP.weapons[item];
        case 3: return config->streamProofESP.projectiles[item];
        case 4: return config->streamProofESP.lootCrates[item];
        case 5: return config->streamProofESP.otherEntities[item];
        }
    };

    constexpr auto getConfigPlayer = [](std::size_t category, const char* item) noexcept -> Player& {
        switch (category) {
        case 0: default: return config->streamProofESP.enemies[item];
        case 1: return config->streamProofESP.allies[item];
        }
    };

    if (ImGui::BeginListBox("##list", { 170.0f, 300.0f })) {
        constexpr std::array categories{ "敌人", "队友", "武器", "投掷物", "战利品箱", "其他实体" };

        for (std::size_t i = 0; i < categories.size(); ++i) {
            if (ImGui::Selectable(categories[i], currentCategory == i && std::string_view{ currentItem } == "All")) {
                currentCategory = i;
                currentItem = "All";
            }

            if (ImGui::BeginDragDropSource()) {
                switch (i) {
                case 0: case 1: ImGui::SetDragDropPayload("Player", &getConfigPlayer(i, "All"), sizeof(Player), ImGuiCond_Once); break;
                case 2: ImGui::SetDragDropPayload("Weapon", &config->streamProofESP.weapons["All"], sizeof(Weapon), ImGuiCond_Once); break;
                case 3: ImGui::SetDragDropPayload("Projectile", &config->streamProofESP.projectiles["All"], sizeof(Projectile), ImGuiCond_Once); break;
                default: ImGui::SetDragDropPayload("Entity", &getConfigShared(i, "All"), sizeof(Shared), ImGuiCond_Once); break;
                }
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Player")) {
                    const auto& data = *(Player*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Weapon")) {
                    const auto& data = *(Weapon*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Projectile")) {
                    const auto& data = *(Projectile*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
                    const auto& data = *(Shared*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::PushID(i);
            ImGui::Indent();

            const auto items = [](std::size_t category) noexcept -> std::vector<const char*> {
                switch (category) {
                case 0:
                case 1: return { "可见时", "不可见时" };
                case 2: return { "手枪", "微型冲锋枪", "步枪", "狙击步枪", "霰弹枪", "重型武器", "手雷", "近战武器", "其他" };
                case 3: return { "闪光震撼弹", "高爆手雷", "遥控炸弹", "弹射地雷", "诱饵手雷", "燃烧瓶", "战术探测手雷", "烟雾弹", "雪球" };
                case 4: return { "手枪盒", "闪光盒", "重型盒", "炸药盒", "工具盒", "现金行李袋" };
                case 5: return { "拆弹器", "鸡", "已安装的 C4 炸弹", "人质", "自动哨兵", "金钱", "弹药盒", "雷达干扰器", "雪球桩", "收藏币" };
                default: return { };
                }
            }(i);

            const auto categoryEnabled = getConfigShared(i, "All").enabled;

            for (std::size_t j = 0; j < items.size(); ++j) {
                static bool selectedSubItem;
                if (!categoryEnabled || getConfigShared(i, items[j]).enabled) {
                    if (ImGui::Selectable(items[j], currentCategory == i && !selectedSubItem && std::string_view{ currentItem } == items[j])) {
                        currentCategory = i;
                        currentItem = items[j];
                        selectedSubItem = false;
                    }

                    if (ImGui::BeginDragDropSource()) {
                        switch (i) {
                        case 0: case 1: ImGui::SetDragDropPayload("Player", &getConfigPlayer(i, items[j]), sizeof(Player), ImGuiCond_Once); break;
                        case 2: ImGui::SetDragDropPayload("Weapon", &config->streamProofESP.weapons[items[j]], sizeof(Weapon), ImGuiCond_Once); break;
                        case 3: ImGui::SetDragDropPayload("Projectile", &config->streamProofESP.projectiles[items[j]], sizeof(Projectile), ImGuiCond_Once); break;
                        default: ImGui::SetDragDropPayload("Entity", &getConfigShared(i, items[j]), sizeof(Shared), ImGuiCond_Once); break;
                        }
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Player")) {
                            const auto& data = *(Player*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Weapon")) {
                            const auto& data = *(Weapon*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Projectile")) {
                            const auto& data = *(Projectile*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
                            const auto& data = *(Shared*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                if (i != 2)
                    continue;

                ImGui::Indent();

                const auto subItems = [](std::size_t item) noexcept -> std::vector<const char*> {
                    switch (item) {
                    case 0: return { "格洛克 18 型", "P2000", "USP 消音型", "双持贝瑞塔", "P250", "Tec-9", "FN57", "CZ75", "沙漠之鹰", "R8 左轮手枪" };
                    case 1: return { "MAC-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP-野牛" };
                    case 2: return { "加利尔 AR", "法玛斯", "AK-47", "M4A4", "M4A1 消音型", "SG 553", "AUG" };
                    case 3: return { "SSG 08", "AWP", "G3SG1", "SCAR-20" };
                    case 4: return { "新星", "XM1014", "截短霰弹枪", "MAG-7" };
                    case 5: return { "M249", "内格夫" };
                    case 6: return { "闪光震撼弹", "高爆手雷", "烟雾弹", "燃烧瓶", "诱饵弹", "燃烧弹", "战术探测手雷", "火焰弹", "干扰型武器", "破片手雷", "雪球" };
                    case 7: return { "斧头", "锤子", "扳手" };
                    case 8: return { "C4 炸弹", "医疗针", "弹射地雷", "排斥装置", "防弹盾" };
                    default: return { };
                    }
                }(j);

                const auto itemEnabled = getConfigShared(i, items[j]).enabled;

                for (const auto subItem : subItems) {
                    auto& subItemConfig = config->streamProofESP.weapons[subItem];
                    if ((categoryEnabled || itemEnabled) && !subItemConfig.enabled)
                        continue;

                    if (ImGui::Selectable(subItem, currentCategory == i && selectedSubItem && std::string_view{ currentItem } == subItem)) {
                        currentCategory = i;
                        currentItem = subItem;
                        selectedSubItem = true;
                    }

                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("Weapon", &subItemConfig, sizeof(Weapon), ImGuiCond_Once);
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Player")) {
                            const auto& data = *(Player*)payload->Data;
                            subItemConfig = data;
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Weapon")) {
                            const auto& data = *(Weapon*)payload->Data;
                            subItemConfig = data;
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Projectile")) {
                            const auto& data = *(Projectile*)payload->Data;
                            subItemConfig = data;
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
                            const auto& data = *(Shared*)payload->Data;
                            subItemConfig = data;
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                ImGui::Unindent();
            }
            ImGui::Unindent();
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }

    ImGui::SameLine();

    if (ImGui::BeginChild("##child", { 400.0f, 0.0f })) {
        auto& sharedConfig = getConfigShared(currentCategory, currentItem);

        ImGui::Checkbox("启用", &sharedConfig.enabled);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 260.0f);
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("字体", config->getSystemFonts()[sharedConfig.font.index].c_str())) {
            for (size_t i = 0; i < config->getSystemFonts().size(); i++) {
                bool isSelected = config->getSystemFonts()[i] == sharedConfig.font.name;
                if (ImGui::Selectable(config->getSystemFonts()[i].c_str(), isSelected, 0, { 250.0f, 0.0f })) {
                    sharedConfig.font.index = i;
                    sharedConfig.font.name = config->getSystemFonts()[i];
                    config->scheduleFontLoad(sharedConfig.font.name);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        constexpr auto spacing = 250.0f;
        ImGuiCustom::colorPicker("追踪线", sharedConfig.snapline);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::Combo("##1", &sharedConfig.snapline.type, "底部\0顶部\0准星\0");
        ImGui::SameLine(spacing);
        ImGuiCustom::colorPicker("方框", sharedConfig.box);
        ImGui::SameLine();

        ImGui::PushID("Box");

        if (ImGui::Button("..."))
            ImGui::OpenPopup("");

        if (ImGui::BeginPopup("")) {
            ImGui::SetNextItemWidth(95.0f);
            ImGui::Combo("类型", &sharedConfig.box.type, "2D\0" "2D 边角\0" "3D\0" "3D 边角\0");
            ImGui::SetNextItemWidth(275.0f);
            ImGui::SliderFloat3("尺寸", sharedConfig.box.scale.data(), 0.0f, 0.50f, "%.2f");
            ImGuiCustom::colorPicker("填充", sharedConfig.box.fill);
            ImGui::EndPopup();
        }

        ImGui::PopID();

        ImGuiCustom::colorPicker("名称", sharedConfig.name);
        ImGui::SameLine(spacing);

        if (currentCategory < 2) {
            auto& playerConfig = getConfigPlayer(currentCategory, currentItem);

            ImGuiCustom::colorPicker("武器", playerConfig.weapon);
            ImGuiCustom::colorPicker("闪光持续时间", playerConfig.flashDuration);
            ImGui::SameLine(spacing);
            ImGuiCustom::colorPicker("骨骼", playerConfig.skeleton);
            ImGui::Checkbox("仅声音", &playerConfig.audibleOnly);
            ImGui::SameLine(spacing);
            ImGui::Checkbox("仅可见", &playerConfig.spottedOnly);

            ImGuiCustom::colorPicker("头部方框", playerConfig.headBox);
            ImGui::SameLine();

            ImGui::PushID("Head Box");

            if (ImGui::Button("..."))
                ImGui::OpenPopup("");

            if (ImGui::BeginPopup("")) {
                ImGui::SetNextItemWidth(95.0f);
                ImGui::Combo("类型", &playerConfig.headBox.type, "2D\0" "2D 边角\0" "3D\0" "3D 边角\0");
                ImGui::SetNextItemWidth(275.0f);
                ImGui::SliderFloat3("尺寸", playerConfig.headBox.scale.data(), 0.0f, 0.50f, "%.2f");
                ImGuiCustom::colorPicker("填充", playerConfig.headBox.fill);
                ImGui::EndPopup();
            }

            ImGui::PopID();
        
            ImGui::SameLine(spacing);
            ImGui::Checkbox("血量条", &playerConfig.healthBar.enabled);
            ImGui::SameLine();

            ImGui::PushID("Health Bar");

            if (ImGui::Button("..."))
                ImGui::OpenPopup("");

            if (ImGui::BeginPopup("")) {
                ImGui::SetNextItemWidth(95.0f);
                ImGui::Combo("类型", &playerConfig.healthBar.type, "渐变\0填充\0基于生命值\0");
                if (playerConfig.healthBar.type == HealthBar::Solid) {
                    ImGui::SameLine();
                    ImGuiCustom::colorPicker("", static_cast<Color4&>(playerConfig.healthBar));
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        } else if (currentCategory == 2) {
            auto& weaponConfig = config->streamProofESP.weapons[currentItem];
            ImGuiCustom::colorPicker("子弹数", weaponConfig.ammo);
        } else if (currentCategory == 3) {
            auto& trails = config->streamProofESP.projectiles[currentItem].trails;

            ImGui::Checkbox("轨迹", &trails.enabled);
            ImGui::SameLine(spacing + 77.0f);
            ImGui::PushID("Trails");

            if (ImGui::Button("..."))
                ImGui::OpenPopup("");

            if (ImGui::BeginPopup("")) {
                constexpr auto trailPicker = [](const char* name, Trail& trail) noexcept {
                    ImGui::PushID(name);
                    ImGuiCustom::colorPicker(name, trail);
                    ImGui::SameLine(150.0f);
                    ImGui::SetNextItemWidth(95.0f);
                    ImGui::Combo("", &trail.type, "线\0圆\0实心圆\0");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(95.0f);
                    ImGui::InputFloat("时间", &trail.time, 0.1f, 0.5f, "%.1f秒");
                    trail.time = std::clamp(trail.time, 1.0f, 60.0f);
                    ImGui::PopID();
                };

                trailPicker("自己", trails.localPlayer);
                trailPicker("队友", trails.allies);
                trailPicker("敌人", trails.enemies);
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::SetNextItemWidth(95.0f);
        ImGui::InputFloat("文字消失距离", &sharedConfig.textCullDistance, 0.4f, 0.8f, "%.1f米");
        sharedConfig.textCullDistance = std::clamp(sharedConfig.textCullDistance, 0.0f, 999.9f);
    }

    ImGui::EndChild();

    if (!contentOnly)
        ImGui::End();
}

void GUI::renderStyleWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.style)
            return;
        ImGui::SetNextWindowSize({ 0.0f, 0.0f });
        ImGui::Begin("样式", &window.style, windowFlags);
    }

    ImGui::PushItemWidth(150.0f);
    if (ImGui::Combo("菜单样式", &config->style.menuStyle, "经典\0悬浮窗\0"))
        window = { };
    if (ImGui::Combo("菜单颜色", &config->style.menuColors, "暗黑\0明亮\0经典\0自定义\0"))
        updateColors();
    ImGui::PopItemWidth();

    if (config->style.menuColors == 3) {
        ImGuiStyle& style = ImGui::GetStyle();
        for (int i = 0; i < ImGuiCol_COUNT; i++) {
            if (i && i & 3) ImGui::SameLine(220.0f * (i & 3));

            ImGuiCustom::colorPicker(ImGui::GetStyleColorName(i), (float*)&style.Colors[i], &style.Colors[i].w);
        }
    }

    if (!contentOnly)
        ImGui::End();
}

void GUI::renderMiscWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.misc)
            return;
        ImGui::SetNextWindowSize({ 580.0f, 0.0f });
        ImGui::Begin("杂项", &window.misc, windowFlags);
    }
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 230.0f);
    ImGui::hotkey("菜单按键", config->misc.menuKey);
    ImGui::Checkbox("反挂机踢出", &config->misc.antiAfkKick);
    ImGui::Checkbox("自动扫射", &config->misc.autoStrafe);
    ImGui::Checkbox("自动连跳", &config->misc.bunnyHop);
    ImGui::Checkbox("快速下蹲", &config->misc.fastDuck);
    ImGui::Checkbox("滑步", &config->misc.moonwalk);
    ImGui::Checkbox("边缘跳", &config->misc.edgejump);
    ImGui::SameLine();
    ImGui::PushID("Edge Jump Key");
    ImGui::hotkey("", config->misc.edgejumpkey);
    ImGui::PopID();
    ImGui::Checkbox("慢走", &config->misc.slowwalk);
    ImGui::SameLine();
    ImGui::PushID("Slowwalk Key");
    ImGui::hotkey("", config->misc.slowwalkKey);
    ImGui::PopID();
    ImGuiCustom::colorPicker("狙击十字准星", config->misc.noscopeCrosshair);
    ImGuiCustom::colorPicker("后座十字准星", config->misc.recoilCrosshair);
    ImGui::Checkbox("自动手枪", &config->misc.autoPistol);
    ImGui::Checkbox("自动换弹", &config->misc.autoReload);
    ImGui::Checkbox("自动接受", &config->misc.autoAccept);
    ImGui::Checkbox("雷达透视", &config->misc.radarHack);
    ImGui::Checkbox("显示段位", &config->misc.revealRanks);
    ImGui::Checkbox("显示金钱", &config->misc.revealMoney);
    ImGui::Checkbox("揭露嫌疑人", &config->misc.revealSuspect);
    ImGui::Checkbox("揭露投票", &config->misc.revealVotes);

    ImGui::Checkbox("观看列表", &config->misc.spectatorList.enabled);
    ImGui::SameLine();

    ImGui::PushID("Spectator list");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("去除标题栏", &config->misc.spectatorList.noTitleBar);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("水印", &config->misc.watermark.enabled);
    ImGuiCustom::colorPicker("显示屏幕外敌人", config->misc.offscreenEnemies, &config->misc.offscreenEnemies.enabled);
    ImGui::SameLine();
    ImGui::PushID("显示屏幕外敌人");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("血量条", &config->misc.offscreenEnemies.healthBar.enabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(95.0f);
        ImGui::Combo("类型", &config->misc.offscreenEnemies.healthBar.type, "渐变\0填充\0基于生命值\0");
        if (config->misc.offscreenEnemies.healthBar.type == HealthBar::Solid) {
            ImGui::SameLine();
            ImGuiCustom::colorPicker("", static_cast<Color4&>(config->misc.offscreenEnemies.healthBar));
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::Checkbox("修复动画LOD", &config->misc.fixAnimationLOD);
    ImGui::Checkbox("修复骨骼矩阵", &config->misc.fixBoneMatrix);
    ImGui::Checkbox("修正动作", &config->misc.fixMovement);
    ImGui::Checkbox("禁用模型遮挡", &config->misc.disableModelOcclusion);
    ImGui::SliderFloat("纵横比", &config->misc.aspectratio, 0.0f, 5.0f, "%.2f");
    ImGui::NextColumn();
    ImGui::Checkbox("禁用HUD模糊", &config->misc.disablePanoramablur);
    ImGui::Checkbox("滚动组名标签", &config->misc.animatedClanTag);
    ImGui::Checkbox("时钟组名标签", &config->misc.clocktag);
    ImGui::Checkbox("自定义组名标签", &config->misc.customClanTag);
    ImGui::SameLine();
    ImGui::PushItemWidth(120.0f);
    ImGui::PushID(0);

    if (ImGui::InputText("", config->misc.clanTag, sizeof(config->misc.clanTag)))
        Misc::updateClanTag(true);
    ImGui::PopID();
    ImGui::Checkbox("击杀消息", &config->misc.killMessage);
    ImGui::SameLine();
    ImGui::PushItemWidth(120.0f);
    ImGui::PushID(1);
    ImGui::InputText("", &config->misc.killMessageString);
    ImGui::PopID();
    ImGui::Checkbox("抢占名称", &config->misc.nameStealer);
    ImGui::PushID(3);
    ImGui::SetNextItemWidth(100.0f);
    //ImGui::Combo("", &config->misc.banColor, "白色\0红色\0紫色\0绿色\0浅绿色\0深绿色\0浅红色\0灰色\0黄色\0灰色 2\0浅蓝色\0灰色/紫色\0蓝色\0粉色\0暗橙色\0橙色\0");
    ImGui::PopID();
    //ImGui::SameLine();
    ImGui::PushID(4);
    ImGui::InputText("", &config->misc.banText);
    ImGui::PopID();
    ImGui::SameLine();
    if (ImGui::Button("自定义名称"))
        Misc::fakeBan(true);
    ImGui::Checkbox("快速安放", &config->misc.fastPlant);
    ImGui::Checkbox("快速急停", &config->misc.fastStop);
    ImGuiCustom::colorPicker("炸弹时间", config->misc.bombTimer);
    ImGui::Checkbox("快速换弹", &config->misc.quickReload);
    ImGui::Checkbox("自动左轮手枪", &config->misc.prepareRevolver);
    ImGui::SameLine();
    ImGui::PushID("Prepare revolver Key");
    ImGui::hotkey("", config->misc.prepareRevolverKey);
    ImGui::PopID();
    ImGui::Combo("击中声音", &config->misc.hitSound, "无\0金属\0游戏感\0钟声\0玻璃\0自定义\0");
    if (config->misc.hitSound == 5) {
        ImGui::InputText("击中声音文件名称", &config->misc.customHitSound);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("音频文件必须放入 csgo/sound/ 目录");
    }
    ImGui::PushID(5);
    ImGui::Combo("击杀声音", &config->misc.killSound, "无\0金属\0游戏感\0钟声\0玻璃\0自定义\0");
    if (config->misc.killSound == 5) {
        ImGui::InputText("击杀声音文件名称", &config->misc.customKillSound);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("音频文件必须放入 csgo/sound/ 目录");
    }
    ImGui::PopID();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputInt("阻塞数据包", &config->misc.chokedPackets, 1, 5);
    config->misc.chokedPackets = std::clamp(config->misc.chokedPackets, 0, 64);
    ImGui::SameLine();
    ImGui::PushID("Choked packets Key");
    ImGui::hotkey("", config->misc.chokedPacketsKey);
    ImGui::PopID();
    /*
    ImGui::Text("Quick healthshot");
    ImGui::SameLine();
    hotkey(config->misc.quickHealthshotKey);
    */
    ImGui::Checkbox("投掷物轨迹预测", &config->misc.nadePredict);
    ImGui::Checkbox("修复平板电脑信号", &config->misc.fixTabletSignal);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("最大角度增量", &config->misc.maxAngleDelta, 0.0f, 255.0f, "%.2f");
    ImGui::Checkbox("镜像持刀手部", &config->misc.oppositeHandKnife);
    ImGui::Checkbox("保留击杀信息", &config->misc.preserveKillfeed.enabled);
    ImGui::SameLine();

    ImGui::PushID("Preserve Killfeed");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("仅爆头", &config->misc.preserveKillfeed.onlyHeadshots);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("购买列表", &config->misc.purchaseList.enabled);
    ImGui::SameLine();

    ImGui::PushID("Purchase List");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::SetNextItemWidth(75.0f);
        ImGui::Combo("模式", &config->misc.purchaseList.mode, "详细\0简单\0");
        ImGui::Checkbox("仅在冻结期间", &config->misc.purchaseList.onlyDuringFreezeTime);
        ImGui::Checkbox("显示价格", &config->misc.purchaseList.showPrices);
        ImGui::Checkbox("去除标题栏", &config->misc.purchaseList.noTitleBar);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("举报机器人", &config->misc.reportbot.enabled);
    ImGui::SameLine();
    ImGui::PushID("Reportbot");

    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::PushItemWidth(80.0f);
        ImGui::Combo("目标", &config->misc.reportbot.target, "敌人\0队友\0所有\0");
        ImGui::InputInt("延迟 (秒)", &config->misc.reportbot.delay);
        config->misc.reportbot.delay = (std::max)(config->misc.reportbot.delay, 1);
        ImGui::InputInt("回合", &config->misc.reportbot.rounds);
        config->misc.reportbot.rounds = (std::max)(config->misc.reportbot.rounds, 1);
        ImGui::PopItemWidth();
        ImGui::Checkbox("恶意个人资料或言语骚扰", &config->misc.reportbot.textAbuse);
        ImGui::Checkbox("骚扰", &config->misc.reportbot.griefing);
        ImGui::Checkbox("穿墙作弊", &config->misc.reportbot.wallhack);
        ImGui::Checkbox("自瞄作弊", &config->misc.reportbot.aimbot);
        ImGui::Checkbox("其他作弊", &config->misc.reportbot.other);
        if (ImGui::Button("重置"))
            Misc::resetReportbot();
        ImGui::EndPopup();
    }
    ImGui::PopID();

    if (ImGui::Button("卸载"))
        hooks->uninstall();

    ImGui::Columns(1);
    if (!contentOnly)
        ImGui::End();
}

void GUI::renderConfigWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.config)
            return;
        ImGui::SetNextWindowSize({ 320.0f, 0.0f });
        if (!ImGui::Begin("配置", &window.config, windowFlags)) {
            ImGui::End();
            return;
        }
    }

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 170.0f);

    static bool incrementalLoad = false;
    ImGui::Checkbox("逐个加载", &incrementalLoad);

    ImGui::PushItemWidth(160.0f);

    auto& configItems = config->getConfigs();
    static int currentConfig = -1;

    static std::string buffer;

    timeToNextConfigRefresh -= ImGui::GetIO().DeltaTime;
    if (timeToNextConfigRefresh <= 0.0f) {
        config->listConfigs();
        if (const auto it = std::find(configItems.begin(), configItems.end(), buffer); it != configItems.end())
            currentConfig = std::distance(configItems.begin(), it);
        timeToNextConfigRefresh = 0.1f;
    }

    if (static_cast<std::size_t>(currentConfig) >= configItems.size())
        currentConfig = -1;

    if (ImGui::ListBox("", &currentConfig, [](void* data, int idx, const char** out_text) {
        auto& vector = *static_cast<std::vector<std::string>*>(data);
        *out_text = vector[idx].c_str();
        return true;
        }, &configItems, configItems.size(), 5) && currentConfig != -1)
            buffer = configItems[currentConfig];

        ImGui::PushID(0);
        if (ImGui::InputTextWithHint("", "配置名称", &buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (currentConfig != -1)
                config->rename(currentConfig, buffer.c_str());
        }
        ImGui::PopID();
        ImGui::NextColumn();

        ImGui::PushItemWidth(100.0f);

        if (ImGui::Button("打开配置目录"))
            config->openConfigDir();

        if (ImGui::Button("创建配置", { 100.0f, 25.0f }))
            config->add(buffer.c_str());

        if (ImGui::Button("重置配置", { 100.0f, 25.0f }))
            ImGui::OpenPopup("Config to reset");

        if (ImGui::BeginPopup("Config to reset")) {
            static constexpr const char* names[]{ "Whole", "Aimbot", "Triggerbot", "Backtrack", "Anti aim", "Glow", "Chams", "ESP", "Visuals", "Inventory Changer", "Sound", "Style", "Misc" };
            for (int i = 0; i < IM_ARRAYSIZE(names); i++) {
                if (i == 1) ImGui::Separator();

                if (ImGui::Selectable(names[i])) {
                    switch (i) {
                    case 0: config->reset(); updateColors(); Misc::updateClanTag(true); InventoryChanger::scheduleHudUpdate(); break;
                    case 1: config->aimbot = { }; break;
                    case 2: config->triggerbot = { }; break;
                    case 3: Backtrack::resetConfig(); break;
                    case 4: AntiAim::resetConfig(); break;
                    case 5: Glow::resetConfig(); break;
                    case 6: config->chams = { }; break;
                    case 7: config->streamProofESP = { }; break;
                    case 8: Visuals::resetConfig(); break;
                    case 9: InventoryChanger::resetConfig(); InventoryChanger::scheduleHudUpdate(); break;
                    case 10: Sound::resetConfig(); break;
                    case 11: config->style = { }; updateColors(); break;
                    case 12: config->misc = { };  Misc::updateClanTag(true); break;
                    }
                }
            }
            ImGui::EndPopup();
        }
        if (currentConfig != -1) {
            if (ImGui::Button("载入选中", { 100.0f, 25.0f })) {
                config->load(currentConfig, incrementalLoad);
                updateColors();
                InventoryChanger::scheduleHudUpdate();
                Misc::updateClanTag(true);
            }
            if (ImGui::Button("保存选中", { 100.0f, 25.0f }))
                config->save(currentConfig);
            if (ImGui::Button("删除选中", { 100.0f, 25.0f })) {
                config->remove(currentConfig);

                if (static_cast<std::size_t>(currentConfig) < configItems.size())
                    buffer = configItems[currentConfig];
                else
                    buffer.clear();
            }
        }
        ImGui::Columns(1);
        if (!contentOnly)
            ImGui::End();
}

void GUI::renderGuiStyle2() noexcept
{
    ImGui::Begin("Osiris", nullptr, windowFlags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::BeginTabBar("TabBar", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_NoTooltip)) {
        if (ImGui::BeginTabItem("自瞄")) {
            renderAimbotWindow(true);
            ImGui::EndTabItem();
        }
        AntiAim::tabItem();
        if (ImGui::BeginTabItem("自动开火")) {
            renderTriggerbotWindow(true);
            ImGui::EndTabItem();
        }
        Backtrack::tabItem();
        Glow::tabItem();
        if (ImGui::BeginTabItem("实体")) {
            renderChamsWindow(true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ESP")) {
            renderStreamProofESPWindow(true);
            ImGui::EndTabItem();
        }
        Visuals::tabItem();
        InventoryChanger::tabItem();
        Sound::tabItem();
        if (ImGui::BeginTabItem("样式")) {
            renderStyleWindow(true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("杂项")) {
            renderMiscWindow(true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("配置")) {
            renderConfigWindow(true);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}
