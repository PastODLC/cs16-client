#include "imgui_window_system.h"
#include "ui_demo_window.h"
#include "ui_scoreboard.h"
#include "ui_commands.h"
#include "ui_cheat_menu.h"

void CImGuiWindowSystem::LinkWindows()
{
    static CImGuiDemoWindow demoWindow;
    AddWindow(&demoWindow);

    static CImGuiScoreboard scoreWindow;
    AddWindow(&scoreWindow);

    static CImGuiCommands commandsWindow;
    AddWindow(&commandsWindow);

    static CImGuiCheatMenu cheatMenu;
    AddWindow(&cheatMenu);
}
