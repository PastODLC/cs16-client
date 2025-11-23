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
// hud_aimbot.cpp
//
// Aimbot implementation
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "hud_aimbot.h"
#include "hud_debug.h"
#include "custom_utils.h"
#include "cl_entity.h"
#include "com_model.h"
#include "pmtrace.h"
#include "pm_defs.h"
#include "event_api.h"
#include "const.h"
#include <math.h>
#include <stdlib.h>
#include "usercmd.h"
#include "view.h"
#include "in_defs.h"
#include "kbutton.h"
#include "cdll_int.h"
#include "cl_dll.h"
#include "com_weapons.h"

// Объявления функций для обработки кнопок (из input.cpp)
extern void KeyDown( kbutton_t *b );
extern void KeyUp( kbutton_t *b );
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "ammohistory.h" // Для доступа к gWR
#include "../dlls/weapontype.h" // Для констант оружий (WEAPON_HEGRENADE и т.д.)

// Доступ к pmove для получения отдачи оружия
extern "C"
{
	extern playermove_t *pmove;
}

// Доступ к in_attack для проверки стрельбы (объявлен в input.cpp)
extern kbutton_t in_attack;

// Кнопка для переключения цели
static kbutton_t in_switch_target;

// Доступ к ev_punchangle из view.cpp
// После включения util_vector.h vec3_t становится Vector
extern vec3_t ev_punchangle;

#if !defined(M_PI)
#define M_PI		3.14159265358979323846
#endif

#if !defined(M_PI_F)
#define M_PI_F		(float)M_PI
#endif

// Инициализация статических переменных
bool CHudAimbot::m_bEnabled = false;
bool CHudAimbot::m_bDrawFOV = true;
bool CHudAimbot::m_bOnlyWhenShooting = false;
bool CHudAimbot::m_bIgnoreVisibility = false;
bool CHudAimbot::m_bRCSEnabled = false;
bool CHudAimbot::m_bNoRecoil = false;
bool CHudAimbot::m_bPSilent = false;
bool CHudAimbot::m_bTriggerBot = false;
bool CHudAimbot::m_bTriggerBotOnetap = false;
float CHudAimbot::m_fSmooth = 0.5f;        // Уменьшено для более быстрого наведения
float CHudAimbot::m_fAimFOV = 45.0f;       // Увеличено до 45° для лучшего захвата целей
float CHudAimbot::m_fAimHeight = 20.0f;    // Оптимизировано для головы
float CHudAimbot::m_fRCSStrength = 1.5f;   // Увеличено для идеальной компенсации отдачи
float CHudAimbot::m_fLastShotTime = 0.0f;
bool CHudAimbot::m_bAutoShoot = false;
int CHudAimbot::m_iTargetBone = 8;         // 8 = голова (по умолчанию)
bool CHudAimbot::m_bResetOnRound = true;
float CHudAimbot::m_fHeadScale = 1.2f;     // Увеличено для лучшего попадания в голову
bool CHudAimbot::m_bBacktrack = false;
float CHudAimbot::m_fBacktrackTime = 200.0f;
float CHudAimbot::m_PSilentAngles[3] = {0.0f, 0.0f, 0.0f};
bool CHudAimbot::m_bPSilentAnglesUpdated = false;
cl_entity_t* CHudAimbot::m_pLastTarget = nullptr;
float CHudAimbot::m_fLastTargetAngles[3] = {0.0f, 0.0f, 0.0f};
float CHudAimbot::m_fLastTargetUpdateTime = 0.0f;
bool CHudAimbot::m_bHasLastTarget = false;
bool CHudAimbot::m_bWeaponGlow = false;
bool CHudAimbot::m_bHandsGlow = false;
float CHudAimbot::m_fGlowColor[3] = {255.0f, 255.0f, 255.0f};
float CHudAimbot::m_fGlowAlpha = 200.0f;
bool CHudAimbot::m_bCustomFOV = false;
float CHudAimbot::m_fCustomFOVValue = 90.0f;
bool CHudAimbot::m_bSpinbot = false;
float CHudAimbot::m_fSpinbotSpeed = 100.0f;
bool CHudAimbot::m_bThirdPerson = false;
float CHudAimbot::m_fThirdPersonDist = 100.0f;
bool CHudAimbot::m_bAntiAim = false;
int CHudAimbot::m_iAntiAimMode = 0;
float CHudAimbot::m_fAntiAimSpeed = 50.0f;
float CHudAimbot::m_fAntiAimJitterRange = 30.0f;
float CHudAimbot::m_fAntiAimFakeAngle = 58.0f;
int CHudAimbot::m_iCurrentTargetIndex = 0; // Индекс текущей выбранной цели

// Глобальные переменные
bool g_AimbotIsShooting = false;
bool g_NoRecoilEnabled = false;

// Функция для вывода дебаг логов в консоль (с ограничением частоты для предотвращения спама)
static void AimbotDebugLog( const char* format, ... )
{
	// Проверяем, включен ли дебаг (используем статическую переменную для быстрого доступа)
	static bool debugEnabled = false;
	static cvar_t* debugCvar = nullptr;
	
	// Получаем cvar при первом вызове
	if ( !debugCvar )
	{
		// Ищем cvar через движок
		debugCvar = gEngfuncs.pfnGetCvarPointer( "ebash3d_aimbot_debug" );
	}
	
	// Проверяем, включен ли дебаг
	if ( debugCvar && debugCvar->value != 0.0f )
		debugEnabled = true;
	else
		debugEnabled = false;
	
	if ( !debugEnabled )
		return;
	
	// Формируем сообщение с префиксом
	char msg[512];
	va_list args;
	va_start( args, format );
	
	// Префикс с зеленым цветом: ^2[eBash3D ^2Debug]^7
	sprintf( msg, "^2[eBash3D ^2Debug]^7 " );
	int prefixLen = strlen( msg );
	
	// Форматируем остальное сообщение
	vsnprintf( msg + prefixLen, sizeof(msg) - prefixLen, format, args );
	va_end( args );
	
	// Выводим в консоль
	gEngfuncs.pfnConsolePrint( msg );
}

// Функция для вывода дебаг логов с ограничением частоты (раз в секунду для одинаковых сообщений)
static void AimbotDebugLogThrottled( const char* format, ... )
{
	// Проверяем, включен ли дебаг
	static bool debugEnabled = false;
	static cvar_t* debugCvar = nullptr;
	static float lastLogTime = 0.0f;
	static char lastLogMessage[256] = "";
	
	// Получаем cvar при первом вызове
	if ( !debugCvar )
	{
		debugCvar = gEngfuncs.pfnGetCvarPointer( "ebash3d_aimbot_debug" );
	}
	
	// Проверяем, включен ли дебаг
	if ( debugCvar && debugCvar->value != 0.0f )
		debugEnabled = true;
	else
		debugEnabled = false;
	
	if ( !debugEnabled )
		return;
	
	// Формируем сообщение
	char msg[512];
	va_list args;
	va_start( args, format );
	
	// Префикс с зеленым цветом: ^2[eBash3D ^2Debug]^7
	sprintf( msg, "^2[eBash3D ^2Debug]^7 " );
	int prefixLen = strlen( msg );
	
	// Форматируем остальное сообщение
	vsnprintf( msg + prefixLen, sizeof(msg) - prefixLen, format, args );
	va_end( args );
	
	// Проверяем, не повторяется ли сообщение
	float currentTime = gEngfuncs.GetClientTime();
	bool isSameMessage = ( strcmp( msg, lastLogMessage ) == 0 );
	
	// Если сообщение повторяется и прошло меньше секунды - не выводим
	if ( isSameMessage && ( currentTime - lastLogTime ) < 1.0f )
		return;
	
	// Обновляем время и сообщение
	lastLogTime = currentTime;
	strncpy( lastLogMessage, msg, sizeof(lastLogMessage) - 1 );
	lastLogMessage[sizeof(lastLogMessage) - 1] = '\0';
	
	// Выводим в консоль
	gEngfuncs.pfnConsolePrint( msg );
}

// Доступ к кэшу позиций из hud_cornerbox.cpp для обхода блокировки ESP
extern float g_PlayerPositionCache[];
extern float g_PlayerPositionCacheTime[];
extern bool g_PlayerPositionCacheValid[];


// Функции для переключения цели
void IN_SwitchTargetDown( void )
{
	KeyDown( &in_switch_target );
}

void IN_SwitchTargetUp( void )
{
	KeyUp( &in_switch_target );
}

int CHudAimbot::Init()
{
	m_iFlags = HUD_ACTIVE | HUD_THINK;
	
	// Регистрируем cvar'ы с префиксом ebash3d_
	m_pCvarAimbot = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot", "0", FCVAR_ARCHIVE );
	m_pCvarPSilent = gEngfuncs.pfnRegisterVariable( "ebash3d_psilent", "0", FCVAR_ARCHIVE );
	m_pCvarRCSEnable = gEngfuncs.pfnRegisterVariable( "ebash3d_rcs_enable", "0", FCVAR_ARCHIVE );
	m_pCvarRCSStrength = gEngfuncs.pfnRegisterVariable( "ebash3d_rcs", "1.0", FCVAR_ARCHIVE );
	m_pCvarSmooth = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_smooth", "0", FCVAR_ARCHIVE );
	m_pCvarFOV = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_fov", "5.0", FCVAR_ARCHIVE );
	m_pCvarHeight = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_height", "28.0", FCVAR_ARCHIVE );
	m_pCvarDrawFOV = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_drawfov", "1", FCVAR_ARCHIVE );
	m_pCvarOnlyShooting = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_onlyshooting", "0", FCVAR_ARCHIVE );
	m_pCvarIgnoreVisibility = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_ignorevisibility", "0", FCVAR_ARCHIVE );
	m_pCvarNoRecoil = gEngfuncs.pfnRegisterVariable( "ebash3d_norecoil", "0", FCVAR_ARCHIVE );
	m_pCvarDeathmatch = gEngfuncs.pfnRegisterVariable( "ebash3d_deathmatch", "0", FCVAR_ARCHIVE );
	m_pCvarTriggerBot = gEngfuncs.pfnRegisterVariable( "ebash3d_triggerbot", "0", FCVAR_ARCHIVE );
	m_pCvarTriggerBotOnetap = gEngfuncs.pfnRegisterVariable( "ebash3d_triggerbot_onetap", "0", FCVAR_ARCHIVE );
	m_pCvarWeaponGlow = gEngfuncs.pfnRegisterVariable( "ebash3d_weapon_glow", "0", FCVAR_ARCHIVE );
	m_pCvarHandsGlow = gEngfuncs.pfnRegisterVariable( "ebash3d_hands_glow", "0", FCVAR_ARCHIVE );
	m_pCvarGlowColorR = gEngfuncs.pfnRegisterVariable( "ebash3d_glow_color_r", "255", FCVAR_ARCHIVE );
	m_pCvarGlowColorG = gEngfuncs.pfnRegisterVariable( "ebash3d_glow_color_g", "255", FCVAR_ARCHIVE );
	m_pCvarGlowColorB = gEngfuncs.pfnRegisterVariable( "ebash3d_glow_color_b", "255", FCVAR_ARCHIVE );
	m_pCvarGlowAlpha = gEngfuncs.pfnRegisterVariable( "ebash3d_glow_alpha", "200", FCVAR_ARCHIVE );
	m_pCvarCustomFOV = gEngfuncs.pfnRegisterVariable( "ebash3d_custom_fov", "0", FCVAR_ARCHIVE );
	m_pCvarCustomFOVValue = gEngfuncs.pfnRegisterVariable( "ebash3d_custom_fov_value", "90", FCVAR_ARCHIVE );
	m_pCvarDisableAnimations = gEngfuncs.pfnRegisterVariable( "ebash3d_disable_animations", "0", FCVAR_ARCHIVE );
	m_pCvarSpinbot = gEngfuncs.pfnRegisterVariable( "ebash3d_spinbot", "0", FCVAR_ARCHIVE );
	m_pCvarSpinbotSpeed = gEngfuncs.pfnRegisterVariable( "ebash3d_spinbot_speed", "100.0", FCVAR_ARCHIVE );
	m_pCvarThirdPerson = gEngfuncs.pfnRegisterVariable( "ebash3d_thirdperson", "0", FCVAR_ARCHIVE );
	m_pCvarThirdPersonDist = gEngfuncs.pfnRegisterVariable( "ebash3d_thirdperson_dist", "100.0", FCVAR_ARCHIVE );
	m_pCvarAntiAim = gEngfuncs.pfnRegisterVariable( "ebash3d_antiaim", "0", FCVAR_ARCHIVE );
	m_pCvarAntiAimMode = gEngfuncs.pfnRegisterVariable( "ebash3d_antiaim_mode", "0", FCVAR_ARCHIVE );
	m_pCvarAntiAimSpeed = gEngfuncs.pfnRegisterVariable( "ebash3d_antiaim_speed", "50.0", FCVAR_ARCHIVE );
	m_pCvarAntiAimJitterRange = gEngfuncs.pfnRegisterVariable( "ebash3d_antiaim_jitter_range", "30.0", FCVAR_ARCHIVE );
	m_pCvarAntiAimFakeAngle = gEngfuncs.pfnRegisterVariable( "ebash3d_antiaim_fake_angle", "58.0", FCVAR_ARCHIVE );
	m_pCvarAutoShoot = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_autoshoot", "0", FCVAR_ARCHIVE );
	m_pCvarTargetBone = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_target_bone", "0", FCVAR_ARCHIVE );
	m_pCvarResetOnRound = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_reset_on_round", "1", FCVAR_ARCHIVE );
	m_pCvarHeadScale = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_head_scale", "1.0", FCVAR_ARCHIVE );
	m_pCvarBacktrack = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_backtrack", "0", FCVAR_ARCHIVE );
	m_pCvarBacktrackTime = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_backtrack_time", "200.0", FCVAR_ARCHIVE );
	m_pCvarDebug = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_debug", "0", FCVAR_ARCHIVE );
	m_pCvarSwitchTarget = gEngfuncs.pfnRegisterVariable( "ebash3d_aimbot_switch_target", "0", FCVAR_ARCHIVE );
	
	// Регистрируем команды для переключения цели
	gEngfuncs.pfnAddCommand( "+ebash3d_aimbot_switch_target", IN_SwitchTargetDown );
	gEngfuncs.pfnAddCommand( "-ebash3d_aimbot_switch_target", IN_SwitchTargetUp );
	
	gHUD.AddHudElem(this);
	return 0;
}

int CHudAimbot::VidInit()
{
	return 1;
}

void CHudAimbot::Reset()
{
	m_bEnabled = false;
	m_bPSilent = false;
	m_bRCSEnabled = false;
	m_bNoRecoil = false;
	m_bPSilentAnglesUpdated = false;
	m_pLastTarget = nullptr;
	m_bHasLastTarget = false;
	m_fLastTargetUpdateTime = 0.0f;
}

void CHudAimbot::Think()
{
	// Обновляем настройки из cvar'ов
	if ( m_pCvarAimbot )
		m_bEnabled = ( m_pCvarAimbot->value != 0.0f );
	if ( m_pCvarPSilent )
		m_bPSilent = ( m_pCvarPSilent->value != 0.0f );
	if ( m_pCvarRCSEnable )
		m_bRCSEnabled = ( m_pCvarRCSEnable->value != 0.0f );
	if ( m_pCvarRCSStrength )
		m_fRCSStrength = m_pCvarRCSStrength->value;
	if ( m_pCvarSmooth )
		m_fSmooth = m_pCvarSmooth->value;
	if ( m_pCvarFOV )
		m_fAimFOV = m_pCvarFOV->value;
	if ( m_pCvarHeight )
		m_fAimHeight = m_pCvarHeight->value;
	if ( m_pCvarAutoShoot )
		m_bAutoShoot = ( m_pCvarAutoShoot->value != 0.0f );
	if ( m_pCvarTargetBone )
		m_iTargetBone = (int)m_pCvarTargetBone->value;
	if ( m_pCvarResetOnRound )
		m_bResetOnRound = ( m_pCvarResetOnRound->value != 0.0f );
	if ( m_pCvarHeadScale )
		m_fHeadScale = m_pCvarHeadScale->value;
	if ( m_pCvarBacktrack )
		m_bBacktrack = ( m_pCvarBacktrack->value != 0.0f );
	if ( m_pCvarBacktrackTime )
		m_fBacktrackTime = m_pCvarBacktrackTime->value;
	if ( m_pCvarDrawFOV )
		m_bDrawFOV = ( m_pCvarDrawFOV->value != 0.0f );
	if ( m_pCvarOnlyShooting )
		m_bOnlyWhenShooting = ( m_pCvarOnlyShooting->value != 0.0f );
	if ( m_pCvarIgnoreVisibility )
		m_bIgnoreVisibility = ( m_pCvarIgnoreVisibility->value != 0.0f );
	if ( m_pCvarNoRecoil )
		m_bNoRecoil = ( m_pCvarNoRecoil->value != 0.0f );
	if ( m_pCvarTriggerBot )
		m_bTriggerBot = ( m_pCvarTriggerBot->value != 0.0f );
	if ( m_pCvarTriggerBotOnetap )
		m_bTriggerBotOnetap = ( m_pCvarTriggerBotOnetap->value != 0.0f );
	if ( m_pCvarWeaponGlow )
		m_bWeaponGlow = ( m_pCvarWeaponGlow->value != 0.0f );
	if ( m_pCvarHandsGlow )
		m_bHandsGlow = ( m_pCvarHandsGlow->value != 0.0f );
	if ( m_pCvarGlowColorR )
		m_fGlowColor[0] = m_pCvarGlowColorR->value;
	if ( m_pCvarGlowColorG )
		m_fGlowColor[1] = m_pCvarGlowColorG->value;
	if ( m_pCvarGlowColorB )
		m_fGlowColor[2] = m_pCvarGlowColorB->value;
	if ( m_pCvarGlowAlpha )
		m_fGlowAlpha = m_pCvarGlowAlpha->value;
	if ( m_pCvarCustomFOV )
		m_bCustomFOV = ( m_pCvarCustomFOV->value != 0.0f );
	if ( m_pCvarCustomFOVValue )
		m_fCustomFOVValue = m_pCvarCustomFOVValue->value;
	
	// Синхронизируем глобальную переменную для No Recoil
	g_NoRecoilEnabled = m_bNoRecoil;
	
	// Применяем отключение анимаций если включено
	if ( m_pCvarDisableAnimations && m_pCvarDisableAnimations->value != 0.0f )
	{
		DisablePlayerAnimations();
	}
	
	// Обновляем настройки третьего лица
	if ( m_pCvarThirdPerson )
		m_bThirdPerson = ( m_pCvarThirdPerson->value != 0.0f );
	if ( m_pCvarThirdPersonDist )
		m_fThirdPersonDist = m_pCvarThirdPersonDist->value;
	
	// Применяем или выключаем третье лицо
	if ( m_bThirdPerson )
	{
		ApplyThirdPerson();
	}
	else
	{
		// Выключаем третье лицо
		extern int cam_thirdperson;
		if ( cam_thirdperson )
		{
			cam_thirdperson = 0;
		}
	}
	
	// Применяем glow эффекты каждый кадр (чтобы они не сбрасывались)
	// Glow для оружия применяется в V_CalcRefdef, но также применяем здесь для надежности
	if ( m_bWeaponGlow )
	{
		cl_entity_t *viewmodel = gEngfuncs.GetViewModel();
		if ( viewmodel && viewmodel->model )
		{
			ApplyWeaponGlow();
		}
	}
	if ( m_bHandsGlow )
	{
		cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
		if ( localPlayer )
		{
			ApplyHandsGlow();
		}
	}
	
	// Применяем No Recoil если включен (каждый кадр)
	if ( m_bNoRecoil )
	{
		ApplyNoRecoil();
	}
	
	// Think() больше не используется для аимбота - вся логика в CL_CreateMove
}

void CHudAimbot::GetAimAngles( float* aimAngles )
{
	// Устаревший метод - углы теперь применяются напрямую в ApplyPSilent
	// Оставляем для совместимости, но не используем
	float viewAngles[3];
	gEngfuncs.GetViewAngles( viewAngles );
	VectorCopy( viewAngles, aimAngles );
}

// Полностью переписанный аимбот и psilent
void CHudAimbot::ApplyPSilent( struct usercmd_s *cmd )
{
	// Обновляем настройки из cvar'ов
	if ( m_pCvarAimbot )
		m_bEnabled = ( m_pCvarAimbot->value != 0.0f );
	if ( m_pCvarPSilent )
		m_bPSilent = ( m_pCvarPSilent->value != 0.0f );
	if ( m_pCvarRCSEnable )
		m_bRCSEnabled = ( m_pCvarRCSEnable->value != 0.0f );
	if ( m_pCvarRCSStrength )
		m_fRCSStrength = m_pCvarRCSStrength->value;
	if ( m_pCvarSmooth )
		m_fSmooth = m_pCvarSmooth->value;
	if ( m_pCvarFOV )
		m_fAimFOV = m_pCvarFOV->value;
	if ( m_pCvarHeight )
		m_fAimHeight = m_pCvarHeight->value;
	if ( m_pCvarOnlyShooting )
		m_bOnlyWhenShooting = ( m_pCvarOnlyShooting->value != 0.0f );
	if ( m_pCvarIgnoreVisibility )
		m_bIgnoreVisibility = ( m_pCvarIgnoreVisibility->value != 0.0f );
	if ( m_pCvarTriggerBot )
		m_bTriggerBot = ( m_pCvarTriggerBot->value != 0.0f );
	if ( m_pCvarTriggerBotOnetap )
		m_bTriggerBotOnetap = ( m_pCvarTriggerBotOnetap->value != 0.0f );
	if ( m_pCvarSpinbot )
		m_bSpinbot = ( m_pCvarSpinbot->value != 0.0f );
	if ( m_pCvarSpinbotSpeed )
		m_fSpinbotSpeed = m_pCvarSpinbotSpeed->value;
	if ( m_pCvarAntiAim )
		m_bAntiAim = ( m_pCvarAntiAim->value != 0.0f );
	if ( m_pCvarAntiAimMode )
		m_iAntiAimMode = (int)m_pCvarAntiAimMode->value;
	if ( m_pCvarAntiAimSpeed )
		m_fAntiAimSpeed = m_pCvarAntiAimSpeed->value;
	if ( m_pCvarAntiAimJitterRange )
		m_fAntiAimJitterRange = m_pCvarAntiAimJitterRange->value;
	if ( m_pCvarAntiAimFakeAngle )
		m_fAntiAimFakeAngle = m_pCvarAntiAimFakeAngle->value;
	if ( m_pCvarAutoShoot )
		m_bAutoShoot = ( m_pCvarAutoShoot->value != 0.0f );
	if ( m_pCvarTargetBone )
		m_iTargetBone = (int)m_pCvarTargetBone->value;
	if ( m_pCvarResetOnRound )
		m_bResetOnRound = ( m_pCvarResetOnRound->value != 0.0f );
	if ( m_pCvarHeadScale )
		m_fHeadScale = m_pCvarHeadScale->value;
	if ( m_pCvarBacktrack )
		m_bBacktrack = ( m_pCvarBacktrack->value != 0.0f );
	if ( m_pCvarBacktrackTime )
		m_fBacktrackTime = m_pCvarBacktrackTime->value;
	if ( m_pCvarDrawFOV )
		m_bDrawFOV = ( m_pCvarDrawFOV->value != 0.0f );
	if ( m_pCvarNoRecoil )
		m_bNoRecoil = ( m_pCvarNoRecoil->value != 0.0f );
	
	// Применяем спинбот ПЕРЕД аимботом (если включен)
	if ( m_bSpinbot )
	{
		ApplySpinbot( cmd );
		return;
	}
	
	// Получаем локального игрока
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if ( !localPlayer )
		return;
	
	// Если игрок мертв, не работаем
	if ( CL_IsDead() || IsPlayerDead( localPlayer ) )
		return;
	
	// Получаем текущие углы обзора из движка (визуальные углы)
	float viewAngles[3];
	gEngfuncs.GetViewAngles( viewAngles );
	
	// Проверяем стрельбу
	g_AimbotIsShooting = IsShooting() || ( cmd->buttons & IN_ATTACK );
	
	// Применяем trigger bot ПЕРЕД аимботом
	if ( m_bTriggerBot )
	{
		ApplyTriggerBot( cmd );
		// Обновляем флаг стрельбы после trigger bot
		g_AimbotIsShooting = IsShooting() || ( cmd->buttons & IN_ATTACK );
	}
	
	// Получаем текущее время
	float currentTime = gEngfuncs.GetClientTime();
	
	// Reset on round - проверяем начало нового раунда
	static float lastRoundTime = 0.0f;
	if ( m_bResetOnRound )
	{
		if ( currentTime < lastRoundTime || (currentTime - lastRoundTime) > 300.0f )
		{
			m_pLastTarget = nullptr;
			m_bHasLastTarget = false;
			m_fLastShotTime = 0.0f;
			m_iCurrentTargetIndex = 0; // Сбрасываем индекс цели при новом раунде
		}
		lastRoundTime = currentTime;
	}
	
	// Если аимбот выключен, применяем только RCS и антиаим
	if ( !m_bEnabled )
	{
		if ( m_bRCSEnabled && g_AimbotIsShooting )
		{
			// Для RCS используем углы из cmd, но обновляем их через движок
			float rcsAngles[3];
			VectorCopy( cmd->viewangles, rcsAngles );
			ApplyRCS( rcsAngles );
			
			if ( m_bPSilent )
			{
				// Silent RCS - только cmd
				cmd->viewangles[PITCH] = rcsAngles[PITCH];
				cmd->viewangles[YAW] = rcsAngles[YAW];
			}
			else
			{
				// Визуальный RCS
				gEngfuncs.SetViewAngles( rcsAngles );
				cmd->viewangles[PITCH] = rcsAngles[PITCH];
				cmd->viewangles[YAW] = rcsAngles[YAW];
			}
		}
		if ( m_bAntiAim && !g_AimbotIsShooting )
		{
			ApplyAntiAim( cmd );
		}
		return;
	}
	
	// Проверяем нажатие кнопки переключения цели
	if ( m_pCvarSwitchTarget && m_pCvarSwitchTarget->value != 0.0f )
	{
		// Проверяем edge trigger (нажатие кнопки)
		// bit 0 = текущее состояние (1 = down, 0 = up)
		// bit 1 = edge triggered on down (up to down transition) - нажатие
		// bit 2 = edge triggered on up (down to up transition) - отпускание
		// В KeyDown устанавливается: state |= 1 + 2 (bit 0 + bit 1)
		// Проверяем bit 1 (edge trigger на нажатие)
		if ( in_switch_target.state & 2 ) // bit 1 = edge triggered on down
		{
			// Переключаем цель (увеличиваем индекс) только один раз при нажатии
			m_iCurrentTargetIndex++;
			AimbotDebugLog( "Switching target (index: %d)\n", m_iCurrentTargetIndex );
			
			// Очищаем edge trigger бит (bit 1), чтобы переключение произошло только один раз
			// Оставляем bit 0 (down state) для отслеживания состояния кнопки
			in_switch_target.state &= ~2;
		}
	}
	
	// Ищем лучшую цель в FOV (используем текущие углы обзора)
	cl_entity_t *target = FindBestTarget( viewAngles, m_fAimFOV );
	
	// ДЕБАГ: Обновляем информацию о silent aim
	if ( m_bPSilent )
	{
		float aimAngles[3] = {0.0f, 0.0f, 0.0f};
		float punchAngles[3] = {0.0f, 0.0f, 0.0f};
		float targetDistance = 0.0f;
		int targetIndex = 0;
		
		if ( target )
		{
			targetIndex = target->index;
			CalculateAimAngles( target, aimAngles );
			
			// Вычисляем расстояние до цели
			cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
			if ( localPlayer )
			{
				float localOrigin[3];
				VectorCopy( localPlayer->origin, localOrigin );
				localOrigin[2] += 17.0f;
				
				float targetPos[3];
				if ( GetPlayerPosition( target, targetPos ) )
				{
					targetPos[2] += 22.0f * m_fHeadScale;
					float dir[3];
					VectorSubtract( targetPos, localOrigin, dir );
					targetDistance = VectorLength( dir );
				}
			}
			
			// Получаем углы отдачи
			GetPunchAngles( punchAngles );
		}
		
		// Обновляем дебаг информацию
		gHUD.m_Debug.UpdateSilentAimDebug( 
			m_bPSilent, 
			(target != nullptr), 
			targetIndex,
			targetDistance,
			aimAngles,
			viewAngles,
			punchAngles
		);
	}
	
	// Проверяем условие: только при стрельбе или всегда
	bool shouldAim = !m_bOnlyWhenShooting || g_AimbotIsShooting;
	
	// Сбрасываем последнюю цель если не стреляем и прошло много времени
	if ( !g_AimbotIsShooting && m_bHasLastTarget )
	{
		float timeSinceUpdate = currentTime - m_fLastTargetUpdateTime;
		if ( timeSinceUpdate > 0.5f )
		{
			m_pLastTarget = nullptr;
			m_bHasLastTarget = false;
		}
	}
	
	// Обновляем последнюю цель
	if ( target && shouldAim )
	{
		m_pLastTarget = target;
		m_bHasLastTarget = true;
		m_fLastTargetUpdateTime = currentTime;
		CalculateAimAngles( target, m_fLastTargetAngles );
	}
	else if ( !target && m_bHasLastTarget && m_pLastTarget )
	{
		// Проверяем, актуальна ли последняя цель
		float timeSinceUpdate = currentTime - m_fLastTargetUpdateTime;
		if ( timeSinceUpdate > 0.2f || IsPlayerDead( m_pLastTarget ) )
		{
			m_pLastTarget = nullptr;
			m_bHasLastTarget = false;
		}
	}
	
	// PSILENT РЕЖИМ - применяем углы ТОЛЬКО к cmd->viewangles (не визуально)
	if ( m_bPSilent )
	{
		float finalAngles[3];
		
		// В silent режиме всегда используем углы из cmd (не визуальные)
		VectorCopy( cmd->viewangles, finalAngles );
		
		// Проверяем, есть ли цель для прицеливания
		bool hasTargetForAiming = (shouldAim && target) || (shouldAim && m_bHasLastTarget && m_pLastTarget && !IsPlayerDead( m_pLastTarget ));
		cl_entity_t* targetToAim = target;
		if ( !targetToAim && m_bHasLastTarget && m_pLastTarget && !IsPlayerDead( m_pLastTarget ) )
		{
			float timeSinceUpdate = currentTime - m_fLastTargetUpdateTime;
			if ( timeSinceUpdate < 1.0f )
			{
				targetToAim = m_pLastTarget;
			}
		}
		
		// В silent режиме обрабатываем autoshoot ПЕРВЫМ, чтобы применить углы перед стрельбой
		if ( m_bAutoShoot && targetToAim && !g_AimbotIsShooting && shouldAim && HasAmmoToShoot() )
		{
			float aimAngles[3];
			CalculateAimAngles( targetToAim, aimAngles );
			
			// Применяем углы прицеливания к cmd->viewangles
			finalAngles[PITCH] = aimAngles[PITCH];
			finalAngles[YAW] = aimAngles[YAW];
			finalAngles[ROLL] = aimAngles[ROLL];
			
			// Применяем RCS если включен
			if ( m_bRCSEnabled )
			{
				ApplyRCS( finalAngles );
			}
			
			// В silent режиме можем мгновенно прицелиться - проверяем угол относительно cmd->viewangles
			// Это более точная проверка, так как в silent режиме визуальные углы не обновляются
			float deltaYaw = aimAngles[YAW] - cmd->viewangles[YAW];
			float deltaPitch = aimAngles[PITCH] - cmd->viewangles[PITCH];
			while ( deltaYaw > 180.0f ) deltaYaw -= 360.0f;
			while ( deltaYaw < -180.0f ) deltaYaw += 360.0f;
			
			// В silent режиме используем более широкий порог (15 градусов) для мгновенного прицеливания
			// Это позволяет стрелять быстрее и точнее
			if ( fabsf( deltaYaw ) < 15.0f && fabsf( deltaPitch ) < 15.0f )
			{
				// Применяем углы к cmd->viewangles
				cmd->viewangles[PITCH] = finalAngles[PITCH];
				cmd->viewangles[YAW] = finalAngles[YAW];
				cmd->viewangles[ROLL] = finalAngles[ROLL];
				
				cmd->buttons |= IN_ATTACK;
				g_AimbotIsShooting = true;
				
				// Сохраняем последнюю цель
				m_pLastTarget = targetToAim;
				m_bHasLastTarget = true;
				VectorCopy( finalAngles, m_fLastTargetAngles );
				m_fLastTargetUpdateTime = currentTime;
				
				// В silent режиме с autoshoot НЕ применяем antiaim, так как мы прицеливаемся
				return;
			}
		}
		
		// Silent aim работает только когда стреляем
		if ( g_AimbotIsShooting )
		{
			bool anglesApplied = false;
			
			// Если должны применять аимбот и есть цель
			if ( shouldAim && targetToAim )
			{
				CalculateAimAngles( targetToAim, finalAngles );
				anglesApplied = true;
				
				// Сохраняем последнюю цель для стабильности
				m_pLastTarget = targetToAim;
				m_bHasLastTarget = true;
				VectorCopy( finalAngles, m_fLastTargetAngles );
				m_fLastTargetUpdateTime = currentTime;
			}
			// Если цель временно пропала, но есть последняя цель, используем ее
			else if ( shouldAim && m_bHasLastTarget && m_pLastTarget && !IsPlayerDead( m_pLastTarget ) )
			{
				float timeSinceUpdate = currentTime - m_fLastTargetUpdateTime;
				if ( timeSinceUpdate < 0.5f ) // Используем последнюю цель до 0.5 секунд
				{
					// Пересчитываем углы для последней цели (она могла двигаться)
					CalculateAimAngles( m_pLastTarget, finalAngles );
					anglesApplied = true;
					VectorCopy( finalAngles, m_fLastTargetAngles );
					m_fLastTargetUpdateTime = currentTime;
				}
			}
			
			// Если углы были применены, используем их
			if ( anglesApplied )
			{
				// КРИТИЧЕСКИ ВАЖНО: Применяем RCS ПЕРЕД применением углов к cmd
				// RCS должен компенсировать накопленную отдачу во время стрельбы
				if ( m_bRCSEnabled )
				{
					ApplyRCS( finalAngles );
				}
				
				// КРИТИЧЕСКИ ВАЖНО: Применяем углы ТОЛЬКО к cmd->viewangles
				// НЕ вызываем SetViewAngles - это предотвращает визуальное наведение камеры
				cmd->viewangles[PITCH] = finalAngles[PITCH];
				cmd->viewangles[YAW] = finalAngles[YAW];
				cmd->viewangles[ROLL] = finalAngles[ROLL];
			}
			// Если нет цели, но стреляем - применяем только RCS для компенсации отдачи
			else if ( m_bRCSEnabled )
			{
				// Применяем RCS к текущим углам cmd для компенсации отдачи
				ApplyRCS( cmd->viewangles );
			}
			
			// Применяем антиаим ТОЛЬКО если нет цели для прицеливания
			// Это позволяет aimbot работать правильно при включенном antiaim
			if ( m_bAntiAim && !anglesApplied )
			{
				ApplyAntiAimYawOnly( cmd );
			}
		}
		else
		{
			// Если не стреляем, применяем антиаим полностью
			if ( m_bAntiAim )
			{
				ApplyAntiAim( cmd );
			}
		}
		
		return; // В psilent режиме всегда выходим (не применяем визуальный аимбот)
	}
	
	// AutoShoot - автоматическая стрельба при нахождении цели (для обычного режима)
	if ( m_bAutoShoot && target && !g_AimbotIsShooting && shouldAim && HasAmmoToShoot() )
	{
		float aimAngles[3];
		CalculateAimAngles( target, aimAngles );
		
		float deltaYaw = aimAngles[YAW] - viewAngles[YAW];
		float deltaPitch = aimAngles[PITCH] - viewAngles[PITCH];
		while ( deltaYaw > 180.0f ) deltaYaw -= 360.0f;
		while ( deltaYaw < -180.0f ) deltaYaw += 360.0f;
		
		if ( fabsf( deltaYaw ) < 2.0f && fabsf( deltaPitch ) < 2.0f )
		{
			cmd->buttons |= IN_ATTACK;
			g_AimbotIsShooting = true;
		}
	}
	
	// ОБЫЧНЫЙ АИМБОТ - визуальное наведение
	if ( !shouldAim )
	{
		// Если не должны применять аимбот, применяем только RCS и антиаим
		if ( m_bRCSEnabled && g_AimbotIsShooting )
		{
			float rcsAngles[3];
			VectorCopy( viewAngles, rcsAngles );
			ApplyRCS( rcsAngles );
			gEngfuncs.SetViewAngles( rcsAngles );
			cmd->viewangles[PITCH] = rcsAngles[PITCH];
			cmd->viewangles[YAW] = rcsAngles[YAW];
		}
		if ( m_bAntiAim && !g_AimbotIsShooting )
		{
			ApplyAntiAim( cmd );
		}
		return;
	}
	
	// Если нет цели, применяем только RCS и антиаим
	if ( !target )
	{
		if ( m_bRCSEnabled && g_AimbotIsShooting )
		{
			float rcsAngles[3];
			VectorCopy( viewAngles, rcsAngles );
			ApplyRCS( rcsAngles );
			gEngfuncs.SetViewAngles( rcsAngles );
			cmd->viewangles[PITCH] = rcsAngles[PITCH];
			cmd->viewangles[YAW] = rcsAngles[YAW];
		}
		if ( m_bAntiAim && !g_AimbotIsShooting )
		{
			ApplyAntiAim( cmd );
		}
		return;
	}
	
	// Вычисляем углы для наведения на цель
	float aimAngles[3];
	CalculateAimAngles( target, aimAngles );
	
	// Применяем плавность если включена
	float finalAngles[3];
	if ( m_fSmooth > 0.0f && m_fSmooth < 100.0f )
	{
		// Плавное наведение
		SmoothAim( viewAngles, aimAngles, finalAngles, m_fSmooth );
	}
	else
	{
		// Мгновенное наведение
		VectorCopy( aimAngles, finalAngles );
	}
	
	// КРИТИЧЕСКИ ВАЖНО: Применяем RCS ПОСЛЕ наведения, если включен и стреляем
	// RCS компенсирует накопленную отдачу во время автоматической стрельбы
	if ( m_bRCSEnabled && g_AimbotIsShooting )
	{
		ApplyRCS( finalAngles );
	}
	
	// ОБЫЧНЫЙ АИМБОТ: Обновляем визуальные углы для визуального наведения
	// Это критически важно для работы обычного аимбота - камера должна двигаться
	gEngfuncs.SetViewAngles( finalAngles );
	cmd->viewangles[PITCH] = finalAngles[PITCH];
	cmd->viewangles[YAW] = finalAngles[YAW];
	cmd->viewangles[ROLL] = finalAngles[ROLL];
	
	// ИСПРАВЛЕНИЕ КОНФЛИКТА: НЕ применяем антиаим во время стрельбы!
	// Антиаим мешает попаданию - отключаем его когда аимбот стреляет
	if ( m_bAntiAim && !g_AimbotIsShooting )
	{
		// Применяем антиаим ТОЛЬКО когда НЕ стреляем
		ApplyAntiAim( cmd );
	}
	// Когда стреляем - антиаим полностью отключен для точного попадания
}

int CHudAimbot::Draw( float flTime )
{
	if ( !m_bEnabled )
		return 0;

	// Отрисовываем FOV круг если включено
	if ( m_bDrawFOV )
	{
		DrawFOVCircle();
	}

	return 0;
}

cl_entity_t* CHudAimbot::FindBestTarget( float* viewAngles, float fov )
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if ( !localPlayer )
		return nullptr;

	// Получаем позицию локального игрока (глаза) - более точная высота
	float localOrigin[3];
	VectorCopy( localPlayer->origin, localOrigin );
	localOrigin[2] += 17.0f; // Высота глаз игрока

	// Вычисляем forward вектор один раз для всех целей
	float forward[3];
	AngleVectors( viewAngles, forward, nullptr, nullptr );
	VectorNormalize( forward );

	// Проверяем, включено ли переключение цели
	bool switchTargetEnabled = ( m_pCvarSwitchTarget && m_pCvarSwitchTarget->value != 0.0f );
	
	// Структура для хранения информации о цели
	struct TargetInfo
	{
		cl_entity_t* ent;
		float score;
		float distance;
		float targetFOV;
		bool isVisible;
	};
	
	// Массив для хранения всех подходящих целей (если включено переключение)
	static TargetInfo targetList[MAX_PLAYERS];
	static int targetListCount = 0;
	
	// Если включено переключение цели, собираем все подходящие цели
	if ( switchTargetEnabled )
	{
		targetListCount = 0; // Сбрасываем счетчик
	}

	cl_entity_t *bestTarget = nullptr;
	float bestScore = -999999.0f; // Используем score вместо отдельных переменных

	// Проходим по всем игрокам
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		cl_entity_t *ent = gEngfuncs.GetEntityByIndex( i );
		if ( !ent )
			continue;

		// Быстрые проверки в начале (самые дешевые)
		if ( !CustomUtils::CheckForPlayer( ent ) )
			continue;

		if ( ent->index == localPlayer->index )
			continue;

		// КРИТИЧЕСКАЯ ПРОВЕРКА: Проверяем кэш позиции ПЕРЕД IsPlayerDead для фильтрации призраков
		// Это самая надежная проверка - кэш инвалидируется при смерти
		// Это предотвращает наведение на призраков (мертвых игроков с остатками хитбоксов)
		if ( !g_PlayerPositionCacheValid[ent->index] )
		{
			// Не логируем каждый кадр - это спам
			continue; // Кэш невалиден - игрок мертв (призрак)
		}

		if ( IsPlayerDead( ent ) )
		{
			// Не логируем каждый кадр - это спам
			continue;
		}
		
		// Проверяем режим deathmatch
		if ( m_pCvarDeathmatch && m_pCvarDeathmatch->value == 0.0f )
		{
			if ( CustomUtils::ArePlayersTeammates( localPlayer->index, ent->index ) )
				continue;
		}

		// Получаем позицию цели
		float targetPos[3];
		if ( !GetPlayerPosition( ent, targetPos ) )
			continue;

		// Вычисляем направление к цели
		float dir[3];
		VectorSubtract( targetPos, localOrigin, dir );
		float distance = VectorLength( dir );
		
		// УВЕЛИЧЕНО максимальное расстояние для работы на дальних дистанциях
		// Было 8192.0f, стало 32768.0f для работы на очень больших картах
		if ( distance > 32768.0f )
		{
			// Не логируем каждый кадр - это спам
			continue;
		}

		// Нормализуем направление
		if ( distance > 0.001f )
		{
			VectorScale( dir, 1.0f / distance, dir );
		}
		else
		{
			continue; // Слишком близко
		}

		// Быстрая проверка FOV через dot product (быстрее чем acos)
		float dot = DotProduct( forward, dir );
		if ( dot < 0.0f ) // Цель за спиной
			continue;

		// Вычисляем FOV (только если dot > 0)
		float targetFOV = acosf( dot ) * ( 180.0f / M_PI_F );
		if ( targetFOV > fov )
		{
			// Не логируем каждый кадр - это спам
			continue;
		}

		// Проверяем видимость (если требуется)
		// В silent режиме пропускаем проверку видимости для стабильности
		// ИСПРАВЛЕНИЕ: ignore visibility не работает на больших расстояниях (более 2000 единиц)
		// Это предотвращает наведение через стены на дальних дистанциях
		bool shouldCheckVisibility = true;
		if ( m_bIgnoreVisibility && distance <= 2000.0f )
		{
			// Ignore visibility включен И цель близко - пропускаем проверку видимости
			shouldCheckVisibility = false;
		}
		
		// Проверяем видимость цели (даже если ignore visibility включен, для приоритизации)
		bool isVisible = false;
		if ( shouldCheckVisibility && !m_bPSilent )
		{
			if ( !IsTargetVisible( ent ) )
			{
				// Не логируем каждый кадр - это спам
				continue;
			}
			isVisible = true;
		}
		else if ( !m_bPSilent )
		{
			// Даже если ignore visibility включен, проверяем видимость для приоритизации
			isVisible = IsTargetVisible( ent );
		}
		else
		{
			// В silent режиме считаем цель видимой для упрощения
			isVisible = true;
		}

		// Вычисляем score для приоритизации
		float score = 0.0f;
		
		// Если включен ignore visibility, выбираем ближайшую цель
		if ( m_bIgnoreVisibility )
		{
			// Используем обратное расстояние как score (ближе = выше score)
			// Используем 1.0 / (1.0 + distance) для нормализации
			score = 1.0f / (1.0f + distance / 1000.0f);
		}
		else
		{
			// Обычная логика: Более высокий score = лучшая цель
			// Приоритет: близость к центру FOV > близость по расстоянию
			float fovScore = (fov - targetFOV) / fov; // 0.0 - 1.0, где 1.0 = центр FOV
			float distanceScore = 1.0f / (1.0f + distance / 1000.0f); // Нормализованное расстояние
			
			// Комбинированный score (FOV важнее расстояния)
			// Улучшенная формула для лучшего выбора цели
			score = fovScore * 0.75f + distanceScore * 0.25f;
			
			// Бонус для целей, которые ближе к центру экрана
			if ( targetFOV < fov * 0.3f ) // Если цель в центральных 30% FOV
			{
				score += 0.2f; // Бонус для более точного выбора
			}
		}
		
		// КРИТИЧЕСКИ ВАЖНО: Видимые цели имеют приоритет над невидимыми
		// Добавляем большой бонус к score для видимых целей
		// Это гарантирует, что видимые цели всегда будут выбраны в первую очередь
		if ( isVisible )
		{
			score += 1.0f; // Большой бонус для видимых целей (гарантирует приоритет)
		}
		
		// Если включено переключение цели, добавляем в список
		if ( switchTargetEnabled )
		{
			if ( targetListCount < MAX_PLAYERS )
			{
				targetList[targetListCount].ent = ent;
				targetList[targetListCount].score = score;
				targetList[targetListCount].distance = distance;
				targetList[targetListCount].targetFOV = targetFOV;
				targetList[targetListCount].isVisible = isVisible;
				targetListCount++;
			}
		}
		else
		{
			// Обычная логика: выбираем лучшую цель
			if ( score > bestScore )
			{
				bestTarget = ent;
				bestScore = score;
				// Логируем только важные события (новая лучшая цель)
				static int lastBestTargetIndex = 0;
				if ( ent->index != lastBestTargetIndex )
				{
					if ( m_bIgnoreVisibility )
					{
						AimbotDebugLog( "New best target (closest): Player %d (distance: %.1f, score: %.3f)\n", 
							ent->index, distance, score );
					}
					else
					{
						AimbotDebugLog( "New best target: Player %d (score: %.3f, distance: %.1f, FOV: %.1f°)\n", 
							ent->index, score, distance, targetFOV );
					}
					lastBestTargetIndex = ent->index;
				}
			}
		}
	}
	
	// Если включено переключение цели, выбираем цель из списка по индексу
	if ( switchTargetEnabled && targetListCount > 0 )
	{
		// Сортируем список по score (лучшие цели первыми)
		// Простая сортировка пузырьком
		for ( int i = 0; i < targetListCount - 1; i++ )
		{
			for ( int j = 0; j < targetListCount - i - 1; j++ )
			{
				if ( targetList[j].score < targetList[j + 1].score )
				{
					TargetInfo temp = targetList[j];
					targetList[j] = targetList[j + 1];
					targetList[j + 1] = temp;
				}
			}
		}
		
		// Выбираем цель по индексу (с зацикливанием)
		int targetIndex = m_iCurrentTargetIndex % targetListCount;
		if ( targetIndex < 0 )
			targetIndex += targetListCount;
		
		bestTarget = targetList[targetIndex].ent;
		bestScore = targetList[targetIndex].score;
		
		// Логируем переключение цели
		static int lastSwitchedTargetIndex = -1;
		if ( targetIndex != lastSwitchedTargetIndex )
		{
			AimbotDebugLog( "Target switched: Player %d (%d/%d, score: %.3f, distance: %.1f, visible: %s)\n", 
				bestTarget->index, targetIndex + 1, targetListCount, bestScore, targetList[targetIndex].distance,
				targetList[targetIndex].isVisible ? "yes" : "no" );
			lastSwitchedTargetIndex = targetIndex;
		}
	}
	else if ( switchTargetEnabled && targetListCount == 0 )
	{
		// Нет целей - сбрасываем индекс
		m_iCurrentTargetIndex = 0;
	}

	// Логируем только когда цель найдена (не спамим "No target found")
	if ( bestTarget )
	{
		// Используем throttled логи для предотвращения спама при частой смене цели
		static int lastTargetIndex = 0;
		if ( bestTarget->index != lastTargetIndex )
		{
			if ( m_bIgnoreVisibility )
			{
				// Вычисляем расстояние для логирования
				float targetPos[3];
				if ( GetPlayerPosition( bestTarget, targetPos ) )
				{
					float dir[3];
					VectorSubtract( targetPos, localOrigin, dir );
					float distance = VectorLength( dir );
					AimbotDebugLog( "Best target selected (closest): Player %d (distance: %.1f)\n", 
						bestTarget->index, distance );
				}
				else
				{
					AimbotDebugLog( "Best target selected (closest): Player %d\n", bestTarget->index );
				}
			}
			else
			{
				AimbotDebugLog( "Best target selected: Player %d (score: %.3f)\n", bestTarget->index, bestScore );
			}
			lastTargetIndex = bestTarget->index;
		}
	}

	return bestTarget;
}

void CHudAimbot::CalculateAimAngles( cl_entity_t* target, float* aimAngles )
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if ( !localPlayer || !target )
	{
		gEngfuncs.GetViewAngles( aimAngles );
		return;
	}

	// Получаем позицию локального игрока (глаза)
	float localOrigin[3];
	VectorCopy( localPlayer->origin, localOrigin );
	localOrigin[2] += 17.0f; // Высота глаз игрока

	// Получаем позицию цели - используем текущую позицию напрямую для точности
	float targetPos[3];
	
	// Приоритет: текущая позиция > prevstate > кэш
	if ( !(target->origin[0] == 0.0f && target->origin[1] == 0.0f && target->origin[2] == 0.0f) )
	{
		VectorCopy( target->origin, targetPos );
	}
	else if ( !GetPlayerPosition( target, targetPos ) )
	{
		gEngfuncs.GetViewAngles( aimAngles );
		return;
	}

	// ПРИНУДИТЕЛЬНО ТОЛЬКО В ГОЛОВУ! Игнорируем target bone
	// Всегда целимся в голову независимо от настроек
	// ИСПРАВЛЕННАЯ высота для точного попадания в центр головы (не выше)
	// 19.0f - оптимальная высота для большинства моделей CS (уменьшено для точности)
	float boneHeight = 19.0f * m_fHeadScale;
	
	// Вычисляем предварительное расстояние для коррекции высоты
	float tempDir[3];
	VectorSubtract( targetPos, localOrigin, tempDir );
	float preDistance = VectorLength( tempDir );
	
	// Дополнительная коррекция на основе расстояния для большей точности
	if ( preDistance > 0.001f )
	{
		// На больших дистанциях немного уменьшаем высоту для компенсации
		// Это помогает избежать промахов выше головы на дальних дистанциях
		if ( preDistance > 1500.0f )
		{
			// На больших дистанциях уменьшаем высоту на 1-2 единицы для точности
			float distanceFactor = (preDistance - 1500.0f) / 6692.0f; // Нормализуем от 0 до 1
			if ( distanceFactor > 1.0f ) distanceFactor = 1.0f;
			boneHeight -= distanceFactor * 1.5f; // Максимальное уменьшение 1.5 единицы
		}
	}
	
	// ОСТАВЛЯЕМ СТАРЫЙ КОД ДЛЯ СОВМЕСТИМОСТИ (НО НЕ ИСПОЛЬЗУЕМ)
	/*
	switch ( m_iTargetBone )
	{
		case 0: // Head
			boneHeight = 17.0f * m_fHeadScale;
			break;
		case 1: // Chest
			boneHeight = 0.0f;
			break;
		case 2: // Body
			boneHeight = -10.0f;
			break;
		default:
			boneHeight = m_fAimHeight;
			break;
	}
	*/
	
	// Применяем высоту прицеливания
	targetPos[2] += boneHeight;

	// Вычисляем направление к цели
	float dir[3];
	dir[0] = targetPos[0] - localOrigin[0];
	dir[1] = targetPos[1] - localOrigin[1];
	dir[2] = targetPos[2] - localOrigin[2];
	
	// Вычисляем расстояние
	float distanceSquared = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
	
	if ( distanceSquared < 0.000001f )
	{
		gEngfuncs.GetViewAngles( aimAngles );
		return;
	}
	
	float distance = sqrtf( distanceSquared );

	// Нормализуем вектор направления
	float invDistance = 1.0f / distance;
	dir[0] *= invDistance;
	dir[1] *= invDistance;
	dir[2] *= invDistance;
		
	// ТОЧНОЕ ВЫЧИСЛЕНИЕ УГЛОВ
	// YAW (горизонталь) - atan2(y, x) дает правильный угол
	// Используем atan2f для максимальной точности
	float yaw = atan2f( dir[1], dir[0] ) * ( 180.0f / M_PI_F );
	
	// PITCH (вертикаль) - asin для вертикального угла
	// В Half-Life: положительный pitch = вниз, отрицательный = вверх
	// dir[2] положительный когда цель выше, отрицательный когда ниже
	float pitchDir = -dir[2]; // Инвертируем для правильного знака
	
	// Ограничиваем значение для asin (должно быть в диапазоне [-1, 1])
	if ( pitchDir > 1.0f ) pitchDir = 1.0f;
	if ( pitchDir < -1.0f ) pitchDir = -1.0f;
	
	// Вычисляем pitch с максимальной точностью
	float pitch = asinf( pitchDir ) * ( 180.0f / M_PI_F );
	
	// УБРАНА коррекция для больших дистанций - она вызывала неточности
	// На больших дистанциях используем точные углы без дополнительных коррекций
		
	aimAngles[YAW] = yaw;
	aimAngles[PITCH] = pitch;
	aimAngles[ROLL] = 0.0f;

	// Нормализуем углы
	if ( aimAngles[PITCH] > 89.0f )
		aimAngles[PITCH] = 89.0f;
	else if ( aimAngles[PITCH] < -89.0f )
		aimAngles[PITCH] = -89.0f;

	// Нормализация YAW в диапазон -180..180
	while ( aimAngles[YAW] > 180.0f )
		aimAngles[YAW] -= 360.0f;
	while ( aimAngles[YAW] < -180.0f )
		aimAngles[YAW] += 360.0f;
}

void CHudAimbot::SmoothAim( float* currentAngles, float* targetAngles, float* outputAngles, float smooth )
{
	// Вычисляем разницу углов
	float deltaPitch = targetAngles[PITCH] - currentAngles[PITCH];
	float deltaYaw = targetAngles[YAW] - currentAngles[YAW];

	// Нормализуем разницу YAW (короткий путь)
	while ( deltaYaw > 180.0f )
		deltaYaw -= 360.0f;
	while ( deltaYaw < -180.0f )
		deltaYaw += 360.0f;

	// Вычисляем фактор плавности (smooth от 0 до 100, где 0 = мгновенно, 100 = очень плавно)
	// Преобразуем smooth в коэффициент от 0.0 (мгновенно) до 0.99 (очень плавно)
	float smoothFactor = 1.0f - ( smooth / 100.0f );
	
	// Ограничиваем фактор плавности
	if ( smoothFactor < 0.01f )
		smoothFactor = 0.01f;
	if ( smoothFactor > 1.0f )
		smoothFactor = 1.0f;
	
	// Применяем плавное наведение
	outputAngles[PITCH] = currentAngles[PITCH] + deltaPitch * smoothFactor;
	outputAngles[YAW] = currentAngles[YAW] + deltaYaw * smoothFactor;
	outputAngles[ROLL] = 0.0f;

	// Нормализуем углы
	if ( outputAngles[PITCH] > 89.0f )
		outputAngles[PITCH] = 89.0f;
	if ( outputAngles[PITCH] < -89.0f )
		outputAngles[PITCH] = -89.0f;

	while ( outputAngles[YAW] > 180.0f )
		outputAngles[YAW] -= 360.0f;
	while ( outputAngles[YAW] < -180.0f )
		outputAngles[YAW] += 360.0f;
}

bool CHudAimbot::IsTargetInFOV( float* viewAngles, float* targetPos, float fov )
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if ( !localPlayer )
		return false;

	float localOrigin[3];
	VectorCopy( localPlayer->origin, localOrigin );
	localOrigin[2] += 17.0f;

	float dir[3];
	VectorSubtract( targetPos, localOrigin, dir );
	float distance = VectorLength( dir );
	
	// Проверяем минимальное расстояние
	if ( distance < 1.0f )
		return false;

	// Нормализуем вектор направления
	// На больших дистанциях это критически важно для точности
	if ( distance > 0.001f )
	{
		VectorNormalize( dir );
	}
	else
	{
		return false; // Слишком близко или нулевое расстояние
	}

	// Вычисляем направление взгляда
	float forward[3];
	AngleVectors( viewAngles, forward, nullptr, nullptr );
	
	// Нормализуем направление взгляда
	float forwardLength = VectorLength( forward );
	if ( forwardLength > 0.001f )
	{
		VectorNormalize( forward );
	}
	else
	{
		return false; // Некорректное направление взгляда
	}

	// Вычисляем угол между направлением взгляда и направлением к цели
	float dot = DotProduct( forward, dir );
	
	// Ограничиваем dot product в диапазоне [-1, 1] для корректного acos
	if ( dot > 1.0f ) dot = 1.0f;
	if ( dot < -1.0f ) dot = -1.0f;
	
	// Вычисляем угол в градусах
	float angle = acosf( dot ) * ( 180.0f / M_PI_F );

	// Проверяем, находится ли цель в FOV
	// Для FOV >= 180 градусов (360 FOV) - всегда возвращаем true (цель всегда в FOV)
	// Для FOV < 180 градусов - проверяем обычным способом
	if ( fov >= 180.0f )
		return true; // 360 FOV - цель всегда в FOV
	
	// Обычная проверка для FOV < 180 градусов
	return angle <= fov;
}

bool CHudAimbot::IsTargetVisible( cl_entity_t* target )
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if ( !localPlayer )
		return false;

	float localOrigin[3];
	VectorCopy( localPlayer->origin, localOrigin );
	localOrigin[2] += 17.0f; // Высота глаз игрока

	// Получаем позицию цели (с обходом блокировки ESP)
	float targetPos[3];
	if ( !GetPlayerPosition( target, targetPos ) )
	{
		// Не логируем каждый кадр - это спам
		return false; // Не удалось получить позицию - цель не видима
	}
	targetPos[2] += m_fAimHeight; // Высота цели

	float dir[3];
	VectorSubtract( targetPos, localOrigin, dir );
	float distance = VectorLength( dir );

	if ( distance < 1.0f )
		return false;

	// Нормализуем направление
	float normalizedDir[3];
	VectorCopy( dir, normalizedDir );
	VectorNormalize( normalizedDir );

	// Для trace используем точное расстояние до цели, но с ограничением на максимум
	// На очень больших дистанциях используем фиксированное большое значение
	float traceDistance = distance;
	// УВЕЛИЧЕНО максимальная дистанция trace для работы на огромных дистанциях
	// Было 16384.0f, стало 32768.0f для работы на очень больших картах
	if ( traceDistance > 32768.0f )
		traceDistance = 32768.0f; // Увеличена максимальная дистанция для trace
	
	// Вычисляем конечную точку trace
	float traceStart[3], traceEnd[3];
	VectorCopy( localOrigin, traceStart );
	VectorMA( localOrigin, traceDistance, normalizedDir, traceEnd );

	// Делаем trace для проверки видимости
	// Используем hull 0 (точка) для более точной проверки на больших дистанциях
	pmtrace_t traceData;
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( localPlayer->index - 1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 0 ); // Используем hull 0 (точка) для точной проверки
	gEngfuncs.pEventAPI->EV_PlayerTrace(
		traceStart, traceEnd, PM_NORMAL,
		-1, &traceData
	);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	// Проверяем, попали ли мы в нужного игрока
	if ( traceData.ent > 0 )
	{
		int hitEntityIndex = gEngfuncs.pEventAPI->EV_IndexFromTrace( &traceData );
		if ( hitEntityIndex == target->index )
		{
		// КРИТИЧЕСКАЯ ПРОВЕРКА: Убеждаемся, что цель все еще жива
		// Даже если trace попал в цель, она может быть мертва (остаток хитбокса)
		if ( IsPlayerDead( target ) )
		{
			// Логируем только важные события (призрак обнаружен)
			AimbotDebugLogThrottled( "IsTargetVisible: Player %d - trace hit but target is dead (ghost)\n", target->index );
			return false; // Цель мертва - не видима
		}
		
		// Не логируем каждый кадр - это спам
		return true; // Попали в нужного игрока и он жив
		}
	}

	// Если trace прошел достаточно далеко, проверяем расстояние от точки попадания до цели
	// На больших дистанциях используем более мягкую проверку
	if ( traceData.fraction >= 0.90f )
	{
		// Вычисляем расстояние от точки попадания до цели
		float hitPos[3];
		VectorCopy( traceData.endpos, hitPos );
		float distToTarget[3];
		VectorSubtract( targetPos, hitPos, distToTarget );
		float distToTargetLength = VectorLength( distToTarget );
		
		// Порог зависит от дистанции - на больших дистанциях допускаем большую погрешность
		// Это критически важно для работы на огромных дистанциях
		// УЛУЧШЕНО: Добавлены дополнительные пороги для очень больших дистанций
		float threshold = 32.0f; // Базовый порог для близких дистанций
		if ( distance > 200.0f )
			threshold = 64.0f;
		if ( distance > 500.0f )
			threshold = 128.0f; // На больших дистанциях
		if ( distance > 1000.0f )
			threshold = 200.0f; // На очень больших дистанциях
		if ( distance > 2000.0f )
			threshold = 300.0f; // На огромных дистанциях
		if ( distance > 5000.0f )
			threshold = 500.0f; // На экстремально больших дистанциях
		if ( distance > 10000.0f )
			threshold = 800.0f; // На очень экстремально больших дистанциях
		if ( distance > 20000.0f )
			threshold = 1200.0f; // На максимальных дистанциях
		
		// Если точка попадания достаточно близка к цели, считаем цель видимой
		if ( distToTargetLength < threshold )
		{
			// Не логируем каждый кадр - это спам
			return true;
		}
	}

	// Если trace прошел почти до конца (98%+) без препятствий, считаем цель видимой
	// Это помогает на очень больших дистанциях, где точность позиции может быть низкой
	if ( traceData.fraction >= 0.98f )
	{
		// Не логируем каждый кадр - это спам
		return true;
	}
	
	// Дополнительная проверка: если trace прошел более 95% и дистанция очень большая,
	// считаем цель видимой (на огромных дистанциях даже небольшие препятствия могут блокировать trace)
	// УЛУЧШЕНО: Добавлены дополнительные проверки для очень больших дистанций
	if ( traceData.fraction >= 0.95f && distance > 1000.0f )
	{
		return true;
	}
	
	// Дополнительная проверка для экстремально больших дистанций (более 5000 единиц)
	// На таких дистанциях даже небольшие препятствия могут блокировать trace
	if ( traceData.fraction >= 0.90f && distance > 5000.0f )
	{
		return true;
	}
	
	// Дополнительная проверка для максимальных дистанций (более 10000 единиц)
	// На таких дистанциях используем более мягкую проверку
	if ( traceData.fraction >= 0.85f && distance > 10000.0f )
	{
		// Не логируем каждый кадр - это спам
		return true;
	}

	// Не логируем каждый кадр - это спам
	return false;
}

void CHudAimbot::DrawFOVCircle()
{
	int centerX = ScreenWidth / 2;
	int centerY = ScreenHeight / 2;

	// Для FOV >= 180 градусов (360 FOV) рисуем полный экран
	int screenRadius;
	if ( m_fAimFOV >= 180.0f )
	{
		// Для 360 FOV рисуем круг на весь экран
		screenRadius = (int)sqrtf( (float)( ScreenWidth * ScreenWidth + ScreenHeight * ScreenHeight ) ) / 2;
	}
	else
	{
		screenRadius = (int)( ScreenHeight * 0.015f * m_fAimFOV );
		if ( screenRadius < 20 )
			screenRadius = 20;
		if ( screenRadius > ScreenHeight / 2 )
			screenRadius = ScreenHeight / 2;
	}

	int segments = 128;
	float angleStep = ( 2.0f * M_PI_F ) / segments;

	for ( int i = 0; i < segments; i++ )
	{
		float angle = i * angleStep;
		float nextAngle = ( i + 1 ) * angleStep;

		int x1 = centerX + (int)( screenRadius * cosf( angle ) );
		int y1 = centerY + (int)( screenRadius * sinf( angle ) );
		int x2 = centerX + (int)( screenRadius * cosf( nextAngle ) );
		int y2 = centerY + (int)( screenRadius * sinf( nextAngle ) );

		int dx = x2 - x1;
		int dy = y2 - y1;
		int len = (int)sqrtf( (float)( dx * dx + dy * dy ) );
		
		if ( len > 0 )
		{
			for ( int j = 0; j <= len; j++ )
			{
				int px = x1 + ( dx * j ) / len;
				int py = y1 + ( dy * j ) / len;
				
				if ( px >= 0 && px < ScreenWidth && py >= 0 && py < ScreenHeight )
				{
					FillRGBABlend( px, py, 2, 2, 255, 255, 0, 180 );
				}
			}
		}
	}
}

bool CHudAimbot::IsShooting()
{
	// Проверяем состояние кнопки атаки
	return ( in_attack.state & 1 ) != 0;
}

bool CHudAimbot::HasAmmoToShoot()
{
	// Получаем ID текущего оружия
	int weaponId = HUD_GetWeapon();
	if ( weaponId <= 0 )
		return false; // Нет оружия
	
	// Получаем структуру оружия через WeaponsResource
	WEAPON *pWeapon = gWR.GetWeapon( weaponId );
	if ( !pWeapon )
		return false; // Оружие не найдено
	
	// Проверяем, является ли оружие ножом или гранатой (не требует патронов)
	// WEAPON_NOCLIP обычно равен -1 для оружия без магазина
	if ( pWeapon->iAmmoType == -1 || pWeapon->iClip == -1 )
		return true; // Нож, граната и т.д. - можно "стрелять"
	
	// КРИТИЧЕСКИ ВАЖНО: Проверяем наличие патронов ТОЛЬКО в магазине
	// Если магазин пуст, НЕ стреляем, даже если есть патроны в инвентаре
	// Это позволяет игроку перезарядиться вручную
	if ( pWeapon->iClip > 0 )
		return true; // Есть патроны в магазине - можно стрелять
	
	// Магазин пуст - не стреляем (игрок может перезарядиться)
	return false;
}

bool CHudAimbot::IsPlayerDead( cl_entity_t* ent )
{
	if ( !ent )
		return true;
	
	// БАЗОВАЯ ПРОВЕРКА #1: Проверяем, что это действительно игрок
	if ( !ent->player )
		return true;
	
	// БАЗОВАЯ ПРОВЕРКА #2: Проверяем индекс
	if ( ent->index < 1 || ent->index > MAX_PLAYERS )
		return true;
	
	extern extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS+1];
	
	// КРИТИЧЕСКАЯ ПРОВЕРКА #0: Проверяем кэш позиции (САМАЯ НАДЕЖНАЯ ПРОВЕРКА)
	// Если кэш невалиден (инвалидирован при смерти), игрок мертв
	// ЭТО САМАЯ НАДЕЖНАЯ ПРОВЕРКА - кэш инвалидируется в DeathMsg и ScoreAttrib
	// Это предотвращает наведение на призраков (мертвых игроков с остатками хитбоксов)
	if ( !g_PlayerPositionCacheValid[ent->index] )
	{
		// Не логируем каждый кадр - это спам
		return true; // Кэш невалиден - игрок мертв (призрак)
	}
	
	// КРИТИЧЕСКАЯ ПРОВЕРКА #1: Проверяем поле dead в g_PlayerExtraInfo
	// Это самый надежный способ - сервер явно устанавливает это поле через флаг PLAYER_DEAD
	// Если dead == true, игрок точно мертв
	// ВАЖНО: проверяем только если dead == true, не блокируем если dead == false или не инициализировано
	if ( g_PlayerExtraInfo[ent->index].dead == true )
	{
		// Не логируем каждый кадр - это спам
		return true; // Игрок мертв
	}
	
	// КРИТИЧЕСКАЯ ПРОВЕРКА #2: Проверяем spectator флаг
	// Спектаторы не являются живыми игроками
	// ВАЖНО: проверяем только если spectator == true (1), не блокируем если 0 или не инициализировано
	if ( ent->curstate.spectator == 1 )
		return true;
	
	// КРИТИЧЕСКАЯ ПРОВЕРКА #3: Проверяем модель игрока - призраки могут иметь другую модель
	// Призраки (мертвые игроки) часто имеют модель "models/player.mdl" или другую базовую модель
	// Проверяем, что модель не является базовой моделью призрака
	if ( ent->model && ent->model->name )
	{
		const char* modelName = ent->model->name;
		// Если модель - базовая модель призрака (без конкретного игрока), это может быть призрак
		// Но это не всегда надежно, поэтому проверяем только в комбинации с другими признаками
		if ( strstr( modelName, "player.mdl" ) && !strstr( modelName, "/" ) )
		{
			// Базовая модель без пути - может быть призрак, но проверяем вместе с другими признаками
			if ( g_PlayerExtraInfo[ent->index].dead == true )
				return true;
		}
	}
	
	// КРИТИЧЕСКАЯ ПРОВЕРКА #4: Проверяем эффекты рендеринга - призраки часто имеют прозрачность
	// Если игрок имеет эффект прозрачности (kRenderTransAdd, kRenderTransAlpha) и мертв - это призрак
	if ( ent->curstate.rendermode == kRenderTransAdd || 
	     ent->curstate.rendermode == kRenderTransAlpha )
	{
		// Прозрачный рендер - может быть призрак, проверяем вместе с dead флагом
		if ( g_PlayerExtraInfo[ent->index].dead == true )
			return true;
	}
	
	// ПРОВЕРКА #5: Нулевая позиция (только если все координаты нулевые)
	// ОСТОРОЖНО: проверяем только если dead == true
	if ( g_PlayerExtraInfo[ent->index].dead == true && 
	     ent->origin[0] == 0.0f && ent->origin[1] == 0.0f && ent->origin[2] == 0.0f )
	{
		// Мертв И нулевая позиция - точно мертв
		return true;
	}
	
	// Если все проверки пройдены - игрок ЖИВ
	return false;
}

bool CHudAimbot::GetPlayerPosition( cl_entity_t* ent, float* origin )
{
	if ( !ent || !origin )
		return false;
	
	// ОБХОД БЛОКИРОВКИ ESP: Получаем позицию игрока с использованием альтернативных источников
	bool isBlockedByPlugin = (ent->curstate.solid == SOLID_NOT);
	bool originSet = false;
	
	if (isBlockedByPlugin)
	{
		// Способ 1: Используем prevstate (предыдущее состояние до блокировки)
		if (ent->prevstate.solid != SOLID_NOT && 
		    !(ent->prevstate.origin[0] == 0.0f && ent->prevstate.origin[1] == 0.0f && ent->prevstate.origin[2] == 0.0f))
		{
			VectorCopy(ent->prevstate.origin, origin);
			originSet = true;
		}
		// Способ 2: Используем baseline (исходное состояние)
		else if (ent->baseline.solid != SOLID_NOT &&
		         !(ent->baseline.origin[0] == 0.0f && ent->baseline.origin[1] == 0.0f && ent->baseline.origin[2] == 0.0f))
		{
			VectorCopy(ent->baseline.origin, origin);
			originSet = true;
		}
		// Способ 3: Используем кэшированную позицию (самый надежный метод)
		// ИСПРАВЛЕНИЕ: Уменьшено время валидности кэша с 5 до 2 секунд (кэш инвалидируется при смерти)
		else if (g_PlayerPositionCacheValid[ent->index] && 
		         (gEngfuncs.GetClientTime() - g_PlayerPositionCacheTime[ent->index]) < 2.0f)
		{
			origin[0] = g_PlayerPositionCache[ent->index * 3 + 0];
			origin[1] = g_PlayerPositionCache[ent->index * 3 + 1];
			origin[2] = g_PlayerPositionCache[ent->index * 3 + 2];
			originSet = true;
		}
		// Способ 4: Используем текущую позицию даже если она заблокирована
		else if (!(ent->origin[0] == 0.0f && ent->origin[1] == 0.0f && ent->origin[2] == 0.0f))
		{
			VectorCopy(ent->origin, origin);
			originSet = true;
		}
		// Способ 5: Используем историю позиций
		else if (ent->player && ent->current_position >= 0)
		{
			// HISTORY_MASK и HISTORY_MAX определены в cl_entity.h
			int histIndex = (ent->current_position - 1) & 63; // HISTORY_MASK = 63
			if (histIndex >= 0 && histIndex < 64) // HISTORY_MAX = 64
			{
				position_history_t *ph = &ent->ph[histIndex];
				if (ph && !(ph->origin[0] == 0.0f && ph->origin[1] == 0.0f && ph->origin[2] == 0.0f))
				{
					VectorCopy(ph->origin, origin);
					originSet = true;
				}
			}
		}
	}
	
	if (!originSet)
	{
		// Используем текущую позицию (не заблокирована)
		VectorCopy(ent->origin, origin);
	}
	
	// Проверяем валидность позиции
	if (origin[0] == 0.0f && origin[1] == 0.0f && origin[2] == 0.0f)
		return false;
	
	return true;
}

// Статические переменные для отслеживания изменений отдачи плагином
static float g_LastPunchAngles[3] = {0.0f, 0.0f, 0.0f};
static float g_OriginalPunchAngles[3] = {0.0f, 0.0f, 0.0f};
static bool g_bTrackingRecoil = false;

// Статические переменные для накопления компенсации отдачи
static float g_AccumulatedRecoilPitch = 0.0f;
static float g_AccumulatedRecoilYaw = 0.0f;
static float g_LastRecoilCompensationTime = 0.0f;
static float g_LastRCSFramePunch[3] = {0.0f, 0.0f, 0.0f};

void CHudAimbot::GetPunchAngles( float* punchAngles )
{
	// Обнуляем углы по умолчанию
	punchAngles[0] = 0.0f;
	punchAngles[1] = 0.0f;
	punchAngles[2] = 0.0f;
	
	// Приоритет 1: ev_punchangle (более актуальный, обновляется каждый кадр)
	// ev_punchangle это глобальная переменная из view.cpp
	float* evPunch = (float*)&ev_punchangle;
	
	float currentPunch[3] = {0.0f, 0.0f, 0.0f};
	bool hasPunch = false;
	
	// Используем ev_punchangle если он не нулевой
	if ( fabsf( evPunch[0] ) > 0.001f || fabsf( evPunch[1] ) > 0.001f )
	{
		currentPunch[0] = evPunch[0];
		currentPunch[1] = evPunch[1];
		currentPunch[2] = evPunch[2];
		hasPunch = true;
	}
	
	// Приоритет 2: pmove->punchangle (может быть устаревшим, но все же использовать)
	if ( !hasPunch && pmove )
	{
		// pmove->punchangle это vec3_t (Vector после util_vector.h)
		// Vector имеет operator float*(), используем приведение типа
		float* pmovePunch = (float*)pmove->punchangle;
		
		if ( fabsf( pmovePunch[0] ) > 0.001f || fabsf( pmovePunch[1] ) > 0.001f )
		{
			currentPunch[0] = pmovePunch[0];
			currentPunch[1] = pmovePunch[1];
			currentPunch[2] = pmovePunch[2];
			hasPunch = true;
		}
	}
	
	if ( !hasPunch )
	{
		// Нет отдачи - сбрасываем отслеживание
		g_bTrackingRecoil = false;
		VectorCopy( currentPunch, g_LastPunchAngles );
		VectorCopy( currentPunch, punchAngles );
		return;
	}
	
	// ОБХОД ПЛАГИНА RECOIL_MANAGER:
	// Плагин умножает punchangle на коэффициент (например, 0.90 для AK47/M4A1)
	// Отслеживаем изменения и вычисляем оригинальное значение
	
	// Проверяем, изменилась ли отдача (новый выстрел)
	float punchDelta = fabsf(currentPunch[0] - g_LastPunchAngles[0]) + 
	                   fabsf(currentPunch[1] - g_LastPunchAngles[1]);
	
	if ( punchDelta > 0.01f && g_bTrackingRecoil )
	{
		// Отдача изменилась - плагин мог применить коэффициент
		// Вычисляем коэффициент изменения
		float ratioX = (g_LastPunchAngles[0] != 0.0f) ? (currentPunch[0] / g_LastPunchAngles[0]) : 1.0f;
		float ratioY = (g_LastPunchAngles[1] != 0.0f) ? (currentPunch[1] / g_LastPunchAngles[1]) : 1.0f;
		
		// Если коэффициент близок к известным значениям плагина (0.90, 1.0), восстанавливаем оригинал
		if ( fabsf(ratioX - 0.90f) < 0.05f || fabsf(ratioY - 0.90f) < 0.05f )
		{
			// Плагин применил коэффициент 0.90 - восстанавливаем оригинальное значение
			punchAngles[0] = currentPunch[0] / 0.90f;
			punchAngles[1] = currentPunch[1] / 0.90f;
			punchAngles[2] = currentPunch[2] / 0.90f;
		}
		else if ( fabsf(ratioX - 1.0f) > 0.05f || fabsf(ratioY - 1.0f) > 0.05f )
		{
			// Плагин применил другой коэффициент - пытаемся восстановить
			// Используем средний коэффициент
			float avgRatio = (ratioX + ratioY) / 2.0f;
			if ( avgRatio > 0.5f && avgRatio < 1.5f )
			{
				punchAngles[0] = currentPunch[0] / avgRatio;
				punchAngles[1] = currentPunch[1] / avgRatio;
				punchAngles[2] = currentPunch[2] / avgRatio;
			}
			else
			{
				// Неизвестный коэффициент - используем текущее значение
				VectorCopy( currentPunch, punchAngles );
			}
		}
		else
		{
			// Коэффициент 1.0 или нет изменений - используем текущее значение
			VectorCopy( currentPunch, punchAngles );
		}
		
		// Сохраняем оригинальное значение для следующей итерации
		VectorCopy( punchAngles, g_OriginalPunchAngles );
	}
	else if ( !g_bTrackingRecoil )
	{
		// Первое обнаружение отдачи - сохраняем как оригинальное
		VectorCopy( currentPunch, g_OriginalPunchAngles );
		VectorCopy( currentPunch, punchAngles );
		g_bTrackingRecoil = true;
	}
	else
	{
		// Используем сохраненное оригинальное значение
		VectorCopy( g_OriginalPunchAngles, punchAngles );
	}
	
	// Обновляем последние значения
	VectorCopy( currentPunch, g_LastPunchAngles );
}

void CHudAimbot::ApplyRCS( float* viewAngles )
{
	if ( !g_AimbotIsShooting )
	{
		// Не стреляем - сбрасываем накопленную отдачу
		g_AccumulatedRecoilPitch = 0.0f;
		g_AccumulatedRecoilYaw = 0.0f;
		g_LastRecoilCompensationTime = 0.0f;
		VectorCopy( viewAngles, g_LastRCSFramePunch );
		return;
	}
	
	float punchAngles[3];
	GetPunchAngles( punchAngles );
	
	// Проверяем, есть ли отдача для компенсации
	if ( fabsf( punchAngles[PITCH] ) < 0.001f && fabsf( punchAngles[YAW] ) < 0.001f )
	{
		// Нет отдачи - сбрасываем накопленную отдачу
		g_AccumulatedRecoilPitch = 0.0f;
		g_AccumulatedRecoilYaw = 0.0f;
		VectorCopy( punchAngles, g_LastRCSFramePunch );
		return;
	}
	
	// КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Используем АБСОЛЮТНОЕ значение отдачи для компенсации
	// Отдача накапливается с каждым выстрелом, поэтому punchAngles уже содержит всю накопленную отдачу
	// НЕ нужно отслеживать изменения - просто компенсируем текущую накопленную отдачу
	
	// Вычисляем изменение отдачи с последнего кадра для отслеживания новых выстрелов
	float deltaPitch = punchAngles[PITCH] - g_LastRCSFramePunch[PITCH];
	float deltaYaw = punchAngles[YAW] - g_LastRCSFramePunch[YAW];
	
	// Если отдача увеличилась (новый выстрел), обновляем накопленную отдачу
	// Pitch увеличивается при отдаче (отдача вверх = положительный pitch)
	if ( deltaPitch > 0.01f || fabsf(deltaYaw) > 0.01f )
	{
		// Новый выстрел - обновляем накопленную отдачу до текущего значения
		// punchAngles уже содержит всю накопленную отдачу от всех выстрелов
		g_AccumulatedRecoilPitch = punchAngles[PITCH];
		g_AccumulatedRecoilYaw = punchAngles[YAW];
		g_LastRecoilCompensationTime = gEngfuncs.GetClientTime();
	}
	// Если отдача уменьшилась (отдача затухает), обновляем накопленную отдачу
	else if ( deltaPitch < -0.01f )
	{
		// Отдача затухает - обновляем накопленную отдачу до текущего значения
		g_AccumulatedRecoilPitch = punchAngles[PITCH];
		if ( g_AccumulatedRecoilPitch < 0.0f )
			g_AccumulatedRecoilPitch = 0.0f;
		g_AccumulatedRecoilYaw = punchAngles[YAW];
	}
	
	// Применяем компенсацию НАКОПЛЕННОЙ отдачи (абсолютное значение)
	// Это критически важно для автоматической стрельбы - компенсируем всю накопленную отдачу
	// Используем полную силу компенсации для правильной работы
	float compensationPitch = g_AccumulatedRecoilPitch * m_fRCSStrength;
	float compensationYaw = g_AccumulatedRecoilYaw * m_fRCSStrength;
	
	// Применяем компенсацию к углам
	viewAngles[PITCH] -= compensationPitch;
	viewAngles[YAW] -= compensationYaw;
	
	// Сохраняем текущие углы отдачи для следующего кадра RCS
	VectorCopy( punchAngles, g_LastRCSFramePunch );
	
	// Нормализуем углы
	if ( viewAngles[PITCH] > 89.0f )
		viewAngles[PITCH] = 89.0f;
	if ( viewAngles[PITCH] < -89.0f )
		viewAngles[PITCH] = -89.0f;
	
	while ( viewAngles[YAW] > 180.0f )
		viewAngles[YAW] -= 360.0f;
	while ( viewAngles[YAW] < -180.0f )
		viewAngles[YAW] += 360.0f;
}

void CHudAimbot::ApplyRCSWithSmoothing( float* viewAngles )
{
	if ( !g_AimbotIsShooting )
		return;
	
	// Статические переменные для хранения состояния RCS
	static float lastAimbotRCSAngles[3] = {0.0f, 0.0f, 0.0f};
	static bool firstAimbotRCSFrame = true;
	
	float punchAngles[3];
	GetPunchAngles( punchAngles );
	
	// Проверяем, есть ли отдача для компенсации
	if ( fabsf( punchAngles[PITCH] ) < 0.001f && fabsf( punchAngles[YAW] ) < 0.001f )
	{
		// Нет отдачи - сбрасываем состояние если не стреляем
		if ( !g_AimbotIsShooting )
		{
			firstAimbotRCSFrame = true;
			VectorCopy( viewAngles, lastAimbotRCSAngles );
		}
		return;
	}
	
	// Вычисляем компенсацию отдачи
	float compensationPitch = punchAngles[PITCH] * m_fRCSStrength;
	float compensationYaw = punchAngles[YAW] * m_fRCSStrength;
	
	// Применяем компенсацию к углам аимбота
	float targetAngles[3];
	VectorCopy( viewAngles, targetAngles );
	targetAngles[PITCH] -= compensationPitch;
	targetAngles[YAW] -= compensationYaw;
	
	// Нормализуем углы
	if ( targetAngles[PITCH] > 89.0f )
		targetAngles[PITCH] = 89.0f;
	if ( targetAngles[PITCH] < -89.0f )
		targetAngles[PITCH] = -89.0f;
	
	while ( targetAngles[YAW] > 180.0f )
		targetAngles[YAW] -= 360.0f;
	while ( targetAngles[YAW] < -180.0f )
		targetAngles[YAW] += 360.0f;
	
	// Применяем плавность для RCS при работе с аимботом
	// Это предотвращает резкие движения и конфликты между аимботом и RCS
	if ( !firstAimbotRCSFrame )
	{
		// Коэффициент плавности для RCS (0.0 = мгновенно, 1.0 = очень плавно)
		// Используем значение 0.7-0.8 для баланса между отзывчивостью и плавностью
		float rcsSmoothFactor = 0.75f;
		
		// Интерполируем pitch (вертикальный угол)
		float pitchDelta = targetAngles[PITCH] - lastAimbotRCSAngles[PITCH];
		viewAngles[PITCH] = lastAimbotRCSAngles[PITCH] + ( pitchDelta * rcsSmoothFactor );
		
		// Интерполируем yaw (горизонтальный угол)
		float yawDelta = targetAngles[YAW] - lastAimbotRCSAngles[YAW];
		// Нормализуем yaw delta для корректной интерполяции через -180/180 границу
		if ( yawDelta > 180.0f )
			yawDelta -= 360.0f;
		else if ( yawDelta < -180.0f )
			yawDelta += 360.0f;
		
		viewAngles[YAW] = lastAimbotRCSAngles[YAW] + ( yawDelta * rcsSmoothFactor );
		
		// Нормализуем yaw обратно в диапазон -180..180
		while ( viewAngles[YAW] > 180.0f )
			viewAngles[YAW] -= 360.0f;
		while ( viewAngles[YAW] < -180.0f )
			viewAngles[YAW] += 360.0f;
	}
	else
	{
		// Первый кадр - применяем сразу без плавности
		VectorCopy( targetAngles, viewAngles );
		firstAimbotRCSFrame = false;
	}
	
	// Сохраняем текущие углы для следующего кадра
	VectorCopy( viewAngles, lastAimbotRCSAngles );
	
	// Сбрасываем состояние если не стреляем
	if ( !g_AimbotIsShooting )
	{
		firstAimbotRCSFrame = true;
	}
}

void CHudAimbot::ApplyNoRecoil()
{
	if ( pmove )
	{
		// pmove->punchangle это vec3_t (Vector после util_vector.h)
		// Vector имеет operator float*(), поэтому можем обнулить через массив
		// Используем operator float*() напрямую
		((float*)pmove->punchangle)[0] = 0.0f;
		((float*)pmove->punchangle)[1] = 0.0f;
		((float*)pmove->punchangle)[2] = 0.0f;
	}
	
	// ev_punchangle это vec3_t (Vector после util_vector.h)
	// Vector имеет operator float*(), поэтому можем обнулить через массив
	// Используем operator float*() напрямую, как в view.cpp: (float *)&ev_punchangle
	((float*)&ev_punchangle)[0] = 0.0f;
	((float*)&ev_punchangle)[1] = 0.0f;
	((float*)&ev_punchangle)[2] = 0.0f;
}

cl_entity_t* CHudAimbot::GetTargetOnCrosshair( float* viewAngles )
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if ( !localPlayer )
		return nullptr;
	
	// Получаем позицию локального игрока (глаза)
	float localOrigin[3];
	VectorCopy( localPlayer->origin, localOrigin );
	localOrigin[2] += 17.0f; // Высота глаз игрока
	
	// Вычисляем направление взгляда
	float forward[3];
	AngleVectors( viewAngles, forward, nullptr, nullptr );
	VectorNormalize( forward );
	
	// Вычисляем конечную точку луча (далеко вперед)
	float traceEnd[3];
	VectorMA( localOrigin, 8192.0f, forward, traceEnd );
	
	// Делаем trace от глаз игрока вперед
	pmtrace_t traceData;
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( localPlayer->index - 1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 0 );
	gEngfuncs.pEventAPI->EV_PlayerTrace(
		localOrigin, traceEnd, PM_NORMAL,
		-1, &traceData
	);
	gEngfuncs.pEventAPI->EV_PopPMStates();
	
	// Проверяем, попали ли мы в игрока
	if ( traceData.fraction >= 1.0f )
		return nullptr;
	
	// Получаем индекс сущности из trace
	int entIndex = gEngfuncs.pEventAPI->EV_IndexFromTrace( &traceData );
	if ( entIndex < 1 || entIndex > MAX_PLAYERS )
		return nullptr;
	
	// Получаем сущность
	cl_entity_t *ent = gEngfuncs.GetEntityByIndex( entIndex );
	if ( !ent )
		return nullptr;
	
	// Проверяем, что это игрок
	if ( !CustomUtils::CheckForPlayer( ent ) )
		return nullptr;
	
	// КРИТИЧЕСКАЯ ПРОВЕРКА #1: Пропускаем локального игрока
	if ( ent->index == localPlayer->index )
		return nullptr;
	
	// КРИТИЧЕСКАЯ ПРОВЕРКА #2: Пропускаем мертвых игроков
	// Это должно быть ПЕРВЫМ делом после проверки локального игрока
	if ( IsPlayerDead( ent ) )
		return nullptr;
	
	// Дополнительная проверка здоровья НЕ требуется здесь,
	// так как IsPlayerDead уже проверяет все необходимое
	// Проверка здоровья в IsPlayerDead сделана более мягкой,
	// чтобы не блокировать игроков с неинициализированным здоровьем
	
	// КРИТИЧЕСКАЯ ПРОВЕРКА #4: Проверяем режим deathmatch
	if ( m_pCvarDeathmatch && m_pCvarDeathmatch->value == 0.0f )
	{
		if ( CustomUtils::ArePlayersTeammates( localPlayer->index, ent->index ) )
			return nullptr; // Пропускаем тимейтов
	}
	
	// КРИТИЧЕСКАЯ ПРОВЕРКА #5: Проверяем видимость цели (если не игнорируем видимость)
	// ИСПРАВЛЕНИЕ: ignore visibility не работает на больших расстояниях
	// Вычисляем расстояние до цели
	float targetPos[3];
	if ( GetPlayerPosition( ent, targetPos ) )
	{
		float localOrigin[3];
		cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
		if ( localPlayer )
		{
			VectorCopy( localPlayer->origin, localOrigin );
			localOrigin[2] += 17.0f; // Высота глаз
			
			float dir[3];
			VectorSubtract( targetPos, localOrigin, dir );
			float distance = VectorLength( dir );
			
			// Ignore visibility не работает на расстояниях более 2000 единиц
			bool shouldCheckVisibility = true;
			if ( m_bIgnoreVisibility && distance <= 2000.0f )
			{
				shouldCheckVisibility = false;
			}
			
			if ( shouldCheckVisibility && !IsTargetVisible( ent ) )
				return nullptr;
		}
		else
		{
			// Если не удалось получить позицию, всегда проверяем видимость
			if ( !m_bIgnoreVisibility && !IsTargetVisible( ent ) )
				return nullptr;
		}
	}
	else
	{
		// Если не удалось получить позицию, всегда проверяем видимость
		if ( !m_bIgnoreVisibility && !IsTargetVisible( ent ) )
			return nullptr;
	}
	
	return ent;
}

bool CHudAimbot::IsTargetOnCrosshair( float* viewAngles )
{
	return ( GetTargetOnCrosshair( viewAngles ) != nullptr );
}

void CHudAimbot::ApplyTriggerBot( struct usercmd_s *cmd )
{
	if ( !m_bTriggerBot )
		return;
	
	// Статическая переменная для отслеживания времени последнего выстрела в onetap режиме
	static float onetapLastShotTime = 0.0f;
	
	// Статические переменные для плавного RCS в triggerbot
	static float lastRCSAngles[3] = { 0.0f, 0.0f, 0.0f };
	static bool firstRCSFrame = true;
	
	// Получаем текущие углы обзора
	float viewAngles[3];
	VectorCopy( cmd->viewangles, viewAngles );
	
	// Проверяем, есть ли цель на прицеле (используем исходные углы)
	cl_entity_t *target = GetTargetOnCrosshair( viewAngles );
	if ( !target )
	{
		// Если цели нет, сбрасываем состояние onetap и RCS
		if ( m_bTriggerBotOnetap )
		{
			onetapLastShotTime = 0.0f; // Сбрасываем время когда цель пропадает
		}
		firstRCSFrame = true;
		return;
	}
	
	// Цель найдена на прицеле - triggerbot будет стрелять
	// Определяем, нужно ли стрелять (onetap режим или обычный)
	bool shouldShoot = false;
	
	// Onetap режим - стреляем по одной пуле с задержкой между выстрелами
	if ( m_bTriggerBotOnetap )
	{
		// Получаем текущее время
		float currentTime = gEngfuncs.GetClientTime();
		
		// Проверяем, прошло ли достаточно времени с последнего выстрела
		float timeSinceLastShot = 0.0f;
		if ( onetapLastShotTime > 0.0f )
		{
			timeSinceLastShot = currentTime - onetapLastShotTime;
		}
		
		// Задержка между выстрелами для onetap (примерно 0.12 секунды)
		// Это время между выстрелами для большинства оружий
		float onetapDelay = 0.12f;
		
		// Если прошло достаточно времени или это первый выстрел
		if ( timeSinceLastShot >= onetapDelay || onetapLastShotTime == 0.0f )
		{
			shouldShoot = true;
			onetapLastShotTime = currentTime;
		}
		// Иначе не стреляем (ждем задержку)
	}
	else
	{
		// Обычный режим - просто стреляем пока цель на прицеле
		shouldShoot = true;
	}
	
	// Применяем RCS если включен и стреляем (или собираемся стрелять)
	// RCS применяется когда уже стреляем или когда triggerbot решил стрелять
	bool isShooting = g_AimbotIsShooting || shouldShoot;
	
	if ( m_bRCSEnabled && isShooting )
	{
		// Применяем RCS к углам
		ApplyRCS( viewAngles );
		
		// ПЛАВНЫЙ RCS: Интерполируем между предыдущими углами RCS и новыми
		// Это предотвращает резкие движения вниз (не сразу в низ)
		if ( !firstRCSFrame )
		{
			// Коэффициент плавности для RCS (0.0 = мгновенно, 1.0 = очень плавно)
			// Используем значение 0.5-0.6 для баланса между отзывчивостью и плавностью
			// Это предотвращает резкие движения вниз
			float rcsSmoothFactor = 0.55f; // Можно настроить через cvar если нужно
			
			// Интерполируем pitch (вертикальный угол) - это предотвращает резкое движение вниз
			float pitchDelta = viewAngles[PITCH] - lastRCSAngles[PITCH];
			viewAngles[PITCH] = lastRCSAngles[PITCH] + ( pitchDelta * rcsSmoothFactor );
			
			// Интерполируем yaw (горизонтальный угол)
			float yawDelta = viewAngles[YAW] - lastRCSAngles[YAW];
			// Нормализуем yaw delta для корректной интерполяции через -180/180 границу
			if ( yawDelta > 180.0f )
				yawDelta -= 360.0f;
			else if ( yawDelta < -180.0f )
				yawDelta += 360.0f;
			
			viewAngles[YAW] = lastRCSAngles[YAW] + ( yawDelta * rcsSmoothFactor );
			
			// Нормализуем yaw обратно в диапазон -180..180
			while ( viewAngles[YAW] > 180.0f )
				viewAngles[YAW] -= 360.0f;
			while ( viewAngles[YAW] < -180.0f )
				viewAngles[YAW] += 360.0f;
		}
		else
		{
			firstRCSFrame = false;
		}
		
		// Сохраняем текущие углы RCS для следующего кадра
		VectorCopy( viewAngles, lastRCSAngles );
		
		// Применяем плавные углы RCS к команде
		// Это позволяет triggerbot работать с компенсацией отдачи
		cmd->viewangles[PITCH] = viewAngles[PITCH];
		cmd->viewangles[YAW] = viewAngles[YAW];
	}
	else
	{
		// Если RCS выключен, сбрасываем состояние только если не стреляем
		if ( !isShooting )
		{
			firstRCSFrame = true;
		}
		VectorCopy( viewAngles, lastRCSAngles );
	}
	
	// Стреляем если нужно
	if ( shouldShoot )
	{
		cmd->buttons |= IN_ATTACK;
	}
}

void CHudAimbot::ApplyWeaponGlow( void )
{
	cl_entity_t *viewmodel = gEngfuncs.GetViewModel();
	if ( !viewmodel )
		return;
	
	// Применяем glow эффект к оружию
	// Используем kRenderTransColor для цветного glow эффекта (kRenderGlow не поддерживает цвета)
	viewmodel->curstate.rendermode = kRenderTransColor;
	viewmodel->curstate.renderfx = kRenderFxGlowShell;
	
	// Устанавливаем цвет
	viewmodel->curstate.rendercolor.r = (byte)m_fGlowColor[0];
	viewmodel->curstate.rendercolor.g = (byte)m_fGlowColor[1];
	viewmodel->curstate.rendercolor.b = (byte)m_fGlowColor[2];
	viewmodel->curstate.renderamt = (byte)m_fGlowAlpha;
}

void CHudAimbot::ApplyHandsGlow( void )
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if ( !localPlayer )
		return;
	
	// Руки обычно являются частью viewmodel или локального игрока
	// Применяем glow к локальному игроку (руки будут светиться)
	// В CS руки обычно рендерятся как часть viewmodel, но можно также применить к локальному игроку
	
	// Применяем glow к локальному игроку для рук
	// Используем kRenderTransColor для цветного glow эффекта (kRenderGlow не поддерживает цвета)
	localPlayer->curstate.rendermode = kRenderTransColor;
	localPlayer->curstate.renderfx = kRenderFxGlowShell;
	
	// Устанавливаем цвет
	localPlayer->curstate.rendercolor.r = (byte)m_fGlowColor[0];
	localPlayer->curstate.rendercolor.g = (byte)m_fGlowColor[1];
	localPlayer->curstate.rendercolor.b = (byte)m_fGlowColor[2];
	localPlayer->curstate.renderamt = (byte)m_fGlowAlpha;
}

void CHudAimbot::ApplyCustomFOV( void )
{
	// Применяем кастомный FOV через gHUD.m_iFOV
	// Ограничиваем FOV в разумных пределах (от 10 до 170 градусов)
	float fovValue = m_fCustomFOVValue;
	if ( fovValue < 10.0f )
		fovValue = 10.0f;
	if ( fovValue > 170.0f )
		fovValue = 170.0f;
	
	// Устанавливаем FOV
	gHUD.m_iFOV = (int)fovValue;
}

void CHudAimbot::DisablePlayerAnimations( void )
{
	// Отключаем анимации всех игроков (кроме локального)
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if ( !localPlayer )
		return;
	
	// Проходим по всем игрокам
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		cl_entity_t *ent = gEngfuncs.GetEntityByIndex( i );
		if ( !ent )
			continue;
		
		// Пропускаем локального игрока
		if ( ent->index == localPlayer->index )
			continue;
		
		// Проверяем, что это игрок
		if ( !CustomUtils::CheckForPlayer( ent ) )
			continue;
		
		// Отключаем анимации: устанавливаем sequence = 0, frame = 0, framerate = 0
		// Это замораживает анимацию игрока в текущей позе
		ent->curstate.sequence = 0;
		ent->curstate.frame = 0.0f;
		ent->curstate.framerate = 0.0f;
		ent->curstate.animtime = 0.0f;
		
		// Отключаем анимацию ног (gaitsequence) - это ключевой момент для заморозки ног
		ent->curstate.gaitsequence = 0;
		
		// Обнуляем латч-переменные, чтобы предотвратить интерполяцию между кадрами
		ent->latched.prevsequence = 0;
		ent->latched.prevframe = 0.0f;
		ent->latched.prevanimtime = 0.0f;
		
		// Устанавливаем контроллеры в нейтральное положение (127) для заморозки ног
		// Это нужно для правильной работы когда gaitsequence = 0
		ent->curstate.controller[0] = 127;
		ent->curstate.controller[1] = 127;
		ent->curstate.controller[2] = 127;
		ent->curstate.controller[3] = 127;
		ent->latched.prevcontroller[0] = 127;
		ent->latched.prevcontroller[1] = 127;
		ent->latched.prevcontroller[2] = 127;
		ent->latched.prevcontroller[3] = 127;
		
		// Обнуляем blending для предотвращения смешивания анимаций
		ent->curstate.blending[0] = 0;
		ent->curstate.blending[1] = 0;
		ent->latched.prevseqblending[0] = 0;
		ent->latched.prevseqblending[1] = 0;
	}
}

void CHudAimbot::ApplySpinbot( struct usercmd_s *cmd )
{
	// Silent spinbot - вращаем только cmd->viewangles, НЕ вызываем SetViewAngles
	// Это означает, что визуально камера не будет вращаться, но на сервере будет
	
	// Получаем текущее время для плавного вращения
	static float lastSpinTime = 0.0f;
	float currentTime = gEngfuncs.GetClientTime();
	float deltaTime = currentTime - lastSpinTime;
	
	// Ограничиваем deltaTime для предотвращения больших скачков
	if ( deltaTime > 0.1f )
		deltaTime = 0.1f;
	if ( deltaTime < 0.0f )
		deltaTime = 0.0f;
	
	lastSpinTime = currentTime;
	
	// Вычисляем изменение угла на основе скорости и времени
	// Скорость в градусах в секунду
	float angleDelta = m_fSpinbotSpeed * deltaTime;
	
	// Применяем вращение только к YAW (горизонтальное вращение)
	// PITCH и ROLL оставляем без изменений
	float currentYaw = cmd->viewangles[YAW];
	currentYaw += angleDelta;
	
	// Нормализуем угол в диапазон -180..180
	while ( currentYaw > 180.0f )
		currentYaw -= 360.0f;
	while ( currentYaw < -180.0f )
		currentYaw += 360.0f;
	
	// Применяем только к cmd->viewangles (silent - не визуально)
	cmd->viewangles[YAW] = currentYaw;
	// PITCH и ROLL не изменяем
}

void CHudAimbot::ApplyThirdPerson( void )
{
	// Включаем третье лицо, обходя ограничение для мультиплеера
	// Используем внешнюю переменную cam_thirdperson из in_camera.cpp
	extern int cam_thirdperson;
	extern cvar_t *cam_idealdist;
	extern cvar_t *cam_command;
	
	// Включаем третье лицо
	if ( !cam_thirdperson )
	{
		cam_thirdperson = 1;
		
		// Устанавливаем начальные углы камеры
		float viewangles[3];
		gEngfuncs.GetViewAngles( viewangles );
		
		extern vec3_t cam_ofs;
		cam_ofs[0] = viewangles[YAW];
		cam_ofs[1] = viewangles[PITCH];
		cam_ofs[2] = m_fThirdPersonDist;
		
		// Устанавливаем расстояние камеры
		if ( cam_idealdist )
		{
			gEngfuncs.Cvar_SetValue( "cam_idealdist", m_fThirdPersonDist );
		}
	}
	else
	{
		// Обновляем расстояние камеры если оно изменилось
		if ( cam_idealdist && cam_idealdist->value != m_fThirdPersonDist )
		{
			gEngfuncs.Cvar_SetValue( "cam_idealdist", m_fThirdPersonDist );
		}
	}
}

void CHudAimbot::ApplyAntiAim( struct usercmd_s *cmd )
{
	// Silent anti-aim - изменяем только cmd->viewangles, НЕ вызываем SetViewAngles
	// Это означает, что визуально камера не будет двигаться, но на сервере будет
	
	// ПРОВЕРКА: Антиаим работает со всеми оружиями КРОМЕ гранат
	// Гранаты требуют точного прицеливания для правильного броска
	int weaponId = HUD_GetWeapon();
	if ( weaponId == WEAPON_HEGRENADE || 
	     weaponId == WEAPON_FLASHBANG || 
	     weaponId == WEAPON_SMOKEGRENADE )
	{
		// Граната в руках - не применяем антиаим
		return;
	}
	
	// УЛУЧШЕНИЕ: Сохраняем оригинальные значения движения для компенсации
	float oldForward = cmd->forwardmove;
	float oldSide = cmd->sidemove;
	
	// Получаем текущие углы
	float viewAngles[3];
	gEngfuncs.GetViewAngles( viewAngles );
	float oldYaw = viewAngles[YAW];
	
	// Применяем антиаим в зависимости от режима
	switch ( m_iAntiAimMode )
	{
	case 0: // Jitter - быстрое дрожание углов (МОЩНОЕ)
		{
			// УЛУЧШЕНИЕ: Всегда полная сила
			float jitterYaw = ((float)(rand() % (int)(m_fAntiAimJitterRange * 2.0f)) - m_fAntiAimJitterRange);
			float jitterPitch = ((float)(rand() % (int)(m_fAntiAimJitterRange * 2.0f)) - m_fAntiAimJitterRange);
			
			cmd->viewangles[YAW] = viewAngles[YAW] + jitterYaw;
			cmd->viewangles[PITCH] = viewAngles[PITCH] + jitterPitch;
		}
		break;
		
	case 1: // Spin - вращение (с компенсацией движения)
		{
			// УЛУЧШЕНИЕ: Убрана проверка движения - работает всегда
			static float spinAngle = 0.0f;
			float currentTime = gEngfuncs.GetClientTime();
			static float lastSpinTime = 0.0f;
			float deltaTime = currentTime - lastSpinTime;
			
			if ( deltaTime > 0.1f ) deltaTime = 0.1f;
			if ( deltaTime < 0.0f ) deltaTime = 0.0f;
			
			lastSpinTime = currentTime;
			
			spinAngle += m_fAntiAimSpeed * deltaTime;
			if ( spinAngle > 360.0f ) spinAngle -= 360.0f;
			if ( spinAngle < -360.0f ) spinAngle += 360.0f;
			
			cmd->viewangles[YAW] = viewAngles[YAW] + spinAngle;
			cmd->viewangles[PITCH] = viewAngles[PITCH];
		}
		break;
		
	case 2: // Fake Angle - фейковый угол (МОЩНОЕ отклонение)
		{
			// УЛУЧШЕНИЕ: Всегда полный угол
			float fakeYaw = viewAngles[YAW] + m_fAntiAimFakeAngle;
			
			// Нормализуем
			while ( fakeYaw > 180.0f ) fakeYaw -= 360.0f;
			while ( fakeYaw < -180.0f ) fakeYaw += 360.0f;
			
			cmd->viewangles[YAW] = fakeYaw;
			cmd->viewangles[PITCH] = viewAngles[PITCH];
		}
		break;
		
	case 3: // Backwards - смотрим назад (с компенсацией движения)
		{
			// УЛУЧШЕНИЕ: Убрана проверка движения
			float backwardsYaw = viewAngles[YAW] + 180.0f;
			
			// Нормализуем
			while ( backwardsYaw > 180.0f ) backwardsYaw -= 360.0f;
			while ( backwardsYaw < -180.0f ) backwardsYaw += 360.0f;
			
			cmd->viewangles[YAW] = backwardsYaw;
			cmd->viewangles[PITCH] = -viewAngles[PITCH]; // Инвертируем pitch
		}
		break;
		
	case 4: // Sideways - смотрим в сторону (90°)
		{
			// УЛУЧШЕНИЕ: Всегда 90°
			float sidewaysYaw = viewAngles[YAW] + 90.0f;
			
			// Нормализуем
			while ( sidewaysYaw > 180.0f ) sidewaysYaw -= 360.0f;
			while ( sidewaysYaw < -180.0f ) sidewaysYaw += 360.0f;
			
			cmd->viewangles[YAW] = sidewaysYaw;
			cmd->viewangles[PITCH] = viewAngles[PITCH];
		}
		break;
		
	case 5: // Legit AA - плавное движение (менее заметное)
		{
			static float legitAngle = 0.0f;
			float currentTime = gEngfuncs.GetClientTime();
			static float lastLegitTime = 0.0f;
			float deltaTime = currentTime - lastLegitTime;
			
			if ( deltaTime > 0.1f ) deltaTime = 0.1f;
			if ( deltaTime < 0.0f ) deltaTime = 0.0f;
			
			lastLegitTime = currentTime;
			
			// Медленное плавное вращение
			legitAngle += (m_fAntiAimSpeed * 0.3f) * deltaTime;
			if ( legitAngle > 360.0f ) legitAngle -= 360.0f;
			if ( legitAngle < -360.0f ) legitAngle += 360.0f;
			
			// Небольшое отклонение для более естественного вида
			// Конвертируем градусы в радианы (1 градус = 0.017453 радиан)
			float angleRad = legitAngle * 0.0174532925f;
			float smoothYaw = viewAngles[YAW] + (sinf(angleRad) * m_fAntiAimJitterRange);
			
			cmd->viewangles[YAW] = smoothYaw;
			cmd->viewangles[PITCH] = viewAngles[PITCH] + (cosf(angleRad) * (m_fAntiAimJitterRange * 0.5f));
		}
		break;
		
	case 6: // Fast Spin - быстрое вращение (с компенсацией движения)
		{
			// УЛУЧШЕНИЕ: Убрана проверка движения
			static float fastSpinAngle = 0.0f;
			float currentTime = gEngfuncs.GetClientTime();
			static float lastFastSpinTime = 0.0f;
			float deltaTime = currentTime - lastFastSpinTime;
			
			if ( deltaTime > 0.1f ) deltaTime = 0.1f;
			if ( deltaTime < 0.0f ) deltaTime = 0.0f;
			
			lastFastSpinTime = currentTime;
			
			// Быстрое вращение (в 3 раза быстрее обычного)
			fastSpinAngle += (m_fAntiAimSpeed * 3.0f) * deltaTime;
			if ( fastSpinAngle > 360.0f ) fastSpinAngle -= 360.0f;
			if ( fastSpinAngle < -360.0f ) fastSpinAngle += 360.0f;
			
			cmd->viewangles[YAW] = viewAngles[YAW] + fastSpinAngle;
			cmd->viewangles[PITCH] = viewAngles[PITCH];
		}
		break;
		
	case 7: // Random - случайные углы каждый кадр (МОЩНОЕ)
		{
			// УЛУЧШЕНИЕ: Всегда полная сила
			float randomYaw = viewAngles[YAW] + ((float)(rand() % (int)(m_fAntiAimJitterRange * 2.0f)) - m_fAntiAimJitterRange);
			float randomPitch = viewAngles[PITCH] + ((float)(rand() % (int)(m_fAntiAimJitterRange * 2.0f)) - m_fAntiAimJitterRange);
			
			// Ограничиваем pitch
			if ( randomPitch > 89.0f ) randomPitch = 89.0f;
			if ( randomPitch < -89.0f ) randomPitch = -89.0f;
			
			cmd->viewangles[YAW] = randomYaw;
			cmd->viewangles[PITCH] = randomPitch;
		}
		break;
		
	default:
		// По умолчанию - jitter (МОЩНОЕ)
		{
			// УЛУЧШЕНИЕ: Всегда полная сила
			float jitterYaw = viewAngles[YAW] + ((float)(rand() % (int)(m_fAntiAimJitterRange * 2.0f)) - m_fAntiAimJitterRange);
			float jitterPitch = viewAngles[PITCH] + ((float)(rand() % (int)(m_fAntiAimJitterRange * 2.0f)) - m_fAntiAimJitterRange);
			
			cmd->viewangles[YAW] = jitterYaw;
			cmd->viewangles[PITCH] = jitterPitch;
		}
		break;
	}
	
	// Нормализуем углы
	if ( cmd->viewangles[PITCH] > 89.0f )
		cmd->viewangles[PITCH] = 89.0f;
	if ( cmd->viewangles[PITCH] < -89.0f )
		cmd->viewangles[PITCH] = -89.0f;
	
	while ( cmd->viewangles[YAW] > 180.0f )
		cmd->viewangles[YAW] -= 360.0f;
	while ( cmd->viewangles[YAW] < -180.0f )
		cmd->viewangles[YAW] += 360.0f;
	
	// КОМПЕНСАЦИЯ ДВИЖЕНИЯ - ИСПРАВЛЯЕТ ПРОБЛЕМУ С ХОДЬБОЙ!
	// Пересчитываем движение относительно новых углов антиаима
	if ( oldForward != 0.0f || oldSide != 0.0f )
	{
		float yawDelta = cmd->viewangles[YAW] - oldYaw;
		float yawRad = yawDelta * 0.0174532925f; // Конвертируем в радианы
		
		float cosYaw = cosf(yawRad);
		float sinYaw = sinf(yawRad);
		
		// Поворачиваем вектор движения на разницу углов
		cmd->forwardmove = oldForward * cosYaw - oldSide * sinYaw;
		cmd->sidemove = oldForward * sinYaw + oldSide * cosYaw;
	}
}

void CHudAimbot::ApplyAntiAimYawOnly( struct usercmd_s *cmd )
{
	// Применяет антиаим только к YAW (горизонталь), не трогая PITCH
	// Это позволяет аимботу работать по вертикали, а антиаим защищает по горизонтали
	
	// ПРОВЕРКА: Антиаим работает со всеми оружиями КРОМЕ гранат
	// Гранаты требуют точного прицеливания для правильного броска
	int weaponId = HUD_GetWeapon();
	if ( weaponId == WEAPON_HEGRENADE || 
	     weaponId == WEAPON_FLASHBANG || 
	     weaponId == WEAPON_SMOKEGRENADE )
	{
		// Граната в руках - не применяем антиаим
		return;
	}
	
	// Получаем текущие углы
	float viewAngles[3];
	gEngfuncs.GetViewAngles( viewAngles );
	
	// Применяем антиаим только к YAW в зависимости от режима
	switch ( m_iAntiAimMode )
	{
	case 0: // Jitter - быстрое дрожание углов (МОЩНОЕ)
		{
			float jitterYaw = ((float)(rand() % (int)(m_fAntiAimJitterRange * 2.0f)) - m_fAntiAimJitterRange);
			cmd->viewangles[YAW] = viewAngles[YAW] + jitterYaw;
		}
		break;
		
	case 1: // Spin - вращение
		{
			static float spinAngle = 0.0f;
			float currentTime = gEngfuncs.GetClientTime();
			static float lastSpinTime = 0.0f;
			float deltaTime = currentTime - lastSpinTime;
			
			if ( deltaTime > 0.1f ) deltaTime = 0.1f;
			if ( deltaTime < 0.0f ) deltaTime = 0.0f;
			
			lastSpinTime = currentTime;
			
			spinAngle += m_fAntiAimSpeed * deltaTime;
			if ( spinAngle > 360.0f ) spinAngle -= 360.0f;
			if ( spinAngle < -360.0f ) spinAngle += 360.0f;
			
			cmd->viewangles[YAW] = viewAngles[YAW] + spinAngle;
		}
		break;
		
	case 2: // Fake Angle - фейковый угол (МОЩНОЕ)
		{
			float fakeYaw = viewAngles[YAW] + m_fAntiAimFakeAngle;
			while ( fakeYaw > 180.0f ) fakeYaw -= 360.0f;
			while ( fakeYaw < -180.0f ) fakeYaw += 360.0f;
			cmd->viewangles[YAW] = fakeYaw;
		}
		break;
		
	case 3: // Backwards - смотрим назад
		{
			float backwardsYaw = viewAngles[YAW] + 180.0f;
			while ( backwardsYaw > 180.0f ) backwardsYaw -= 360.0f;
			while ( backwardsYaw < -180.0f ) backwardsYaw += 360.0f;
			cmd->viewangles[YAW] = backwardsYaw;
		}
		break;
		
	case 4: // Sideways - смотрим в сторону (90°)
		{
			float sidewaysYaw = viewAngles[YAW] + 90.0f;
			while ( sidewaysYaw > 180.0f ) sidewaysYaw -= 360.0f;
			while ( sidewaysYaw < -180.0f ) sidewaysYaw += 360.0f;
			cmd->viewangles[YAW] = sidewaysYaw;
		}
		break;
		
	case 5: // Legit AA - плавное движение
		{
			static float legitAngle = 0.0f;
			float currentTime = gEngfuncs.GetClientTime();
			static float lastLegitTime = 0.0f;
			float deltaTime = currentTime - lastLegitTime;
			
			if ( deltaTime > 0.1f ) deltaTime = 0.1f;
			if ( deltaTime < 0.0f ) deltaTime = 0.0f;
			
			lastLegitTime = currentTime;
			
			legitAngle += (m_fAntiAimSpeed * 0.3f) * deltaTime;
			if ( legitAngle > 360.0f ) legitAngle -= 360.0f;
			if ( legitAngle < -360.0f ) legitAngle += 360.0f;
			
			float angleRad = legitAngle * 0.0174532925f;
			cmd->viewangles[YAW] = viewAngles[YAW] + (sinf(angleRad) * m_fAntiAimJitterRange);
		}
		break;
		
	case 6: // Fast Spin - быстрое вращение
		{
			static float fastSpinAngle = 0.0f;
			float currentTime = gEngfuncs.GetClientTime();
			static float lastFastSpinTime = 0.0f;
			float deltaTime = currentTime - lastFastSpinTime;
			
			if ( deltaTime > 0.1f ) deltaTime = 0.1f;
			if ( deltaTime < 0.0f ) deltaTime = 0.0f;
			
			lastFastSpinTime = currentTime;
			
			fastSpinAngle += (m_fAntiAimSpeed * 3.0f) * deltaTime;
			if ( fastSpinAngle > 360.0f ) fastSpinAngle -= 360.0f;
			if ( fastSpinAngle < -360.0f ) fastSpinAngle += 360.0f;
			
			cmd->viewangles[YAW] = viewAngles[YAW] + fastSpinAngle;
		}
		break;
		
	case 7: // Random - случайные углы (МОЩНОЕ)
		{
			float randomYaw = viewAngles[YAW] + ((float)(rand() % (int)(m_fAntiAimJitterRange * 2.0f)) - m_fAntiAimJitterRange);
			cmd->viewangles[YAW] = randomYaw;
		}
		break;
		
	default:
		{
			float jitterYaw = viewAngles[YAW] + ((float)(rand() % (int)(m_fAntiAimJitterRange * 2.0f)) - m_fAntiAimJitterRange);
			cmd->viewangles[YAW] = jitterYaw;
		}
		break;
	}
	
	// Нормализуем YAW
	while ( cmd->viewangles[YAW] > 180.0f )
		cmd->viewangles[YAW] -= 360.0f;
	while ( cmd->viewangles[YAW] < -180.0f )
		cmd->viewangles[YAW] += 360.0f;
}



