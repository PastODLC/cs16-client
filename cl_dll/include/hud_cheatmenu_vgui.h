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
// hud_cheatmenu_vgui.h
//
// VGUI Cheat Menu implementation
//
#pragma once

#include "hud.h"

// Типы элементов меню
enum CheatMenuElementType
{
	CHEAT_MENU_CHECKBOX,    // Чекбокс (вкл/выкл)
	CHEAT_MENU_SLIDER,      // Слайдер (числовое значение)
	CHEAT_MENU_BUTTON,      // Кнопка (выполнение команды)
	CHEAT_MENU_SEPARATOR,   // Разделитель
	CHEAT_MENU_LABEL        // Текстовая метка
};

// Элемент меню
struct CheatMenuElement
{
	CheatMenuElementType type;
	char name[64];              // Название элемента
	char cvarName[64];          // Имя cvar (для чекбоксов и слайдеров)
	float minValue;             // Минимальное значение (для слайдеров)
	float maxValue;             // Максимальное значение (для слайдеров)
	float stepValue;            // Шаг изменения (для слайдеров)
	char command[64];           // Команда (для кнопок)
	bool enabled;               // Включен/выключен (для чекбоксов)
	float value;                // Значение (для слайдеров)
};

// Вкладка меню
struct CheatMenuTab
{
	char name[32];              // Название вкладки
	CheatMenuElement elements[32]; // Элементы вкладки
	int elementCount;           // Количество элементов
};

class CHudCheatMenuVGUI : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );
	
	// Показать/скрыть меню
	void ShowMenu();
	void HideMenu();
	bool IsMenuOpen() { return m_bMenuVisible; }
	
	// Обработка ввода
	void HandleKeyInput(int key);
	void HandleMouseInput(int x, int y);
	void HandleMouseClick(int x, int y);
	
	// Навигация
	void NextTab();
	void PrevTab();
	void NextElement();
	void PrevElement();
	void ToggleElement();
	void IncreaseValue();
	void DecreaseValue();
	
private:
	// Инициализация меню
	void InitMenuTabs();
	void InitVisualTab();
	void InitAimbotTab();
	void InitMiscTab();
	void InitMovementTab();
	
	// Отрисовка
	void DrawMenuBackground();
	void DrawTabs();
	void DrawTabContent();
	void DrawElement(int index, int x, int y, bool selected);
	void DrawCheckbox(int x, int y, const char* name, bool checked, bool selected);
	void DrawSlider(int x, int y, const char* name, float value, float min, float max, bool selected);
	void DrawButton(int x, int y, const char* name, bool selected);
	void DrawLabel(int x, int y, const char* text);
	void DrawCursor();
	
	// Обновление выбранного элемента на основе позиции курсора
	void UpdateSelectedElementFromMouse();
	
	// Обновление значений из cvar
	void UpdateElementValues();
	void UpdateElementValue(CheatMenuElement* element);
	
	// Применение значений к cvar
	void ApplyElementValue(CheatMenuElement* element);
	
	// Состояние меню
	bool m_bMenuVisible;
	int m_iCurrentTab;
	int m_iSelectedElement;
	int m_iScrollOffset;  // Смещение для скроллинга элементов
	
	// Позиция курсора
	int m_iMouseX;
	int m_iMouseY;
	
	// Вкладки меню
	CheatMenuTab m_Tabs[4];
	int m_iTabCount;
	
	// Размеры и позиции
	int m_iMenuX;
	int m_iMenuY;
	int m_iMenuWidth;
	int m_iMenuHeight;
	int m_iLeftPanelWidth;   // Ширина левой панели (вкладки)
	int m_iRightPanelWidth;  // Ширина правой панели (настройки)
	int m_iTabHeight;
	int m_iElementHeight;
	int m_iPadding;
	
	// Цвета
	int m_iColorBackground[4];
	int m_iColorTabActive[4];
	int m_iColorTabInactive[4];
	int m_iColorText[4];
	int m_iColorTextSelected[4];
	int m_iColorBorder[4];
};

