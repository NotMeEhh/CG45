#include <windows.h>
#include <iostream>
#include "Game.h"

int main()
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    game::Game game;
    if (!game.Initialize(hInstance)) {
        std::cerr << "Failed to initialize game!" << std::endl;
        return 1;
    }

    game.Run();
    game.Shutdown();
    return 0;
}