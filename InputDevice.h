
#pragma once

#include <unordered_map>
#include <windows.h>

namespace megaEngine {
    class InputDevice
    {
    public:
        void ProcessKeyDown(unsigned int key) { m_keys[key] = true; }
        void ProcessKeyUp(unsigned int key) { m_keys[key] = false; }

        bool IsKeyPressed(unsigned int key) const {
            auto it = m_keys.find(key);
            return it != m_keys.end() && it->second;
        }

        void ClearFrameState() {
        }

        void ProcessMouseMoveClient(int x, int y, bool rightButtonDown)
        {
            if (rightButtonDown) {
                if (mouseHasLast_) {
                    mouseAccumDx_ += x - mouseLastX_;
                    mouseAccumDy_ += y - mouseLastY_;
                }
                mouseHasLast_ = true;
                mouseLastX_ = x;
                mouseLastY_ = y;
            }
            else {
                mouseHasLast_ = false;
                mouseLastX_ = x;
                mouseLastY_ = y;
            }
        }

        void ConsumeMouseDelta(int& outDx, int& outDy)
        {
            outDx = mouseAccumDx_;
            outDy = mouseAccumDy_;
            mouseAccumDx_ = 0;
            mouseAccumDy_ = 0;
        }

        void NotifyMouseCaptureLost()
        {
            mouseHasLast_ = false;
            mouseAccumDx_ = 0;
            mouseAccumDy_ = 0;
        }

    private:
        std::unordered_map<unsigned int, bool> m_keys;

        int mouseAccumDx_ = 0;
        int mouseAccumDy_ = 0;
        int mouseLastX_ = 0;
        int mouseLastY_ = 0;
        bool mouseHasLast_ = false;
    };
}