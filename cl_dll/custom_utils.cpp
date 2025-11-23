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
// custom_utils.cpp
//
// Custom utility functions implementation
//
#include "hud.h"
#include "cl_util.h"
#include "custom_utils.h"
#include "triangleapi.h"
#include "cl_entity.h"
#include "com_model.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

bool CustomUtils::CalcScreen( vec3_t origin, float *vecScreen )
{
	if ( !vecScreen )
		return false;
	
	// Use engine's WorldToScreen function
	// vec3_t is float[3], so we can pass it directly
	if ( gEngfuncs.pTriAPI->WorldToScreen( origin, vecScreen ) )
		return false; // Point is behind viewer or invalid
	
	// Convert to screen coordinates
	vecScreen[0] = XPROJECT( vecScreen[0] );
	vecScreen[1] = YPROJECT( vecScreen[1] );
	vecScreen[2] = 0.0f;
	
	return true;
}

bool CustomUtils::CheckForPlayer( cl_entity_t *ent )
{
	if ( !ent )
		return false;
	
	// Check if entity index is valid player index (1 to MAX_PLAYERS)
	if ( ent->index < 1 || ent->index > MAX_PLAYERS )
		return false;
	
	// Check if it's a player by checking the player flag
	// This is the most reliable way to check if entity is a player
	if ( ent->player )
		return true;
	
	// Additional check: if entity has a player model
	if ( ent->model && ent->model->name )
	{
		const char *modelName = ent->model->name;
		// Check for player model names
		if ( strstr( modelName, "player" ) || 
		     strstr( modelName, "vip" ) ||
		     strstr( modelName, "terror" ) ||
		     strstr( modelName, "gign" ) ||
		     strstr( modelName, "leet" ) ||
		     strstr( modelName, "gsg9" ) ||
		     strstr( modelName, "sas" ) ||
		     strstr( modelName, "urban" ) ||
		     strstr( modelName, "arctic" ) ||
		     strstr( modelName, "guerilla" ) )
		{
			return true;
		}
	}
	
	return false;
}

bool CustomUtils::ArePlayersTeammates( int localPlayerIndex, int targetPlayerIndex )
{
	// Validate indices
	if ( localPlayerIndex < 1 || localPlayerIndex > MAX_PLAYERS )
		return false;
	if ( targetPlayerIndex < 1 || targetPlayerIndex > MAX_PLAYERS )
		return false;
	
	// Get team numbers
	int localTeam = g_PlayerExtraInfo[localPlayerIndex].teamnumber;
	int targetTeam = g_PlayerExtraInfo[targetPlayerIndex].teamnumber;
	
	// Check if teams are valid (not spectator, not unassigned)
	// TEAM_UNASSIGNED = 0, TEAM_TERRORIST = 1, TEAM_CT = 2, TEAM_SPECTATOR = 3
	// Константы определены в dlls/cdll_dll.h, который включен через hud.h -> cl_dll.h
	if ( localTeam == TEAM_UNASSIGNED || localTeam == TEAM_SPECTATOR )
		return false;
	if ( targetTeam == TEAM_UNASSIGNED || targetTeam == TEAM_SPECTATOR )
		return false;
	
	// Players are teammates if they have the same team number
	return ( localTeam == targetTeam );
}

void CustomUtils::DrawBoxCornerOutline( int x, int y, int width, int height, int lineWidth, int r, int g, int b, int alpha )
{
	if ( lineWidth <= 0 )
		lineWidth = 1;
	
	int cornerLength = ( width < height ? width : height ) / 4;
	if ( cornerLength < 5 )
		cornerLength = 5;
	if ( cornerLength > 20 )
		cornerLength = 20;
	
	// Top-left corner
	FillRGBABlend( x, y, cornerLength, lineWidth, r, g, b, alpha ); // Top horizontal
	FillRGBABlend( x, y, lineWidth, cornerLength, r, g, b, alpha ); // Left vertical
	
	// Top-right corner
	FillRGBABlend( x + width - cornerLength, y, cornerLength, lineWidth, r, g, b, alpha ); // Top horizontal
	FillRGBABlend( x + width - lineWidth, y, lineWidth, cornerLength, r, g, b, alpha ); // Right vertical
	
	// Bottom-left corner
	FillRGBABlend( x, y + height - lineWidth, cornerLength, lineWidth, r, g, b, alpha ); // Bottom horizontal
	FillRGBABlend( x, y + height - cornerLength, lineWidth, cornerLength, r, g, b, alpha ); // Left vertical
	
	// Bottom-right corner
	FillRGBABlend( x + width - cornerLength, y + height - lineWidth, cornerLength, lineWidth, r, g, b, alpha ); // Bottom horizontal
	FillRGBABlend( x + width - lineWidth, y + height - cornerLength, lineWidth, cornerLength, r, g, b, alpha ); // Right vertical
}

void CustomUtils::DrawLine( int x1, int y1, int x2, int y2, int lineWidth, int r, int g, int b, int alpha )
{
	if ( lineWidth <= 0 )
		lineWidth = 1;
	
	// Bresenham's line algorithm для рисования линии
	int dx = abs(x2 - x1);
	int dy = abs(y2 - y1);
	int sx = (x1 < x2) ? 1 : -1;
	int sy = (y1 < y2) ? 1 : -1;
	int err = dx - dy;
	
	int x = x1;
	int y = y1;
	
	while (true)
	{
		// Рисуем пиксель (или маленький прямоугольник для толщины)
		if (lineWidth == 1)
		{
			FillRGBABlend(x, y, 1, 1, r, g, b, alpha);
		}
		else
		{
			// Рисуем квадрат для толщины линии
			int halfWidth = lineWidth / 2;
			FillRGBABlend(x - halfWidth, y - halfWidth, lineWidth, lineWidth, r, g, b, alpha);
		}
		
		if (x == x2 && y == y2)
			break;
		
		int e2 = 2 * err;
		if (e2 > -dy)
		{
			err -= dy;
			x += sx;
		}
		if (e2 < dx)
		{
			err += dx;
			y += sy;
		}
	}
}

