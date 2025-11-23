#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "hud_cornerbox.h"
#include "custom_utils.h"
#include "cl_entity.h"
#include <math.h>

// Инициализация статической переменной
bool CHudCornerBox::m_bEnabled = false;

int CHudCornerBox::Init()
{
	m_iFlags = HUD_ACTIVE;

	gHUD.AddHudElem(this);
	return 0;
}

int CHudCornerBox::VidInit()
{
	return 1;
}

void CHudCornerBox::Reset()
{
}

int CHudCornerBox::Draw(float flTime)
{
	if (!m_bEnabled)
		return 0;

	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if (!localPlayer)
		return 0;

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

		// Пропускаем мертвых игроков
		// CheckForPlayer уже проверил модель и player, поэтому проверяем только renderfx
		if (ent->curstate.renderfx == kRenderFxDeadPlayer)
			continue;

		// Получаем позицию игрока
		vec3_t origin;
		VectorCopy(ent->origin, origin);
		
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

		// Простой цвет - белый
		int r = 255;
		int g = 255;
		int b = 255;
		int alpha = 255;
		int lineWidth = 2;

		// Рисуем corner box
		CustomUtils::DrawBoxCornerOutline(x, y, width, height, lineWidth, r, g, b, alpha);
	}

	return 0;
}


