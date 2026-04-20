#include <windows.h>
#include <objbase.h>
#include <iostream>
#include "Game.h"

#pragma comment(lib, "ole32.lib")

int main()
{
    const HRESULT coinit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comReady = SUCCEEDED(coinit) || coinit == S_FALSE;

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    game::Game game;
    if (!game.Initialize(hInstance)) {
        std::cerr << "Failed to initialize game!" << std::endl;
        if (comReady) CoUninitialize();
        return 1;
    }

    game.Run();
    game.Shutdown();
    if (comReady) CoUninitialize();
    return 0;
}