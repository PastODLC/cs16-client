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
// hud_debug.cpp
//
// Debug HUD implementation
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "hud_debug.h"
#include "hud_aimbot.h"
#include "hud_cornerbox.h"
#include "draw_util.h"
#include "cl_entity.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

int CHudDebug::Init()
{
	m_iFlags = HUD_ACTIVE | HUD_THINK;
	
	// Регистрируем cvar для включения/выключения дебага
	m_pCvarDebug = gEngfuncs.pfnRegisterVariable( "ebash3d_debug", "0", FCVAR_ARCHIVE );
	
	// Инициализация истории урона
	m_iDamageHistoryCount = 0;
	memset( m_DamageHistory, 0, sizeof(m_DamageHistory) );
	
	// Инициализация дебаг информации о silent aim
	memset( &m_SilentAimDebug, 0, sizeof(m_SilentAimDebug) );
	
	// Инициализация дебаг информации о ESP
	memset( &m_ESPDebug, 0, sizeof(m_ESPDebug) );
	
	// Инициализация FPS
	m_flLastFPSUpdate = 0.0f;
	m_iFPSFrames = 0;
	m_flFPS = 0.0f;
	
	gHUD.AddHudElem(this);
	return 0;
}

int CHudDebug::VidInit()
{
	return 1;
}

void CHudDebug::Reset()
{
	m_iDamageHistoryCount = 0;
	memset( m_DamageHistory, 0, sizeof(m_DamageHistory) );
	memset( &m_SilentAimDebug, 0, sizeof(m_SilentAimDebug) );
	memset( &m_ESPDebug, 0, sizeof(m_ESPDebug) );
	m_flLastFPSUpdate = 0.0f;
	m_iFPSFrames = 0;
	m_flFPS = 0.0f;
}

void CHudDebug::Think()
{
	// Обновляем FPS каждый кадр
	float currentTime = gEngfuncs.GetClientTime();
	if ( m_flLastFPSUpdate == 0.0f )
		m_flLastFPSUpdate = currentTime;
	
	m_iFPSFrames++;
	
	// Обновляем FPS каждую секунду
	if ( currentTime - m_flLastFPSUpdate >= 1.0f )
	{
		m_flFPS = (float)m_iFPSFrames / (currentTime - m_flLastFPSUpdate);
		m_iFPSFrames = 0;
		m_flLastFPSUpdate = currentTime;
	}
	
	// Очищаем старую историю урона (старше 10 секунд)
	float expireTime = currentTime - 10.0f;
	for ( int i = 0; i < m_iDamageHistoryCount; i++ )
	{
		if ( m_DamageHistory[i].time < expireTime )
		{
			// Сдвигаем массив влево
			for ( int j = i; j < m_iDamageHistoryCount - 1; j++ )
			{
				m_DamageHistory[j] = m_DamageHistory[j + 1];
			}
			m_iDamageHistoryCount--;
			i--; // Проверяем этот же индекс снова
		}
	}
}

int CHudDebug::Draw( float flTime )
{
	// Проверяем, включен ли дебаг
	if ( !m_pCvarDebug || m_pCvarDebug->value == 0.0f )
		return 0;
	
	// Отрисовываем все секции дебага
	DrawStats( flTime );
	DrawDamageHistory( flTime );
	DrawSilentAimDebug( flTime );
	DrawESPDebug( flTime );
	
	return 0;
}

void CHudDebug::DrawStats( float flTime )
{
	int x = 10;
	int y = 10;
	int lineHeight = gHUD.m_iFontHeight + 2;
	
	// FPS
	char fpsText[64];
	sprintf( fpsText, "FPS: %.0f", GetFPS() );
	DrawUtils::DrawHudString( x, y, ScreenWidth, fpsText, 0, 255, 0 );
	y += lineHeight;
	
	// Ping
	char pingText[64];
	sprintf( pingText, "Ping: %d ms", GetPing() );
	DrawUtils::DrawHudString( x, y, ScreenWidth, pingText, 0, 255, 0 );
	y += lineHeight;
	
	// Packet Loss
	char lossText[64];
	sprintf( lossText, "Loss: %d%%", GetPacketLoss() );
	int lossColorR = (GetPacketLoss() > 5) ? 255 : 0;
	int lossColorG = (GetPacketLoss() > 5) ? 0 : 255;
	DrawUtils::DrawHudString( x, y, ScreenWidth, lossText, lossColorR, lossColorG, 0 );
	y += lineHeight;
	
	// Разделитель
	y += lineHeight;
}

void CHudDebug::DrawDamageHistory( float flTime )
{
	int x = 10;
	int y = 10 + (gHUD.m_iFontHeight + 2) * 4; // После статистики
	int lineHeight = gHUD.m_iFontHeight + 2;
	
	// Заголовок
	DrawUtils::DrawHudString( x, y, ScreenWidth, "=== DAMAGE HISTORY ===", 255, 255, 0 );
	y += lineHeight;
	
	// История урона (последние 10 записей)
	int count = (m_iDamageHistoryCount < MAX_DAMAGE_HISTORY) ? m_iDamageHistoryCount : MAX_DAMAGE_HISTORY;
	for ( int i = count - 1; i >= 0; i-- )
	{
		char damageText[128];
		float timeAgo = flTime - m_DamageHistory[i].time;
		
		if ( m_DamageHistory[i].isReceived )
		{
			// Получен урон
			sprintf( damageText, "[%.1fs] -%d HP from %s (ID: %d)", 
			         timeAgo, (int)m_DamageHistory[i].damage, 
			         m_DamageHistory[i].attackerName, 
			         m_DamageHistory[i].attackerIndex );
			DrawUtils::DrawHudString( x, y, ScreenWidth, damageText, 255, 0, 0 );
		}
		else
		{
			// Нанесен урон
			sprintf( damageText, "[%.1fs] +%d HP to %s (ID: %d)", 
			         timeAgo, (int)m_DamageHistory[i].damage, 
			         m_DamageHistory[i].attackerName, 
			         m_DamageHistory[i].attackerIndex );
			DrawUtils::DrawHudString( x, y, ScreenWidth, damageText, 0, 255, 0 );
		}
		
		y += lineHeight;
		
		// Ограничиваем количество отображаемых строк
		if ( y > ScreenHeight - 100 )
			break;
	}
}

void CHudDebug::DrawSilentAimDebug( float flTime )
{
	int x = ScreenWidth - 300; // Справа
	int y = 10;
	int lineHeight = gHUD.m_iFontHeight + 2;
	
	// Заголовок
	DrawUtils::DrawHudString( x, y, ScreenWidth, "=== SILENT AIM DEBUG ===", 255, 255, 0 );
	y += lineHeight;
	
	// Статус
	char statusText[64];
	if ( m_SilentAimDebug.isActive )
	{
		sprintf( statusText, "Status: ACTIVE", m_SilentAimDebug.isActive ? "ON" : "OFF" );
		DrawUtils::DrawHudString( x, y, ScreenWidth, statusText, 0, 255, 0 );
	}
	else
	{
		sprintf( statusText, "Status: INACTIVE" );
		DrawUtils::DrawHudString( x, y, ScreenWidth, statusText, 255, 0, 0 );
	}
	y += lineHeight;
	
	if ( m_SilentAimDebug.isActive )
	{
		// Цель
		if ( m_SilentAimDebug.hasTarget )
		{
			char targetText[128];
			sprintf( targetText, "Target: %s (ID: %d)", 
			         m_SilentAimDebug.targetName, m_SilentAimDebug.targetIndex );
			DrawUtils::DrawHudString( x, y, ScreenWidth, targetText, 0, 255, 0 );
			y += lineHeight;
			
			char distText[64];
			sprintf( distText, "Distance: %.1f", m_SilentAimDebug.targetDistance );
			DrawUtils::DrawHudString( x, y, ScreenWidth, distText, 255, 255, 255 );
			y += lineHeight;
		}
		else
		{
			DrawUtils::DrawHudString( x, y, ScreenWidth, "Target: NONE", 255, 0, 0 );
			y += lineHeight;
		}
		
		// Углы прицеливания
		char aimText[128];
		sprintf( aimText, "Aim: P:%.1f Y:%.1f R:%.1f", 
		         m_SilentAimDebug.aimAngles[0], 
		         m_SilentAimDebug.aimAngles[1], 
		         m_SilentAimDebug.aimAngles[2] );
		DrawUtils::DrawHudString( x, y, ScreenWidth, aimText, 255, 255, 255 );
		y += lineHeight;
		
		// Углы обзора
		char viewText[128];
		sprintf( viewText, "View: P:%.1f Y:%.1f R:%.1f", 
		         m_SilentAimDebug.viewAngles[0], 
		         m_SilentAimDebug.viewAngles[1], 
		         m_SilentAimDebug.viewAngles[2] );
		DrawUtils::DrawHudString( x, y, ScreenWidth, viewText, 255, 255, 255 );
		y += lineHeight;
		
		// Углы отдачи
		char punchText[128];
		sprintf( punchText, "Punch: P:%.2f Y:%.2f R:%.2f", 
		         m_SilentAimDebug.punchAngles[0], 
		         m_SilentAimDebug.punchAngles[1], 
		         m_SilentAimDebug.punchAngles[2] );
		DrawUtils::DrawHudString( x, y, ScreenWidth, punchText, 255, 200, 0 );
		y += lineHeight;
	}
}

void CHudDebug::DrawESPDebug( float flTime )
{
	int x = ScreenWidth - 300; // Справа
	int y = ScreenHeight - 150; // Внизу
	int lineHeight = gHUD.m_iFontHeight + 2;
	
	// Заголовок
	DrawUtils::DrawHudString( x, y, ScreenWidth, "=== ESP DEBUG ===", 255, 255, 0 );
	y += lineHeight;
	
	// Статус
	char statusText[64];
	if ( m_ESPDebug.isActive )
	{
		sprintf( statusText, "Status: ACTIVE" );
		DrawUtils::DrawHudString( x, y, ScreenWidth, statusText, 0, 255, 0 );
	}
	else
	{
		sprintf( statusText, "Status: INACTIVE" );
		DrawUtils::DrawHudString( x, y, ScreenWidth, statusText, 255, 0, 0 );
	}
	y += lineHeight;
	
	if ( m_ESPDebug.isActive )
	{
		// Количество целей
		char countText[64];
		sprintf( countText, "Targets: %d", m_ESPDebug.targetCount );
		DrawUtils::DrawHudString( x, y, ScreenWidth, countText, 255, 255, 255 );
		y += lineHeight;
		
		// Последняя найденная цель
		if ( m_ESPDebug.lastFoundTarget > 0 )
		{
			float timeAgo = flTime - m_ESPDebug.lastFoundTime;
			char lastText[128];
			sprintf( lastText, "Last: %s (ID: %d) [%.1fs ago]", 
			         m_ESPDebug.lastFoundTargetName, 
			         m_ESPDebug.lastFoundTarget, 
			         timeAgo );
			DrawUtils::DrawHudString( x, y, ScreenWidth, lastText, 0, 255, 255 );
			y += lineHeight;
		}
	}
}

void CHudDebug::RegisterDamageReceived( int attackerIndex, float damage )
{
	AddDamageToHistory( attackerIndex, damage, true );
}

void CHudDebug::RegisterDamageDealt( int victimIndex, float damage )
{
	AddDamageToHistory( victimIndex, damage, false );
}

void CHudDebug::AddDamageToHistory( int playerIndex, float damage, bool isReceived )
{
	// Находим свободное место или перезаписываем самое старое
	int index = m_iDamageHistoryCount;
	if ( index >= MAX_DAMAGE_HISTORY )
	{
		// Находим самую старую запись
		float oldestTime = m_DamageHistory[0].time;
		index = 0;
		for ( int i = 1; i < MAX_DAMAGE_HISTORY; i++ )
		{
			if ( m_DamageHistory[i].time < oldestTime )
			{
				oldestTime = m_DamageHistory[i].time;
				index = i;
			}
		}
	}
	else
	{
		m_iDamageHistoryCount++;
	}
	
	// Заполняем информацию
	m_DamageHistory[index].attackerIndex = playerIndex;
	m_DamageHistory[index].damage = damage;
	m_DamageHistory[index].time = gEngfuncs.GetClientTime();
	m_DamageHistory[index].isReceived = isReceived;
	
	// Получаем имя игрока
	const char* name = GetPlayerName( playerIndex );
	if ( name )
		strncpy( m_DamageHistory[index].attackerName, name, sizeof(m_DamageHistory[index].attackerName) - 1 );
	else
		sprintf( m_DamageHistory[index].attackerName, "Unknown" );
	m_DamageHistory[index].attackerName[sizeof(m_DamageHistory[index].attackerName) - 1] = '\0';
}

void CHudDebug::UpdateSilentAimDebug( bool isActive, bool hasTarget, int targetIndex, 
                                     float targetDistance, float* aimAngles, 
                                     float* viewAngles, float* punchAngles )
{
	m_SilentAimDebug.isActive = isActive;
	m_SilentAimDebug.hasTarget = hasTarget;
	m_SilentAimDebug.targetIndex = targetIndex;
	m_SilentAimDebug.targetDistance = targetDistance;
	
	if ( aimAngles )
	{
		m_SilentAimDebug.aimAngles[0] = aimAngles[0];
		m_SilentAimDebug.aimAngles[1] = aimAngles[1];
		m_SilentAimDebug.aimAngles[2] = aimAngles[2];
	}
	
	if ( viewAngles )
	{
		m_SilentAimDebug.viewAngles[0] = viewAngles[0];
		m_SilentAimDebug.viewAngles[1] = viewAngles[1];
		m_SilentAimDebug.viewAngles[2] = viewAngles[2];
	}
	
	if ( punchAngles )
	{
		m_SilentAimDebug.punchAngles[0] = punchAngles[0];
		m_SilentAimDebug.punchAngles[1] = punchAngles[1];
		m_SilentAimDebug.punchAngles[2] = punchAngles[2];
	}
	
	// Получаем имя цели
	if ( hasTarget && targetIndex > 0 && targetIndex <= MAX_PLAYERS )
	{
		const char* name = GetPlayerName( targetIndex );
		if ( name )
			strncpy( m_SilentAimDebug.targetName, name, sizeof(m_SilentAimDebug.targetName) - 1 );
		else
			sprintf( m_SilentAimDebug.targetName, "Unknown" );
		m_SilentAimDebug.targetName[sizeof(m_SilentAimDebug.targetName) - 1] = '\0';
	}
	else
	{
		m_SilentAimDebug.targetName[0] = '\0';
	}
}

void CHudDebug::UpdateESPDebug( bool isActive, int targetCount, int lastFoundTarget, 
                               const char* targetName )
{
	m_ESPDebug.isActive = isActive;
	m_ESPDebug.targetCount = targetCount;
	m_ESPDebug.lastFoundTarget = lastFoundTarget;
	
	if ( targetName && lastFoundTarget > 0 )
	{
		strncpy( m_ESPDebug.lastFoundTargetName, targetName, sizeof(m_ESPDebug.lastFoundTargetName) - 1 );
		m_ESPDebug.lastFoundTargetName[sizeof(m_ESPDebug.lastFoundTargetName) - 1] = '\0';
		m_ESPDebug.lastFoundTime = gEngfuncs.GetClientTime();
	}
}

float CHudDebug::GetFPS( void )
{
	return m_flFPS;
}

int CHudDebug::GetPing( void )
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if ( !localPlayer )
		return 0;
	
	GetPlayerInfo( localPlayer->index, &g_PlayerInfoList[localPlayer->index] );
	return g_PlayerInfoList[localPlayer->index].ping;
}

int CHudDebug::GetPacketLoss( void )
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if ( !localPlayer )
		return 0;
	
	GetPlayerInfo( localPlayer->index, &g_PlayerInfoList[localPlayer->index] );
	return g_PlayerInfoList[localPlayer->index].packetloss;
}

const char* CHudDebug::GetPlayerName( int index )
{
	if ( index < 1 || index > MAX_PLAYERS )
		return NULL;
	
	GetPlayerInfo( index, &g_PlayerInfoList[index] );
	if ( g_PlayerInfoList[index].name && g_PlayerInfoList[index].name[0] != '\0' )
		return g_PlayerInfoList[index].name;
	
	return NULL;
}

