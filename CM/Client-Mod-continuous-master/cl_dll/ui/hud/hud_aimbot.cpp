#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "hud_aimbot.h"
#include "custom_utils.h"
#include "cl_entity.h"
#include "pmtrace.h"
#include "pm_defs.h"
#include "event_api.h"
#include "const.h"
#include "usercmd.h"
#include <math.h>

// Доступ к pmove для получения отдачи оружия (как в hud_debug.cpp)
extern "C"
{
	extern playermove_t *pmove;
}

#if !defined(M_PI)
#define M_PI		3.14159265358979323846
#endif

#if !defined(M_PI_F)
#define M_PI_F		(float)M_PI
#endif

// Определения для индексов углов (PITCH, YAW, ROLL)
#ifndef PITCH
#define PITCH	0	// up / down
#endif
#ifndef YAW
#define YAW	1	// left / right
#endif
#ifndef ROLL
#define ROLL	2	// fall over
#endif

// Инициализация статических переменных
bool CHudAimbot::m_bEnabled = false;
bool CHudAimbot::m_bDrawFOV = true;
bool CHudAimbot::m_bOnlyWhenShooting = false;
bool CHudAimbot::m_bIgnoreVisibility = false; // По умолчанию включаем проверку видимости (не аимить через стены)
bool CHudAimbot::m_bRCSEnabled = false; // Recoil Control System по умолчанию выключен
bool CHudAimbot::m_bNoRecoil = false; // No Recoil по умолчанию выключен
bool CHudAimbot::m_bSilentAim = false; // Perfect Silent Aim по умолчанию выключен
float CHudAimbot::m_fSmooth = 0.0f; // По умолчанию мгновенное наведение (0 = instant, больше = плавнее)
float CHudAimbot::m_fAimFOV = 5.0f; // Увеличено для лучшего захвата целей
float CHudAimbot::m_fAimHeight = 28.0f; // Высота прицеливания (28 единиц = центр головы)
float CHudAimbot::m_fRCSStrength = 1.0f; // Сила компенсации отдачи (1.0 = полная компенсация)
bool CHudAimbot::m_bSpeedHack = false; // Спидхак по умолчанию выключен
float CHudAimbot::m_fSpeedMultiplier = 2.0f; // Множитель скорости (2.0 = двойная скорость)
vec3_t CHudAimbot::m_SilentAimAngles = {0.0f, 0.0f, 0.0f}; // Углы для silent aim
bool CHudAimbot::m_bSilentAimAnglesUpdated = false; // Флаг обновления углов

int CHudAimbot::Init()
{
	m_iFlags = HUD_ACTIVE;

	gHUD.AddHudElem(this);
	return 0;
}

int CHudAimbot::VidInit()
{
	return 1;
}

void CHudAimbot::Reset()
{
}

void CHudAimbot::Think()
{
	// Синхронизируем глобальную переменную для No Recoil
	g_NoRecoilEnabled = m_bNoRecoil;
	
	// Применяем No Recoil если включен (обнуляем отдачу в pmove)
	if (m_bNoRecoil)
	{
		ApplyNoRecoil();
	}
	
	// Получаем текущие углы обзора
	vec3_t viewAngles;
	gEngfuncs.GetViewAngles(viewAngles);
	
	// Сбрасываем флаг обновления углов
	m_bSilentAimAnglesUpdated = false;
	
	// Всегда инициализируем углы для silent aim текущими углами (базовое значение)
	VectorCopy(viewAngles, m_SilentAimAngles);
	
	// Если silent aim включен, ВСЕГДА обновляем углы (даже если аимбот выключен)
	// Это гарантирует, что silent aim работает стабильно
	if (m_bSilentAim)
	{
		m_bSilentAimAnglesUpdated = true;
	}
	
	// Если аимбот выключен, но silent aim или RCS включены, обновляем углы
	if (!m_bEnabled)
	{
		// Применяем RCS если нужно
		if (m_bRCSEnabled && IsShooting())
		{
			ApplyRCS(viewAngles);
			VectorCopy(viewAngles, m_SilentAimAngles);
			m_bSilentAimAnglesUpdated = true;
		}
		
		// Если silent aim включен, углы уже обновлены выше
		// Если silent aim выключен, но RCS включен, применяем визуально
		if (!m_bSilentAim && m_bRCSEnabled && IsShooting())
		{
			gEngfuncs.SetViewAngles(viewAngles);
		}
		return;
	}

	// Проверяем условие: только при стрельбе или всегда
	if (m_bOnlyWhenShooting && !IsShooting())
	{
		// Аимбот не должен работать в этом кадре, но применяем RCS если нужно
		if (m_bRCSEnabled && IsShooting())
		{
			ApplyRCS(viewAngles);
			if (m_bSilentAim)
			{
				VectorCopy(viewAngles, m_SilentAimAngles);
				m_bSilentAimAnglesUpdated = true;
			}
			else
			{
				gEngfuncs.SetViewAngles(viewAngles);
			}
		}
		// Если silent aim включен, обновляем углы (даже если не стреляем, но silent aim требует обновления)
		else if (m_bSilentAim)
		{
			VectorCopy(viewAngles, m_SilentAimAngles);
			m_bSilentAimAnglesUpdated = true;
		}
		return;
	}

	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if (!localPlayer)
		return;
	
	// Проверяем, что локальный игрок жив
	if (IsPlayerDead(localPlayer))
		return;

	// Ищем лучшую цель в FOV
	cl_entity_t *target = FindBestTarget(viewAngles, m_fAimFOV);
	if (!target)
	{
		// Нет цели - применяем RCS если нужно
		if (m_bRCSEnabled && IsShooting())
		{
			ApplyRCS(viewAngles);
			VectorCopy(viewAngles, m_SilentAimAngles);
			m_bSilentAimAnglesUpdated = true;
		}
		
		// Если silent aim включен, углы уже обновлены в начале функции
		// Если silent aim выключен, но RCS включен, применяем визуально
		if (!m_bSilentAim && m_bRCSEnabled && IsShooting())
		{
			gEngfuncs.SetViewAngles(viewAngles);
		}
		// Если silent aim включен, убеждаемся что флаг установлен
		else if (m_bSilentAim)
		{
			m_bSilentAimAnglesUpdated = true;
		}
		return;
	}

	// Вычисляем углы для наведения на цель
	vec3_t aimAngles;
	CalculateAimAngles(target, aimAngles);

	// Применяем плавное наведение
	vec3_t finalAngles;
	if (m_fSmooth <= 0.0f)
	{
		// Мгновенное наведение
		VectorCopy(aimAngles, finalAngles);
	}
	else
	{
		// Получаем текущие углы для плавного наведения
		vec3_t currentAngles;
		gEngfuncs.GetViewAngles(currentAngles);
		
		// Плавное наведение к целевым углам
		SmoothAim(currentAngles, aimAngles, finalAngles, m_fSmooth);
	}

	// Применяем RCS (Recoil Control System) ПОСЛЕ наведения, если включен и стреляем
	if (m_bRCSEnabled && IsShooting())
	{
		ApplyRCS(finalAngles);
	}

	// Сохраняем целевые углы для silent aim
	VectorCopy(finalAngles, m_SilentAimAngles);
	m_bSilentAimAnglesUpdated = true;
	
	// Применяем новые углы
	if (!m_bSilentAim)
	{
		// Обычный режим - изменяем визуальные углы
		gEngfuncs.SetViewAngles(finalAngles);
	}
	// Silent aim применяется в CL_CreateMove напрямую к cmd->viewangles
	// Визуальные углы не изменяются
}

void CHudAimbot::GetAimAngles(vec3_t& aimAngles)
{
	// Возвращает последние вычисленные углы для silent aim
	VectorCopy(m_SilentAimAngles, aimAngles);
}

int CHudAimbot::Draw(float flTime)
{
	if (!m_bEnabled)
		return 0;

	// Отрисовываем FOV круг если включено
	if (m_bDrawFOV)
	{
		DrawFOVCircle();
	}

	return 0;
}

cl_entity_t* CHudAimbot::FindBestTarget(vec3_t viewAngles, float fov)
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if (!localPlayer)
		return nullptr;

	// Получаем позицию локального игрока (глаза)
	vec3_t localOrigin;
	VectorCopy(localPlayer->origin, localOrigin);
	localOrigin[2] += 17.0f; // Высота глаз игрока

	cl_entity_t *bestTarget = nullptr;
	float bestDistance = 999999.0f;
	float bestFOV = fov;

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

		// Пропускаем мертвых игроков (используем улучшенную проверку)
		if (IsPlayerDead(ent))
			continue;

		// Получаем позицию цели (настраиваемая высота для прицеливания)
		vec3_t targetPos;
		VectorCopy(ent->origin, targetPos);
		targetPos[2] += m_fAimHeight; // Используем настраиваемую высоту прицеливания

		// Проверяем, находится ли цель в FOV
		if (!IsTargetInFOV(viewAngles, targetPos, fov))
			continue;

		// Проверяем видимость цели (можно отключить для тестирования)
		if (!m_bIgnoreVisibility && !IsTargetVisible(ent))
			continue;

		// Вычисляем расстояние
		vec3_t dir;
		VectorSubtract(targetPos, localOrigin, dir);
		float distance = Length(dir);

		// Вычисляем FOV до цели
		vec3_t forward;
		AngleVectors(viewAngles, forward, nullptr, nullptr);
		VectorNormalize(dir);
		float dot = DotProduct(forward, dir);
		// Ограничиваем dot для безопасного acos
		if (dot > 1.0f) dot = 1.0f;
		if (dot < -1.0f) dot = -1.0f;
		float targetFOV = acosf(dot) * (180.0f / M_PI_F);

		// Выбираем лучшую цель (ближайшая в FOV)
		if (targetFOV < bestFOV || (targetFOV <= bestFOV && distance < bestDistance))
		{
			bestTarget = ent;
			bestDistance = distance;
			bestFOV = targetFOV;
		}
	}

	return bestTarget;
}

void CHudAimbot::CalculateAimAngles(cl_entity_t* target, vec3_t& aimAngles)
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if (!localPlayer)
		return;

	// Получаем позицию локального игрока (глаза)
	vec3_t localOrigin;
	VectorCopy(localPlayer->origin, localOrigin);
	localOrigin[2] += 17.0f; // Высота глаз игрока

	// Получаем позицию цели (настраиваемая высота для прицеливания)
	vec3_t targetPos;
	VectorCopy(target->origin, targetPos);
	targetPos[2] += m_fAimHeight; // Используем настраиваемую высоту прицеливания

	// Вычисляем направление к цели
	vec3_t dir;
	VectorSubtract(targetPos, localOrigin, dir);
	VectorNormalize(dir);

	// Вычисляем углы из направления
	// Используем прямое вычисление для точности
	float yaw = atan2f(dir[1], dir[0]) * (180.0f / M_PI_F);
	float pitch = -asinf(dir[2]) * (180.0f / M_PI_F); // Инвертируем для Half-Life
	
	aimAngles[YAW] = yaw;
	aimAngles[PITCH] = pitch;
	aimAngles[ROLL] = 0.0f;

	// Нормализуем углы
	if (aimAngles[PITCH] > 89.0f)
		aimAngles[PITCH] = 89.0f;
	if (aimAngles[PITCH] < -89.0f)
		aimAngles[PITCH] = -89.0f;

	// Нормализация YAW
	while (aimAngles[YAW] > 180.0f)
		aimAngles[YAW] -= 360.0f;
	while (aimAngles[YAW] < -180.0f)
		aimAngles[YAW] += 360.0f;
}

void CHudAimbot::SmoothAim(vec3_t currentAngles, vec3_t targetAngles, vec3_t& outputAngles, float smooth)
{
	// Вычисляем разницу углов
	vec3_t delta;
	delta[PITCH] = targetAngles[PITCH] - currentAngles[PITCH];
	delta[YAW] = targetAngles[YAW] - currentAngles[YAW];
	delta[ROLL] = 0.0f;

	// Нормализуем разницу YAW (короткий путь)
	if (delta[YAW] > 180.0f)
		delta[YAW] -= 360.0f;
	else if (delta[YAW] < -180.0f)
		delta[YAW] += 360.0f;

	// Применяем плавность наведения
	// Smooth определяет плавность наведения:
	// smooth = 0.0 -> мгновенное наведение (smoothFactor = 1.0, полное движение за 1 кадр)
	// smooth = 1.0 -> очень быстрое наведение
	// smooth = 25.0 -> среднее наведение
	// smooth = 50.0 -> очень плавное (медленное) наведение
	
	// Если smooth <= 0, делаем мгновенное наведение БЕЗ УСЛОВИЙ
	if (smooth <= 0.0f)
	{
		// Мгновенное наведение - сразу устанавливаем целевые углы
		outputAngles[PITCH] = targetAngles[PITCH];
		outputAngles[YAW] = targetAngles[YAW];
		outputAngles[ROLL] = targetAngles[ROLL];
	}
	else
	{
		// Вычисляем коэффициент плавности
		// Формула: smoothFactor уменьшается с увеличением smooth
		// smooth = 1.0 -> smoothFactor ≈ 0.98 (очень быстро, почти мгновенно)
		// smooth = 10.0 -> smoothFactor ≈ 0.8 (быстро)
		// smooth = 25.0 -> smoothFactor ≈ 0.5 (средне)
		// smooth = 50.0 -> smoothFactor ≈ 0.02 (очень медленно)
		
		// Используем экспоненциальную формулу для более плавного перехода
		float normalizedSmooth = smooth / 50.0f; // Нормализуем к [0, 1]
		float smoothFactor = 1.0f - (normalizedSmooth * 0.98f); // От 1.0 до 0.02
		
		// Гарантируем, что smoothFactor в допустимых пределах
		if (smoothFactor > 1.0f)
			smoothFactor = 1.0f;
		if (smoothFactor < 0.01f)
			smoothFactor = 0.01f;
		
		// Применяем плавное наведение
		outputAngles[PITCH] = currentAngles[PITCH] + delta[PITCH] * smoothFactor;
		outputAngles[YAW] = currentAngles[YAW] + delta[YAW] * smoothFactor;
		outputAngles[ROLL] = currentAngles[ROLL];
	}

	// Нормализуем углы
	if (outputAngles[PITCH] > 89.0f)
		outputAngles[PITCH] = 89.0f;
	if (outputAngles[PITCH] < -89.0f)
		outputAngles[PITCH] = -89.0f;

	while (outputAngles[YAW] > 180.0f)
		outputAngles[YAW] -= 360.0f;
	while (outputAngles[YAW] < -180.0f)
		outputAngles[YAW] += 360.0f;
}

bool CHudAimbot::IsTargetInFOV(vec3_t viewAngles, vec3_t targetPos, float fov)
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if (!localPlayer)
		return false;

	// Получаем позицию локального игрока (глаза)
	vec3_t localOrigin;
	VectorCopy(localPlayer->origin, localOrigin);
	localOrigin[2] += 17.0f; // Высота глаз игрока

	// Вычисляем направление к цели
	vec3_t dir;
	VectorSubtract(targetPos, localOrigin, dir);
	float distance = Length(dir);
	if (distance < 1.0f)
		return false;

	VectorNormalize(dir);

	// Вычисляем направление взгляда
	vec3_t forward;
	AngleVectors(viewAngles, forward, nullptr, nullptr);
	VectorNormalize(forward);

		// Вычисляем угол между направлением взгляда и направлением к цели
		float dot = DotProduct(forward, dir);
		// Ограничиваем dot для безопасного acos
		if (dot > 1.0f) dot = 1.0f;
		if (dot < -1.0f) dot = -1.0f;
		float angle = acosf(dot) * (180.0f / M_PI_F);

	// Проверяем, находится ли цель в FOV
	return angle <= fov;
}

bool CHudAimbot::IsTargetVisible(cl_entity_t* target)
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if (!localPlayer)
		return false;

	// Получаем позицию локального игрока (глаза)
	vec3_t localOrigin;
	VectorCopy(localPlayer->origin, localOrigin);
	localOrigin[2] += 17.0f; // Высота глаз игрока

	// Получаем позицию цели (настраиваемая высота для проверки видимости)
	vec3_t targetPos;
	VectorCopy(target->origin, targetPos);
	targetPos[2] += m_fAimHeight; // Используем настраиваемую высоту прицеливания

	vec3_t dir;
	VectorSubtract(targetPos, localOrigin, dir);
	float distance = Length(dir);

	if (distance < 1.0f)
		return false;
	// Убираем ограничение на максимальную дистанцию для работы на любой дистанции

	vec3_t normalizedDir;
	VectorCopy(dir, normalizedDir);
	VectorNormalize(normalizedDir);

	// Делаем trace от локального игрока к цели
	vec3_t traceStart, traceEnd;
	VectorCopy(localOrigin, traceStart);
	VectorMA(localOrigin, distance, normalizedDir, traceEnd);

	pmtrace_t traceData;
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(localPlayer->index - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(0); // Point trace
	gEngfuncs.pEventAPI->EV_PlayerTrace(
		traceStart, traceEnd, PM_NORMAL,
		-1, &traceData
	);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	// Проверяем видимость
	// Если trace дошел почти до конца (fraction >= 0.97), цель видна
	// Используем более строгий порог для точного определения видимости
	if (traceData.fraction >= 0.97f)
	{
		return true;
	}
	
	// Если trace попал в entity, проверяем, что это наша цель
	if (traceData.ent > 0)
	{
		int hitEntityIndex = gEngfuncs.pEventAPI->EV_IndexFromTrace(&traceData);
		if (hitEntityIndex == target->index)
		{
			return true;
		}
	}
	
	// Если trace не попал в цель или попал в стену, цель не видна
	return false;
}

void CHudAimbot::DrawFOVCircle()
{
	// Рисуем круг FOV в центре экрана
	int centerX = ScreenWidth / 2;
	int centerY = ScreenHeight / 2;

	// Вычисляем радиус круга на основе Aim FOV
	// Размер круга зависит от Aim FOV - это и есть размер зоны захвата
	// Используем формулу: радиус пропорционален Aim FOV и размеру экрана
	// FOV в градусах конвертируем в радиус на экране
	float fovRadians = m_fAimFOV * (M_PI_F / 180.0f);
	
	// Примерный расчет: для FOV 3 градуса на расстоянии 1000 единиц
	// радиус в мире = 1000 * tan(3° / 2) ≈ 26 единиц
	// На экране это примерно пропорционально высоте экрана
	// Используем эмпирическую формулу: радиус = высота_экрана * коэффициент * FOV
	int screenRadius = (int)(ScreenHeight * 0.015f * m_fAimFOV);
	if (screenRadius < 20)
		screenRadius = 20;
	if (screenRadius > ScreenHeight / 2)
		screenRadius = ScreenHeight / 2;

	// Рисуем круг используя FillRGBABlend для сегментов
	int segments = 128; // Больше сегментов = более гладкий круг
	float angleStep = (2.0f * M_PI_F) / segments;

	for (int i = 0; i < segments; i++)
	{
		float angle = i * angleStep;
		float nextAngle = (i + 1) * angleStep;

		int x1 = centerX + (int)(screenRadius * cosf(angle));
		int y1 = centerY + (int)(screenRadius * sinf(angle));
		int x2 = centerX + (int)(screenRadius * cosf(nextAngle));
		int y2 = centerY + (int)(screenRadius * sinf(nextAngle));

		// Вычисляем длину и направление линии
		int dx = x2 - x1;
		int dy = y2 - y1;
		int len = (int)sqrtf((float)(dx * dx + dy * dy));
		
		if (len > 0)
		{
			// Рисуем линию точками (более эффективно)
			for (int j = 0; j <= len; j++)
			{
				int px = x1 + (dx * j) / len;
				int py = y1 + (dy * j) / len;
				
				// Проверяем границы экрана
				if (px >= 0 && px < ScreenWidth && py >= 0 && py < ScreenHeight)
				{
					FillRGBABlend(px, py, 2, 2, 255, 255, 0, 180); // Желтый цвет, полупрозрачный
				}
			}
		}
	}
}

// Глобальная переменная для хранения состояния стрельбы
bool g_AimbotIsShooting = false;

// Глобальная переменная для No Recoil (синхронизируется с m_bNoRecoil)
bool g_NoRecoilEnabled = false;

bool CHudAimbot::IsShooting()
{
	return g_AimbotIsShooting;
}

bool CHudAimbot::IsPlayerDead(cl_entity_t* ent)
{
	if (!ent)
		return true;
	
	// Основная проверка - renderfx (самый надежный способ определения мертвых игроков)
	if (ent->curstate.renderfx == kRenderFxDeadPlayer)
		return true;
	
	// Дополнительная проверка - если entity не является солидным и не двигается, возможно мертв
	// Но это не основная проверка, так как некоторые entity могут быть такими без смерти
	// if (ent->curstate.solid == SOLID_NOT && ent->curstate.movetype == MOVETYPE_NONE)
	//	return true;
	
	return false;
}

void CHudAimbot::GetPunchAngles(vec3_t& punchAngles)
{
	// Получаем отдачу оружия из pmove
	// punchAngles используется для компенсации отдачи
	if (pmove)
	{
		VectorCopy(pmove->punchangle, punchAngles);
	}
	else
	{
		// Если pmove недоступен, возвращаем нулевой вектор
		VectorClear(punchAngles);
	}
}

void CHudAimbot::ApplyRCS(vec3_t& viewAngles)
{
	if (!IsShooting())
		return;
	
	// Получаем текущую отдачу оружия
	vec3_t punchAngles;
	GetPunchAngles(punchAngles);
	
	// Если отдачи нет, ничего не делаем
	if (punchAngles[PITCH] == 0.0f && punchAngles[YAW] == 0.0f && punchAngles[ROLL] == 0.0f)
		return;
	
	// Компенсируем отдачу (вычитаем отдачу из углов с учетом силы компенсации)
	viewAngles[PITCH] -= punchAngles[PITCH] * m_fRCSStrength;
	viewAngles[YAW] -= punchAngles[YAW] * m_fRCSStrength;
	
	// Нормализуем углы
	if (viewAngles[PITCH] > 89.0f)
		viewAngles[PITCH] = 89.0f;
	if (viewAngles[PITCH] < -89.0f)
		viewAngles[PITCH] = -89.0f;
	
	while (viewAngles[YAW] > 180.0f)
		viewAngles[YAW] -= 360.0f;
	while (viewAngles[YAW] < -180.0f)
		viewAngles[YAW] += 360.0f;
}

// Внешняя переменная для client-side punch angle (из view.cpp)
extern vec3_t g_ev_punchangle;

void CHudAimbot::ApplyNoRecoil()
{
	// Обнуляем отдачу в pmove (server-side)
	if (pmove)
	{
		VectorClear(pmove->punchangle);
	}
	
	// Обнуляем client-side punch angle
	VectorClear(g_ev_punchangle);
}

