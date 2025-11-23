/***
*
*	Copyright (c) 1996-2002, Valve LLC, All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// hud_cheatmenu_vgui.cpp
//
// VGUI Cheat Menu implementation
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "hud_cheatmenu_vgui.h"
#include "hud_aimbot.h"
#include "hud_cornerbox.h"
#include "draw_util.h"
#include "cvardef.h"
#include "../engine/keydefs.h"
#include "in_defs.h"
#include <string.h>
#include <stdio.h>

// Команда для открытия/закрытия меню
void UserCmd_CheatMenuVGUI()
{
	gHUD.m_CheatMenuVGUI.ShowMenu();
}

int CHudCheatMenuVGUI::Init()
{
	gHUD.AddHudElem(this);
	
	m_bMenuVisible = false;
	m_iCurrentTab = 0;
	m_iSelectedElement = 0;
	m_iScrollOffset = 0;
	m_iTabCount = 0;
	m_iMouseX = 0;
	m_iMouseY = 0;
	
	// Размеры меню (компактный минималистичный стиль - меньше и тоньше)
	m_iMenuWidth = XRES(320);
	m_iMenuHeight = YRES(360);
	m_iMenuX = (ScreenWidth - m_iMenuWidth) / 2;
	m_iMenuY = (ScreenHeight - m_iMenuHeight) / 2;
	
	// Размеры панелей (горизонтальные вкладки вверху)
	m_iLeftPanelWidth = 0;  // Не используется - вкладки горизонтальные
	m_iRightPanelWidth = m_iMenuWidth;  // Вся ширина для контента
	m_iTabHeight = YRES(24);  // Высота вкладок (горизонтальные) - меньше
	m_iElementHeight = YRES(18);  // Компактная высота элементов - меньше
	m_iPadding = XRES(6);  // Меньше padding
	
	// Цвета (R, G, B, A) - минималистичный темный стиль
	m_iColorBackground[0] = 18;
	m_iColorBackground[1] = 18;
	m_iColorBackground[2] = 20;
	m_iColorBackground[3] = 245;
	
	m_iColorTabActive[0] = 255;
	m_iColorTabActive[1] = 255;
	m_iColorTabActive[2] = 255;
	m_iColorTabActive[3] = 255;
	
	m_iColorTabInactive[0] = 120;
	m_iColorTabInactive[1] = 120;
	m_iColorTabInactive[2] = 120;
	m_iColorTabInactive[3] = 255;
	
	m_iColorText[0] = 220;
	m_iColorText[1] = 220;
	m_iColorText[2] = 220;
	m_iColorText[3] = 255;
	
	m_iColorTextSelected[0] = 255;
	m_iColorTextSelected[1] = 255;
	m_iColorTextSelected[2] = 255;
	m_iColorTextSelected[3] = 255;
	
	m_iColorBorder[0] = 40;
	m_iColorBorder[1] = 40;
	m_iColorBorder[2] = 45;
	m_iColorBorder[3] = 255;
	
	// Инициализируем вкладки меню
	InitMenuTabs();
	
	// Регистрируем команду для открытия меню
	gEngfuncs.pfnAddCommand("ebash3d_menu", UserCmd_CheatMenuVGUI);
	
	m_iFlags = 0; // По умолчанию скрыто
	
	return 1;
}

int CHudCheatMenuVGUI::VidInit()
{
	// Обновляем размеры при изменении разрешения
	m_iMenuWidth = XRES(320);
	m_iMenuHeight = YRES(360);
	m_iMenuX = (ScreenWidth - m_iMenuWidth) / 2;
	m_iMenuY = (ScreenHeight - m_iMenuHeight) / 2;
	m_iLeftPanelWidth = 0;
	m_iRightPanelWidth = m_iMenuWidth;
	m_iTabHeight = YRES(24);
	m_iElementHeight = YRES(18);
	m_iPadding = XRES(6);
	
	return 1;
}

void CHudCheatMenuVGUI::Reset()
{
	m_bMenuVisible = false;
	m_iCurrentTab = 0;
	m_iSelectedElement = 0;
	m_iScrollOffset = 0;
	m_iFlags = 0;
}

int CHudCheatMenuVGUI::Draw(float flTime)
{
	if (!m_bMenuVisible)
		return 0;
	
	// Обновляем значения элементов из cvar
	UpdateElementValues();
	
	// Отрисовываем меню
	DrawMenuBackground();
	DrawTabs();
	DrawTabContent();
	DrawCursor();
	
	return 0;
}

void CHudCheatMenuVGUI::ShowMenu()
{
	m_bMenuVisible = true;
	m_iFlags |= HUD_DRAW;
	m_iCurrentTab = 0;
	m_iSelectedElement = 0;
	m_iScrollOffset = 0;
	
	// Инициализируем позицию курсора в центре экрана
	m_iMouseX = ScreenWidth / 2;
	m_iMouseY = ScreenHeight / 2;
}

void CHudCheatMenuVGUI::HideMenu()
{
	m_bMenuVisible = false;
	m_iFlags &= ~HUD_DRAW;
}

void CHudCheatMenuVGUI::InitMenuTabs()
{
	m_iTabCount = 0;
	
	// Инициализируем вкладки: Visual, Aimbot, Misc, Movement
	InitVisualTab();
	InitAimbotTab();
	InitMiscTab();
	InitMovementTab();
}

void CHudCheatMenuVGUI::InitVisualTab()
{
	CheatMenuTab& tab = m_Tabs[m_iTabCount];
	strncpy(tab.name, "Visual", sizeof(tab.name) - 1);
	tab.elementCount = 0;
	
	// ESP Enable
	CheatMenuElement& elem1 = tab.elements[tab.elementCount++];
	elem1.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem1.name, "ESP", sizeof(elem1.name) - 1);
	strncpy(elem1.cvarName, "ebash3d_esp", sizeof(elem1.cvarName) - 1);
	elem1.enabled = false;
	elem1.value = 0.0f;
	
	// ESP Box
	CheatMenuElement& elem_box_r = tab.elements[tab.elementCount++];
	elem_box_r.type = CHEAT_MENU_SLIDER;
	strncpy(elem_box_r.name, "Box R", sizeof(elem_box_r.name) - 1);
	strncpy(elem_box_r.cvarName, "ebash3d_esp_box_r", sizeof(elem_box_r.cvarName) - 1);
	elem_box_r.minValue = 0.0f;
	elem_box_r.maxValue = 255.0f;
	elem_box_r.stepValue = 1.0f;
	elem_box_r.value = 255.0f;
	
	CheatMenuElement& elem_box_g = tab.elements[tab.elementCount++];
	elem_box_g.type = CHEAT_MENU_SLIDER;
	strncpy(elem_box_g.name, "Box G", sizeof(elem_box_g.name) - 1);
	strncpy(elem_box_g.cvarName, "ebash3d_esp_box_g", sizeof(elem_box_g.cvarName) - 1);
	elem_box_g.minValue = 0.0f;
	elem_box_g.maxValue = 255.0f;
	elem_box_g.stepValue = 1.0f;
	elem_box_g.value = 0.0f;
	
	CheatMenuElement& elem_box_b = tab.elements[tab.elementCount++];
	elem_box_b.type = CHEAT_MENU_SLIDER;
	strncpy(elem_box_b.name, "Box B", sizeof(elem_box_b.name) - 1);
	strncpy(elem_box_b.cvarName, "ebash3d_esp_box_b", sizeof(elem_box_b.cvarName) - 1);
	elem_box_b.minValue = 0.0f;
	elem_box_b.maxValue = 255.0f;
	elem_box_b.stepValue = 1.0f;
	elem_box_b.value = 0.0f;
	
	// ESP Line
	CheatMenuElement& elem_line_r = tab.elements[tab.elementCount++];
	elem_line_r.type = CHEAT_MENU_SLIDER;
	strncpy(elem_line_r.name, "Line R", sizeof(elem_line_r.name) - 1);
	strncpy(elem_line_r.cvarName, "ebash3d_esp_line_r", sizeof(elem_line_r.cvarName) - 1);
	elem_line_r.minValue = 0.0f;
	elem_line_r.maxValue = 255.0f;
	elem_line_r.stepValue = 1.0f;
	elem_line_r.value = 255.0f;
	
	CheatMenuElement& elem_line_g = tab.elements[tab.elementCount++];
	elem_line_g.type = CHEAT_MENU_SLIDER;
	strncpy(elem_line_g.name, "Line G", sizeof(elem_line_g.name) - 1);
	strncpy(elem_line_g.cvarName, "ebash3d_esp_line_g", sizeof(elem_line_g.cvarName) - 1);
	elem_line_g.minValue = 0.0f;
	elem_line_g.maxValue = 255.0f;
	elem_line_g.stepValue = 1.0f;
	elem_line_g.value = 0.0f;
	
	CheatMenuElement& elem_line_b = tab.elements[tab.elementCount++];
	elem_line_b.type = CHEAT_MENU_SLIDER;
	strncpy(elem_line_b.name, "Line B", sizeof(elem_line_b.name) - 1);
	strncpy(elem_line_b.cvarName, "ebash3d_esp_line_b", sizeof(elem_line_b.cvarName) - 1);
	elem_line_b.minValue = 0.0f;
	elem_line_b.maxValue = 255.0f;
	elem_line_b.stepValue = 1.0f;
	elem_line_b.value = 0.0f;
	
	// ESP Weapon
	CheatMenuElement& elem_weapon_r = tab.elements[tab.elementCount++];
	elem_weapon_r.type = CHEAT_MENU_SLIDER;
	strncpy(elem_weapon_r.name, "Weapon R", sizeof(elem_weapon_r.name) - 1);
	strncpy(elem_weapon_r.cvarName, "ebash3d_esp_weapon_r", sizeof(elem_weapon_r.cvarName) - 1);
	elem_weapon_r.minValue = 0.0f;
	elem_weapon_r.maxValue = 255.0f;
	elem_weapon_r.stepValue = 1.0f;
	elem_weapon_r.value = 255.0f;
	
	CheatMenuElement& elem_weapon_g = tab.elements[tab.elementCount++];
	elem_weapon_g.type = CHEAT_MENU_SLIDER;
	strncpy(elem_weapon_g.name, "Weapon G", sizeof(elem_weapon_g.name) - 1);
	strncpy(elem_weapon_g.cvarName, "ebash3d_esp_weapon_g", sizeof(elem_weapon_g.cvarName) - 1);
	elem_weapon_g.minValue = 0.0f;
	elem_weapon_g.maxValue = 255.0f;
	elem_weapon_g.stepValue = 1.0f;
	elem_weapon_g.value = 0.0f;
	
	CheatMenuElement& elem_weapon_b = tab.elements[tab.elementCount++];
	elem_weapon_b.type = CHEAT_MENU_SLIDER;
	strncpy(elem_weapon_b.name, "Weapon B", sizeof(elem_weapon_b.name) - 1);
	strncpy(elem_weapon_b.cvarName, "ebash3d_esp_weapon_b", sizeof(elem_weapon_b.cvarName) - 1);
	elem_weapon_b.minValue = 0.0f;
	elem_weapon_b.maxValue = 255.0f;
	elem_weapon_b.stepValue = 1.0f;
	elem_weapon_b.value = 0.0f;
	
	// Player List
	CheatMenuElement& elem_playerlist = tab.elements[tab.elementCount++];
	elem_playerlist.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_playerlist.name, "Player List", sizeof(elem_playerlist.name) - 1);
	strncpy(elem_playerlist.cvarName, "ebash3d_playerlist", sizeof(elem_playerlist.cvarName) - 1);
	elem_playerlist.enabled = false;
	elem_playerlist.value = 0.0f;
	
	// Weapon Glow
	CheatMenuElement& elem_weapon_glow = tab.elements[tab.elementCount++];
	elem_weapon_glow.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_weapon_glow.name, "Weapon Glow", sizeof(elem_weapon_glow.name) - 1);
	strncpy(elem_weapon_glow.cvarName, "ebash3d_weapon_glow", sizeof(elem_weapon_glow.cvarName) - 1);
	elem_weapon_glow.enabled = false;
	elem_weapon_glow.value = 0.0f;
	
	// Hands Glow
	CheatMenuElement& elem_hands_glow = tab.elements[tab.elementCount++];
	elem_hands_glow.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_hands_glow.name, "Hands Glow", sizeof(elem_hands_glow.name) - 1);
	strncpy(elem_hands_glow.cvarName, "ebash3d_hands_glow", sizeof(elem_hands_glow.cvarName) - 1);
	elem_hands_glow.enabled = false;
	elem_hands_glow.value = 0.0f;
	
	// Glow Color
	CheatMenuElement& elem_glow_r = tab.elements[tab.elementCount++];
	elem_glow_r.type = CHEAT_MENU_SLIDER;
	strncpy(elem_glow_r.name, "Glow R", sizeof(elem_glow_r.name) - 1);
	strncpy(elem_glow_r.cvarName, "ebash3d_glow_color_r", sizeof(elem_glow_r.cvarName) - 1);
	elem_glow_r.minValue = 0.0f;
	elem_glow_r.maxValue = 255.0f;
	elem_glow_r.stepValue = 1.0f;
	elem_glow_r.value = 255.0f;
	
	CheatMenuElement& elem_glow_g = tab.elements[tab.elementCount++];
	elem_glow_g.type = CHEAT_MENU_SLIDER;
	strncpy(elem_glow_g.name, "Glow G", sizeof(elem_glow_g.name) - 1);
	strncpy(elem_glow_g.cvarName, "ebash3d_glow_color_g", sizeof(elem_glow_g.cvarName) - 1);
	elem_glow_g.minValue = 0.0f;
	elem_glow_g.maxValue = 255.0f;
	elem_glow_g.stepValue = 1.0f;
	elem_glow_g.value = 255.0f;
	
	CheatMenuElement& elem_glow_b = tab.elements[tab.elementCount++];
	elem_glow_b.type = CHEAT_MENU_SLIDER;
	strncpy(elem_glow_b.name, "Glow B", sizeof(elem_glow_b.name) - 1);
	strncpy(elem_glow_b.cvarName, "ebash3d_glow_color_b", sizeof(elem_glow_b.cvarName) - 1);
	elem_glow_b.minValue = 0.0f;
	elem_glow_b.maxValue = 255.0f;
	elem_glow_b.stepValue = 1.0f;
	elem_glow_b.value = 255.0f;
	
	CheatMenuElement& elem_glow_alpha = tab.elements[tab.elementCount++];
	elem_glow_alpha.type = CHEAT_MENU_SLIDER;
	strncpy(elem_glow_alpha.name, "Glow Alpha", sizeof(elem_glow_alpha.name) - 1);
	strncpy(elem_glow_alpha.cvarName, "ebash3d_glow_alpha", sizeof(elem_glow_alpha.cvarName) - 1);
	elem_glow_alpha.minValue = 0.0f;
	elem_glow_alpha.maxValue = 255.0f;
	elem_glow_alpha.stepValue = 1.0f;
	elem_glow_alpha.value = 200.0f;
	
	// Third Person
	CheatMenuElement& elem_thirdperson = tab.elements[tab.elementCount++];
	elem_thirdperson.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_thirdperson.name, "Third Person", sizeof(elem_thirdperson.name) - 1);
	strncpy(elem_thirdperson.cvarName, "ebash3d_thirdperson", sizeof(elem_thirdperson.cvarName) - 1);
	elem_thirdperson.enabled = false;
	elem_thirdperson.value = 0.0f;
	
	CheatMenuElement& elem_thirdperson_dist = tab.elements[tab.elementCount++];
	elem_thirdperson_dist.type = CHEAT_MENU_SLIDER;
	strncpy(elem_thirdperson_dist.name, "Third Person Dist", sizeof(elem_thirdperson_dist.name) - 1);
	strncpy(elem_thirdperson_dist.cvarName, "ebash3d_thirdperson_dist", sizeof(elem_thirdperson_dist.cvarName) - 1);
	elem_thirdperson_dist.minValue = 30.0f;
	elem_thirdperson_dist.maxValue = 500.0f;
	elem_thirdperson_dist.stepValue = 10.0f;
	elem_thirdperson_dist.value = 100.0f;
	
	// Disable Animations
	CheatMenuElement& elem_disable_anim = tab.elements[tab.elementCount++];
	elem_disable_anim.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_disable_anim.name, "Disable Animations", sizeof(elem_disable_anim.name) - 1);
	strncpy(elem_disable_anim.cvarName, "ebash3d_disable_animations", sizeof(elem_disable_anim.cvarName) - 1);
	elem_disable_anim.enabled = false;
	elem_disable_anim.value = 0.0f;
	
	// Custom FOV
	CheatMenuElement& elem_custom_fov = tab.elements[tab.elementCount++];
	elem_custom_fov.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_custom_fov.name, "Custom FOV", sizeof(elem_custom_fov.name) - 1);
	strncpy(elem_custom_fov.cvarName, "ebash3d_custom_fov", sizeof(elem_custom_fov.cvarName) - 1);
	elem_custom_fov.enabled = false;
	elem_custom_fov.value = 0.0f;
	
	CheatMenuElement& elem_custom_fov_val = tab.elements[tab.elementCount++];
	elem_custom_fov_val.type = CHEAT_MENU_SLIDER;
	strncpy(elem_custom_fov_val.name, "FOV Value", sizeof(elem_custom_fov_val.name) - 1);
	strncpy(elem_custom_fov_val.cvarName, "ebash3d_custom_fov_value", sizeof(elem_custom_fov_val.cvarName) - 1);
	elem_custom_fov_val.minValue = 10.0f;
	elem_custom_fov_val.maxValue = 170.0f;
	elem_custom_fov_val.stepValue = 1.0f;
	elem_custom_fov_val.value = 90.0f;
	
	m_iTabCount++;
}

void CHudCheatMenuVGUI::InitAimbotTab()
{
	CheatMenuTab& tab = m_Tabs[m_iTabCount];
	strncpy(tab.name, "Aimbot", sizeof(tab.name) - 1);
	tab.elementCount = 0;
	
	// Aimbot Enable
	CheatMenuElement& elem1 = tab.elements[tab.elementCount++];
	elem1.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem1.name, "Aimbot", sizeof(elem1.name) - 1);
	strncpy(elem1.cvarName, "ebash3d_aimbot", sizeof(elem1.cvarName) - 1);
	elem1.enabled = false;
	elem1.value = 0.0f;
	
	// Silent Aim
	CheatMenuElement& elem2 = tab.elements[tab.elementCount++];
	elem2.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem2.name, "Silent Aim", sizeof(elem2.name) - 1);
	strncpy(elem2.cvarName, "ebash3d_psilent", sizeof(elem2.cvarName) - 1);
	elem2.enabled = false;
	elem2.value = 0.0f;
	
	// RCS Enable
	CheatMenuElement& elem_rcs = tab.elements[tab.elementCount++];
	elem_rcs.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_rcs.name, "RCS", sizeof(elem_rcs.name) - 1);
	strncpy(elem_rcs.cvarName, "ebash3d_rcs_enable", sizeof(elem_rcs.cvarName) - 1);
	elem_rcs.enabled = false;
	elem_rcs.value = 0.0f;
	
	// RCS Strength
	CheatMenuElement& elem_rcs_str = tab.elements[tab.elementCount++];
	elem_rcs_str.type = CHEAT_MENU_SLIDER;
	strncpy(elem_rcs_str.name, "RCS Strength", sizeof(elem_rcs_str.name) - 1);
	strncpy(elem_rcs_str.cvarName, "ebash3d_rcs", sizeof(elem_rcs_str.cvarName) - 1);
	elem_rcs_str.minValue = 0.1f;
	elem_rcs_str.maxValue = 2.0f;
	elem_rcs_str.stepValue = 0.1f;
	elem_rcs_str.value = 1.0f;
	
	// No Recoil
	CheatMenuElement& elem_norecoil = tab.elements[tab.elementCount++];
	elem_norecoil.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_norecoil.name, "No Recoil", sizeof(elem_norecoil.name) - 1);
	strncpy(elem_norecoil.cvarName, "ebash3d_norecoil", sizeof(elem_norecoil.cvarName) - 1);
	elem_norecoil.enabled = false;
	elem_norecoil.value = 0.0f;
	
	// Trigger Bot
	CheatMenuElement& elem_trigger = tab.elements[tab.elementCount++];
	elem_trigger.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_trigger.name, "Trigger Bot", sizeof(elem_trigger.name) - 1);
	strncpy(elem_trigger.cvarName, "ebash3d_triggerbot", sizeof(elem_trigger.cvarName) - 1);
	elem_trigger.enabled = false;
	elem_trigger.value = 0.0f;
	
	// Trigger Bot Onetap
	CheatMenuElement& elem_trigger_onetap = tab.elements[tab.elementCount++];
	elem_trigger_onetap.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_trigger_onetap.name, "Trigger Onetap", sizeof(elem_trigger_onetap.name) - 1);
	strncpy(elem_trigger_onetap.cvarName, "ebash3d_triggerbot_onetap", sizeof(elem_trigger_onetap.cvarName) - 1);
	elem_trigger_onetap.enabled = false;
	elem_trigger_onetap.value = 0.0f;
	
	// FOV
	CheatMenuElement& elem3 = tab.elements[tab.elementCount++];
	elem3.type = CHEAT_MENU_SLIDER;
	strncpy(elem3.name, "FOV", sizeof(elem3.name) - 1);
	strncpy(elem3.cvarName, "ebash3d_aimbot_fov", sizeof(elem3.cvarName) - 1);
	elem3.minValue = 1.0f;
	elem3.maxValue = 360.0f;
	elem3.stepValue = 1.0f;
	elem3.value = 5.0f;
	
	// Smooth
	CheatMenuElement& elem4 = tab.elements[tab.elementCount++];
	elem4.type = CHEAT_MENU_SLIDER;
	strncpy(elem4.name, "Smooth", sizeof(elem4.name) - 1);
	strncpy(elem4.cvarName, "ebash3d_aimbot_smooth", sizeof(elem4.cvarName) - 1);
	elem4.minValue = 0.0f;
	elem4.maxValue = 100.0f;
	elem4.stepValue = 1.0f;
	elem4.value = 0.0f;
	
	// Aim Height
	CheatMenuElement& elem5 = tab.elements[tab.elementCount++];
	elem5.type = CHEAT_MENU_SLIDER;
	strncpy(elem5.name, "Aim Height", sizeof(elem5.name) - 1);
	strncpy(elem5.cvarName, "ebash3d_aimbot_height", sizeof(elem5.cvarName) - 1);
	elem5.minValue = 0.0f;
	elem5.maxValue = 50.0f;
	elem5.stepValue = 1.0f;
	elem5.value = 28.0f;
	
	// Only When Shooting
	CheatMenuElement& elem6 = tab.elements[tab.elementCount++];
	elem6.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem6.name, "Only When Shooting", sizeof(elem6.name) - 1);
	strncpy(elem6.cvarName, "ebash3d_aimbot_onlyshooting", sizeof(elem6.cvarName) - 1);
	elem6.enabled = false;
	elem6.value = 0.0f;
	
	// Ignore Visibility
	CheatMenuElement& elem7 = tab.elements[tab.elementCount++];
	elem7.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem7.name, "Ignore Visibility", sizeof(elem7.name) - 1);
	strncpy(elem7.cvarName, "ebash3d_aimbot_ignorevisibility", sizeof(elem7.cvarName) - 1);
	elem7.enabled = false;
	elem7.value = 0.0f;
	
	// Draw FOV
	CheatMenuElement& elem8 = tab.elements[tab.elementCount++];
	elem8.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem8.name, "Draw FOV Circle", sizeof(elem8.name) - 1);
	strncpy(elem8.cvarName, "ebash3d_aimbot_drawfov", sizeof(elem8.cvarName) - 1);
	elem8.enabled = true;
	elem8.value = 1.0f;
	
	// AutoShoot
	CheatMenuElement& elem9 = tab.elements[tab.elementCount++];
	elem9.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem9.name, "AutoShoot", sizeof(elem9.name) - 1);
	strncpy(elem9.cvarName, "ebash3d_aimbot_autoshoot", sizeof(elem9.cvarName) - 1);
	elem9.enabled = false;
	elem9.value = 0.0f;
	
	// Target Bone
	CheatMenuElement& elem10 = tab.elements[tab.elementCount++];
	elem10.type = CHEAT_MENU_SLIDER;
	strncpy(elem10.name, "Target Bone", sizeof(elem10.name) - 1);
	strncpy(elem10.cvarName, "ebash3d_aimbot_target_bone", sizeof(elem10.cvarName) - 1);
	elem10.minValue = 0.0f;
	elem10.maxValue = 2.0f;
	elem10.stepValue = 1.0f;
	elem10.value = 0.0f;
	
	// Reset On Round
	CheatMenuElement& elem11 = tab.elements[tab.elementCount++];
	elem11.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem11.name, "Reset On Round", sizeof(elem11.name) - 1);
	strncpy(elem11.cvarName, "ebash3d_aimbot_reset_on_round", sizeof(elem11.cvarName) - 1);
	elem11.enabled = true;
	elem11.value = 1.0f;
	
	// Head Scale
	CheatMenuElement& elem12 = tab.elements[tab.elementCount++];
	elem12.type = CHEAT_MENU_SLIDER;
	strncpy(elem12.name, "Head Scale", sizeof(elem12.name) - 1);
	strncpy(elem12.cvarName, "ebash3d_aimbot_head_scale", sizeof(elem12.cvarName) - 1);
	elem12.minValue = 0.1f;
	elem12.maxValue = 2.0f;
	elem12.stepValue = 0.1f;
	elem12.value = 1.0f;
	
	// Backtrack
	CheatMenuElement& elem13 = tab.elements[tab.elementCount++];
	elem13.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem13.name, "Backtrack", sizeof(elem13.name) - 1);
	strncpy(elem13.cvarName, "ebash3d_aimbot_backtrack", sizeof(elem13.cvarName) - 1);
	elem13.enabled = false;
	elem13.value = 0.0f;
	
	// Backtrack Time
	CheatMenuElement& elem14 = tab.elements[tab.elementCount++];
	elem14.type = CHEAT_MENU_SLIDER;
	strncpy(elem14.name, "Backtrack Time", sizeof(elem14.name) - 1);
	strncpy(elem14.cvarName, "ebash3d_aimbot_backtrack_time", sizeof(elem14.cvarName) - 1);
	elem14.minValue = 0.0f;
	elem14.maxValue = 1000.0f;
	elem14.stepValue = 10.0f;
	elem14.value = 200.0f;
	
	m_iTabCount++;
}

void CHudCheatMenuVGUI::InitMiscTab()
{
	CheatMenuTab& tab = m_Tabs[m_iTabCount];
	strncpy(tab.name, "Misc", sizeof(tab.name) - 1);
	tab.elementCount = 0;
	
	// CS Protector
	CheatMenuElement& elem_protector = tab.elements[tab.elementCount++];
	elem_protector.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_protector.name, "CS Protector", sizeof(elem_protector.name) - 1);
	strncpy(elem_protector.cvarName, "ebash3d_cs_protector", sizeof(elem_protector.cvarName) - 1);
	elem_protector.enabled = true;
	elem_protector.value = 1.0f;
	
	// CS Protector Block All
	CheatMenuElement& elem_protector_block = tab.elements[tab.elementCount++];
	elem_protector_block.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_protector_block.name, "Block All Cmds", sizeof(elem_protector_block.name) - 1);
	strncpy(elem_protector_block.cvarName, "ebash3d_cs_protector_block_all", sizeof(elem_protector_block.cvarName) - 1);
	elem_protector_block.enabled = true;
	elem_protector_block.value = 1.0f;
	
	// CS Protector Warn
	CheatMenuElement& elem_protector_warn = tab.elements[tab.elementCount++];
	elem_protector_warn.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_protector_warn.name, "CS Protector Warn", sizeof(elem_protector_warn.name) - 1);
	strncpy(elem_protector_warn.cvarName, "ebash3d_cs_protector_warn", sizeof(elem_protector_warn.cvarName) - 1);
	elem_protector_warn.enabled = true;
	elem_protector_warn.value = 1.0f;
	
	// FPS Hack
	CheatMenuElement& elem_fps = tab.elements[tab.elementCount++];
	elem_fps.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_fps.name, "FPS Hack", sizeof(elem_fps.name) - 1);
	strncpy(elem_fps.cvarName, "ebash3d_fps_hack", sizeof(elem_fps.cvarName) - 1);
	elem_fps.enabled = false;
	elem_fps.value = 0.0f;
	
	// OSCS Bypass
	CheatMenuElement& elem_oscs = tab.elements[tab.elementCount++];
	elem_oscs.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_oscs.name, "OSCS Bypass", sizeof(elem_oscs.name) - 1);
	strncpy(elem_oscs.cvarName, "ebash3d_oscs_bypass", sizeof(elem_oscs.cvarName) - 1);
	elem_oscs.enabled = false;
	elem_oscs.value = 0.0f;
	
	// Kill Message
	CheatMenuElement& elem_killmsg = tab.elements[tab.elementCount++];
	elem_killmsg.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_killmsg.name, "Kill Message", sizeof(elem_killmsg.name) - 1);
	strncpy(elem_killmsg.cvarName, "ebash3d_killmessage", sizeof(elem_killmsg.cvarName) - 1);
	elem_killmsg.enabled = false;
	elem_killmsg.value = 0.0f;
	
	// Deathmatch Mode
	CheatMenuElement& elem_dm = tab.elements[tab.elementCount++];
	elem_dm.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_dm.name, "Deathmatch Mode", sizeof(elem_dm.name) - 1);
	strncpy(elem_dm.cvarName, "ebash3d_deathmatch", sizeof(elem_dm.cvarName) - 1);
	elem_dm.enabled = false;
	elem_dm.value = 0.0f;
	
	// Spinbot
	CheatMenuElement& elem_spin = tab.elements[tab.elementCount++];
	elem_spin.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_spin.name, "Spinbot", sizeof(elem_spin.name) - 1);
	strncpy(elem_spin.cvarName, "ebash3d_spinbot", sizeof(elem_spin.cvarName) - 1);
	elem_spin.enabled = false;
	elem_spin.value = 0.0f;
	
	// Spinbot Speed
	CheatMenuElement& elem_spin_speed = tab.elements[tab.elementCount++];
	elem_spin_speed.type = CHEAT_MENU_SLIDER;
	strncpy(elem_spin_speed.name, "Spinbot Speed", sizeof(elem_spin_speed.name) - 1);
	strncpy(elem_spin_speed.cvarName, "ebash3d_spinbot_speed", sizeof(elem_spin_speed.cvarName) - 1);
	elem_spin_speed.minValue = 1.0f;
	elem_spin_speed.maxValue = 1000.0f;
	elem_spin_speed.stepValue = 10.0f;
	elem_spin_speed.value = 100.0f;
	
	// Anti-Aim
	CheatMenuElement& elem_aa = tab.elements[tab.elementCount++];
	elem_aa.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_aa.name, "Anti-Aim", sizeof(elem_aa.name) - 1);
	strncpy(elem_aa.cvarName, "ebash3d_antiaim", sizeof(elem_aa.cvarName) - 1);
	elem_aa.enabled = false;
	elem_aa.value = 0.0f;
	
	// Anti-Aim Mode
	CheatMenuElement& elem_aa_mode = tab.elements[tab.elementCount++];
	elem_aa_mode.type = CHEAT_MENU_SLIDER;
	strncpy(elem_aa_mode.name, "AA Mode", sizeof(elem_aa_mode.name) - 1);
	strncpy(elem_aa_mode.cvarName, "ebash3d_antiaim_mode", sizeof(elem_aa_mode.cvarName) - 1);
	elem_aa_mode.minValue = 0.0f;
	elem_aa_mode.maxValue = 7.0f;
	elem_aa_mode.stepValue = 1.0f;
	elem_aa_mode.value = 0.0f;
	
	// Anti-Aim Speed
	CheatMenuElement& elem_aa_speed = tab.elements[tab.elementCount++];
	elem_aa_speed.type = CHEAT_MENU_SLIDER;
	strncpy(elem_aa_speed.name, "AA Speed", sizeof(elem_aa_speed.name) - 1);
	strncpy(elem_aa_speed.cvarName, "ebash3d_antiaim_speed", sizeof(elem_aa_speed.cvarName) - 1);
	elem_aa_speed.minValue = 1.0f;
	elem_aa_speed.maxValue = 500.0f;
	elem_aa_speed.stepValue = 10.0f;
	elem_aa_speed.value = 50.0f;
	
	// Anti-Aim Jitter Range
	CheatMenuElement& elem_aa_jitter = tab.elements[tab.elementCount++];
	elem_aa_jitter.type = CHEAT_MENU_SLIDER;
	strncpy(elem_aa_jitter.name, "AA Jitter Range", sizeof(elem_aa_jitter.name) - 1);
	strncpy(elem_aa_jitter.cvarName, "ebash3d_antiaim_jitter_range", sizeof(elem_aa_jitter.cvarName) - 1);
	elem_aa_jitter.minValue = 1.0f;
	elem_aa_jitter.maxValue = 180.0f;
	elem_aa_jitter.stepValue = 5.0f;
	elem_aa_jitter.value = 30.0f;
	
	// Anti-Aim Fake Angle
	CheatMenuElement& elem_aa_fake = tab.elements[tab.elementCount++];
	elem_aa_fake.type = CHEAT_MENU_SLIDER;
	strncpy(elem_aa_fake.name, "AA Fake Angle", sizeof(elem_aa_fake.name) - 1);
	strncpy(elem_aa_fake.cvarName, "ebash3d_antiaim_fake_angle", sizeof(elem_aa_fake.cvarName) - 1);
	elem_aa_fake.minValue = 1.0f;
	elem_aa_fake.maxValue = 180.0f;
	elem_aa_fake.stepValue = 5.0f;
	elem_aa_fake.value = 58.0f;
	
	m_iTabCount++;
}

void CHudCheatMenuVGUI::InitMovementTab()
{
	CheatMenuTab& tab = m_Tabs[m_iTabCount];
	strncpy(tab.name, "Movement", sizeof(tab.name) - 1);
	tab.elementCount = 0;
	
	// Autostrafe
	CheatMenuElement& elem_autostrafe = tab.elements[tab.elementCount++];
	elem_autostrafe.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_autostrafe.name, "Autostrafe", sizeof(elem_autostrafe.name) - 1);
	strncpy(elem_autostrafe.cvarName, "cl_autostrafe", sizeof(elem_autostrafe.cvarName) - 1);
	elem_autostrafe.enabled = false;
	elem_autostrafe.value = 0.0f;
	
	// Autojump
	CheatMenuElement& elem_autojump = tab.elements[tab.elementCount++];
	elem_autojump.type = CHEAT_MENU_CHECKBOX;
	strncpy(elem_autojump.name, "Autojump", sizeof(elem_autojump.name) - 1);
	strncpy(elem_autojump.cvarName, "cl_autojump", sizeof(elem_autojump.cvarName) - 1);
	elem_autojump.enabled = false;
	elem_autojump.value = 0.0f;
	
	m_iTabCount++;
}

void CHudCheatMenuVGUI::DrawMenuBackground()
{
	// Тонкая тень
	FillRGBABlend(
		m_iMenuX + 2,
		m_iMenuY + 2,
		m_iMenuWidth,
		m_iMenuHeight,
		0,
		0,
		0,
		120
	);
	
	// Фон меню - темный минималистичный
	FillRGBABlend(
		m_iMenuX,
		m_iMenuY,
		m_iMenuWidth,
		m_iMenuHeight,
		m_iColorBackground[0],
		m_iColorBackground[1],
		m_iColorBackground[2],
		m_iColorBackground[3]
	);
	
	// Внешние границы - тонкие темные линии
	// Верхняя
	FillRGBABlend(
		m_iMenuX,
		m_iMenuY,
		m_iMenuWidth,
		1,
		m_iColorBorder[0],
		m_iColorBorder[1],
		m_iColorBorder[2],
		m_iColorBorder[3]
	);
	
	// Нижняя
	FillRGBABlend(
		m_iMenuX,
		m_iMenuY + m_iMenuHeight - 1,
		m_iMenuWidth,
		1,
		m_iColorBorder[0],
		m_iColorBorder[1],
		m_iColorBorder[2],
		m_iColorBorder[3]
	);
	
	// Левая
	FillRGBABlend(
		m_iMenuX,
		m_iMenuY,
		1,
		m_iMenuHeight,
		m_iColorBorder[0],
		m_iColorBorder[1],
		m_iColorBorder[2],
		m_iColorBorder[3]
	);
	
	// Правая
	FillRGBABlend(
		m_iMenuX + m_iMenuWidth - 1,
		m_iMenuY,
		1,
		m_iMenuHeight,
		m_iColorBorder[0],
		m_iColorBorder[1],
		m_iColorBorder[2],
		m_iColorBorder[3]
	);
}

void CHudCheatMenuVGUI::DrawTabs()
{
	// Горизонтальные вкладки вверху меню (компактный стиль)
	int tabY = m_iMenuY;
	int tabTotalWidth = m_iMenuWidth;
	int tabWidth = tabTotalWidth / m_iTabCount;
	
	// Фон для всех вкладок
	FillRGBABlend(
		m_iMenuX,
		tabY,
		m_iMenuWidth,
		m_iTabHeight,
		12,
		12,
		14,
		255
	);
	
	// Разделитель под вкладками
	FillRGBABlend(
		m_iMenuX,
		tabY + m_iTabHeight - 1,
		m_iMenuWidth,
		1,
		m_iColorBorder[0],
		m_iColorBorder[1],
		m_iColorBorder[2],
		m_iColorBorder[3]
	);
	
	for (int i = 0; i < m_iTabCount; i++)
	{
		int tabX = m_iMenuX + (i * tabWidth);
		bool isActive = (i == m_iCurrentTab);
		
		// Активная вкладка - белая линия снизу
		if (isActive)
		{
			FillRGBABlend(
				tabX,
				tabY + m_iTabHeight - 2,
				tabWidth,
				2,
				255,
				255,
				255,
				255
			);
		}
		
		// Разделитель между вкладками
		if (i < m_iTabCount - 1)
		{
			FillRGBABlend(
				tabX + tabWidth - 1,
				tabY,
				1,
				m_iTabHeight,
				30,
				30,
				35,
				255
			);
		}
		
		// Текст вкладки (центрирован)
		int textX = tabX + (tabWidth / 2) - (strlen(m_Tabs[i].name) * 3);
		int textY = tabY + (m_iTabHeight / 2) - YRES(5);
		DrawUtils::DrawHudString(
			textX,
			textY,
			ScreenWidth,
			m_Tabs[i].name,
			isActive ? m_iColorTextSelected[0] : m_iColorTabInactive[0],
			isActive ? m_iColorTextSelected[1] : m_iColorTabInactive[1],
			isActive ? m_iColorTextSelected[2] : m_iColorTabInactive[2]
		);
	}
}

void CHudCheatMenuVGUI::DrawTabContent()
{
	if (m_iCurrentTab < 0 || m_iCurrentTab >= m_iTabCount)
		return;
	
	CheatMenuTab& tab = m_Tabs[m_iCurrentTab];
	
	// Настройки отображаются под вкладками
	int contentY = m_iMenuY + m_iTabHeight + m_iPadding;
	int contentX = m_iMenuX + m_iPadding;
	int contentHeight = m_iMenuHeight - m_iTabHeight - (m_iPadding * 2);
	int maxVisibleElements = contentHeight / m_iElementHeight;
	
	// Ограничиваем выбранный элемент
	if (m_iSelectedElement < 0)
		m_iSelectedElement = 0;
	if (m_iSelectedElement >= tab.elementCount)
		m_iSelectedElement = tab.elementCount - 1;
	
	// Автоматический скроллинг: обновляем смещение, чтобы выбранный элемент был виден
	if (tab.elementCount > maxVisibleElements)
	{
		if (m_iSelectedElement < m_iScrollOffset)
		{
			m_iScrollOffset = m_iSelectedElement;
		}
		else if (m_iSelectedElement >= m_iScrollOffset + maxVisibleElements)
		{
			m_iScrollOffset = m_iSelectedElement - maxVisibleElements + 1;
		}
		
		// Ограничиваем смещение
		if (m_iScrollOffset < 0)
			m_iScrollOffset = 0;
		if (m_iScrollOffset > tab.elementCount - maxVisibleElements)
			m_iScrollOffset = tab.elementCount - maxVisibleElements;
	}
	else
	{
		m_iScrollOffset = 0;
	}
	
	// Рисуем только видимые элементы
	int startIndex = m_iScrollOffset;
	int endIndex = startIndex + maxVisibleElements;
	if (endIndex > tab.elementCount)
		endIndex = tab.elementCount;
	
	for (int i = startIndex; i < endIndex; i++)
	{
		bool selected = (i == m_iSelectedElement);
		int y = contentY + ((i - startIndex) * m_iElementHeight);
		DrawElement(i, contentX, y, selected);
	}
}

void CHudCheatMenuVGUI::DrawElement(int index, int x, int y, bool selected)
{
	if (m_iCurrentTab < 0 || m_iCurrentTab >= m_iTabCount)
		return;
	
	CheatMenuTab& tab = m_Tabs[m_iCurrentTab];
	if (index < 0 || index >= tab.elementCount)
		return;
	
	CheatMenuElement& element = tab.elements[index];
	
	// Подсветка выбранного элемента (минималистичный стиль)
	if (selected)
	{
		// Тонкая линия слева для выбранного элемента
		FillRGBABlend(
			x - m_iPadding,
			y - 1,
			2,
			m_iElementHeight,
			255,
			255,
			255,
			200
		);
	}
	
	switch (element.type)
	{
	case CHEAT_MENU_CHECKBOX:
		DrawCheckbox(x, y, element.name, element.enabled, selected);
		break;
	case CHEAT_MENU_SLIDER:
		DrawSlider(x, y, element.name, element.value, element.minValue, element.maxValue, selected);
		break;
	case CHEAT_MENU_BUTTON:
		DrawButton(x, y, element.name, selected);
		break;
	case CHEAT_MENU_LABEL:
		DrawLabel(x, y, element.name);
		break;
	case CHEAT_MENU_SEPARATOR:
		// Рисуем линию-разделитель
		FillRGBABlend(
			x,
			y + (m_iElementHeight / 2),
			m_iRightPanelWidth - (m_iPadding * 2),
			1,
			m_iColorBorder[0],
			m_iColorBorder[1],
			m_iColorBorder[2],
			m_iColorBorder[3]
		);
		break;
	}
}

void CHudCheatMenuVGUI::DrawCheckbox(int x, int y, const char* name, bool checked, bool selected)
{
	// Цвет текста
	int r = selected ? m_iColorTextSelected[0] : m_iColorText[0];
	int g = selected ? m_iColorTextSelected[1] : m_iColorText[1];
	int b = selected ? m_iColorTextSelected[2] : m_iColorText[2];
	
	// Компактный чекбокс (меньше)
	int checkboxSize = YRES(10);
	int checkboxX = x;
	int checkboxY = y + (m_iElementHeight / 2) - (checkboxSize / 2);
	
	// Рамка чекбокса (белая если выбран, серая если нет)
	int borderR = selected ? 255 : 120;
	int borderG = selected ? 255 : 120;
	int borderB = selected ? 255 : 120;
	
	// Рамка
	FillRGBABlend(checkboxX, checkboxY, checkboxSize, 1, borderR, borderG, borderB, 255);
	FillRGBABlend(checkboxX, checkboxY + checkboxSize - 1, checkboxSize, 1, borderR, borderG, borderB, 255);
	FillRGBABlend(checkboxX, checkboxY, 1, checkboxSize, borderR, borderG, borderB, 255);
	FillRGBABlend(checkboxX + checkboxSize - 1, checkboxY, 1, checkboxSize, borderR, borderG, borderB, 255);
	
	// Галочка если включено (белая)
	if (checked)
	{
		int checkSize = checkboxSize - 4;
		int checkX = checkboxX + 2;
		int checkY = checkboxY + 2;
		// Простая галочка
		for (int i = 0; i < checkSize / 2; i++)
		{
			FillRGBABlend(checkX + i, checkY + checkSize / 2 + i, 1, 1, 255, 255, 255, 255);
		}
		for (int i = 0; i < checkSize / 2; i++)
		{
			FillRGBABlend(checkX + checkSize / 2 + i, checkY + checkSize - i - 1, 1, 1, 255, 255, 255, 255);
		}
	}
	
	// Текст (ограничиваем длину, чтобы не выходил за границы)
	char text[128];
	char shortName[64];
	strncpy(shortName, name, sizeof(shortName) - 1);
	shortName[sizeof(shortName) - 1] = 0;
	// Обрезаем длинное название
	int maxNameLength = (m_iMenuWidth - (checkboxX + checkboxSize + XRES(16))) / XRES(6);
	if (strlen(shortName) > maxNameLength)
	{
		shortName[maxNameLength - 3] = '.';
		shortName[maxNameLength - 2] = '.';
		shortName[maxNameLength - 1] = '.';
		shortName[maxNameLength] = 0;
	}
	sprintf(text, "%s", shortName);
	DrawUtils::DrawHudString(
		checkboxX + checkboxSize + XRES(6),
		y + (m_iElementHeight / 2) - YRES(5),
		ScreenWidth,
		text,
		r,
		g,
		b
	);
}

void CHudCheatMenuVGUI::DrawSlider(int x, int y, const char* name, float value, float min, float max, bool selected)
{
	// Цвет текста
	int r = selected ? m_iColorTextSelected[0] : m_iColorText[0];
	int g = selected ? m_iColorTextSelected[1] : m_iColorText[1];
	int b = selected ? m_iColorTextSelected[2] : m_iColorText[2];
	
	// Компактный слайдер - название и значение в одной строке
	// Ограничиваем длину текста, чтобы не выходил за границы
	char text[128];
	char shortName[32];
	strncpy(shortName, name, sizeof(shortName) - 1);
	shortName[sizeof(shortName) - 1] = 0;
	// Обрезаем длинное название
	if (strlen(shortName) > 20)
	{
		shortName[17] = '.';
		shortName[18] = '.';
		shortName[19] = '.';
		shortName[20] = 0;
	}
	sprintf(text, "%s: %.1f", shortName, value);
	
	// Вычисляем максимальную ширину текста
	int maxTextWidth = m_iMenuWidth - (m_iPadding * 4) - XRES(20);
	DrawUtils::DrawHudString(
		x,
		y + (m_iElementHeight / 2) - YRES(5),
		ScreenWidth,
		text,
		r,
		g,
		b
	);
	
	// Слайдер (компактная полоса) - под текстом
	int sliderWidth = m_iMenuWidth - (m_iPadding * 4);
	int sliderHeight = YRES(4);
	int sliderX = x;
	int sliderY = y + m_iElementHeight - YRES(5);
	
	// Фон слайдера
	FillRGBABlend(
		sliderX,
		sliderY,
		sliderWidth,
		sliderHeight,
		30,
		30,
		35,
		255
	);
	
	// Значение слайдера
	float normalizedValue = (value - min) / (max - min);
	if (normalizedValue < 0.0f) normalizedValue = 0.0f;
	if (normalizedValue > 1.0f) normalizedValue = 1.0f;
	int valueWidth = (int)(sliderWidth * normalizedValue);
	
	// Заливка значения (белая)
	if (valueWidth > 0)
	{
		FillRGBABlend(
			sliderX,
			sliderY,
			valueWidth,
			sliderHeight,
			255,
			255,
			255,
			255
		);
	}
	
	// Рамка слайдера (серая)
	FillRGBABlend(sliderX, sliderY, sliderWidth, 1, 80, 80, 80, 255);
	FillRGBABlend(sliderX, sliderY + sliderHeight - 1, sliderWidth, 1, 80, 80, 80, 255);
	FillRGBABlend(sliderX, sliderY, 1, sliderHeight, 80, 80, 80, 255);
	FillRGBABlend(sliderX + sliderWidth - 1, sliderY, 1, sliderHeight, 80, 80, 80, 255);
}

void CHudCheatMenuVGUI::DrawButton(int x, int y, const char* name, bool selected)
{
	// Цвет текста
	int r = selected ? m_iColorTextSelected[0] : m_iColorText[0];
	int g = selected ? m_iColorTextSelected[1] : m_iColorText[1];
	int b = selected ? m_iColorTextSelected[2] : m_iColorText[2];
	
	// Ограничиваем длину текста кнопки
	char shortName[64];
	strncpy(shortName, name, sizeof(shortName) - 1);
	shortName[sizeof(shortName) - 1] = 0;
	int maxNameLength = (m_iMenuWidth - (m_iPadding * 4)) / XRES(6);
	if (strlen(shortName) > maxNameLength)
	{
		shortName[maxNameLength - 3] = '.';
		shortName[maxNameLength - 2] = '.';
		shortName[maxNameLength - 1] = '.';
		shortName[maxNameLength] = 0;
	}
	
	// Текст кнопки
	DrawUtils::DrawHudString(
		x,
		y + (m_iElementHeight / 2) - YRES(5),
		ScreenWidth,
		shortName,
		r,
		g,
		b
	);
	
	// Подчеркивание для кнопки (белая линия)
	if (selected)
	{
		int textWidth = strlen(shortName) * XRES(6);
		FillRGBABlend(
			x,
			y + m_iElementHeight - YRES(3),
			textWidth,
			1,
			255,
			255,
			255,
			255
		);
	}
}

void CHudCheatMenuVGUI::DrawLabel(int x, int y, const char* text)
{
	DrawUtils::DrawHudString(
		x,
		y + (m_iElementHeight / 2) - YRES(6),
		ScreenWidth,
		text,
		m_iColorText[0],
		m_iColorText[1],
		m_iColorText[2]
	);
}

void CHudCheatMenuVGUI::DrawCursor()
{
	if (!m_bMenuVisible)
		return;
	
	// Минималистичный курсор (белый крестик)
	int cursorSize = XRES(8);
	int halfSize = cursorSize / 2;
	
	// Горизонтальная линия
	FillRGBABlend(
		m_iMouseX - halfSize,
		m_iMouseY - 1,
		cursorSize,
		2,
		255,
		255,
		255,
		255
	);
	
	// Вертикальная линия
	FillRGBABlend(
		m_iMouseX - 1,
		m_iMouseY - halfSize,
		2,
		cursorSize,
		255,
		255,
		255,
		255
	);
}

void CHudCheatMenuVGUI::UpdateSelectedElementFromMouse()
{
	if (m_iCurrentTab < 0 || m_iCurrentTab >= m_iTabCount)
		return;
	
	// НЕ переключаем вкладки при наведении - только при клике
	// Обновляем только выбранный элемент внутри текущей вкладки
	
	// Проверяем наведение по настройкам
	CheatMenuTab& tab = m_Tabs[m_iCurrentTab];
	int contentY = m_iMenuY + m_iTabHeight + m_iPadding;
	int contentX = m_iMenuX + m_iPadding;
	int contentHeight = m_iMenuHeight - m_iTabHeight - (m_iPadding * 2);
	int maxVisibleElements = contentHeight / m_iElementHeight;
	
	// Проверяем, что курсор находится в области контента (не в области вкладок)
	if (m_iMouseY >= contentY && m_iMouseY < contentY + contentHeight && 
	    m_iMouseX >= contentX && m_iMouseX < m_iMenuX + m_iMenuWidth - m_iPadding)
	{
		// Вычисляем индекс элемента на основе позиции Y
		// Используем текущий скроллинг для определения видимых элементов
		int relativeY = m_iMouseY - contentY;
		int elementIndex = m_iScrollOffset + (relativeY / m_iElementHeight);
		
		// Проверяем, что индекс находится в допустимых пределах
		if (elementIndex >= 0 && elementIndex < tab.elementCount)
		{
			m_iSelectedElement = elementIndex;
			// Скроллинг будет обновлен автоматически в DrawTabContent в следующем кадре
			return;
		}
	}
}

void CHudCheatMenuVGUI::UpdateElementValues()
{
	if (m_iCurrentTab < 0 || m_iCurrentTab >= m_iTabCount)
		return;
	
	CheatMenuTab& tab = m_Tabs[m_iCurrentTab];
	for (int i = 0; i < tab.elementCount; i++)
	{
		UpdateElementValue(&tab.elements[i]);
	}
}

void CHudCheatMenuVGUI::UpdateElementValue(CheatMenuElement* element)
{
	if (!element)
		return;
	
	// Получаем cvar по имени
	cvar_t* pCvar = gEngfuncs.pfnGetCvarPointer(element->cvarName);
	if (!pCvar)
		return;
	
	if (element->type == CHEAT_MENU_CHECKBOX)
	{
		element->enabled = (pCvar->value != 0.0f);
		element->value = pCvar->value;
	}
	else if (element->type == CHEAT_MENU_SLIDER)
	{
		element->value = pCvar->value;
		// Ограничиваем значение
		if (element->value < element->minValue)
			element->value = element->minValue;
		if (element->value > element->maxValue)
			element->value = element->maxValue;
	}
}

void CHudCheatMenuVGUI::ApplyElementValue(CheatMenuElement* element)
{
	if (!element)
		return;
	
	char cmd[128];
	
	if (element->type == CHEAT_MENU_CHECKBOX)
	{
		sprintf(cmd, "%s %s\n", element->cvarName, element->enabled ? "1" : "0");
		ClientCmd(cmd);
	}
	else if (element->type == CHEAT_MENU_SLIDER)
	{
		sprintf(cmd, "%s %.2f\n", element->cvarName, element->value);
		ClientCmd(cmd);
	}
	else if (element->type == CHEAT_MENU_BUTTON)
	{
		sprintf(cmd, "%s\n", element->command);
		ClientCmd(cmd);
	}
}

void CHudCheatMenuVGUI::HandleKeyInput(int key)
{
	if (!m_bMenuVisible)
		return;
	
	if (m_iCurrentTab < 0 || m_iCurrentTab >= m_iTabCount)
		return;
	
	CheatMenuTab& tab = m_Tabs[m_iCurrentTab];
	
	switch (key)
	{
	case K_ESCAPE:
	case K_TAB:
		HideMenu();
		break;
	case K_UPARROW:
		PrevElement();
		break;
	case K_DOWNARROW:
		NextElement();
		break;
	case K_LEFTARROW:
		if (m_iSelectedElement >= 0 && m_iSelectedElement < tab.elementCount)
		{
			CheatMenuElement& element = tab.elements[m_iSelectedElement];
			if (element.type == CHEAT_MENU_SLIDER)
			{
				element.value -= element.stepValue;
				if (element.value < element.minValue)
					element.value = element.minValue;
				ApplyElementValue(&element);
			}
		}
		break;
	case K_RIGHTARROW:
		if (m_iSelectedElement >= 0 && m_iSelectedElement < tab.elementCount)
		{
			CheatMenuElement& element = tab.elements[m_iSelectedElement];
			if (element.type == CHEAT_MENU_SLIDER)
			{
				element.value += element.stepValue;
				if (element.value > element.maxValue)
					element.value = element.maxValue;
				ApplyElementValue(&element);
			}
		}
		break;
	case K_ENTER:
	case K_SPACE:
		ToggleElement();
		break;
	case 91: // '[' key
	case 44: // ',' key
		PrevTab();
		break;
	case 93: // ']' key
	case 46: // '.' key
		NextTab();
		break;
	}
}

void CHudCheatMenuVGUI::HandleMouseInput(int x, int y)
{
	if (!m_bMenuVisible)
		return;
	
	// Обновляем позицию курсора
	m_iMouseX += x;
	m_iMouseY += y;
	
	// Ограничиваем курсор в пределах экрана
	if (m_iMouseX < 0) m_iMouseX = 0;
	if (m_iMouseX >= ScreenWidth) m_iMouseX = ScreenWidth - 1;
	if (m_iMouseY < 0) m_iMouseY = 0;
	if (m_iMouseY >= ScreenHeight) m_iMouseY = ScreenHeight - 1;
	
	// Обновляем выбранный элемент на основе позиции курсора (но не переключаем вкладки)
	UpdateSelectedElementFromMouse();
}

void CHudCheatMenuVGUI::HandleMouseClick(int x, int y)
{
	if (!m_bMenuVisible)
		return;
	
	// Проверяем клик по вкладкам (горизонтальные вверху) - ТОЛЬКО при клике ЛКМ
	if (m_iMouseY >= m_iMenuY && m_iMouseY < m_iMenuY + m_iTabHeight)
	{
		int tabTotalWidth = m_iMenuWidth;
		int tabWidth = tabTotalWidth / m_iTabCount;
		for (int i = 0; i < m_iTabCount; i++)
		{
			int tabStartX = m_iMenuX + (i * tabWidth);
			int tabEndX = tabStartX + tabWidth;
			if (m_iMouseX >= tabStartX && m_iMouseX < tabEndX)
			{
				m_iCurrentTab = i;
				m_iSelectedElement = 0;
				m_iScrollOffset = 0;  // Сбрасываем скроллинг при переключении вкладок
				return;
			}
		}
	}
	
	// Проверяем клик по настройкам
	if (m_iCurrentTab >= 0 && m_iCurrentTab < m_iTabCount)
	{
		CheatMenuTab& tab = m_Tabs[m_iCurrentTab];
		int contentY = m_iMenuY + m_iTabHeight + m_iPadding;
		int contentX = m_iMenuX + m_iPadding;
		int contentHeight = m_iMenuHeight - m_iTabHeight - (m_iPadding * 2);
		int maxVisibleElements = contentHeight / m_iElementHeight;
		
		// Проверяем клик по видимым элементам
		if (m_iMouseY >= contentY && m_iMouseY < contentY + contentHeight && 
		    m_iMouseX >= contentX && m_iMouseX < m_iMenuX + m_iMenuWidth - m_iPadding)
		{
			// Вычисляем индекс элемента на основе позиции Y
			// Используем текущий скроллинг для определения видимых элементов
			int relativeY = m_iMouseY - contentY;
			int elementIndex = m_iScrollOffset + (relativeY / m_iElementHeight);
			
			// Проверяем, что индекс находится в допустимых пределах
			if (elementIndex >= 0 && elementIndex < tab.elementCount)
			{
				m_iSelectedElement = elementIndex;
				CheatMenuElement& element = tab.elements[elementIndex];
				
				// Для слайдеров - изменяем значение на основе позиции X
				if (element.type == CHEAT_MENU_SLIDER)
				{
					int sliderWidth = m_iMenuWidth - (m_iPadding * 4);
					int sliderX = contentX;
					int relativeX = m_iMouseX - sliderX;
					
					if (relativeX >= 0 && relativeX <= sliderWidth)
					{
						float normalizedValue = (float)relativeX / (float)sliderWidth;
						if (normalizedValue < 0.0f) normalizedValue = 0.0f;
						if (normalizedValue > 1.0f) normalizedValue = 1.0f;
						
						element.value = element.minValue + (normalizedValue * (element.maxValue - element.minValue));
						// Округляем до шага
						element.value = (float)((int)((element.value / element.stepValue) + 0.5f)) * element.stepValue;
						if (element.value < element.minValue) element.value = element.minValue;
						if (element.value > element.maxValue) element.value = element.maxValue;
						
						ApplyElementValue(&element);
					}
				}
				else
				{
					// Для чекбоксов и кнопок - переключаем
					ToggleElement();
				}
				return;
			}
		}
	}
}

void CHudCheatMenuVGUI::NextTab()
{
	m_iCurrentTab++;
	if (m_iCurrentTab >= m_iTabCount)
		m_iCurrentTab = 0;
	m_iSelectedElement = 0;
	m_iScrollOffset = 0;  // Сбрасываем скроллинг при переключении вкладок
}

void CHudCheatMenuVGUI::PrevTab()
{
	m_iCurrentTab--;
	if (m_iCurrentTab < 0)
		m_iCurrentTab = m_iTabCount - 1;
	m_iSelectedElement = 0;
	m_iScrollOffset = 0;  // Сбрасываем скроллинг при переключении вкладок
}

void CHudCheatMenuVGUI::NextElement()
{
	if (m_iCurrentTab < 0 || m_iCurrentTab >= m_iTabCount)
		return;
	
	CheatMenuTab& tab = m_Tabs[m_iCurrentTab];
	m_iSelectedElement++;
	if (m_iSelectedElement >= tab.elementCount)
		m_iSelectedElement = 0;
}

void CHudCheatMenuVGUI::PrevElement()
{
	if (m_iCurrentTab < 0 || m_iCurrentTab >= m_iTabCount)
		return;
	
	CheatMenuTab& tab = m_Tabs[m_iCurrentTab];
	m_iSelectedElement--;
	if (m_iSelectedElement < 0)
		m_iSelectedElement = tab.elementCount - 1;
}

void CHudCheatMenuVGUI::ToggleElement()
{
	if (m_iCurrentTab < 0 || m_iCurrentTab >= m_iTabCount)
		return;
	
	CheatMenuTab& tab = m_Tabs[m_iCurrentTab];
	if (m_iSelectedElement < 0 || m_iSelectedElement >= tab.elementCount)
		return;
	
	CheatMenuElement& element = tab.elements[m_iSelectedElement];
	
	if (element.type == CHEAT_MENU_CHECKBOX)
	{
		element.enabled = !element.enabled;
		ApplyElementValue(&element);
	}
	else if (element.type == CHEAT_MENU_BUTTON)
	{
		ApplyElementValue(&element);
	}
}

void CHudCheatMenuVGUI::IncreaseValue()
{
	if (m_iCurrentTab < 0 || m_iCurrentTab >= m_iTabCount)
		return;
	
	CheatMenuTab& tab = m_Tabs[m_iCurrentTab];
	if (m_iSelectedElement < 0 || m_iSelectedElement >= tab.elementCount)
		return;
	
	CheatMenuElement& element = tab.elements[m_iSelectedElement];
	if (element.type == CHEAT_MENU_SLIDER)
	{
		element.value += element.stepValue;
		if (element.value > element.maxValue)
			element.value = element.maxValue;
		ApplyElementValue(&element);
	}
}

void CHudCheatMenuVGUI::DecreaseValue()
{
	if (m_iCurrentTab < 0 || m_iCurrentTab >= m_iTabCount)
		return;
	
	CheatMenuTab& tab = m_Tabs[m_iCurrentTab];
	if (m_iSelectedElement < 0 || m_iSelectedElement >= tab.elementCount)
		return;
	
	CheatMenuElement& element = tab.elements[m_iSelectedElement];
	if (element.type == CHEAT_MENU_SLIDER)
	{
		element.value -= element.stepValue;
		if (element.value < element.minValue)
			element.value = element.minValue;
		ApplyElementValue(&element);
	}
}

