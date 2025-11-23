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
// hud_cornerbox.h
//
// Corner box ESP for players
//
#pragma once

#include "hud.h"

class CHudCornerBox : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );
	
	static bool IsEnabled( void ) { return m_bEnabled; }
	static void SetEnabled( bool enabled ) { m_bEnabled = enabled; }

private:
	static bool m_bEnabled;
	cvar_t *m_pCvarESP;
	cvar_t *m_pCvarDeathmatch;
	
	// Цвета ESP
	cvar_t *m_pCvarESPBoxR;
	cvar_t *m_pCvarESPBoxG;
	cvar_t *m_pCvarESPBoxB;
	cvar_t *m_pCvarESPLineR;
	cvar_t *m_pCvarESPLineG;
	cvar_t *m_pCvarESPLineB;
	cvar_t *m_pCvarESPNameR;
	cvar_t *m_pCvarESPNameG;
	cvar_t *m_pCvarESPNameB;
	cvar_t *m_pCvarESPWeaponR;
	cvar_t *m_pCvarESPWeaponG;
	cvar_t *m_pCvarESPWeaponB;
};

