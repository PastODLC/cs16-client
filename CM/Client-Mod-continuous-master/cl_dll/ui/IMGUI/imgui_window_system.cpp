#include "imgui_window_system.h"
#include "imgui.h"
#include "keydefs.h"
#include "hud.h"

void CImGuiWindowSystem::Initialize()
{
    LinkWindows();
    for (IImGuiWindow *window : m_WindowList) {
        window->Initialize();
    }
}

void CImGuiWindowSystem::VidInitialize()
{
    for (IImGuiWindow *window : m_WindowList) {
        window->VidInitialize();
    }
}

void CImGuiWindowSystem::Terminate()
{
    for (IImGuiWindow *window : m_WindowList) {
        window->Terminate();
    }
}

void CImGuiWindowSystem::NewFrame()
{
    for (IImGuiWindow *window : m_WindowList)
    {
        window->Think();
        if (window->Active()) {
            window->Draw();
        }
    }
}

void CImGuiWindowSystem::AddWindow(IImGuiWindow *window)
{
    m_WindowList.push_back(window);
}

CImGuiWindowSystem::CImGuiWindowSystem()
{
    m_WindowList.clear();
}

bool CImGuiWindowSystem::KeyInput(bool keyDown, int keyNumber, const char *bindName)
{
    // Для определенных клавиш (например, INSERT) проверяем все окна сначала
    // Это позволяет открывать/закрывать меню независимо от состояния других окон
    if (keyDown && keyNumber == K_INS)
    {
        for (IImGuiWindow *window : m_WindowList)
        {
            bool handled = window->HandleKey(keyDown, keyNumber, bindName);
            if (!handled) {
                return false; // Окно обработало клавишу (например, открыло/закрыло меню)
            }
        }
        return true; // INSERT не был обработан, передаем в движок
    }
    
    // Для остальных клавиш проверяем только активные окна
    for (IImGuiWindow *window : m_WindowList)
    {
        if (window->Active()) {
            bool handled = window->HandleKey(keyDown, keyNumber, bindName);
            if (!handled) {
                return false; // Окно обработало клавишу и не хочет передавать ее в движок
            }
        }
    }
    
    return true;
}

bool CImGuiWindowSystem::CursorRequired()
{
    for (IImGuiWindow *window : m_WindowList)
    {
        if (window->Active() && window->CursorRequired()) {
            return true;
        }
    }
    return false;
}
