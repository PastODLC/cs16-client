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
// custom_utils.h
//
// Custom utility functions for ESP and other features
//
#pragma once

#include "cl_entity.h"

class CustomUtils
{
public:
	// Convert 3D world coordinates to 2D screen coordinates
	// Returns true if point is visible on screen, false otherwise
	static bool CalcScreen( vec3_t origin, float *vecScreen );
	
	// Check if entity is a player
	static bool CheckForPlayer( cl_entity_t *ent );
	
	// Check if two players are on the same team
	// Returns true if players are teammates, false otherwise
	// Returns false if either player is invalid, spectator, or unassigned
	static bool ArePlayersTeammates( int localPlayerIndex, int targetPlayerIndex );
	
	// Draw a corner box outline
	static void DrawBoxCornerOutline( int x, int y, int width, int height, int lineWidth, int r, int g, int b, int alpha );
	
	// Draw a line between two screen coordinates
	static void DrawLine( int x1, int y1, int x2, int y2, int lineWidth, int r, int g, int b, int alpha );
};

