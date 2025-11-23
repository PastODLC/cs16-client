#pragma once
#if !defined(HUD_CORNERBOX_H)
#define HUD_CORNERBOX_H

#include "hud.h"

class CHudCornerBox : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );

	// Глобальная переменная для включения/выключения через ImGui
	static bool m_bEnabled;
	
	// Функция для доступа из ImGui
	static bool IsEnabled() { return m_bEnabled; }
	static void SetEnabled(bool enabled) { m_bEnabled = enabled; }
};

#endif

