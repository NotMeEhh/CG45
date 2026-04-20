#pragma once

namespace game {

    class BackgroundMusic {
    public:
        void Start();
        void Stop();
        ~BackgroundMusic() { Stop(); }

        BackgroundMusic() = default;
        BackgroundMusic(const BackgroundMusic&) = delete;
        BackgroundMusic& operator=(const BackgroundMusic&) = delete;

    private:
        bool playing_ = false;
    };
}
