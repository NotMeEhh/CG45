#include "DisplayWin32.h"

namespace megaEngine {

DisplayWin32 *DisplayWin32::s_instance_ = nullptr;

DisplayWin32::DisplayWin32() { s_instance_ = this; }
DisplayWin32::~DisplayWin32() { Shutdown(); }

bool DisplayWin32::Initialize(const wchar_t *title, int width, int height,
                              HINSTANCE hInstance, InputDevice *input) {
  hInstance_ = hInstance;
  width_ = width;
  height_ = height;
  inputDevice_ = input;
  className_ = title;
  running_ = true;

  WNDCLASSEX wc = {};
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = StaticWndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
  wc.hIconSm = wc.hIcon;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = title;
  wc.cbSize = sizeof(WNDCLASSEX);

  if (!RegisterClassEx(&wc))
    return false;

  RECT windowRect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

  auto dwStyle = WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_THICKFRAME;
  auto posX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
  auto posY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

  hWnd_ = CreateWindowEx(WS_EX_APPWINDOW, title, title, dwStyle, posX, posY,
                         windowRect.right - windowRect.left,
                         windowRect.bottom - windowRect.top, nullptr, nullptr,
                         hInstance, this);

  if (!hWnd_)
    return false;

  ShowWindow(hWnd_, SW_SHOW);
  SetForegroundWindow(hWnd_);
  SetFocus(hWnd_);
  return true;
}

void DisplayWin32::Shutdown() {
  if (hWnd_) {
    DestroyWindow(hWnd_);
    hWnd_ = nullptr;
  }
  UnregisterClass(className_.c_str(), hInstance_);
}

bool DisplayWin32::ProcessMessages(InputDevice *input) {
  MSG msg = {};
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);

    if (msg.message == WM_QUIT) {
      running_ = false;
      return false;
    }
  }
  return true;
}

LRESULT DisplayWin32::WndProc(HWND hwnd, UINT message, WPARAM wParam,
                              LPARAM lParam, InputDevice *input) {
  switch (message) {
  case WM_KEYDOWN:
    if (input)
      input->ProcessKeyDown(static_cast<unsigned int>(wParam));
    if (static_cast<unsigned int>(wParam) == VK_ESCAPE)
      PostQuitMessage(0);
    return 0;

  case WM_KEYUP:
    if (input)
      input->ProcessKeyUp(static_cast<unsigned int>(wParam));
    return 0;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK DisplayWin32::StaticWndProc(HWND hwnd, UINT message,
                                             WPARAM wParam, LPARAM lParam) {
  if (message == WM_NCCREATE) {
    CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
    s_instance_ = static_cast<DisplayWin32 *>(pCreate->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA,
                     reinterpret_cast<LONG_PTR>(s_instance_));
  } else {
    s_instance_ =
        reinterpret_cast<DisplayWin32 *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  if (s_instance_) {
    return s_instance_->WndProc(hwnd, message, wParam, lParam,
                                s_instance_->inputDevice_);
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}
} // namespace megaEngine