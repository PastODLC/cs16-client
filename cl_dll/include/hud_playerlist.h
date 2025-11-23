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
// hud_playerlist.h
//
// Player list HUD element
//
#pragma once

#include "hud.h"

class CHudPlayerList : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );
	
private:
	cvar_t *m_pCvarPlayerList;
	
	// Получить название оружия игрока
	const char* GetPlayerWeaponName(int playerIndex);
	
	// Получить дистанцию до игрока
	float GetPlayerDistance(int playerIndex);
};

