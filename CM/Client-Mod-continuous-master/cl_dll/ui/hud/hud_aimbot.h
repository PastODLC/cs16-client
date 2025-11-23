#pragma once
#if !defined(HUD_AIMBOT_H)
#define HUD_AIMBOT_H

#include "hud.h"

class CHudAimbot : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );
	void Think( void ); // Вызывается каждый кадр для наведения

	// Настройки аимбота
	static bool m_bEnabled;
	static bool m_bDrawFOV;
	static bool m_bOnlyWhenShooting;
	static bool m_bIgnoreVisibility; // Временная опция для тестирования (наведение через стены)
	static bool m_bRCSEnabled; // Recoil Control System
	static bool m_bNoRecoil; // No Recoil - полностью убирает отдачу
	static bool m_bSilentAim; // Perfect Silent Aim (тестово)
	static float m_fSmooth;
	static float m_fAimFOV;
	static float m_fAimHeight; // Высота прицеливания (относительно origin цели)
	static float m_fRCSStrength; // Сила компенсации отдачи (0.0 - 1.0)
	
	// Спидхак
	static bool m_bSpeedHack;
	static float m_fSpeedMultiplier; // Множитель скорости (1.0 = нормальная скорость)
	
	// Углы для silent aim (не визуальные)
	static vec3_t m_SilentAimAngles;
	
	// Флаг, указывающий что углы были обновлены в этом кадре
	static bool m_bSilentAimAnglesUpdated;

	// Функции для доступа из ImGui
	static bool IsEnabled() { return m_bEnabled; }
	static void SetEnabled(bool enabled) { m_bEnabled = enabled; }
	
	static bool IsDrawFOV() { return m_bDrawFOV; }
	static void SetDrawFOV(bool enabled) { m_bDrawFOV = enabled; }
	
	static bool IsOnlyWhenShooting() { return m_bOnlyWhenShooting; }
	static void SetOnlyWhenShooting(bool enabled) { m_bOnlyWhenShooting = enabled; }
	
	static bool IsIgnoreVisibility() { return m_bIgnoreVisibility; }
	static void SetIgnoreVisibility(bool enabled) { m_bIgnoreVisibility = enabled; }
	
	static float GetSmooth() { return m_fSmooth; }
	static void SetSmooth(float smooth) { m_fSmooth = smooth; }
	
	static float GetAimFOV() { return m_fAimFOV; }
	static void SetAimFOV(float fov) { m_fAimFOV = fov; }
	
	static float GetAimHeight() { return m_fAimHeight; }
	static void SetAimHeight(float height) { m_fAimHeight = height; }
	
	static bool IsRCSEnabled() { return m_bRCSEnabled; }
	static void SetRCSEnabled(bool enabled) { m_bRCSEnabled = enabled; }
	
	static bool IsNoRecoil() { return m_bNoRecoil; }
	static void SetNoRecoil(bool enabled) { m_bNoRecoil = enabled; }
	
	static float GetRCSStrength() { return m_fRCSStrength; }
	static void SetRCSStrength(float strength) { m_fRCSStrength = strength; }
	
	static bool IsSilentAim() { return m_bSilentAim; }
	static void SetSilentAim(bool enabled) { m_bSilentAim = enabled; }
	
	static bool IsSpeedHack() { return m_bSpeedHack; }
	static void SetSpeedHack(bool enabled) { m_bSpeedHack = enabled; }
	
	static float GetSpeedMultiplier() { return m_fSpeedMultiplier; }
	static void SetSpeedMultiplier(float multiplier) { m_fSpeedMultiplier = multiplier; }
	
	// Получение целевых углов для silent aim (без применения к визуальным углам)
	void GetAimAngles(vec3_t& aimAngles);
	
	// Проверка, были ли углы обновлены в этом кадре
	static bool AreSilentAimAnglesUpdated() { return m_bSilentAimAnglesUpdated; }

private:
	// Поиск ближайшей цели в FOV
	cl_entity_t* FindBestTarget(vec3_t viewAngles, float fov);
	
	// Вычисление углов для наведения на цель
	void CalculateAimAngles(cl_entity_t* target, vec3_t& aimAngles);
	
	// Плавное наведение на цель
	void SmoothAim(vec3_t currentAngles, vec3_t targetAngles, vec3_t& outputAngles, float smooth);
	
	// Проверка, находится ли цель в FOV
	bool IsTargetInFOV(vec3_t viewAngles, vec3_t targetPos, float fov);
	
	// Проверка видимости цели
	bool IsTargetVisible(cl_entity_t* target);
	
	// Отрисовка FOV круга
	void DrawFOVCircle();
	
	// Проверка, нажата ли кнопка стрельбы
	bool IsShooting();
	
	// Компенсация отдачи (RCS)
	void ApplyRCS(vec3_t& viewAngles);
	
	// Получение отдачи оружия
	void GetPunchAngles(vec3_t& punchAngles);
	
	// Применение No Recoil (обнуление отдачи)
	void ApplyNoRecoil();
	
	// Проверка, что игрок действительно мертв
	bool IsPlayerDead(cl_entity_t* ent);
};

// Глобальная переменная для отслеживания состояния стрельбы
// Устанавливается из CL_CreateMove
extern bool g_AimbotIsShooting;

// Глобальная переменная для No Recoil (используется в view.cpp)
extern bool g_NoRecoilEnabled;

#endif

