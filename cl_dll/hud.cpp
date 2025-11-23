/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
// hud.cpp
//
// implementation of CHud class
//

#include <new>

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "parsemsg.h"

#include "demo.h"
#include "demo_api.h"
#include "vgui_parser.h"
#include "rain.h"

#include "camera.h"

#include "draw_util.h"
#include "hud_cheatmenu_vgui.h"

#if _WIN32
#define strncasecmp _strnicmp
#endif

cvar_t *cl_fog_r;
cvar_t *cl_fog_g;
cvar_t *cl_fog_b;
cvar_t *cl_fog_density;

extern client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

// Forward declaration для команды консоли
void UserCmd_CollectPlayerInfo();

// Team Colors
int iNumberOfTeamColors = 3;
int iTeamColors[3][3] =
{
	{ 204, 204, 204 }, // Spectators
	{ 255, 64, 64 }, // CT's
	{ 153, 204, 255 }  // T's
};

class CCStrikeVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	virtual void GetPlayerTextColor( int entindex, int color[3] )
	{
		color[0] = color[1] = color[2] = 255;

		if ( entindex < MAX_PLAYERS )
		{
			int iTeam = g_PlayerExtraInfo[entindex].teamnumber;

			if ( iTeam < 0 )
			{
				iTeam = 0;
			}

			iTeam = iTeam % iNumberOfTeamColors;

			color[0] = iTeamColors[iTeam][0];
			color[1] = iTeamColors[iTeam][1];
			color[2] = iTeamColors[iTeam][2];
		}
	}

	virtual void UpdateCursorState()
	{
		// gViewPort->UpdateCursorState();
	}

	virtual int GetAckIconHeight()
	{
		return gHUD.m_iFontHeight * 3 + 6;
	}

	virtual bool CanShowSpeakerLabels()
	{
		return !gHUD.m_Scoreboard.m_bForceDraw && !gHUD.m_Scoreboard.m_bShowscoresHeld;
	}
};
static CCStrikeVoiceStatusHelper g_VoiceStatusHelper;

wrect_t nullrc = { 0, 0, 0, 0 };
float g_lastFOV = 0.0;
const char *sPlayerModelFiles[12] =
{
	"models/player.mdl",
	"models/player/leet/leet.mdl", // t
	"models/player/gign/gign.mdl", // ct
	"models/player/vip/vip.mdl", //ct
	"models/player/gsg9/gsg9.mdl", // ct
	"models/player/guerilla/guerilla.mdl", // t
	"models/player/arctic/arctic.mdl", // t
	"models/player/sas/sas.mdl", // ct
	"models/player/terror/terror.mdl", // t
	"models/player/urban/urban.mdl", // ct
	"models/player/spetsnaz/spetsnaz.mdl", // ct
	"models/player/militia/militia.mdl" // t
};

void __CmdFunc_InputCommandSpecial()
{
#ifdef _CS16CLIENT_ALLOW_SPECIAL_SCRIPTING
	gEngfuncs.pfnClientCmd("_special");
#endif
}

void __CmdFunc_GunSmoke()
{
	if( gHUD.cl_gunsmoke->value )
		gEngfuncs.Cvar_SetValue( "cl_gunsmoke", 0 );
	else
		gEngfuncs.Cvar_SetValue( "cl_gunsmoke", 1 );
}

/*
============
COM_FileBase
============
*/
// Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
void COM_FileBase ( const char *in, char *out)
{
	int len, start, end;

	len = strlen( in );

	// scan backward for '.'
	end = len - 1;
	while ( end && in[end] != '.' && in[end] != '/' && in[end] != '\\' )
		end--;

	if ( in[end] != '.' )		// no '.', copy to end
		end = len-1;
	else
		end--;					// Found ',', copy to left of '.'


	// Scan backward for '/'
	start = len-1;
	while ( start >= 0 && in[start] != '/' && in[start] != '\\' )
		start--;

	if ( in[start] != '/' && in[start] != '\\' )
		start = 0;
	else
		start++;

	// Length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy( out, &in[start], len );
	// Terminate it
	out[len] = 0;
}

/*
=================
HUD_IsGame

=================
*/
int HUD_IsGame( const char *game )
{
	const char *gamedir;
	char gd[ 1024 ];

	gamedir = gEngfuncs.pfnGetGameDirectory();
	if ( gamedir && gamedir[0] )
	{
		COM_FileBase( gamedir, gd );
		if ( !stricmp( gd, game ) )
			return 1;
	}
	return 0;
}

#define XASH_GENERATE_BUILDNUM

#if defined(XASH_GENERATE_BUILDNUM)
static const char *date = __DATE__;
static const char *mon[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static char mond[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
#endif

char *Q_buildnum( void )
{
// do not touch this! Only author of Xash3D can increase buildnumbers!
// Xash3D SDL: HAHAHA! I TOUCHED THIS!
	int m = 0, d = 0, y = 0;
	static int b = 0;
	static char buildnum[16];

	if( b != 0 )
		return buildnum;

	for( m = 0; m < 11; m++ )
	{
		if( !strncasecmp( &date[0], mon[m], 3 ))
			break;
		d += mond[m];
	}

	d += atoi( &date[4] ) - 1;
	y = atoi( &date[7] ) - 1900;
	b = d + (int)((y - 1) * 365.25f );

	if((( y % 4 ) == 0 ) && m > 1 )
	{
		b += 1;
	}
	//b -= 38752; // Feb 13 2007
	b -= 41940; // Oct 29 2015.
	// Happy birthday, cs16client! :)

	snprintf( buildnum, sizeof(buildnum), "%i", b );

	return buildnum;
}

int __MsgFunc_ADStop( const char *name, int size, void *buf ) { return 1; }
int __MsgFunc_ItemStatus( const char *name, int size, void *buf ) { return 1; }
// int __MsgFunc_ReqState( const char *name, int size, void *buf ) { return 1; }
int __MsgFunc_ForceCam( const char *name, int size, void *buf ) { return 1; }
int __MsgFunc_Spectator( const char *name, int size, void *buf ) { return 1; }

#ifdef __ANDROID__
bool evdev_open = false;
void __CmdFunc_MouseSucksOpen( void ) { evdev_open = true; }
void __CmdFunc_MouseSucksClose( void ) { evdev_open = false; }
#endif


// This is called every time the DLL is loaded
void CHud :: Init( void )
{
	SetGameType(); // call it first, so we will know gamedir at very early stage

	HOOK_COMMAND_FUNC( "special", __CmdFunc_InputCommandSpecial, );
	HOOK_COMMAND_FUNC( "gunsmoke", __CmdFunc_GunSmoke, );

#ifdef __ANDROID__
	HOOK_COMMAND_FUNC( "evdev_mouseopen", __CmdFunc_MouseSucksOpen );
	HOOK_COMMAND_FUNC( "evdev_mouseclose", __CmdFunc_MouseSucksClose );
#endif
	
	HOOK_MESSAGE( gHUD, Logo );
	HOOK_MESSAGE( gHUD, ResetHUD );
	HOOK_MESSAGE( gHUD, GameMode );
	HOOK_MESSAGE( gHUD, InitHUD );
	HOOK_MESSAGE( gHUD, ViewMode );
	HOOK_MESSAGE( gHUD, SetFOV );
	HOOK_MESSAGE( gHUD, Concuss );
	HOOK_MESSAGE( gHUD, ServerName );
	HOOK_MESSAGE( gHUD, ShadowIdx );

	gEngfuncs.pfnHookUserMsg( "ADStop", __MsgFunc_ADStop );
	gEngfuncs.pfnHookUserMsg( "ItemStatus", __MsgFunc_ItemStatus );
	// gEngfuncs.pfnHookUserMsg( "ReqState", __MsgFunc_ReqState );
	gEngfuncs.pfnHookUserMsg( "ForceCam", __MsgFunc_ForceCam );
	gEngfuncs.pfnHookUserMsg( "Spectator", __MsgFunc_Spectator );

	HOOK_MESSAGE( gHUD, Fog );

	// Регистрируем команду для сбора информации о игроках
	gEngfuncs.pfnAddCommand("ebash3d_collect_playerinfo", UserCmd_CollectPlayerInfo);

	CVAR_CREATE( "_vgui_menus", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	CVAR_CREATE( "_cl_autowepswitch", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	CVAR_CREATE( "_ah", "0", FCVAR_ARCHIVE | FCVAR_USERINFO );

	// TODO remove hack later
	CVAR_CREATE( "numericalmenu", "1", FCVAR_ARCHIVE );
	CVAR_CREATE( "numericalmenu_clientonly", "1", FCVAR_ARCHIVE );
	CVAR_CREATE( "checkscoreboard", "1", FCVAR_ARCHIVE );
	cscl_currentmap = CVAR_CREATE( "cscl_currentmap", "", 0 );
	cscl_mapprefix = CVAR_CREATE( "cscl_mapprefix", "", 0 );
	cscl_currentmoney = CVAR_CREATE( "cscl_currentmoney", "0", 0 );
	CVAR_CREATE( "teammenu_showscores", "0", FCVAR_ARCHIVE );
	CVAR_CREATE( "menu_bg_fill", "0", FCVAR_ARCHIVE );
	CVAR_CREATE( "buymenu_stayon", "0", FCVAR_ARCHIVE );

	hud_textmode = CVAR_CREATE( "hud_textmode", "0", FCVAR_ARCHIVE );
	hud_colored  = CVAR_CREATE( "hud_colored", "0", FCVAR_ARCHIVE );
	cl_righthand = CVAR_CREATE( "cl_righthand", "1", FCVAR_ARCHIVE );
	cl_weather   = CVAR_CREATE( "cl_weather", "1", FCVAR_ARCHIVE );
	cl_minmodels = CVAR_CREATE( "cl_minmodels", "0", FCVAR_ARCHIVE );
	cl_min_t     = CVAR_CREATE( "cl_min_t", "1", FCVAR_ARCHIVE );
	cl_min_ct    = CVAR_CREATE( "cl_min_ct", "2", FCVAR_ARCHIVE );
	default_fov  = CVAR_CREATE( "default_fov", "90", 0 );
	m_pCvarDraw  = CVAR_CREATE( "hud_draw", "1", FCVAR_ARCHIVE );
	fastsprites  = CVAR_CREATE( "fastsprites", "0", FCVAR_ARCHIVE );
	cl_gunsmoke  = CVAR_CREATE( "cl_gunsmoke", "0", FCVAR_ARCHIVE );
	cl_weapon_sparks = CVAR_CREATE( "cl_weapon_sparks", "1", FCVAR_ARCHIVE );
	cl_weapon_wallpuff = CVAR_CREATE( "cl_weapon_wallpuff", "1", FCVAR_ARCHIVE );
	zoom_sens_ratio = CVAR_CREATE( "zoom_sensitivity_ratio", "1.2", 0 );

	cl_charset = gEngfuncs.pfnGetCvarPointer( "cl_charset" );
	con_charset = gEngfuncs.pfnGetCvarPointer( "con_charset" );

	cl_viewbob = CVAR_CREATE( "cl_viewbob", "1", FCVAR_ARCHIVE );

	m_pShowHealth = CVAR_CREATE( "scoreboard_showhealth", "1", FCVAR_ARCHIVE );
	m_pShowMoney = CVAR_CREATE( "scoreboard_showmoney", "1", FCVAR_ARCHIVE );

	// The cvar was taken from the OpenAG client
	m_pCvarColor = CVAR_CREATE( "hud_color", "255 160 0", FCVAR_ARCHIVE );

	if ( gEngfuncs.pfnGetCvarFloat( "developer" ) > 0.0f )
	{
		cl_fog_density = CVAR_CREATE( "cl_fog_density", "0", 0 );
		cl_fog_r = CVAR_CREATE( "cl_fog_r", "0", 0 );
		cl_fog_g = CVAR_CREATE( "cl_fog_g", "0", 0 );
		cl_fog_b = CVAR_CREATE( "cl_fog_b", "0", 0 );
	}

	CVAR_CREATE( "cscl_ver", Q_buildnum(), 1<<14 | FCVAR_USERINFO ); // init and userinfo

	m_iLogo = 0;
	m_iFOV = 0;

	m_pSpriteList = NULL;

	// Clear any old HUD list
	for( HUDLIST *pList = m_pHudList; pList; pList = m_pHudList )
	{
		m_pHudList = m_pHudList->pNext;
		delete pList;
	}
	m_pHudList = NULL;

	// In case we get messages before the first update -- time will be valid
	m_flTime = 1.0;
	m_iNoConsolePrint = 0;
	m_szServerName[0] = 0;



	Localize_Init();

	// fullscreen overlays
	m_SniperScope.Init();
	m_NVG.Init();

	// Spectator GUI is not need in singleplayer czeror
	if( GetGameType() != GAME_CZERODS )
		m_SpectatorGui.Init();

	// Game HUD things
	m_Ammo.Init();
	m_Health.Init();
	m_Radio.Init();
	m_Timer.Init();
	m_Money.Init();
	m_AmmoSecondary.Init();
	m_Train.Init();
	m_Battery.Init();
	m_StatusIcons.Init();
	m_Radar.Init();
	m_Scenario.Init();

	// chat, death notice, status bars and other
	m_SayText.Init();
	m_Spectator.Init();
	m_Geiger.Init();
	m_Flash.Init();
	m_Message.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_TextMessage.Init();
	m_MOTD.Init();

	// all things that have own background and must be drawn last
	m_ProgressBar.Init();
	m_Menu.Init();
	m_Scoreboard.Init();
	// m_CheatMenu.Init(); // Disabled
	m_CornerBox.Init();
	m_Aimbot.Init();
	m_Debug.Init();
	m_CheatMenuVGUI.Init();
	m_PlayerList.Init();

	GetClientVoice()->Init( &g_VoiceStatusHelper );

	InitRain();

	//ServersInit();

	gEngfuncs.Con_Printf( "%s: ^2CS16Client^7 ver. %s initialized.\n", __FUNCTION__, CVAR_GET_STRING( "cscl_ver" ) );

	MsgFunc_ResetHUD(0, 0, NULL );
}

// CHud destructor
// cleans up memory allocated for m_rg* arrays
CHud :: ~CHud()
{
	delete [] m_rghSprites;
	delete [] m_rgrcRects;
	delete [] m_rgszSpriteNames;

	// Clear any old HUD list
	for( HUDLIST *pList = m_pHudList; pList; pList = m_pHudList )
	{
		m_pHudList = m_pHudList->pNext;
		delete pList;
	}
	m_pHudList = NULL;
}

void CHud :: VidInit( void )
{
	static bool firstinit = true;
	m_scrinfo.iSize = sizeof( m_scrinfo );
	GetScreenInfo( &m_scrinfo );

	m_truescrinfo.iWidth = CVAR_GET_FLOAT("width");
	m_truescrinfo.iHeight = CVAR_GET_FLOAT("height");

	// ----------
	// Load Sprites
	// ---------
	//	m_hsprFont = LoadSprite("sprites/%d_font.spr");
	
	m_hsprLogo = 0;

	m_flScale = (float)TrueWidth / (float)ScreenWidth;

	m_iRes = 640;

	// Only load this once
	if( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList("sprites/hud.txt", &m_iSpriteCountAllRes);

		if( m_pSpriteList )
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			client_sprite_t *p = m_pSpriteList;
			for ( int j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
			m_rghSprites      = new(std::nothrow) HSPRITE[m_iSpriteCount];
			m_rgrcRects       = new(std::nothrow) wrect_t[m_iSpriteCount];
			m_rgszSpriteNames = new(std::nothrow) char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];;

			if( !m_rghSprites || !m_rgrcRects || !m_rgszSpriteNames )
			{
				gEngfuncs.pfnConsolePrint("CHud::VidInit(): Cannot allocate memory");
				if( g_iXash )
					gRenderAPI.Host_Error("CHud::VidInit(): Cannot allocate memory");
			}

			p = m_pSpriteList;
			for ( int index = 0, j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
				{
					char sz[256];
					sprintf(sz, "sprites/%s.spr", p->szSprite);
					m_rghSprites[index] = SPR_Load(sz);
					m_rgrcRects[index] = p->rc;
					strncpy( &m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH );

					index++;
				}

				p++;
			}
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t *p = m_pSpriteList;
		int index = 0;
		for ( int j = 0; j < m_iSpriteCountAllRes; j++ )
		{
			if ( p->iRes == m_iRes )
			{
				char sz[256];
				sprintf( sz, "sprites/%s.spr", p->szSprite );
				m_rghSprites[index] = SPR_Load(sz);
				index++;
			}

			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );

	if( m_HUD_number_0 == -1 && g_iXash )
	{
		gRenderAPI.Host_Error( "Failed to get number_0 sprite index. Check your game data!" );
		return;
	}

	if( g_iXash )
	{
		// get internal texture
		m_WhiteTex = gRenderAPI.GL_LoadTexture( "*white", NULL, 0, 0 );
	}

	m_iFontWidth  = GetSpriteRect(m_HUD_number_0).Width();
	m_iFontHeight = GetSpriteRect(m_HUD_number_0).Height();

	m_hGasPuff = SPR_Load("sprites/gas_puff_01.spr");

	for( HUDLIST *pList = m_pHudList; pList; pList = pList->pNext )
		pList->p->VidInit();

#if 0
	if( firstinit && gEngfuncs.CheckParm( "-firsttime", NULL ) )
	{
		ConsolePrint( "firstrun\n" );

		ClientCmd( "exec touch_presets/phone_ahsim" );
		gEngfuncs.Cvar_Set( "touch_config_file", "touch_presets/phone_ahsim.cfg" );
	}
#endif

	firstinit = false;
}

void CHud::Shutdown( void )
{
	for( HUDLIST *pList = m_pHudList; pList; pList = pList->pNext )
	{
		pList->p->Shutdown();
	}
}

int CHud::MsgFunc_Logo(const char *pszName,  int iSize, void *pbuf)
{
	BufferReader reader( pszName, pbuf, iSize );

	// update Train data
	m_iLogo = reader.ReadByte();

	return 1;
}

void CHud::SetGameType()
{
	if( HUD_IsGame( "czeror" ) )
		m_iGameType = GAME_CZERODS;
	else if( HUD_IsGame( "czero" ))
		m_iGameType = GAME_CZERO;
	else m_iGameType = GAME_CSTRIKE;
}

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
float HUD_GetFOV( void )
{
	if ( gEngfuncs.pDemoAPI->IsRecording() )
	{
		// Write it
		unsigned char buf[ sizeof(float) ];

		// Active
		*( float * )&buf = g_lastFOV;

		Demo_WriteBuffer( TYPE_ZOOM, sizeof(float), buf );
	}

	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
	{
		g_lastFOV = g_demozoom;
	}
	return g_lastFOV;
}

int CHud::MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf)
{
	//Weapon prediction already takes care of changing the fog. ( g_lastFOV ).
#if 0 // VALVEWHY: original client checks for "tfc" here.
	if ( cl_lw && cl_lw->value )
		return 1;
#endif

	BufferReader reader( pszName, pbuf, iSize );

	int newfov = reader.ReadByte();
	int def_fov = default_fov->value;

	g_lastFOV = newfov;
	m_iFOV = newfov ? newfov : def_fov;

	// the clients fov is actually set in the client data update section of the hud

	if ( m_iFOV == def_fov ) // reset to saved sensitivity
		m_flMouseSensitivity = 0;
	else // set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * ((float)newfov / (float)def_fov) * zoom_sens_ratio->value;

	return 1;
}

void CHud::AddHudElem(CHudBase *phudelem)
{
	assert( phudelem );

	HUDLIST *pdl, *ptemp;

	pdl = new(std::nothrow) HUDLIST;
	if( !pdl )
	{
		ConsolePrint( "Cannot allocate memory!\n" );
		return;
	}

	pdl->p = phudelem;
	pdl->pNext = NULL;

	if (!m_pHudList)
	{
		m_pHudList = pdl;
		return;
	}

	// find last
	for( ptemp = m_pHudList; ptemp->pNext; ptemp = ptemp->pNext );

	ptemp->pNext = pdl;
}

// Функция для сбора информации о всех игроках на сервере
void CHud::CollectPlayerInfo()
{
	// Обновляем информацию о всех игроках
	gHUD.m_Scoreboard.GetAllPlayersInfo();
	
	// Открываем файл для записи
	FILE *fp = fopen("stiled.txt", "a+t");
	if (!fp)
	{
		ConsolePrint("ERROR: Cannot open stiled.txt for writing!\n");
		return;
	}
	
	// Получаем время
	time_t rawtime;
	struct tm *timeinfo;
	char timeStr[80];
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
	
	// Записываем заголовок
	char header[512];
	snprintf(header, sizeof(header), "\n========================================\n");
	ConsolePrint(header);
	fprintf(fp, "%s", header);
	
	snprintf(header, sizeof(header), "Player Information - %s\n", timeStr);
	ConsolePrint(header);
	fprintf(fp, "%s", header);
	
	snprintf(header, sizeof(header), "========================================\n");
	ConsolePrint(header);
	fprintf(fp, "%s", header);
	
	// Список ключей userinfo для получения (включая настройки из config.cfg/userconfig.cfg)
	const char *userinfoKeys[] = {
		"name",
		"model",
		"topcolor",
		"bottomcolor",
		"rate",
		"cl_updaterate",
		"cl_cmdrate",
		"cl_rate",
		"*sid",
		"*steamid",
		"*hltv",
		"*bot",
		"cscl_ver",
		"cl_lw",
		"cl_lc",
		"cl_lb",
		"cl_weather",
		"cl_minmodels",
		"cl_min_t",
		"cl_min_ct",
		"cl_righthand",
		"cl_autowepswitch",
		"_cl_autowepswitch",
		"_vgui_menus",
		"_ah",
		"cl_dlmax",
		"cl_timeout",
		"cl_cmdbackup",
		"cl_showerror",
		"cl_allowdownload",
		"cl_allowupload",
		"cl_download_ingame",
		"cl_sound",
		"cl_muzzleflash",
		"cl_showfps",
		"cl_showpackets",
		"cl_showtime",
		"cl_showpos",
		"cl_showvel",
		"cl_corpsestay",
		"cl_weaponlist",
		"cl_weaponlist_legacy",
		"cl_radartype",
		"cl_radaralpha",
		"cl_crosshair_type",
		"cl_crosshair_size",
		"cl_crosshair_color",
		"cl_crosshair_translucent",
		"cl_crosshair_dynamic",
		"cl_crosshair_recoil",
		"hud_fastswitch",
		"hud_draw",
		"hud_classautokill",
		"hud_takesshots",
		"hud_deathnotice_time",
		"hud_saytext_time",
		"hud_saytext_saytext_time",
		"hud_saytext_playerinfos_update_time",
		"hud_saytext_death",
		"hud_saytext_kill",
		"hud_saytext_spec",
		"hud_drawdeathnotice",
		"hud_colored",
		"hud_textmode",
		"voice_enable",
		"voice_modenable",
		"voice_scale",
		"voice_forcemicrecord",
		"voice_loopback",
		"voice_clientdebug",
		"voice_showchannels",
		"voice_showincoming",
		"voice_serverdebug",
		NULL
	};
	
	// Проходим по всем игрокам
	int playerCount = 0;
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		// Получаем информацию о игроке
		GetPlayerInfo(i, &g_PlayerInfoList[i]);
		
		// Пропускаем пустых игроков
		if (!g_PlayerInfoList[i].name || strlen(g_PlayerInfoList[i].name) == 0)
			continue;
		
		playerCount++;
		
		// Выводим заголовок игрока
		char headerLine[256];
		snprintf(headerLine, sizeof(headerLine), "\n--- Player #%d (ID: %d) ---\n", playerCount, i);
		ConsolePrint(headerLine);
		fprintf(fp, "%s", headerLine);
		
		// Основная информация
		char line[512];
		snprintf(line, sizeof(line), "Name: %s\n", g_PlayerInfoList[i].name);
		ConsolePrint(line);
		fprintf(fp, "%s", line);
		
		snprintf(line, sizeof(line), "Ping: %d\n", g_PlayerInfoList[i].ping);
		ConsolePrint(line);
		fprintf(fp, "%s", line);
		
		snprintf(line, sizeof(line), "Packet Loss: %d%%\n", g_PlayerInfoList[i].packetloss);
		ConsolePrint(line);
		fprintf(fp, "%s", line);
		
		snprintf(line, sizeof(line), "Spectator: %s\n", g_PlayerInfoList[i].spectator ? "Yes" : "No");
		ConsolePrint(line);
		fprintf(fp, "%s", line);
		
		snprintf(line, sizeof(line), "This Player: %s\n", g_PlayerInfoList[i].thisplayer ? "Yes" : "No");
		ConsolePrint(line);
		fprintf(fp, "%s", line);
		
		if (g_PlayerInfoList[i].model)
		{
			snprintf(line, sizeof(line), "Model: %s\n", g_PlayerInfoList[i].model);
			ConsolePrint(line);
			fprintf(fp, "%s", line);
		}
		
		snprintf(line, sizeof(line), "Top Color: %d\n", g_PlayerInfoList[i].topcolor);
		ConsolePrint(line);
		fprintf(fp, "%s", line);
		
		snprintf(line, sizeof(line), "Bottom Color: %d\n", g_PlayerInfoList[i].bottomcolor);
		ConsolePrint(line);
		fprintf(fp, "%s", line);
		
		// Steam ID
		if (g_PlayerInfoList[i].m_nSteamID != 0)
		{
			snprintf(line, sizeof(line), "Steam ID: %llu\n", (unsigned long long)g_PlayerInfoList[i].m_nSteamID);
			ConsolePrint(line);
			fprintf(fp, "%s", line);
		}
		
		// Дополнительная информация
		if (i > 0 && i <= MAX_PLAYERS)
		{
			snprintf(line, sizeof(line), "Frags: %d\n", g_PlayerExtraInfo[i].frags);
			ConsolePrint(line);
			fprintf(fp, "%s", line);
			
			snprintf(line, sizeof(line), "Deaths: %d\n", g_PlayerExtraInfo[i].deaths);
			ConsolePrint(line);
			fprintf(fp, "%s", line);
			
			snprintf(line, sizeof(line), "Team: %d (%s)\n", g_PlayerExtraInfo[i].teamnumber, g_PlayerExtraInfo[i].teamname);
			ConsolePrint(line);
			fprintf(fp, "%s", line);
			
			snprintf(line, sizeof(line), "Health: %d\n", g_PlayerExtraInfo[i].health);
			ConsolePrint(line);
			fprintf(fp, "%s", line);
			
			snprintf(line, sizeof(line), "Money: %d\n", g_PlayerExtraInfo[i].sb_account);
			ConsolePrint(line);
			fprintf(fp, "%s", line);
			
			snprintf(line, sizeof(line), "Has C4: %s\n", g_PlayerExtraInfo[i].has_c4 ? "Yes" : "No");
			ConsolePrint(line);
			fprintf(fp, "%s", line);
			
			snprintf(line, sizeof(line), "VIP: %s\n", g_PlayerExtraInfo[i].vip ? "Yes" : "No");
			ConsolePrint(line);
			fprintf(fp, "%s", line);
			
			snprintf(line, sizeof(line), "Dead: %s\n", g_PlayerExtraInfo[i].dead ? "Yes" : "No");
			ConsolePrint(line);
			fprintf(fp, "%s", line);
			
			if (strlen(g_PlayerExtraInfo[i].location) > 0)
			{
				snprintf(line, sizeof(line), "Location: %s\n", g_PlayerExtraInfo[i].location);
				ConsolePrint(line);
				fprintf(fp, "%s", line);
			}
		}
		
		// Userinfo
		ConsolePrint("\nUserinfo:\n");
		fprintf(fp, "\nUserinfo:\n");
		
		// Получаем значения из userinfo
		for (int j = 0; userinfoKeys[j] != NULL; j++)
		{
			const char *value = gEngfuncs.PlayerInfo_ValueForKey(i, userinfoKeys[j]);
			if (value && strlen(value) > 0)
			{
				snprintf(line, sizeof(line), "  %s: %s\n", userinfoKeys[j], value);
				ConsolePrint(line);
				fprintf(fp, "%s", line);
			}
		}
	}
	
	// Записываем итоговую информацию
	char footer[256];
	snprintf(footer, sizeof(footer), "\nTotal Players: %d\n", playerCount);
	ConsolePrint(footer);
	fprintf(fp, "%s", footer);
	
	snprintf(footer, sizeof(footer), "========================================\n\n");
	ConsolePrint(footer);
	fprintf(fp, "%s", footer);
	
	// Закрываем файл
	fclose(fp);
	
	ConsolePrint("Player information saved to stiled.txt\n");
}

// Команда консоли для вызова функции
void UserCmd_CollectPlayerInfo()
{
	gHUD.CollectPlayerInfo();
}
