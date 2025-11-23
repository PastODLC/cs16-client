#pragma once
#include "imgui_window.h"

class CImGuiCheatMenu : public IImGuiWindow
{
public:
    void Initialize();
    void VidInitialize();
    void Terminate();
    void Think();
    void Draw();
    bool Active();
    bool CursorRequired();
    bool HandleKey(bool keyDown, int keyNumber, const char *bindName);

private:
    static void CmdToggleCheatMenu();
    static bool m_ShowMenu;
};

