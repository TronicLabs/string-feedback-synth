#include "FeedbackSynthEngine.h"
#include "DSPUtils.h"

#ifdef TARGET_DAISY
#include "memory/sdram_alloc.h"
#endif

using namespace infrasonic::FeedbackSynth;
using namespace daisysp;

void Engine::Init(const float sample_rate)
{
    using ED = EchoDelay<kMaxEchoDelaySamp>;
    #ifdef TARGET_DAISY
    echo_delay_[0] = EchoDelayPtr(SDRAM::allocate<ED>());
    echo_delay_[1] = EchoDelayPtr(SDRAM::allocate<ED>());
    #else
    echo_delay_[0] = std::make_unique<ED>();
    echo_delay_[1] = std::make_unique<ED>();
    #endif

    sample_rate_ = sample_rate;
    fb_delay_smooth_coef_ = onepole_coef(0.2f, sample_rate);

    noise_.Init();
    noise_.SetAmp(dbfs2lin(-90.0f));

    for (unsigned int i=0; i<2; i++) {

        strings_[i].Init(sample_rate);
        strings_[i].SetBrightness(0.85f);
        strings_[i].SetFreq(mtof(40.0f));
        strings_[i].SetDamping(0.5f);

        fb_delayline_[i].Init();

        echo_delay_[i]->Init(sample_rate);
        echo_delay_[i]->SetDelayTime(5.0f, true);
        echo_delay_[i]->SetFeedback(0.5f);
        echo_delay_[i]->SetLagTime(0.5f);
    }

    fb_lpf_.Init(sample_rate);
    fb_lpf_.SetQ(0.9f);
    fb_lpf_.SetCutoff(18000.0f);

    fb_hpf_.Init(sample_rate);
    fb_hpf_.SetQ(0.9f);
    fb_hpf_.SetCutoff(60.f);
}

void Engine::SetStringPitch(const float nn)
{
    const auto freq = mtof(nn);
    strings_[0].SetFreq(freq);
    strings_[1].SetFreq(freq);
}

void Engine::SetFeedbackGain(const float gain_db)
{
    fb_gain_ = dbfs2lin(gain_db);
}

void Engine::SetFeedbackDelay(const float delay_s)
{
    fb_delay_samp_target_ = DSY_CLAMP(delay_s * sample_rate_, 1.0f, static_cast<float>(kMaxFeedbackDelaySamp - 1));
}

void Engine::SetFeedbackLPFCutoff(const float cutoff_hz)
{
    fb_lpf_.SetCutoff(cutoff_hz);
}

void Engine::SetFeedbackHPFCutoff(const float cutoff_hz)
{
    fb_hpf_.SetCutoff(cutoff_hz);
}

void Engine::SetEchoDelayTime(const float echo_time)
{
    echo_delay_[0]->SetDelayTime(echo_time);
    echo_delay_[1]->SetDelayTime(echo_time);
}

void Engine::SetEchoDelayFeedback(const float echo_fb)
{
    echo_delay_[0]->SetFeedback(echo_fb);
    echo_delay_[1]->SetFeedback(echo_fb);
}

void Engine::Process(float &outL, float &outR)
{
    // --- Update audio-rate-smoothed control params ---

    fonepole(fb_delay_samp_, fb_delay_samp_target_, fb_delay_smooth_coef_);

    // --- Process Samples ---

    const float noise_samp = noise_.Process();

    // ---> Feedback Loop

    // Get noise + feedback output
    float inL = fb_delayline_[0].Read(fb_delay_samp_) + noise_samp; 
    float inR = fb_delayline_[1].Read(daisysp::fmax(1.0f, fb_delay_samp_ - 4.f)) + noise_samp;

    // Process through KS resonator
    float sampL = strings_[0].Process(inL);
    float sampR = strings_[1].Process(inR);

    // Distort + Clip
    // TODO: Oversample this? Another distortion algo maybe?
    sampL = SoftClip(sampL * 4.0f);
    sampR = SoftClip(sampR * 4.0f);

    // Filter in feedback loop
    fb_lpf_.ProcessStereo(sampL, sampR);
    fb_hpf_.ProcessStereo(sampL, sampR);

    // Write back into delay with attenuation
    fb_delayline_[0].Write(sampL * fb_gain_);
    fb_delayline_[1].Write(sampR * fb_gain_);

    // ---> Output

    sampL = sampL * 0.5f + echo_delay_[0]->Process(sampL) * 0.5f;
    sampR = sampR * 0.5f + echo_delay_[1]->Process(sampR) * 0.5f;

    outL = sampL * 0.25f;
    outR = sampR * 0.25f;

    // TODO: Allpass

}