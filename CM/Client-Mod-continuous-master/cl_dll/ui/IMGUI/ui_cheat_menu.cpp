#include "ui_cheat_menu.h"
#include "imgui.h"
#include "hud.h"
#include "keydefs.h"
#include "cl_util.h"
#include "hud_cornerbox.h"
#include "hud_aimbot.h"

bool CImGuiCheatMenu::m_ShowMenu = false;

void CImGuiCheatMenu::Initialize()
{
    // Регистрируем консольную команду для открытия/закрытия меню
    // Использование: xg_gui (в консоли или через bind)
    gEngfuncs.pfnAddCommand("xg_gui", CImGuiCheatMenu::CmdToggleCheatMenu);
}

void CImGuiCheatMenu::VidInitialize()
{
}

void CImGuiCheatMenu::Terminate()
{
}

void CImGuiCheatMenu::Think()
{
    // Команда xg_gui обрабатывается через CmdToggleCheatMenu
    // Здесь можно добавить дополнительную логику при необходимости
}

void CImGuiCheatMenu::Draw()
{
    if (!m_ShowMenu)
        return;

    // Убираем AlwaysAutoResize чтобы можно было изменять размер
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("XGHook 1.0", &m_ShowMenu, window_flags);
    
    // Устанавливаем минимальный размер окна
    ImGui::SetWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    
    // Добавляем подсказку о горячих клавишах
    ImGui::TextDisabled("Command: xg_gui | INSERT to toggle | ESC to close");
    ImGui::Separator();
    
    // Ограничиваем размер окна границами экрана (но позволяем изменять размер)
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    
    if (pos.x < 0)
        ImGui::SetWindowPos(ImVec2(0, pos.y));
    if (pos.y < 0)
        ImGui::SetWindowPos(ImVec2(pos.x, 0));
    if (pos.x + size.x > ScreenWidth)
        ImGui::SetWindowPos(ImVec2(ScreenWidth - size.x, pos.y));
    if (pos.y + size.y > ScreenHeight)
        ImGui::SetWindowPos(ImVec2(pos.x, ScreenHeight - size.y));

    // ESP Section
    ImGui::PushID("ESP_Section");
    if (ImGui::CollapsingHeader("ESP", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Corner Box ESP
        ImGui::PushID("CornerBoxESP");
        bool corner_box_enabled = CHudCornerBox::IsEnabled();
        if (ImGui::Checkbox("Corner Box ESP", &corner_box_enabled))
        {
            CHudCornerBox::SetEnabled(corner_box_enabled);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Shows corner boxes around players");
        }
        ImGui::PopID();
    }
    ImGui::PopID();

    // Aimbot Section
    ImGui::PushID("Aimbot_Section");
    if (ImGui::CollapsingHeader("Aimbot", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Aimbot Enable
        ImGui::PushID("AimbotEnable");
        bool aimbot_enabled = CHudAimbot::IsEnabled();
        if (ImGui::Checkbox("Enable", &aimbot_enabled))
        {
            CHudAimbot::SetEnabled(aimbot_enabled);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Automatically aims at players");
        }
        ImGui::PopID();

        // Показываем дополнительные настройки только если аимбот включен
        if (aimbot_enabled)
        {
            ImGui::Indent();

            // Aim FOV
            ImGui::PushID("AimFOV");
            float aim_fov = CHudAimbot::GetAimFOV();
            if (ImGui::SliderFloat("Aim FOV", &aim_fov, 1.0f, 180.0f, "%.1f"))
            {
                CHudAimbot::SetAimFOV(aim_fov);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Field of view for target detection (degrees)");
            }
            ImGui::PopID();

            // Draw Aim FOV
            ImGui::PushID("DrawAimFOV");
            bool draw_fov = CHudAimbot::IsDrawFOV();
            if (ImGui::Checkbox("Draw Aim FOV", &draw_fov))
            {
                CHudAimbot::SetDrawFOV(draw_fov);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Draw Aim FOV circle on screen (size depends on Aim FOV)");
            }
            ImGui::PopID();

            // Aim Height
            ImGui::PushID("AimHeight");
            float aim_height = CHudAimbot::GetAimHeight();
            if (ImGui::SliderFloat("Aim Height", &aim_height, 15.0f, 50.0f, "%.1f"))
            {
                CHudAimbot::SetAimHeight(aim_height);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Height offset for aiming (15=chest, 28=head, 50=above head)");
            }
            ImGui::PopID();

            // RCS (Recoil Control System)
            ImGui::PushID("RCS");
            bool rcs_enabled = CHudAimbot::IsRCSEnabled();
            if (ImGui::Checkbox("RCS", &rcs_enabled))
            {
                CHudAimbot::SetRCSEnabled(rcs_enabled);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Recoil Control System - компенсация отдачи оружия");
            }
            ImGui::PopID();

            // RCS Strength (только если RCS включен)
            if (rcs_enabled)
            {
                ImGui::PushID("RCSStrength");
                float rcs_strength = CHudAimbot::GetRCSStrength();
                if (ImGui::SliderFloat("RCS Strength", &rcs_strength, 0.0f, 1.0f, "%.2f"))
                {
                    CHudAimbot::SetRCSStrength(rcs_strength);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Сила компенсации отдачи (0.0=нет, 1.0=полная)");
                }
                ImGui::PopID();
            }

            // No Recoil
            ImGui::PushID("NoRecoil");
            bool no_recoil = CHudAimbot::IsNoRecoil();
            if (ImGui::Checkbox("No Recoil", &no_recoil))
            {
                CHudAimbot::SetNoRecoil(no_recoil);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Полностью убирает отдачу оружия (обнуляет punchangle)");
            }
            ImGui::PopID();

            // Silent Aim (тестово)
            ImGui::PushID("SilentAim");
            bool silent_aim = CHudAimbot::IsSilentAim();
            if (ImGui::Checkbox("Silent Aim", &silent_aim))
            {
                CHudAimbot::SetSilentAim(silent_aim);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Perfect Silent Aim (тестово) - наведение без движения камеры");
            }
            ImGui::PopID();

            // Ignore Visibility (для тестирования)
            ImGui::PushID("IgnoreVisibility");
            bool ignore_visibility = CHudAimbot::IsIgnoreVisibility();
            if (ImGui::Checkbox("Ignore Visibility", &ignore_visibility))
            {
                CHudAimbot::SetIgnoreVisibility(ignore_visibility);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Aim through walls (for testing)");
            }
            ImGui::PopID();

            // Aim Smooth
            ImGui::PushID("AimSmooth");
            float aim_smooth = CHudAimbot::GetSmooth();
            if (ImGui::SliderFloat("Aim Smooth", &aim_smooth, 0.0f, 50.0f, "%.1f"))
            {
                CHudAimbot::SetSmooth(aim_smooth);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Smoothness of aim movement (0=instant, 1=very slow, 50=fast)");
            }
            ImGui::PopID();

            // Only When Shooting
            ImGui::PushID("OnlyWhenShooting");
            bool only_when_shooting = CHudAimbot::IsOnlyWhenShooting();
            if (ImGui::Checkbox("Only When Shooting", &only_when_shooting))
            {
                CHudAimbot::SetOnlyWhenShooting(only_when_shooting);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Aim only when shooting button is pressed");
            }
            ImGui::PopID();

            ImGui::Unindent();
        }
    }
    ImGui::PopID();

    // Speed Hack Section
    ImGui::PushID("SpeedHack_Section");
    if (ImGui::CollapsingHeader("Speed Hack", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Speed Hack Enable
        ImGui::PushID("SpeedHackEnable");
        bool speed_hack_enabled = CHudAimbot::IsSpeedHack();
        if (ImGui::Checkbox("Enable", &speed_hack_enabled))
        {
            CHudAimbot::SetSpeedHack(speed_hack_enabled);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Increases movement speed");
        }
        ImGui::PopID();

        // Speed Multiplier (только если спидхак включен)
        if (speed_hack_enabled)
        {
            ImGui::Indent();

            ImGui::PushID("SpeedMultiplier");
            float speed_multiplier = CHudAimbot::GetSpeedMultiplier();
            if (ImGui::SliderFloat("Speed Multiplier", &speed_multiplier, 1.0f, 999999.0f, "%.1f"))
            {
                CHudAimbot::SetSpeedMultiplier(speed_multiplier);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Speed multiplier (1.0=normal, 2.0=double speed, etc.)");
            }
            ImGui::PopID();

            ImGui::Unindent();
        }
    }
    ImGui::PopID();

    ImGui::End();
}

bool CImGuiCheatMenu::Active()
{
    return m_ShowMenu;
}

bool CImGuiCheatMenu::CursorRequired()
{
    return true;
}

void CImGuiCheatMenu::CmdToggleCheatMenu()
{
    m_ShowMenu = !m_ShowMenu;
}

bool CImGuiCheatMenu::HandleKey(bool keyDown, int keyNumber, const char *bindName)
{
    // Обрабатываем INSERT для переключения меню (работает всегда, даже когда меню закрыто)
    if (keyNumber == K_INS && keyDown)
    {
        m_ShowMenu = !m_ShowMenu;
        return false; // Обработали клавишу, не передаем в движок
    }
    
    // Когда меню открыто, обрабатываем другие клавиши
    if (m_ShowMenu)
    {
        // ESC закрывает меню
        if (keyNumber == K_ESCAPE && keyDown)
        {
            m_ShowMenu = false;
            return false; // Закрыли меню, не передаем ESC в движок
        }
        
        // Когда меню открыто, не передаем остальные клавиши в движок
        // (они используются для взаимодействия с ImGui)
        return false;
    }
    
    // Когда меню закрыто и это не INSERT, не обрабатываем клавишу
    // (возвращаем true, чтобы другие обработчики могли обработать)
    return true;
}

