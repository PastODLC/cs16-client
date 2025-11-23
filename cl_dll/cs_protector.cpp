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
// cs_protector.cpp
//
// CS Protector - защита от команд сервера и разблокировка read-only команд
//

#include "hud.h"
#include "cl_util.h"
#include "cvardef.h"
#include <string.h>
#include <stdio.h>

// Глобальные переменные для защиты
static cvar_t *g_pCSProtectorEnabled = NULL;
static cvar_t *g_pCSProtectorWarn = NULL;
static cvar_t *g_pCSProtectorBlockAll = NULL;

// Буфер для хранения последней команды от сервера
static char g_LastServerCommand[512] = {0};
static bool g_bServerCommandReceived = false;

// Оригинальная функция FilteredClientCmd от движка
// Тип: int (*)(char *szCmdString)
static int (*g_pfnOriginalFilteredClientCmd)(char *szCmdString) = NULL;

// Флаг для отслеживания, инициализирован ли перехватчик
static bool g_bFilteredClientCmdHooked = false;

// Обёртка для перехвата команд от сервера
static int CSProtector_FilteredClientCmd_Wrapper(char *szCmdString)
{
	if (!szCmdString || !szCmdString[0])
	{
		// Если оригинальная функция существует, вызываем её
		if (g_pfnOriginalFilteredClientCmd)
			return g_pfnOriginalFilteredClientCmd(szCmdString);
		// Если оригинальной функции нет, но есть pfnClientCmd, используем его
		// pfnClientCmd принимает const char*, поэтому можем безопасно передать char*
		else if (gEngfuncs.pfnClientCmd)
			return gEngfuncs.pfnClientCmd((const char*)szCmdString);
		return 0;
	}
	
	// Проверяем, включена ли защита
	bool bProtectionEnabled = (g_pCSProtectorEnabled && g_pCSProtectorEnabled->value != 0.0f);
	bool bBlockAll = (g_pCSProtectorBlockAll && g_pCSProtectorBlockAll->value != 0.0f);
	bool bWarn = (g_pCSProtectorWarn && g_pCSProtectorWarn->value != 0.0f);
	
	// Если защита включена и нужно блокировать все команды
	if (bProtectionEnabled && bBlockAll)
	{
		// Сохраняем команду для логирования
		strncpy(g_LastServerCommand, szCmdString, sizeof(g_LastServerCommand) - 1);
		g_LastServerCommand[sizeof(g_LastServerCommand) - 1] = '\0';
		g_bServerCommandReceived = true;
		
		// Выводим предупреждение, если включено
		if (bWarn)
		{
			char msg[768];
			snprintf(msg, sizeof(msg), "^3[CS Protector]^7 Блокирована команда от сервера: ^1%s^7\n", szCmdString);
			gEngfuncs.pfnConsolePrint(msg);
			
			// Также выводим в центр экрана (CenterPrint)
			snprintf(msg, sizeof(msg), "Сервер попытался выполнить команду: %s", szCmdString);
			gEngfuncs.pfnCenterPrint(msg);
		}
		
		// Блокируем выполнение команды - НЕ вызываем оригинальную функцию
		return 0;
	}
	
	// Если защита не включена, вызываем оригинальную функцию
	if (g_pfnOriginalFilteredClientCmd)
		return g_pfnOriginalFilteredClientCmd(szCmdString);
	// Если оригинальной функции нет, но есть pfnClientCmd, используем его
	// pfnClientCmd принимает const char*, поэтому можем безопасно передать char*
	else if (gEngfuncs.pfnClientCmd)
		return gEngfuncs.pfnClientCmd((const char*)szCmdString);
	
	return 0;
}

// Функция для инициализации перехватчика команд
void CSProtector_SetupCommandHook()
{
	// Проверяем, не установлен ли уже перехватчик
	if (g_bFilteredClientCmdHooked)
		return;
	
	// Сохраняем оригинальную функцию, если она существует
	if (gEngfuncs.pfnFilteredClientCmd)
	{
		// Движок предоставил pfnFilteredClientCmd - это идеальный случай
		g_pfnOriginalFilteredClientCmd = gEngfuncs.pfnFilteredClientCmd;
		// Заменяем на нашу обёртку
		gEngfuncs.pfnFilteredClientCmd = CSProtector_FilteredClientCmd_Wrapper;
		g_bFilteredClientCmdHooked = true;
	}
	else if (gEngfuncs.pfnClientCmd)
	{
		// Если pfnFilteredClientCmd не установлен движком, устанавливаем нашу обёртку
		// Обёртка будет использовать pfnClientCmd как fallback
		// Это позволит перехватывать команды от сервера через FilteredClientCmd
		g_pfnOriginalFilteredClientCmd = NULL; // Нет оригинальной FilteredClientCmd
		// Устанавливаем нашу обёртку как FilteredClientCmd
		gEngfuncs.pfnFilteredClientCmd = CSProtector_FilteredClientCmd_Wrapper;
		g_bFilteredClientCmdHooked = true;
	}
	else
	{
		// Нет возможности перехватить команды - ни pfnFilteredClientCmd, ни pfnClientCmd не доступны
		g_bFilteredClientCmdHooked = false;
	}
}

// Эта функция не используется напрямую, так как команды от сервера обрабатываются в движке
// Но мы можем использовать механизм через cvar для блокировки команд
void CSProtector_HandleServerCommand(const char* cmd)
{
	// Эта функция вызывается для обработки команд от сервера
	// Теперь мы перехватываем их через FilteredClientCmd
	if (cmd && cmd[0])
	{
		CSProtector_FilteredClientCmd_Wrapper((char*)cmd);
	}
}

// Разблокировка read-only команд
void UnlockReadOnlyCommands()
{
	// Получаем первый cvar
	if (!gEngfuncs.GetFirstCvarPtr)
		return;
	
	cvar_t* pCvar = gEngfuncs.GetFirstCvarPtr();
	if (!pCvar)
		return;
	
	// Проходим по всем cvar'ам через поле next
	int count = 0;
	while (pCvar && count < 1000) // Защита от бесконечного цикла
	{
		// Убираем флаги защиты для read-only команд
		if (pCvar->flags & FCVAR_PROTECTED)
		{
			pCvar->flags &= ~FCVAR_PROTECTED;
		}
		
		if (pCvar->flags & FCVAR_SPONLY)
		{
			pCvar->flags &= ~FCVAR_SPONLY;
		}
		
		// Переходим к следующему cvar через поле next
		cvar_t* nextCvar = pCvar->next;
		if (!nextCvar || nextCvar == pCvar)
			break;
		
		pCvar = nextCvar;
		count++;
	}
}

// Флаг для отслеживания, были ли выведены сообщения о инициализации
static bool g_bProtectorMessagesPrinted = false;
static float g_fProtectorInitTime = 0.0f;

// Инициализация CS Protector
void CSProtector_Init()
{
	// Регистрируем cvar'ы
	g_pCSProtectorEnabled = gEngfuncs.pfnRegisterVariable("ebash3d_cs_protector", "1", FCVAR_ARCHIVE);
	g_pCSProtectorWarn = gEngfuncs.pfnRegisterVariable("ebash3d_cs_protector_warn", "1", FCVAR_ARCHIVE);
	g_pCSProtectorBlockAll = gEngfuncs.pfnRegisterVariable("ebash3d_cs_protector_block_all", "1", FCVAR_ARCHIVE);
	
	// Устанавливаем перехватчик команд от сервера
	CSProtector_SetupCommandHook();
	
	// Разблокируем read-only команды
	UnlockReadOnlyCommands();
	
	// Очищаем консоль перед выводом сообщений о инициализации
	gEngfuncs.pfnClientCmd("clear\n");
	
	// Инициализируем флаг для задержки сообщений
	// Сообщения будут выведены в CSProtector_Update через небольшой промежуток времени
	g_fProtectorInitTime = -1.0f; // Флаг: время будет установлено в Update
	g_bProtectorMessagesPrinted = false;
}

// Функция для вывода сообщений о инициализации с задержкой
void CSProtector_PrintInitMessages()
{
	if (g_bProtectorMessagesPrinted)
		return;
	
	// Инициализируем время при первом вызове Update, если оно еще не установлено
	if (g_fProtectorInitTime < 0.0f)
	{
		float currentTime = gEngfuncs.GetClientTime();
		if (currentTime > 0.0f)
		{
			g_fProtectorInitTime = currentTime;
		}
		else
		{
			// Время еще не доступно, ждем следующего кадра
			return;
		}
	}
	
	// Проверяем, прошла ли задержка (0.15 секунды)
	float currentTime = gEngfuncs.GetClientTime();
	if (currentTime <= 0.0f || g_fProtectorInitTime <= 0.0f)
		return;
	
	// Задержка перед выводом сообщений (чтобы команда clear успела выполниться)
	if (currentTime - g_fProtectorInitTime < 0.15f)
		return;
	
	// Выводим сообщение о инициализации
	gEngfuncs.pfnConsolePrint("^2[CS Protector]^7 Инициализирован. Защита от команд сервера активна.\n");
	gEngfuncs.pfnConsolePrint("^2[CS Protector]^7 Read-only команды разблокированы.\n");
	if (g_bFilteredClientCmdHooked)
	{
		gEngfuncs.pfnConsolePrint("^2[CS Protector]^7 Перехватчик команд от сервера установлен.\n");
	}
	else
	{
		gEngfuncs.pfnConsolePrint("^3[CS Protector]^7 ВНИМАНИЕ: Перехватчик команд не установлен. Защита может не работать.\n");
	}
	gEngfuncs.pfnConsolePrint("^2[CS Protector]^7 Cvar'ы: ebash3d_cs_protector (1/0), ebash3d_cs_protector_block_all (1/0), ebash3d_cs_protector_warn (1/0)\n");
	
	g_bProtectorMessagesPrinted = true;
}

// Обновление защиты (вызывается каждый кадр)
void CSProtector_Update()
{
	// Выводим сообщения о инициализации с задержкой
	CSProtector_PrintInitMessages();
	
	// Разблокируем read-only команды периодически (на случай если движок их снова заблокирует)
	if (g_pCSProtectorEnabled && g_pCSProtectorEnabled->value != 0.0f)
	{
		static float lastUnlockTime = 0.0f;
		float currentTime = gEngfuncs.GetClientTime();
		
		// Разблокируем каждые 10 секунд
		if (currentTime - lastUnlockTime > 10.0f)
		{
			UnlockReadOnlyCommands();
			lastUnlockTime = currentTime;
		}
	}
}

