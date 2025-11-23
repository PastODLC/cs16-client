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
// hud_playerlist.cpp
//
// Player list implementation
//
#include "hud.h"
#include "cl_util.h"
#include "hud_playerlist.h"
#include "draw_util.h"
#include "cl_entity.h"
#include "com_model.h"
#include "r_studioint.h"
#include <math.h>
#include <string.h>
#include <ctype.h>

int CHudPlayerList::Init()
{
	m_iFlags = HUD_ACTIVE;
	m_pCvarPlayerList = gEngfuncs.pfnRegisterVariable("ebash3d_playerlist", "0", FCVAR_ARCHIVE);
	
	gHUD.AddHudElem(this);
	return 1;
}

int CHudPlayerList::VidInit()
{
	return 1;
}

void CHudPlayerList::Reset()
{
}

const char* CHudPlayerList::GetPlayerWeaponName(int playerIndex)
{
	cl_entity_t *ent = gEngfuncs.GetEntityByIndex(playerIndex);
	if (!ent || !ent->player)
		return "NONE";
	
	if (ent->curstate.weaponmodel <= 0)
		return "NONE";
	
	extern engine_studio_api_t IEngineStudio;
	model_t *pWeaponModel = IEngineStudio.GetModelByIndex(ent->curstate.weaponmodel);
	if (!pWeaponModel || !pWeaponModel->name)
		return "NONE";
	
	const char *weaponModelName = pWeaponModel->name;
	static char weaponName[64] = {0};
	
	// Извлекаем название оружия из пути модели
	const char *lastSlash = strrchr(weaponModelName, '/');
	const char *lastBackslash = strrchr(weaponModelName, '\\');
	const char *start = (lastSlash > lastBackslash) ? lastSlash + 1 : (lastBackslash ? lastBackslash + 1 : weaponModelName);
	
	// Копируем название
	strncpy(weaponName, start, sizeof(weaponName) - 1);
	weaponName[sizeof(weaponName) - 1] = 0;
	
	// Убираем расширение
	char *dot = strrchr(weaponName, '.');
	if (dot)
		*dot = 0;
	
	// Убираем префиксы типа "w_" или "v_"
	if (weaponName[0] == 'w' && weaponName[1] == '_')
		memmove(weaponName, weaponName + 2, strlen(weaponName) - 1);
	else if (weaponName[0] == 'v' && weaponName[1] == '_')
		memmove(weaponName, weaponName + 2, strlen(weaponName) - 1);
	
	// Делаем заглавными первые буквы
	if (weaponName[0] >= 'a' && weaponName[0] <= 'z')
		weaponName[0] = toupper(weaponName[0]);
	
	return weaponName;
}

float CHudPlayerList::GetPlayerDistance(int playerIndex)
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if (!localPlayer)
		return 0.0f;
	
	cl_entity_t *ent = gEngfuncs.GetEntityByIndex(playerIndex);
	if (!ent || !ent->player)
		return 0.0f;
	
	// Используем кэш позиций для обхода блокировки
	extern float g_PlayerPositionCache[];
	extern bool g_PlayerPositionCacheValid[];
	
	vec3_t playerPos;
	if (g_PlayerPositionCacheValid[playerIndex])
	{
		playerPos[0] = g_PlayerPositionCache[playerIndex * 3 + 0];
		playerPos[1] = g_PlayerPositionCache[playerIndex * 3 + 1];
		playerPos[2] = g_PlayerPositionCache[playerIndex * 3 + 2];
	}
	else
	{
		VectorCopy(ent->origin, playerPos);
	}
	
	vec3_t localPos;
	VectorCopy(localPlayer->origin, localPos);
	
	vec3_t delta;
	VectorSubtract(playerPos, localPos, delta);
	
	return sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
}

int CHudPlayerList::Draw(float flTime)
{
	if (!m_pCvarPlayerList || m_pCvarPlayerList->value == 0.0f)
		return 0;
	
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if (!localPlayer)
		return 0;
	
	// Собираем список игроков
	int playerCount = 0;
	struct PlayerListEntry {
		int index;
		char name[32];
		char weapon[32];
		float distance;
		int team;
	} players[16];
	
	for (int i = 1; i <= MAX_PLAYERS && playerCount < 15; i++)
	{
		cl_entity_t *ent = gEngfuncs.GetEntityByIndex(i);
		if (!ent || !ent->player)
			continue;
		
		// Пропускаем локального игрока
		if (i == localPlayer->index)
			continue;
		
		// Пропускаем мертвых и спектаторов
		if (g_PlayerExtraInfo[i].dead || ent->curstate.spectator == 1)
			continue;
		
		// Получаем имя игрока
		GetPlayerInfo(i, &g_PlayerInfoList[i]);
		if (!g_PlayerInfoList[i].name || strlen(g_PlayerInfoList[i].name) == 0)
			continue;
		
		// Сохраняем информацию об игроке
		strncpy(players[playerCount].name, g_PlayerInfoList[i].name, sizeof(players[playerCount].name) - 1);
		players[playerCount].name[sizeof(players[playerCount].name) - 1] = 0;
		
		const char *weaponName = GetPlayerWeaponName(i);
		strncpy(players[playerCount].weapon, weaponName, sizeof(players[playerCount].weapon) - 1);
		players[playerCount].weapon[sizeof(players[playerCount].weapon) - 1] = 0;
		
		players[playerCount].distance = GetPlayerDistance(i);
		players[playerCount].team = g_PlayerExtraInfo[i].teamnumber;
		players[playerCount].index = i;
		playerCount++;
	}
	
	if (playerCount == 0)
		return 0;
	
	// Размеры окна (без фона, только текст) - компактный стиль
	int padding = XRES(4);
	int lineHeight = YRES(14);
	int headerHeight = YRES(16);
	int windowWidth = XRES(180);
	int windowHeight = headerHeight + (playerCount * lineHeight) + padding;
	int windowX = XRES(10);
	int windowY = YRES(30);
	
	// Заголовок (без фона)
	int headerY = windowY;
	DrawUtils::DrawHudString(windowX, headerY, ScreenWidth, "Player List", 255, 255, 255);
	
	// Рисуем список игроков (без фона, только текст)
	int contentY = windowY + headerHeight + padding;
	for (int i = 0; i < playerCount; i++)
	{
		int y = contentY + (i * lineHeight);
		
		// Формируем строку: "Name | Weapon | Distance"
		char line[128];
		sprintf(line, "%s | %s | %.0fm", players[i].name, players[i].weapon, players[i].distance);
		
		// Цвет в зависимости от команды
		int r = 220, g = 220, b = 220;
		if (players[i].team == 1) // T
		{
			r = 255; g = 100; b = 100;
		}
		else if (players[i].team == 2) // CT
		{
			r = 100; g = 150; b = 255;
		}
		
		// Рисуем строку (без фона)
		DrawUtils::DrawHudString(windowX, y, ScreenWidth, line, r, g, b);
	}
	
	return 1;
}

