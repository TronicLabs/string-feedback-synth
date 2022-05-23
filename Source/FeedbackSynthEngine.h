#pragma once
#ifndef IFS_FEEDBACK_SYNTH_ENGINE_H
#define IFS_FEEDBACK_SYNTH_ENGINE_H

#include <memory>
#include <daisysp.h>
#include "BiquadFilters.h"
#include "EchoDelay.h"
#include "KarplusString.h"

#ifdef __arm__
#include <dev/sdram.h>
#endif

namespace infrasonic {
namespace FeedbackSynth {

class Engine {

    public:

        Engine();
        ~Engine() {};

        void Init(const float sample_rate);

        void SetStringPitch(const float nn);

        void SetFeedbackGain(const float gain_dbfs);

        void SetFeedbackDelay(const float delay_s);
        void SetFeedbackLPFCutoff(const float cutoff_hz);
        void SetFeedbackHPFCutoff(const float cutoff_hz);

        void SetEchoDelayTime(const float echo_time);
        void SetEchoDelayFeedback(const float echo_fb);

        void Process(float &outL, float &outR);

    private:
        // long enough for 250ms at 48kHz
        static constexpr size_t kMaxFeedbackDelaySamp = 12000;
        // long enough for 5s at 48kHz
        static constexpr size_t kMaxEchoDelaySamp = 48000 * 5;

        float sample_rate_;
        float fb_gain_ = 0.0f;

        float fb_delay_smooth_coef_;
        float fb_delay_samp_ = 64.f;
        float fb_delay_samp_target_ = 64.f;

        infrasonic::String strings_[2];
        daisysp::WhiteNoise noise_;
        daisysp::DelayLine<float, kMaxFeedbackDelaySamp> fb_delayline_[2];

        LPF12 fb_lpf_;
        HPF12 fb_hpf_;

        #ifdef TARGET_DAISY
        using EchoDelayPtr = EchoDelay<kMaxEchoDelaySamp>*;
        #else
        using EchoDelayPtr = std::unique_ptr<EchoDelay<kMaxEchoDelaySamp>>;
        #endif

        EchoDelayPtr echo_delay_[2];

        Engine(const Engine &other) = delete;
        Engine(Engine &&other) = delete;
        Engine& operator=(const Engine &other) = delete;
        Engine& operator=(Engine &&other) = delete;
};

}
}

#endif
