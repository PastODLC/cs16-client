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
// hud_debug.h
//
// Debug HUD implementation
//
#pragma once

#include "hud.h"

// Структура для хранения информации об уроне
struct DamageInfo
{
	char attackerName[32];
	int attackerIndex;
	float damage;
	float time;
	bool isReceived; // true = получен урон, false = нанесен урон
};

// Структура для дебаг информации о silent aim
struct SilentAimDebugInfo
{
	bool isActive;
	bool hasTarget;
	int targetIndex;
	char targetName[32];
	float targetDistance;
	float aimAngles[3];
	float viewAngles[3];
	float punchAngles[3];
};

// Структура для дебаг информации о ESP
struct ESPDebugInfo
{
	bool isActive;
	int targetCount;
	int lastFoundTarget;
	char lastFoundTargetName[32];
	float lastFoundTime;
};

class CHudDebug : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );
	void Think( void );
	
	// Регистрация урона
	void RegisterDamageReceived( int attackerIndex, float damage );
	void RegisterDamageDealt( int victimIndex, float damage );
	
	// Обновление дебаг информации о silent aim
	void UpdateSilentAimDebug( bool isActive, bool hasTarget, int targetIndex, 
	                          float targetDistance, float* aimAngles, 
	                          float* viewAngles, float* punchAngles );
	
	// Обновление дебаг информации о ESP
	void UpdateESPDebug( bool isActive, int targetCount, int lastFoundTarget, 
	                    const char* targetName );
	
	// Получение FPS
	float GetFPS( void );
	
	// Получение Ping
	int GetPing( void );
	
	// Получение Packet Loss
	int GetPacketLoss( void );

private:
	// Cvar для включения/выключения дебага
	cvar_t *m_pCvarDebug;
	
	// История урона (полученного и нанесенного)
	static const int MAX_DAMAGE_HISTORY = 10;
	DamageInfo m_DamageHistory[MAX_DAMAGE_HISTORY];
	int m_iDamageHistoryCount;
	
	// Дебаг информация о silent aim
	SilentAimDebugInfo m_SilentAimDebug;
	
	// Дебаг информация о ESP
	ESPDebugInfo m_ESPDebug;
	
	// FPS расчет
	float m_flLastFPSUpdate;
	int m_iFPSFrames;
	float m_flFPS;
	
	// Отрисовка
	void DrawStats( float flTime );
	void DrawDamageHistory( float flTime );
	void DrawSilentAimDebug( float flTime );
	void DrawESPDebug( float flTime );
	
	// Вспомогательные функции
	const char* GetPlayerName( int index );
	void AddDamageToHistory( int playerIndex, float damage, bool isReceived );
};

