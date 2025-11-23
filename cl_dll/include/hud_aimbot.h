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
// hud_aimbot.h
//
// Aimbot implementation
//
#pragma once

#include "hud.h"

class CHudAimbot : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );
	void Think( void );
	
	// Получить углы для psilent
	void GetAimAngles( float* aimAngles );
	
	// Проверить, обновлены ли углы для psilent
	bool IsPSilentReady( void ) { return m_bPSilentAnglesUpdated; }
	
	// Применить psilent к команде (вызывается из CL_CreateMove)
	void ApplyPSilent( struct usercmd_s *cmd );
	
	// Glow эффекты (публичные для использования из других файлов)
	void ApplyWeaponGlow( void );
	void ApplyHandsGlow( void );
	
	// FOV (публичная для использования из других файлов)
	void ApplyCustomFOV( void );
	
	// Отключение анимаций игроков (публичная для использования из других файлов)
	void DisablePlayerAnimations( void );
	
	// Третье лицо (публичная для использования из других файлов)
	void ApplyThirdPerson( void );
	
	// Glow эффекты (публичные статические переменные для использования из других файлов)
	static bool m_bWeaponGlow;
	static bool m_bHandsGlow;
	static float m_fGlowColor[3];
	static float m_fGlowAlpha;
	
	// FOV (публичные статические переменные для использования из других файлов)
	static bool m_bCustomFOV;
	static float m_fCustomFOVValue;
	
	// Spinbot (публичные статические переменные)
	static bool m_bSpinbot;
	static float m_fSpinbotSpeed;
	
	// Third Person (публичные статические переменные)
	static bool m_bThirdPerson;
	static float m_fThirdPersonDist;
	
	// Anti-Aim (публичные статические переменные)
	static bool m_bAntiAim;
	static int m_iAntiAimMode;
	static float m_fAntiAimSpeed;
	static float m_fAntiAimJitterRange;
	static float m_fAntiAimFakeAngle;

private:
	// Поиск цели
	cl_entity_t* FindBestTarget( float* viewAngles, float fov );
	
	// Вычисление углов прицеливания
	void CalculateAimAngles( cl_entity_t* target, float* aimAngles );
	
	// Плавное наведение
	void SmoothAim( float* currentAngles, float* targetAngles, float* outputAngles, float smooth );
	
	// Проверки
	bool IsTargetInFOV( float* viewAngles, float* targetPos, float fov );
	bool IsTargetVisible( cl_entity_t* target );
	bool IsShooting( void );
	bool IsPlayerDead( cl_entity_t* ent );
	bool HasAmmoToShoot( void ); // Проверка наличия патронов для стрельбы
	
	// Получить позицию игрока с обходом блокировки ESP (использует альтернативные источники)
	bool GetPlayerPosition( cl_entity_t* ent, float* origin );
	
	// RCS (Recoil Control System)
	void GetPunchAngles( float* punchAngles );
	void ApplyRCS( float* viewAngles );
	void ApplyRCSWithSmoothing( float* viewAngles );
	void ApplyNoRecoil( void );
	
	// Spinbot
	void ApplySpinbot( struct usercmd_s *cmd );
	
	// Anti-Aim
	void ApplyAntiAim( struct usercmd_s *cmd );
	void ApplyAntiAimYawOnly( struct usercmd_s *cmd ); // Применяет антиаим только к YAW (для работы с аимботом)
	
	// Отрисовка
	void DrawFOVCircle( void );
	
	// Trigger bot
	void ApplyTriggerBot( struct usercmd_s *cmd );
	bool IsTargetOnCrosshair( float* viewAngles );
	cl_entity_t* GetTargetOnCrosshair( float* viewAngles );
	
	// Статические переменные (настройки)
	static bool m_bEnabled;
	static bool m_bDrawFOV;
	static bool m_bOnlyWhenShooting;
	static bool m_bIgnoreVisibility;
	static bool m_bRCSEnabled;
	static bool m_bNoRecoil;
	static bool m_bPSilent;
	static bool m_bTriggerBot;
	static bool m_bTriggerBotOnetap;
	static float m_fSmooth;
	static float m_fAimFOV;
	static float m_fAimHeight;
	static float m_fRCSStrength;
	static float m_fLastShotTime;
	static bool m_bAutoShoot;
	static int m_iTargetBone;
	static bool m_bResetOnRound;
	static float m_fHeadScale;
	static bool m_bBacktrack;
	static float m_fBacktrackTime;
	static bool m_bHitboxExpansion;
	static float m_fHitboxExpansionScale;
	
	// Углы для psilent
	static float m_PSilentAngles[3];
	static bool m_bPSilentAnglesUpdated;
	
	// Последняя цель для psilent (чтобы применять углы даже если цель временно пропадает)
	static cl_entity_t* m_pLastTarget;
	static float m_fLastTargetAngles[3];
	static float m_fLastTargetUpdateTime;
	static bool m_bHasLastTarget;
	
	// Переключение цели
	static int m_iCurrentTargetIndex; // Индекс текущей выбранной цели
	
	// Cvar'ы
	cvar_t *m_pCvarAimbot;
	cvar_t *m_pCvarPSilent;
	cvar_t *m_pCvarRCSEnable;
	cvar_t *m_pCvarRCSStrength;
	cvar_t *m_pCvarSmooth;
	cvar_t *m_pCvarFOV;
	cvar_t *m_pCvarHeight;
	cvar_t *m_pCvarDrawFOV;
	cvar_t *m_pCvarOnlyShooting;
	cvar_t *m_pCvarIgnoreVisibility;
	cvar_t *m_pCvarNoRecoil;
	cvar_t *m_pCvarDeathmatch;
	cvar_t *m_pCvarTriggerBot;
	cvar_t *m_pCvarTriggerBotOnetap;
	cvar_t *m_pCvarWeaponGlow;
	cvar_t *m_pCvarHandsGlow;
	cvar_t *m_pCvarGlowColorR;
	cvar_t *m_pCvarGlowColorG;
	cvar_t *m_pCvarGlowColorB;
	cvar_t *m_pCvarGlowAlpha;
	cvar_t *m_pCvarCustomFOV;
	cvar_t *m_pCvarCustomFOVValue;
	cvar_t *m_pCvarDisableAnimations;
	cvar_t *m_pCvarSpinbot;
	cvar_t *m_pCvarSpinbotSpeed;
	cvar_t *m_pCvarThirdPerson;
	cvar_t *m_pCvarThirdPersonDist;
	cvar_t *m_pCvarAntiAim;
	cvar_t *m_pCvarAntiAimMode;
	cvar_t *m_pCvarAntiAimSpeed;
	cvar_t *m_pCvarAntiAimJitterRange;
	cvar_t *m_pCvarAntiAimFakeAngle;
	cvar_t *m_pCvarAutoShoot;
	cvar_t *m_pCvarTargetBone;
	cvar_t *m_pCvarResetOnRound;
	cvar_t *m_pCvarHeadScale;
	cvar_t *m_pCvarBacktrack;
	cvar_t *m_pCvarBacktrackTime;
	cvar_t *m_pCvarHitboxExpansion;
	cvar_t *m_pCvarHitboxExpansionScale;
	cvar_t *m_pCvarDebug;
	cvar_t *m_pCvarSwitchTarget;
	
	// Функция для увеличения хитбоксов
	void ExpandHitboxes( cl_entity_t* target );
};

// Глобальные переменные
extern bool g_AimbotIsShooting;
extern bool g_NoRecoilEnabled;

