#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vocal
{
using Frame = std::array<float, 2>;
constexpr double pi = 3.14159265358979323846;

inline Frame blend(Frame a, Frame b, float mix)
{
    for (size_t c = 0; c < a.size(); ++c)
        a[c] += mix * (b[c] - a[c]);
    return a;
}

struct History
{
    void prepare(int length) { data.assign(static_cast<size_t>(length), {}); reset(); }
    void reset() { written = 0; }
    void push(Frame frame)
    {
        data[written % data.size()] = frame;
        ++written;
    }

    Frame read(int64_t index) const
    {
        if (index < 0 || uint64_t(index) >= written || written - uint64_t(index) > data.size())
            return {};
        return data[uint64_t(index) % data.size()];
    }

    Frame delay(int samples) const { return read(position() - samples); }
    Frame fractional(double samples) const
    {
        const auto whole = static_cast<int>(samples);
        return blend(delay(whole), delay(whole + 1), static_cast<float>(samples - whole));
    }

    int64_t position() const { return static_cast<int64_t>(written) - 1; }
    std::vector<Frame> data;
    uint64_t written = 0;
};

struct BitcrusherProcessor
{
    Frame process(Frame input, int bits, float release)
    {
        for (size_t channel = 0; channel < input.size(); ++channel)
        {
            auto& sample = input[channel];
            const float magnitude = std::abs(sample);
            auto& peak = peaks[channel];
            peak = std::max(magnitude, peak * release);
            float crushed;
            if (bits == 1)
            {
                // One adaptive magnitude per polarity, bounded by the incoming signal.
                crushed = magnitude >= peak * 0.20f ? peak * 0.50f : 0.0f;
            }
            else if (bits <= 4 && magnitude <= 1.0f)
            {
                // Space the few available steps closer together at vocal levels.
                const float steps = static_cast<float>((1 << (bits - 1)) - 1);
                const float code = std::round(std::sqrt(magnitude) * steps) / steps;
                const float quantized = std::min(magnitude, code * code);
                const float floorMix = 0.65f * (1.0f - std::min(1.0f, magnitude * 4.0f));
                crushed = quantized + floorMix * (magnitude - quantized);
            }
            else
            {
                const auto scale = std::ldexp(1.0f, bits - 1);
                crushed = std::round(magnitude * scale) / scale;
            }
            sample = std::copysign(std::min(magnitude, crushed), sample);
        }
        return input;
    }

    Frame peaks{};
};

struct SampleRateReducerProcessor
{
    Frame process(Frame frame, double ratio)
    {
        if (phase >= 1.0 || ratio >= 1.0)
        {
            held = frame;
            phase -= std::floor(phase);
        }
        phase += ratio;
        return held;
    }

    Frame held{};
    double phase = 1.0;
};

struct BeatRepeatProcessor
{
    void reset()
    {
        history.reset();
        running = false;
        offset = 0;
        count = 0;
    }

    Frame process(Frame input, bool trigger, int length, int fade)
    {
        if (!running || !trigger)
            history.push(input);

        if (!trigger || history.written < static_cast<uint64_t>(length))
        {
            running = false;
            return input;
        }
        if (!running || length != count)
        {
            count = length;
            start = history.position() - count + 1;
            offset = 0;
            last = input;
        }
        running = true;

        const int overlap = std::min(fade, count / 4);
        if (offset == 0)
            seam = last;
        Frame result = history.read(start + offset);
        if (offset < overlap)
            result = blend(seam, result, static_cast<float>(offset + 1) / static_cast<float>(overlap + 1));
        if (++offset == count)
            offset = 0;
        last = result;
        return result;
    }

    History history;
    bool running = false;
    int64_t start = 0;
    int offset = 0, count = 0;
    Frame last{}, seam{};
};

struct GateChopperProcessor
{
    static float target(double beat, int division, int pattern)
    {
        const double cycle = beat * std::ldexp(1.0, division) * (pattern == 2 ? 1.5 : 1.0);
        const auto step = std::floor(cycle);
        const auto phase = cycle - step;
        if (pattern == 3)
        {
            auto seed = static_cast<uint32_t>(static_cast<int64_t>(step));
            seed = (seed ^ 0x71cabfebU) * 0x9e3779b9U;
            seed ^= seed >> 16;
            return phase < (0.20 + 0.60 * (seed & 255U) / 255.0) ? 1.0f : 0.0f;
        }
        return phase < (pattern == 1 ? 0.25 : 0.50) ? 1.0f : 0.0f;
    }
};

struct PitchShifterProcessor
{
    Frame process(Frame input, double semitones, int grain)
    {
        history.push(input);
        if (std::abs(semitones) < 1.0e-5)
            return history.delay(grain / 2 + 2);
        const double speed = std::exp2(semitones / 12.0);
        phase += (1.0 - speed) / grain;
        phase -= std::floor(phase);
        const double second = phase < 0.5 ? phase + 0.5 : phase - 0.5;
        const auto weight = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * pi * phase));
        const auto shifted = blend(history.fractional(2.0 + second * grain),
                     history.fractional(2.0 + phase * grain), weight);
        return blend(history.delay(grain / 2 + 2), shifted,
                     static_cast<float>(std::min(1.0, std::abs(semitones) * 4.0)));
    }

    History history;
    double phase = 0.0;
};

struct ReverserProcessor
{
    void reset()
    {
        history.reset();
        remaining = 0;
        recording = false;
        previousTrigger = false;
        reversedLast = false;
        isContinuous = false;
    }

    Frame process(Frame input, bool continuous, bool trigger, int length, int fade)
    {
        history.push(input);
        const auto now = history.position();
        const auto dry = history.delay(length);
        if (length != currentLength || continuous != isContinuous)
        {
            recording = false;
            remaining = 0;
            currentLength = length;
            isContinuous = continuous;
        }
        if (!continuous && trigger && !previousTrigger && remaining == 0 && !recording)
        {
            start = now;
            recording = true;
        }
        previousTrigger = trigger;

        if (continuous && remaining == 0 && now >= length)
        {
            start = now - length;
            remaining = length;
        }
        if (recording && now - start >= length)
        {
            remaining = length;
            recording = false;
        }

        const bool reversing = remaining > 0;
        if (reversing && remaining == length)
            seam = reversedLast ? last : dry;

        auto result = dry;
        if (reversing)
        {
            const int offset = length - remaining;
            const int overlap = std::min(fade, length / 4);
            result = history.read(start + remaining - 1);
            if (offset < overlap)
                result = blend(seam, result, float(offset + 1) / float(overlap + 1));
            if (!continuous && remaining <= overlap)
                result = blend(dry, result, float(remaining - 1) / float(overlap));
            --remaining;
        }
        last = result;
        reversedLast = reversing;
        return result;
    }

    History history;
    Frame last{}, seam{};
    int64_t start = 0;
    int remaining = 0, currentLength = 0;
    bool recording = false, previousTrigger = false, reversedLast = false, isContinuous = false;
};

template<bool HighPass>
struct ButterworthFilter
{
    void tune(double frequency, double sampleRate)
    {
        const double k = std::tan(pi * frequency / sampleRate);
        const double square = k * k;
        const double scale = 1.0 / (1.0 + std::sqrt(2.0) * k + square);
        b0 = HighPass ? scale : square * scale;
        b1 = HighPass ? -2.0 * b0 : 2.0 * b0;
        a1 = 2.0 * (square - 1.0) * scale;
        a2 = (1.0 - std::sqrt(2.0) * k + square) * scale;
    }

    Frame process(Frame input)
    {
        for (size_t c = 0; c < input.size(); ++c)
        {
            const double output = input[c] * b0 + z1[c];
            z1[c] = input[c] * b1 - output * a1 + z2[c];
            z2[c] = input[c] * b0 - output * a2;
            input[c] = static_cast<float>(output);
        }
        return input;
    }

    std::array<double, 2> z1{}, z2{};
    double b0 = 1.0, b1 = 0.0, a1 = 0.0, a2 = 0.0;
};
using HighPassFilterProcessor = ButterworthFilter<true>;
using LowPassFilterProcessor = ButterworthFilter<false>;
}


