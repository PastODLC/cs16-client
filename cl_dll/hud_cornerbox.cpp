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
// hud_cornerbox.cpp
//
// Corner box ESP implementation
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "hud_cornerbox.h"
#include "hud_debug.h"
#include "custom_utils.h"
#include "cl_entity.h"
#include "com_model.h"
#include "const.h"
#include "draw_util.h"
#include "r_studioint.h"
#include <math.h>
#include <string.h>

// Инициализация статической переменной
bool CHudCornerBox::m_bEnabled = false;

// Кэш позиций игроков для обхода блокировки ESP
// Используем простые массивы для совместимости с entity.cpp
// Формат: [playerIndex * 3 + 0] = x, [playerIndex * 3 + 1] = y, [playerIndex * 3 + 2] = z
float g_PlayerPositionCache[(MAX_PLAYERS + 1) * 3]; // Позиции (x, y, z для каждого игрока)
float g_PlayerPositionCacheTime[MAX_PLAYERS + 1];   // Время последнего обновления
bool g_PlayerPositionCacheValid[MAX_PLAYERS + 1];   // Валидность кэша

int CHudCornerBox::Init()
{
	m_iFlags = HUD_ACTIVE;
	m_pCvarESP = gEngfuncs.pfnRegisterVariable( "ebash3d_esp", "0", FCVAR_ARCHIVE );
	m_pCvarDeathmatch = gEngfuncs.pfnRegisterVariable( "ebash3d_deathmatch", "0", FCVAR_ARCHIVE );
	
	// Цвета ESP Box (по умолчанию красный)
	m_pCvarESPBoxR = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_box_r", "255", FCVAR_ARCHIVE );
	m_pCvarESPBoxG = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_box_g", "0", FCVAR_ARCHIVE );
	m_pCvarESPBoxB = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_box_b", "0", FCVAR_ARCHIVE );
	
	// Цвета ESP Line (по умолчанию красный)
	m_pCvarESPLineR = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_line_r", "255", FCVAR_ARCHIVE );
	m_pCvarESPLineG = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_line_g", "0", FCVAR_ARCHIVE );
	m_pCvarESPLineB = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_line_b", "0", FCVAR_ARCHIVE );
	
	// Цвета ESP Name (по умолчанию красный)
	m_pCvarESPNameR = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_name_r", "255", FCVAR_ARCHIVE );
	m_pCvarESPNameG = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_name_g", "0", FCVAR_ARCHIVE );
	m_pCvarESPNameB = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_name_b", "0", FCVAR_ARCHIVE );
	
	// Цвета ESP Weapon (по умолчанию красный)
	m_pCvarESPWeaponR = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_weapon_r", "255", FCVAR_ARCHIVE );
	m_pCvarESPWeaponG = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_weapon_g", "0", FCVAR_ARCHIVE );
	m_pCvarESPWeaponB = gEngfuncs.pfnRegisterVariable( "ebash3d_esp_weapon_b", "0", FCVAR_ARCHIVE );
	
	// ОЧИСТКА ESP КЭША ПРИ ИНИЦИАЛИЗАЦИИ
	// ИСПРАВЛЕНИЕ: Очищаем все позиции и делаем кэш невалидным
	// Это предотвращает остатки ESP при запуске
	for (int i = 0; i <= MAX_PLAYERS; i++)
	{
		g_PlayerPositionCache[i * 3 + 0] = 0.0f;
		g_PlayerPositionCache[i * 3 + 1] = 0.0f;
		g_PlayerPositionCache[i * 3 + 2] = 0.0f;
		g_PlayerPositionCacheTime[i] = 0.0f;
		g_PlayerPositionCacheValid[i] = false; // Невалиден по умолчанию - никаких остатков!
	}
	
	gHUD.AddHudElem(this);
	return 0;
}

int CHudCornerBox::VidInit()
{
	return 1;
}

void CHudCornerBox::Reset()
{
	m_bEnabled = false;
}

int CHudCornerBox::Draw(float flTime)
{
	// Update enabled state from cvar
	if ( m_pCvarESP )
		m_bEnabled = ( m_pCvarESP->value != 0.0f );
	
	if (!m_bEnabled)
	{
		// ДЕБАГ: Обновляем информацию о ESP (неактивен)
		gHUD.m_Debug.UpdateESPDebug( false, 0, 0, NULL );
		return 0;
	}

	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if (!localPlayer)
		return 0;

	int targetCount = 0;
	int lastFoundTarget = 0;
	const char* lastFoundTargetName = NULL;

	// Проходим по всем игрокам
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		cl_entity_t *ent = gEngfuncs.GetEntityByIndex(i);
		if (!ent)
			continue;

		// Проверяем, что это игрок
		if (!CustomUtils::CheckForPlayer(ent))
			continue;

		// Пропускаем локального игрока
		if (ent->index == localPlayer->index)
			continue;

		// Проверяем, что это действительно активный игрок (не остаток хитбокса)
		if ( !ent->player )
			continue;
		
		// КРИТИЧЕСКАЯ ПРОВЕРКА #0: Проверяем кэш позиции
		// Если кэш невалиден (инвалидирован при смерти), пропускаем игрока
		// ЭТО САМАЯ НАДЕЖНАЯ ПРОВЕРКА - кэш инвалидируется в DeathMsg и ScoreAttrib
		if ( !g_PlayerPositionCacheValid[i] )
			continue; // Кэш невалиден - игрок мертв
		
		// КРИТИЧЕСКАЯ ПРОВЕРКА #1: Пропускаем мертвых игров через поле dead в g_PlayerExtraInfo
		// Это дополнительная проверка - сервер явно устанавливает это поле через флаг PLAYER_DEAD
		if ( g_PlayerExtraInfo[ent->index].dead )
			continue; // Игрок мертв - пропускаем
		
		// КРИТИЧЕСКАЯ ПРОВЕРКА #2: Проверяем spectator флаг
		// Спектаторы не являются живыми игроками
		// ВАЖНО: проверяем только если spectator == true (1), не блокируем если 0 или не инициализировано
		if ( ent->curstate.spectator == 1 )
			continue;
		
		// ВАЖНО: НЕ проверяем здоровье из entity state, так как оно может быть не инициализировано
		// для живых игроков на серверах. Проверка g_PlayerExtraInfo[index].dead + инвалидация
		// кэша при смерти (в DeathMsg) достаточна для корректной работы ESP
		
		// ОБХОД БЛОКИРОВКИ ESP: Проверяем solid, но не пропускаем сразу
		// Плагин устанавливает SOLID_NOT для блокировки ESP, но мы можем использовать альтернативные источники позиций
		bool isBlockedByPlugin = (ent->curstate.solid == SOLID_NOT);
		bool useCachedPosition = false;
		bool usePrevState = false;
		bool useBaseline = false;
		
		// Если игрок заблокирован плагином, пытаемся использовать альтернативные источники
		if (isBlockedByPlugin)
		{
			// ВАЖНО: Плагин устанавливает SOLID_NOT на сервере, но мы можем обойти это несколькими способами:
			
			// Способ 1: Используем кэшированную позицию из HUD_ProcessPlayerState (самый надежный метод)
			// Кэш обновляется ДО обработки плагином, поэтому содержит актуальные позиции
			// ИСПРАВЛЕНИЕ: Уменьшено время валидности кэша с 5 до 2 секунд для предотвращения
			// отображения мертвых игроков на серверах (кэш инвалидируется при смерти)
			if (g_PlayerPositionCacheValid[i] && 
			    (gEngfuncs.GetClientTime() - g_PlayerPositionCacheTime[i]) < 2.0f) // Кэш актуален до 2 секунд
			{
				useCachedPosition = true;
			}
			// Способ 2: Используем prevstate (предыдущее состояние до блокировки)
			// prevstate содержит данные из предыдущего сетевого пакета, до блокировки
			else if (ent->prevstate.solid != SOLID_NOT && 
			         !(ent->prevstate.origin[0] == 0.0f && ent->prevstate.origin[1] == 0.0f && ent->prevstate.origin[2] == 0.0f))
			{
				usePrevState = true;
			}
			// Способ 3: Используем baseline (исходное состояние при создании сущности)
			// baseline содержит начальное состояние сущности, которое не изменяется плагином
			else if (ent->baseline.solid != SOLID_NOT &&
			         !(ent->baseline.origin[0] == 0.0f && ent->baseline.origin[1] == 0.0f && ent->baseline.origin[2] == 0.0f))
			{
				useBaseline = true;
			}
			// Способ 4: Используем текущую позицию даже если она заблокирована
			// Плагин блокирует только solid, но origin может оставаться валидным
			// Это работает, потому что плагин не всегда обнуляет origin
			// Если текущая позиция валидна, мы используем ее в основном блоке ниже
			// Если нет альтернативных источников и текущая позиция невалидна, пропускаем этого игрока
			else if (ent->origin[0] == 0.0f && ent->origin[1] == 0.0f && ent->origin[2] == 0.0f)
			{
				// Все источники исчерпаны и текущая позиция невалидна - пропускаем
				continue;
			}
		}
		else
		{
			// Игрок не заблокирован - обновляем кэш для будущего использования
			if (!(ent->origin[0] == 0.0f && ent->origin[1] == 0.0f && ent->origin[2] == 0.0f))
			{
				g_PlayerPositionCache[i * 3 + 0] = ent->origin[0];
				g_PlayerPositionCache[i * 3 + 1] = ent->origin[1];
				g_PlayerPositionCache[i * 3 + 2] = ent->origin[2];
				g_PlayerPositionCacheTime[i] = gEngfuncs.GetClientTime();
				g_PlayerPositionCacheValid[i] = true;
			}
		}
		
		// НЕ проверяем модель, так как это может блокировать живых игроков
		// если модель еще не загружена
		
		// Дополнительная проверка: если индекс выходит за пределы, это не игрок
		if ( ent->index < 1 || ent->index > MAX_PLAYERS )
			continue;
		
		// Проверяем режим deathmatch
		// Если deathmatch выключен, пропускаем тимейтов
		if ( m_pCvarDeathmatch && m_pCvarDeathmatch->value == 0.0f )
		{
			if ( CustomUtils::ArePlayersTeammates( localPlayer->index, ent->index ) )
				continue; // Пропускаем тимейтов
		}

		// Получаем позицию игрока (используем альтернативные источники при блокировке)
		vec3_t origin;
		bool originSet = false;
		
		if (usePrevState)
		{
			// Используем позицию из prevstate (до блокировки)
			VectorCopy(ent->prevstate.origin, origin);
			originSet = true;
		}
		else if (useBaseline)
		{
			// Используем позицию из baseline (исходное состояние)
			VectorCopy(ent->baseline.origin, origin);
			originSet = true;
		}
		else if (useCachedPosition)
		{
			// Используем кэшированную позицию из массива
			origin[0] = g_PlayerPositionCache[i * 3 + 0];
			origin[1] = g_PlayerPositionCache[i * 3 + 1];
			origin[2] = g_PlayerPositionCache[i * 3 + 2];
			originSet = true;
		}
		else if (isBlockedByPlugin && ent->player && ent->current_position >= 0)
		{
			// Способ 5: Используем историю позиций (position_history_t ph[])
			// История позиций может содержать последние валидные позиции до блокировки
			int histIndex = (ent->current_position - 1) & HISTORY_MASK;
			if (histIndex >= 0 && histIndex < HISTORY_MAX)
			{
				position_history_t *ph = &ent->ph[histIndex];
				if (ph && !(ph->origin[0] == 0.0f && ph->origin[1] == 0.0f && ph->origin[2] == 0.0f))
				{
					// Используем позицию из истории
					VectorCopy(ph->origin, origin);
					originSet = true;
				}
			}
		}
		
		if (!originSet)
		{
			// Используем текущую позицию (не заблокирована или используем заблокированную)
			VectorCopy(ent->origin, origin);
		}
		
		// Проверяем, что позиция валидна (остатки хитбоксов часто имеют нулевую или невалидную позицию)
		if ( origin[0] == 0.0f && origin[1] == 0.0f && origin[2] == 0.0f )
			continue;
		
		// Верхняя точка (голова)
		vec3_t topPoint;
		VectorCopy(origin, topPoint);
		topPoint[2] += 36.0f;
		
		// Нижняя точка (ноги)
		vec3_t bottomPoint;
		VectorCopy(origin, bottomPoint);
		bottomPoint[2] -= 36.0f;
		
		// Конвертируем в экранные координаты
		float vecScreenTop[3];
		float vecScreenBottom[3];
		bool topValid = CustomUtils::CalcScreen(topPoint, vecScreenTop);
		bool bottomValid = CustomUtils::CalcScreen(bottomPoint, vecScreenBottom);
		
		if (!topValid || !bottomValid)
			continue;
		
		// Вычисляем размеры коробки на экране
		float screenHeight = fabsf(vecScreenBottom[1] - vecScreenTop[1]);
		float screenWidth = screenHeight * 0.4f;
		
		float centerX = (vecScreenTop[0] + vecScreenBottom[0]) * 0.5f;
		float screenX = centerX - screenWidth * 0.5f;
		float screenY = vecScreenTop[1];
		
		// Проверяем, что box видим на экране
		if (screenX + screenWidth < 0 || screenX > ScreenWidth || 
			screenY + screenHeight < 0 || screenY > ScreenHeight)
			continue;
		
		// Округляем до целых чисел
		int x = (int)(screenX + 0.5f);
		int y = (int)(screenY + 0.5f);
		int width = (int)(screenWidth + 0.5f);
		int height = (int)(screenHeight + 0.5f);
		
		// Минимальные размеры
		if (width < 8) width = 8;
		if (height < 8) height = 8;
		
		// Максимальные размеры
		if (width > ScreenWidth / 2) width = ScreenWidth / 2;
		if (height > ScreenHeight / 2) height = ScreenHeight / 2;

		// Получаем цвета из кваров
		int boxR = m_pCvarESPBoxR ? (int)m_pCvarESPBoxR->value : 255;
		int boxG = m_pCvarESPBoxG ? (int)m_pCvarESPBoxG->value : 0;
		int boxB = m_pCvarESPBoxB ? (int)m_pCvarESPBoxB->value : 0;
		
		int lineR = m_pCvarESPLineR ? (int)m_pCvarESPLineR->value : 255;
		int lineG = m_pCvarESPLineG ? (int)m_pCvarESPLineG->value : 0;
		int lineB = m_pCvarESPLineB ? (int)m_pCvarESPLineB->value : 0;
		
		int nameR = m_pCvarESPNameR ? (int)m_pCvarESPNameR->value : 255;
		int nameG = m_pCvarESPNameG ? (int)m_pCvarESPNameG->value : 0;
		int nameB = m_pCvarESPNameB ? (int)m_pCvarESPNameB->value : 0;
		
		int weaponR = m_pCvarESPWeaponR ? (int)m_pCvarESPWeaponR->value : 255;
		int weaponG = m_pCvarESPWeaponG ? (int)m_pCvarESPWeaponG->value : 0;
		int weaponB = m_pCvarESPWeaponB ? (int)m_pCvarESPWeaponB->value : 0;
		
		// Ограничиваем значения 0-255
		boxR = (boxR < 0) ? 0 : (boxR > 255) ? 255 : boxR;
		boxG = (boxG < 0) ? 0 : (boxG > 255) ? 255 : boxG;
		boxB = (boxB < 0) ? 0 : (boxB > 255) ? 255 : boxB;
		lineR = (lineR < 0) ? 0 : (lineR > 255) ? 255 : lineR;
		lineG = (lineG < 0) ? 0 : (lineG > 255) ? 255 : lineG;
		lineB = (lineB < 0) ? 0 : (lineB > 255) ? 255 : lineB;
		nameR = (nameR < 0) ? 0 : (nameR > 255) ? 255 : nameR;
		nameG = (nameG < 0) ? 0 : (nameG > 255) ? 255 : nameG;
		nameB = (nameB < 0) ? 0 : (nameB > 255) ? 255 : nameB;
		weaponR = (weaponR < 0) ? 0 : (weaponR > 255) ? 255 : weaponR;
		weaponG = (weaponG < 0) ? 0 : (weaponG > 255) ? 255 : weaponG;
		weaponB = (weaponB < 0) ? 0 : (weaponB > 255) ? 255 : weaponB;
		
		int alpha = 255;
		int lineWidth = 2;

		// Дополнительная проверка на валидность ent перед использованием (защита от краша в спектаторе)
		if (!ent || !ent->player || ent->curstate.spectator == 1)
			continue;
		
		// ESP Line - рисуем линию от центра экрана к игроку
		int screenCenterX = ScreenWidth / 2;
		int screenCenterY = ScreenHeight / 2;
		int playerScreenX = (int)(centerX + 0.5f);
		int playerScreenY = (int)((vecScreenTop[1] + vecScreenBottom[1]) * 0.5f + 0.5f);
		CustomUtils::DrawLine(screenCenterX, screenCenterY, playerScreenX, playerScreenY, lineWidth, lineR, lineG, lineB, alpha);
		
		// Рисуем corner box
		CustomUtils::DrawBoxCornerOutline(x, y, width, height, lineWidth, boxR, boxG, boxB, alpha);
		
		// ESP Name - рисуем имя игрока над боксом (с проверкой)
		if (ent && ent->index >= 1 && ent->index <= MAX_PLAYERS)
		{
			GetPlayerInfo(ent->index, &g_PlayerInfoList[ent->index]);
			if (g_PlayerInfoList[ent->index].name && g_PlayerInfoList[ent->index].name[0] != '\0')
			{
				const char *playerName = g_PlayerInfoList[ent->index].name;
				int nameX = (int)(centerX + 0.5f);
				int nameY = y - YRES(12); // Над боксом
				
				// Центрируем текст
				int nameWidth = DrawUtils::HudStringLen(playerName);
				nameX -= nameWidth / 2;
				
				// Рисуем имя с настраиваемым цветом
				DrawUtils::DrawHudString(nameX, nameY, ScreenWidth, playerName, nameR, nameG, nameB);
			}
		}
		
		// ESP Weapon - рисуем оружие под боксом (с проверкой)
		if (ent && ent->curstate.weaponmodel > 0)
		{
			extern engine_studio_api_t IEngineStudio;
			model_t *pWeaponModel = IEngineStudio.GetModelByIndex(ent->curstate.weaponmodel);
			if (pWeaponModel && pWeaponModel->name)
			{
				const char *weaponModelName = pWeaponModel->name;
				char weaponName[64] = {0};
				
				// Извлекаем название оружия из пути модели
				// Например: "models/w_ak47.mdl" -> "AK47"
				const char *lastSlash = strrchr(weaponModelName, '/');
				const char *weaponFile = lastSlash ? lastSlash + 1 : weaponModelName;
				
				// Копируем название файла без расширения
				int len = strlen(weaponFile);
				int copyLen = len;
				if (copyLen > 4 && strcmp(weaponFile + copyLen - 4, ".mdl") == 0)
					copyLen -= 4;
				if (copyLen > sizeof(weaponName) - 1)
					copyLen = sizeof(weaponName) - 1;
				
				strncpy(weaponName, weaponFile, copyLen);
				weaponName[copyLen] = '\0';
				
				// Убираем префикс "w_" если есть
				if (strncmp(weaponName, "w_", 2) == 0)
				{
					memmove(weaponName, weaponName + 2, strlen(weaponName) - 1);
				}
				
				// Преобразуем в верхний регистр и делаем красивое название
				for (int i = 0; weaponName[i]; i++)
				{
					if (weaponName[i] >= 'a' && weaponName[i] <= 'z')
						weaponName[i] = weaponName[i] - 'a' + 'A';
				}
				
				// Специальные случаи для некоторых оружий
				if (strcmp(weaponName, "MP5NAVY") == 0 || strcmp(weaponName, "MP5") == 0)
					strncpy(weaponName, "MP5", sizeof(weaponName) - 1);
				else if (strcmp(weaponName, "M4A1") == 0)
					strncpy(weaponName, "M4A1", sizeof(weaponName) - 1);
				else if (strcmp(weaponName, "GLOCK18") == 0)
					strncpy(weaponName, "GLOCK", sizeof(weaponName) - 1);
				else if (strcmp(weaponName, "FIVESEVEN") == 0)
					strncpy(weaponName, "FIVE-SEVEN", sizeof(weaponName) - 1);
				else if (strcmp(weaponName, "XM1014") == 0)
					strncpy(weaponName, "XM1014", sizeof(weaponName) - 1);
				else if (strcmp(weaponName, "SG550") == 0)
					strncpy(weaponName, "SG550", sizeof(weaponName) - 1);
				else if (strcmp(weaponName, "SG552") == 0)
					strncpy(weaponName, "SG552", sizeof(weaponName) - 1);
				else if (strcmp(weaponName, "G3SG1") == 0)
					strncpy(weaponName, "G3SG1", sizeof(weaponName) - 1);
				else if (strcmp(weaponName, "HEGRENADE") == 0)
					strncpy(weaponName, "HE", sizeof(weaponName) - 1);
				else if (strcmp(weaponName, "FLASHBANG") == 0)
					strncpy(weaponName, "FLASH", sizeof(weaponName) - 1);
				else if (strcmp(weaponName, "SMOKEGRENADE") == 0)
					strncpy(weaponName, "SMOKE", sizeof(weaponName) - 1);
				
				if (weaponName[0] != '\0')
				{
					int weaponX = (int)(centerX + 0.5f);
					int weaponY = y + height + YRES(2); // Под боксом
					
					// Центрируем текст
					int weaponWidth = DrawUtils::HudStringLen(weaponName);
					weaponX -= weaponWidth / 2;
					
					// Рисуем оружие с настраиваемым цветом
					DrawUtils::DrawHudString(weaponX, weaponY, ScreenWidth, weaponName, weaponR, weaponG, weaponB);
				}
			}
		}
		
		// ДЕБАГ: Отслеживаем найденные цели ESP
		targetCount++;
		lastFoundTarget = ent->index;
		GetPlayerInfo(ent->index, &g_PlayerInfoList[ent->index]);
		if (g_PlayerInfoList[ent->index].name && g_PlayerInfoList[ent->index].name[0] != '\0')
		{
			lastFoundTargetName = g_PlayerInfoList[ent->index].name;
		}
	}
	
	// ДЕБАГ: Обновляем информацию о ESP
	gHUD.m_Debug.UpdateESPDebug( m_bEnabled, targetCount, lastFoundTarget, lastFoundTargetName );

	return 0;
}

