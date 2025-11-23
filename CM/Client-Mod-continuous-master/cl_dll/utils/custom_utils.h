#pragma once
#include "pmtrace.h"

class CustomUtils
{
    static float m_frameTime;
    static float m_lastFrameTime;
    static float m_lastSysTime;

public:
    static float GetCurrentSysTime();
    static float GetFrameTime();
    static const char* FormatTime(float totalSeconds);
    static bool CalcScreen(float *Origin, float *VecScreen);
    static bool CheckForPlayer(cl_entity_s *pEnt);
    static vec3_t GetEntityVelocityApprox(cl_entity_t *entity, int approxStep);
    static void TraceLine(vec3_t &origin, vec3_t &dir, float lineLen, pmtrace_t *traceData);
    static int TraceEntity(vec3_t origin, vec3_t dir, float distance, vec3_t &intersect);
    static const char* GetMovetypeName(int moveType);
    static const char* GetRenderModeName(int renderMode);
    static const char* GetRenderFxName(int renderFx);
    static void DrawBox(int x, int y, int w, int h, int linewidth, int r, int g, int b, int a);
    static void DrawBoxOutline(float x, float y, float w, float h, float linewidth, int r, int g, int b, int a);
    static void DrawBoxCorner(int x, int y, int w, int h, int linewidth, int r, int g, int b, int a);
    static void DrawBoxCornerOutline(int x, int y, int w, int h, int linewidth, int r, int g, int b, int a);
};
