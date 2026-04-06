#pragma once

#include <windows.h>
#include <WinUser.h>
#include <wrl.h>
#include <iostream>
#include <d3d.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <chrono>

#include "InputDevice.h"

namespace megaEngine {

    class DisplayWin32 {

    public:
        DisplayWin32();
        ~DisplayWin32();


        bool Initialize(const wchar_t* title, int width, int height, HINSTANCE hInstance, InputDevice* input);
        void Shutdown();


        HWND GetHwnd() const { return hWnd_; }
        HINSTANCE GetHInstance() const { return hInstance_; }
        int GetWidth() const { return width_; }
        int GetHeight() const { return height_; }

        bool ProcessMessages(InputDevice* input);
        bool IsRunning() const { return running_; }


        LRESULT WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, InputDevice* input);


    private:
        HINSTANCE hInstance_;
        HWND hWnd_;
        WNDCLASSEX wc_;
        int width_;
        int height_;
        bool running_;
        std::wstring className_;

        InputDevice* inputDevice_;

        static DisplayWin32* s_instance_;

    private:
        static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    };
}