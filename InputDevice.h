
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

    private:
        std::unordered_map<unsigned int, bool> m_keys;
    };
}