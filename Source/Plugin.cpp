#include <juce_audio_utils/juce_audio_utils.h>
#include "Effects.h"

namespace
{
enum Param
{
    crushOn, bits, crushMix, reduceOn, rate, reduceMix, repeatOn, repeatRate, repeatMix,
    repeatTrigger, gateOn, gateRate, depth, pattern, pitchOn, pitch, pitchMix, reverseOn,
    reverseRate, reverseMix, reverseMode, reverseTrigger, input, output, mix, hpOn,
    hp, lpOn, lp, bypass, crusherOn, crusherMix, repeatRandom, reverseRandom, chopperRandom, count
};

enum class Kind { knob, toggle, choice };
struct Definition
{
    const char* id;
    const char* name;
    Kind kind;
    float initial;
    float low = 0.0f, high = 100.0f, step = 0.1f;
    const char* unit = "";
    bool logarithmic = false;
    const char* options = "";
};

constexpr Definition definitions[] {
    {"crushOn", "Bitcrusher On", Kind::toggle, 0},
    {"bits", "Bits", Kind::knob, 8, 1, 24, 1, "bit"},
    {"crushMix", "Bitcrusher Mix", Kind::knob, 100, 0, 100, 0.1f, "%"},
    {"reduceOn", "Sample Rate Reducer On", Kind::toggle, 0},
    {"rate", "Sample Rate", Kind::knob, 12000, 500, 48000, 1, "Hz", true},
    {"reduceMix", "Sample Rate Reducer Mix", Kind::knob, 100, 0, 100, 0.1f, "%"},
    {"repeatOn", "Beat Repeat On", Kind::toggle, 0},
    {"repeatRate", "Beat Repeat Rate", Kind::choice, 2, 0, 5, 1, "", false, "1/4|1/8|1/16|1/32|1/64|1/128"},
    {"repeatMix", "Beat Repeat Mix", Kind::knob, 100, 0, 100, 0.1f, "%"},
    {"repeatTrigger", "Beat Repeat Trigger", Kind::toggle, 0},
    {"gateOn", "Gate Chopper On", Kind::toggle, 0},
    {"gateRate", "Gate Chopper Rate", Kind::choice, 2, 0, 5, 1, "", false, "1/4|1/8|1/16|1/32|1/64|1/128"},
    {"depth", "Depth", Kind::knob, 100, 0, 100, 0.1f, "%"},
    {"pattern", "Pattern", Kind::choice, 0, 0, 3, 1, "", false, "Standard|Pulse|Triplet|Random"},
    {"pitchOn", "Pitch Shifter On", Kind::toggle, 0},
    {"pitch", "Pitch", Kind::knob, 0, -24, 24, 1, "st"},
    {"pitchMix", "Pitch Shifter Mix", Kind::knob, 100, 0, 100, 0.1f, "%"},
    {"reverseOn", "Reverser On", Kind::toggle, 0},
    {"reverseRate", "Reverse Length", Kind::choice, 1, 0, 4, 1, "", false, "1/4|1/8|1/16|1/32|1/64"},
    {"reverseMix", "Reverser Mix", Kind::knob, 100, 0, 100, 0.1f, "%"},
    {"reverseMode", "Reverse Mode", Kind::choice, 0, 0, 1, 1, "", false, "Continuous|Trigger"},
    {"reverseTrigger", "Reverse Trigger", Kind::toggle, 0},
    {"input", "In", Kind::knob, 0, -24, 24, 0.1f, "dB"},
    {"output", "Out", Kind::knob, 0, -24, 24, 0.1f, "dB"},
    {"mix", "Global Mix", Kind::knob, 100, 0, 100, 0.1f, "%"},
    {"hpOn", "High-Pass On", Kind::toggle, 0},
    {"hp", "High-Pass", Kind::knob, 20, 20, 20000, 1, "Hz", true},
    {"lpOn", "Low-Pass On", Kind::toggle, 0},
    {"lp", "Low-Pass", Kind::knob, 20000, 20, 20000, 1, "Hz", true},
    {"bypass", "Bypass", Kind::toggle, 0},
    {"crusherOn", "Crusher On", Kind::toggle, 0},
    {"crusherMix", "Crusher Mix", Kind::knob, 100, 0, 100, 0.1f, "%"},
    {"repeatRandom", "Repeater Random Trigger", Kind::toggle, 0},
    {"reverseRandom", "Reverser Random Trigger", Kind::toggle, 0},
    {"chopperRandom", "Chopper Random Trigger", Kind::toggle, 0}
};
static_assert(std::size(definitions) == count);

juce::StringArray choices(const Definition& d)
{
    return juce::StringArray::fromTokens(d.options, "|", "");
}

juce::NormalisableRange<float> range(const Definition& d)
{
    if (!d.logarithmic)
        return { d.low, d.high, d.step };
    return {
        d.low, d.high,
        [](float a, float b, float normalized) { return a * std::pow(b / a, normalized); },
        [](float a, float b, float value) { return std::log(value / a) / std::log(b / a); },
        [step = d.step](float a, float b, float value)
        {
            return juce::jlimit(a, b, a + step * std::round((value - a) / step));
        }
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for (const auto& d : definitions)
    {
        const juce::ParameterID id{d.id, 1};
        if (d.kind == Kind::toggle)
            result.add(std::make_unique<juce::AudioParameterBool>(id, d.name, d.initial != 0));
        else if (d.kind == Kind::choice)
            result.add(std::make_unique<juce::AudioParameterChoice>(id, d.name, choices(d), int(d.initial)));
        else
            result.add(std::make_unique<juce::AudioParameterFloat>(id, d.name, range(d), d.initial,
                juce::AudioParameterFloatAttributes().withLabel(d.unit)
                    .withStringFromValueFunction([d](float value, int)
                    {
                        return juce::String(value, d.step >= 1.0f ? 0 : 1);
                    })));
    }
    return result;
}

struct Preset
{
    const char* name;
    std::initializer_list<std::pair<Param, float>> values;
};

const Preset presets[] {
    {"Init", {}},
    {"8-Bit Vocal", {{crushOn, 1.0f}, {bits, 8.0f}}},
    {"Digital Crunch", {{crushOn, 1.0f}, {bits, 5.0f}, {reduceOn, 1.0f}, {rate, 7000.0f}}},
    {"Machine Gun", {{repeatOn, 1.0f}, {repeatRate, 3.0f}}},
    {"Rhythm Chop", {{gateOn, 1.0f}, {gateRate, 2.0f}, {pattern, 1.0f}}},
    {"Chipmunk", {{pitchOn, 1.0f}, {pitch, 12.0f}}},
    {"Demon", {{pitchOn, 1.0f}, {pitch, -12.0f}}},
    {"Reverse Vocal", {{reverseOn, 1.0f}, {reverseRate, 1.0f}}},
    {"Broken Vocal", {{crushOn, 1.0f}, {bits, 6.0f}, {repeatOn, 1.0f}, {reverseOn, 1.0f}, {reverseMix, 65.0f}}},
    {"Hyper Glitch", {{reduceOn, 1.0f}, {rate, 9000.0f}, {gateOn, 1.0f}, {pattern, 3.0f}, {pitchOn, 1.0f}, {pitch, 7.0f}}},
    {"Telephone Vocal", {{hpOn, 1.0f}, {hp, 420.0f}, {lpOn, 1.0f}, {lp, 3400.0f}}},
    {"Underwater Vocal", {{lpOn, 1.0f}, {lp, 900.0f}}},
    {"Thin Digital Vocal", {{hpOn, 1.0f}, {hp, 750.0f}, {crushOn, 1.0f}, {bits, 7.0f}}}
};

enum Module { crusher, reducer, repeater, chopper, shifter, reverser, moduleCount };
using Route = std::array<int, moduleCount>;
constexpr int defaultRoute = 0 | (1 << 3) | (2 << 6) | (3 << 9) | (4 << 12) | (5 << 15);

Route decodeRoute(int packed)
{
    Route route{};
    unsigned seen = 0;
    for (int slot = 0; slot < moduleCount; ++slot)
    {
        const int module = (packed >> (slot * 3)) & 7;
        if (module >= moduleCount || (seen & (1u << module)) != 0)
            return {crusher, reducer, repeater, chopper, shifter, reverser};
        seen |= 1u << module;
        route[slot] = module;
    }
    return route;
}

bool randomHit(double gridStep, uint32_t salt)
{
    auto key = static_cast<uint32_t>(static_cast<int64_t>(gridStep)) ^ salt;
    key ^= key >> 16;
    key *= 0x7feb352du;
    key ^= key >> 15;
    key *= 0x846ca68bu;
    key ^= key >> 16;
    return (key & 3u) == 0;
}

class Processor final : public juce::AudioProcessor, private juce::Timer
{
public:
    Processor() : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
                  state(*this, nullptr, "VocalDMG", layout())
    {
        for (int i = 0; i < count; ++i)
            params[static_cast<size_t>(i)] = state.getRawParameterValue(definitions[i].id);
        startTimerHz(50);
    }

    ~Processor() override { stopTimer(); }
    const juce::String getName() const override { return "VOCAL DMG"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override
    {
        if (value(bypass) > 0.5f)
            return 0.0;
        if (value(reverseOn) > 0.5f)
            return 6.1;
        return value(pitchOn) > 0.5f ? 0.04 : 0.0;
    }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    int getNumPrograms() override { return int(std::size(presets)); }
    int getCurrentProgram() override { return juce::jlimit(0, getNumPrograms() - 1, program.load()); }
    void setCurrentProgram(int selected) override
    {
        if (!juce::isPositiveAndBelow(selected, getNumPrograms()))
            return;
        std::array<float, count> values{};
        for (int i = 0; i < count; ++i)
            values[static_cast<size_t>(i)] = definitions[i].initial;
        for (auto [id, value] : presets[selected].values)
            values[id] = value;
        values[crusherOn] = values[crushOn] > 0.5f || values[reduceOn] > 0.5f ? 1.0f : 0.0f;
        if (values[crusherOn] > 0.5f)
        {
            if (values[crushOn] <= 0.5f)
                values[bits] = definitions[bits].high;
            if (values[reduceOn] <= 0.5f)
                values[rate] = definitions[rate].high;
        }
        for (int i = 0; i < count; ++i)
        {
            auto* parameter = state.getParameter(definitions[i].id);
            const auto normalized = parameter->convertTo0to1(values[static_cast<size_t>(i)]);
            if (parameter->getValue() != normalized)
                parameter->setValueNotifyingHost(normalized);
        }
        program = selected;
    }

    const juce::String getProgramName(int index) override
    {
        return juce::isPositiveAndBelow(index, getNumPrograms()) ? presets[index].name : "";
    }
    void changeProgramName(int, const juce::String&) override {}

    bool isBusesLayoutSupported(const BusesLayout& buses) const override
    {
        auto channels = buses.getMainOutputChannelSet();
        return channels == buses.getMainInputChannelSet()
            && (channels == juce::AudioChannelSet::mono() || channels == juce::AudioChannelSet::stereo());
    }

    void getStateInformation(juce::MemoryBlock& dest) override
    {
        auto copy = state.copyState();
        copy.setProperty("program", program.load(), nullptr);
        copy.setProperty("route", routing.load(), nullptr);
        if (auto xml = copy.createXml())
            copyXmlToBinary(*xml, dest);
    }

    void setStateInformation(const void* data, int bytes) override { restore(data, bytes); }

    bool restore(const void* data, int bytes)
    {
        auto xml = getXmlFromBinary(data, bytes);
        if (!xml || !xml->hasTagName(state.state.getType().toString()))
            return false;
        auto tree = juce::ValueTree::fromXml(*xml);
        program = juce::jlimit(-1, getNumPrograms() - 1, int(tree.getProperty("program", -1)));
        setRoute(decodeRoute(int(tree.getProperty("route", defaultRoute))));
        const auto parameterNode = [&tree](Param id)
        {
            return tree.getChildWithProperty("id", definitions[id].id);
        };
        if (!parameterNode(crusherOn).isValid())
        {
            juce::ValueTree master("PARAM");
            master.setProperty("id", definitions[crusherOn].id, nullptr);
            const bool wasCrushing = float(parameterNode(crushOn).getProperty("value", 0)) > 0.5f
                                  || float(parameterNode(reduceOn).getProperty("value", 0)) > 0.5f;
            master.setProperty("value", wasCrushing ? 1.0f : 0.0f, nullptr);
            tree.addChild(master, -1, nullptr);
            juce::ValueTree masterMix("PARAM");
            masterMix.setProperty("id", definitions[crusherMix].id, nullptr);
            masterMix.setProperty("value", 100.0f, nullptr);
            tree.addChild(masterMix, -1, nullptr);
            for (const auto& pair : {std::pair{hpOn, hp}, std::pair{lpOn, lp}})
                if (float(parameterNode(pair.first).getProperty("value", 0)) <= 0.5f)
                    if (auto cutoff = parameterNode(pair.second); cutoff.isValid())
                        cutoff.setProperty("value", definitions[pair.second].initial, nullptr);
        }
        if (!parameterNode(chopperRandom).isValid())
        {
            const bool crushing = float(parameterNode(crushOn).getProperty("value", 0)) > 0.5f;
            const bool reducing = float(parameterNode(reduceOn).getProperty("value", 0)) > 0.5f;
            if (auto stageBits = parameterNode(bits); stageBits.isValid())
            {
                if (!crushing)
                    stageBits.setProperty("value", reducing ? definitions[bits].high : definitions[bits].initial, nullptr);
            }
            if (auto stageRate = parameterNode(rate); stageRate.isValid() && crushing && !reducing)
                stageRate.setProperty("value", definitions[rate].high, nullptr);
        }
        for (const auto id : {repeatRandom, reverseRandom, chopperRandom})
            if (!parameterNode(id).isValid())
            {
                juce::ValueTree node("PARAM");
                node.setProperty("id", definitions[id].id, nullptr);
                node.setProperty("value", 0.0f, nullptr);
                tree.addChild(node, -1, nullptr);
            }
        state.replaceState(tree);
        return true;
    }

    void prepareToPlay(double sampleRate, int) override
    {
        sr = sampleRate;
        grain = juce::jmax(16, int(sr * 0.036));
        fade = juce::jmax(4, int(sr * 0.003));
        maxFragment = int(std::ceil(sr * 6.0)) + 2;
        original.prepare(maxFragment + grain + 8);
        repeatEffect.history.prepare(maxFragment + 4);
        reverseEffect.history.prepare(maxFragment * 2 + 4);
        pitchEffect.history.prepare(grain + 8);
        for (auto& smoother : smooth)
            smoother.reset(sr, 0.008);
        triggerRamp.reset(sr, 0.004);
        reset();
        const auto latency = configuredLatency(120.0);
        requestedLatency.store(latency);
        setLatencySamples(latency);
    }

    void releaseResources() override {}
    void reset() override
    {
        original.reset();
        repeatEffect.reset();
        reverseEffect.reset();
        pitchEffect.history.reset();
        pitchEffect.phase = 0.0;
        crushEffect = {};
        reduceEffect = {};
        previousRoute = getRoute();
        hpFilter = {};
        lpFilter = {};
        freeBeat = 0.0;
        gateEnvelope = 1.0f;
        expectedTimeline.reset();
        wasPlaying = false;
        transitionLeft = 0;
        previousLatency = grain > 0 ? configuredLatency(120.0) : 0;
        lastOutput = {};
        hadPitch = hadReverse = hadRepeat = false;
        for (int i = 0; i < count; ++i)
            smooth[static_cast<size_t>(i)].setCurrentAndTargetValue(value(static_cast<Param>(i)));
        triggerRamp.setCurrentAndTargetValue(0.0f);
        smooth[hpOn].setCurrentAndTargetValue(value(hp) != definitions[hp].initial ? 1.0f : 0.0f);
        smooth[lpOn].setCurrentAndTargetValue(value(lp) != definitions[lp].initial ? 1.0f : 0.0f);
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { render(buffer, false); }
    void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { render(buffer, true); }

    Route getRoute() const { return decodeRoute(routing.load(std::memory_order_relaxed)); }

    void setRoute(const Route& route)
    {
        // Earlier projects could place the two crusher stages separately.
        int packed = 0, slot = 0;
        bool insertedCrusher = false;
        for (const int module : route)
        {
            if (module == crusher || module == reducer)
            {
                if (!insertedCrusher)
                {
                    packed |= crusher << (slot++ * 3);
                    packed |= reducer << (slot++ * 3);
                    insertedCrusher = true;
                }
            }
            else
            {
                packed |= module << (slot++ * 3);
            }
        }
        routing.store(packed, std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState state;
    std::atomic<int> program{0};

private:
    float value(Param p) const { return params[p]->load(std::memory_order_relaxed); }
    int divisionLength(double bpm, int division) const
    {
        return juce::jlimit(8, maxFragment,
                           int(std::round(sr * 60.0 / (bpm * std::ldexp(1.0, division)))));
    }

    int configuredLatency(double bpm) const
    {
        return (value(pitchOn) > 0.5f ? grain / 2 + 2 : 0)
             + (value(reverseOn) > 0.5f ? divisionLength(bpm, int(value(reverseRate))) : 0);
    }

    void timerCallback() override
    {
        const int latency = requestedLatency.load(std::memory_order_relaxed);
        if (latency != getLatencySamples())
            setLatencySamples(latency);
    }

    void render(juce::AudioBuffer<float>& buffer, bool hostBypass);

    std::atomic<int> routing{defaultRoute};
    Route previousRoute = decodeRoute(defaultRoute);
    double sr = 48000.0, freeBeat = 0.0;
    int grain = 0, fade = 0, maxFragment = 0;
    std::atomic<int> requestedLatency{0};
    std::optional<int64_t> expectedTimeline;
    bool wasPlaying = false;
    std::array<std::atomic<float>*, count> params{};
    std::array<juce::SmoothedValue<float>, count> smooth;
    juce::SmoothedValue<float> triggerRamp;
    float gateEnvelope = 1.0f;
    int previousLatency = 0, transitionLeft = 0;
    vocal::Frame lastOutput{}, transitionFrom{};
    bool hadPitch = false, hadReverse = false, hadRepeat = false;
    vocal::History original;
    vocal::BitcrusherProcessor crushEffect;
    vocal::SampleRateReducerProcessor reduceEffect;
    vocal::BeatRepeatProcessor repeatEffect;
    vocal::PitchShifterProcessor pitchEffect;
    vocal::ReverserProcessor reverseEffect;
    vocal::HighPassFilterProcessor hpFilter;
    vocal::LowPassFilterProcessor lpFilter;
};

void Processor::render(juce::AudioBuffer<float>& buffer, bool hostBypass)
{
    const juce::ScopedNoDenormals noDenormals;
    const int samples = buffer.getNumSamples();
    const int channels = juce::jmin(2, buffer.getNumChannels());
    if (channels == 0 || samples == 0 || grain == 0)
        return;

    double bpm = 120.0;
    double beat = freeBeat;
    if (auto* playhead = getPlayHead())
    {
        if (auto position = playhead->getPosition())
        {
            const auto hostBpm = position->getBpm();
            if (hostBpm && std::isfinite(*hostBpm) && *hostBpm > 0.0)
                bpm = juce::jlimit(10.0, 999.0, *hostBpm);
            const bool playing = position->getIsPlaying();
            const auto timeline = position->getTimeInSamples();
            if ((playing != wasPlaying) || (playing && timeline && expectedTimeline && *timeline != *expectedTimeline))
            {
                repeatEffect.reset();
                reverseEffect.reset();
                triggerRamp.setCurrentAndTargetValue(0.0f);
            }
            if (playing && timeline)
                expectedTimeline = *timeline + samples;
            else
                expectedTimeline.reset();
            wasPlaying = playing;
            if (auto ppq = position->getPpqPosition(); playing && ppq && std::isfinite(*ppq))
                beat = *ppq;
        }
    }

    std::array<float, count> setting{};
    for (int i = 0; i < count; ++i)
    {
        const auto p = static_cast<Param>(i);
        setting[p] = value(p);
        if (p != hpOn && p != lpOn)
            smooth[p].setTargetValue(p == bypass && hostBypass ? 1.0f : setting[p]);
    }
    for (const auto& pair : {std::pair{hpOn, hp}, std::pair{lpOn, lp}})
        smooth[pair.first].setTargetValue(setting[pair.second] != definitions[pair.second].initial ? 1.0f : 0.0f);
    const int repeatLength = divisionLength(bpm, int(setting[repeatRate]));
    const int reverseLength = divisionLength(bpm, int(setting[reverseRate]));
    const int pitchLatency = setting[pitchOn] > 0.5f ? grain / 2 + 2 : 0;
    const int latency = pitchLatency + (setting[reverseOn] > 0.5f ? reverseLength : 0);
    requestedLatency.store(latency, std::memory_order_relaxed);
    const bool hasPitch = pitchLatency > 0;
    const bool hasReverse = setting[reverseOn] > 0.5f;
    const bool hasRepeat = setting[repeatOn] > 0.5f;
    if (hasPitch && !hadPitch)
    {
        pitchEffect.history.reset();
        pitchEffect.phase = 0.0;
    }
    if (hasReverse && !hadReverse)
        reverseEffect.reset();
    if (hasRepeat && !hadRepeat)
        repeatEffect.reset();
    hadPitch = hasPitch;
    hadReverse = hasReverse;
    hadRepeat = hasRepeat;
    const auto route = getRoute();
    if (latency != previousLatency || route != previousRoute)
    {
        transitionFrom = lastOutput;
        transitionLeft = fade;
        previousLatency = latency;
        previousRoute = route;
    }
    const bool manualRepeat = hasRepeat && setting[repeatTrigger] > 0.5f;
    const bool randomRepeat = hasRepeat && setting[repeatRandom] > 0.5f;
    const bool randomReverse = hasReverse && setting[reverseRandom] > 0.5f;
    const auto repeatGrid = std::ldexp(1.0, int(setting[repeatRate]));
    const int crushBits = int(std::lround(setting[bits]));
    const bool randomChopper = setting[chopperRandom] > 0.5f;
    const int chopperPattern = int(setting[pattern]);
    const int chopperDivision = int(setting[gateRate]);
    const auto chopperGrid = std::ldexp(1.0, chopperDivision) * (chopperPattern == 2 ? 1.5 : 1.0);
    const auto reverseGrid = std::ldexp(1.0, int(setting[reverseRate]));
    const bool reverseContinuous = setting[reverseMode] < 0.5f && !randomReverse;
    const auto crushRelease = static_cast<float>(std::exp(-1.0 / (sr * 0.035)));
    const auto beatStep = bpm / (60.0 * sr);
    const auto gateSlew = static_cast<float>(1.0 - std::exp(-1.0 / (sr * 0.00055)));

    auto* left = buffer.getWritePointer(0);
    auto* right = channels == 2 ? buffer.getWritePointer(1) : nullptr;
    for (int i = 0; i < samples; ++i, beat += beatStep)
    {
        const auto safe = [](float sample) { return std::isfinite(sample) ? sample : 0.0f; };
        const vocal::Frame source{safe(left[i]), right ? safe(right[i]) : 0.0f};
        original.push(source);
        const auto dry = original.delay(latency);
        auto wet = source;
        const auto gainIn = juce::Decibels::decibelsToGain(smooth[input].getNextValue());
        const auto gainOut = juce::Decibels::decibelsToGain(smooth[output].getNextValue());
        const auto globalMix = smooth[mix].getNextValue() * 0.01f;
        const auto globalBypass = smooth[bypass].getNextValue();
        const auto crushAmount = smooth[crushMix].getNextValue() * 0.01f;
        const auto reduceAmount = smooth[reduceMix].getNextValue() * 0.01f;
        const auto crusherAmount = smooth[crusherOn].getNextValue() * smooth[crusherMix].getNextValue() * 0.01f;
        const auto reduceRate = smooth[rate].getNextValue();
        const bool repeating = manualRepeat
            || (randomRepeat && randomHit(std::floor(beat * repeatGrid), 0x3145ca07u));
        triggerRamp.setTargetValue(repeating ? 1.0f : 0.0f);
        const auto repeatAmount = triggerRamp.getNextValue() * smooth[repeatMix].getNextValue() * 0.01f;
        const auto gateAmount = smooth[gateOn].getNextValue() * smooth[depth].getNextValue() * 0.01f;
        const auto pitchAmount = smooth[pitchOn].getNextValue() * smooth[pitchMix].getNextValue() * 0.01f;
        const auto shift = smooth[pitch].getNextValue();
        const auto reverseAmount = smooth[reverseOn].getNextValue() * smooth[reverseMix].getNextValue() * 0.01f;
        const auto hpAmount = smooth[hpOn].getNextValue();
        const auto lpAmount = smooth[lpOn].getNextValue();
        auto hpCutoff = juce::jmin(double(smooth[hp].getNextValue()), sr * 0.49);
        const auto lpCutoff = juce::jmin(double(smooth[lp].getNextValue()), sr * 0.49);
        if (hpAmount > 0.0f && lpAmount > 0.0f)
            hpCutoff = juce::jmin(hpCutoff, lpCutoff);

        if (globalBypass < 1.0f || hasPitch || hasReverse || hasRepeat)
        {
            for (auto& sample : wet)
                sample *= gainIn;

            for (const auto module : route)
            {
                switch (module)
                {
                    case crusher:
                        if (crusherAmount > 0.0f)
                        {
                            auto processed = wet;
                            if (crushAmount > 0.0f && crushBits < int(definitions[bits].high))
                                processed = vocal::blend(processed, crushEffect.process(processed,
                                    crushBits, crushRelease), crushAmount);
                            if (reduceAmount > 0.0f && reduceRate < definitions[rate].high - 0.5f)
                                processed = vocal::blend(processed, reduceEffect.process(processed,
                                    juce::jmin(1.0, reduceRate / sr)), reduceAmount);
                            wet = vocal::blend(wet, processed, crusherAmount);
                        }
                        break;
                    case reducer:
                        break;
                    case repeater:
                        if (setting[repeatOn] > 0.5f || repeatAmount > 0.0f)
                        {
                            const bool hold = repeating || triggerRamp.isSmoothing();
                            wet = vocal::blend(wet, repeatEffect.process(wet, hold, repeatLength, fade), repeatAmount);
                        }
                        break;
                    case chopper:
                        if (gateAmount > 0.0f)
                        {
                            const bool chop = !randomChopper || randomHit(std::floor(beat * chopperGrid), 0x88be420du);
                            const auto target = chop ? vocal::GateChopperProcessor::target(beat, chopperDivision, chopperPattern) : 1.0f;
                            gateEnvelope += gateSlew * (target - gateEnvelope);
                            for (auto& sample : wet)
                                sample *= 1.0f - gateAmount * (1.0f - gateEnvelope);
                        }
                        break;
                    case shifter:
                        if (pitchLatency > 0)
                        {
                            auto shifted = pitchEffect.process(wet, shift, grain);
                            wet = vocal::blend(pitchEffect.history.delay(pitchLatency), shifted, pitchAmount);
                        }
                        break;
                    case reverser:
                        if (setting[reverseOn] > 0.5f)
                        {
                            const double reverseStep = std::floor(beat * reverseGrid);
                            const bool gridTrigger = randomReverse
                                && reverseStep != std::floor((beat - beatStep) * reverseGrid)
                                && randomHit(reverseStep, 0xe9d253b1u);
                            const bool trigger = setting[reverseTrigger] > 0.5f || gridTrigger;
                            const auto reversed = reverseEffect.process(wet, reverseContinuous, trigger, reverseLength, fade);
                            wet = vocal::blend(reverseEffect.history.delay(reverseLength), reversed, reverseAmount);
                        }
                        break;
                    default: break;
                }
            }
            if (hpAmount > 0.0f)
            {
                if ((i & 15) == 0)
                    hpFilter.tune(hpCutoff, sr);
                wet = vocal::blend(wet, hpFilter.process(wet), hpAmount);
            }
            if (lpAmount > 0.0f)
            {
                if ((i & 15) == 0)
                    lpFilter.tune(lpCutoff, sr);
                wet = vocal::blend(wet, lpFilter.process(wet), lpAmount);
            }
            wet = vocal::blend(dry, wet, globalMix);
            for (auto& sample : wet)
                sample *= gainOut;
        }
        else
        {
            wet = dry;
        }
        auto result = vocal::blend(wet, dry, globalBypass);
        if (transitionLeft > 0)
        {
            result = vocal::blend(transitionFrom, result, 1.0f - float(transitionLeft) / float(fade));
            --transitionLeft;
        }
        lastOutput = result;
        left[i] = safe(result[0]);
        if (right)
            right[i] = safe(result[1]);
    }
    freeBeat = beat;
}


class DmgSlider final : public juce::Slider
{
    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
            showTextBox();
        else
            Slider::mouseDown(event);
    }
};

class RackStyle final : public juce::LookAndFeel_V4
{
public:
    static void bevel(juce::Graphics& g, juce::Rectangle<int> r, juce::Colour face, bool recessed = false)
    {
        g.setColour(face);
        g.fillRect(r);
        g.setColour(recessed ? juce::Colour(0xff404040) : juce::Colours::white);
        g.fillRect(r.getX(), r.getY(), r.getWidth(), 1);
        g.fillRect(r.getX(), r.getY(), 1, r.getHeight());
        g.setColour(recessed ? juce::Colours::white : juce::Colour(0xff404040));
        g.fillRect(r.getX(), r.getBottom() - 1, r.getWidth(), 1);
        g.fillRect(r.getRight() - 1, r.getY(), 1, r.getHeight());
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float position,
                          float start, float end, juce::Slider&) override
    {
        const float cx = float(x) + float(w) * 0.5f, cy = float(y) + float(h) * 0.5f;
        const float radius = std::min(float(w), float(h)) * 0.46f;
        if (radius <= 1.0f)
            return;
        juce::Path arc;
        arc.addCentredArc(cx, cy, radius, radius, 0, start, end, true);
        g.setColour(juce::Colour(0xff777777));
        g.strokePath(arc, juce::PathStrokeType(1.0f));
        juce::Path star;
        // Trace the supplied silhouette, then rotate the long tip around its hub.
        star.startNewSubPath(77, 91);
        for (const juce::Point<float> point : {juce::Point<float>{111, 91}, {127, 56}, {133, 91},
                                               {252, 91}, {137, 113}, {143, 148},
                                               {118, 126}, {86, 148}, {102, 112}})
            star.lineTo(point);
        star.closeSubPath();
        const float zero = std::atan2(-13.0f, 133.0f);
        const float angle = start + position * (end - start) - juce::MathConstants<float>::halfPi - zero;
        const auto transform = juce::AffineTransform::translation(-119, -104)
            .scaled(radius / 134.0f).rotated(angle).translated(cx, cy);
        g.setColour(juce::Colour(0xff101010));
        g.fillPath(star, transform);
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox(slider);
        label->setColour(juce::Label::textColourId, juce::Colours::black);
        label->setColour(juce::TextEditor::textColourId, juce::Colours::black);
        label->setBorderSize(juce::BorderSize<int>(1));
        return label;
    }

    void drawComboBox(juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box) override
    {
        bevel(g, {0, 0, w, h}, juce::Colour(0xffc0c0c0), down);
        juce::Path arrow;
        const float cy = float(h) * 0.5f;
        arrow.addTriangle(6.0f, cy - 2.0f, 12.0f, cy - 2.0f, 9.0f, cy + 2.0f);
        g.setColour(juce::Colours::black.withAlpha(box.isEnabled() ? 1.0f : 0.5f));
        g.fillPath(arrow);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(16, 1, box.getWidth() - 18, box.getHeight() - 2);
        label.setFont(juce::Font(juce::FontOptions(11.0f)));
        label.setBorderSize(juce::BorderSize<int>(0));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                              bool highlighted, bool down) override
    {
        const bool pressed = down || button.getToggleState();
        bevel(g, button.getLocalBounds(), juce::Colour(highlighted ? 0xffdddddd : 0xffc0c0c0), pressed);
    }
};

class IconButton final : public juce::TextButton
{
public:
    enum Icon { power, previous, next };
    IconButton(Icon initialIcon, bool inverted = false) : icon(initialIcon), reverseSense(inverted) {}
    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        juce::TextButton::paintButton(g, hovered, down);
        const auto centre = getLocalBounds().toFloat().getCentre();
        if (icon == power)
        {
            const bool active = getToggleState() != reverseSense;
            g.setColour(juce::Colour(active ? 0xff003e85 : reverseSense ? 0xffbd1818 : 0xff505050));
            juce::Path path;
            path.addCentredArc(centre.x, centre.y + 0.7f, 4.8f, 4.8f, 0.0f, 0.62f, 5.66f, true);
            g.strokePath(path, juce::PathStrokeType(1.45f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.drawLine(centre.x, centre.y - 6.1f, centre.x, centre.y - 0.7f, 1.55f);
        }
        else
        {
            const float direction = icon == previous ? -1.0f : 1.0f;
            juce::Path arrow;
            arrow.addTriangle(centre.x + direction * 4.5f, centre.y,
                              centre.x - direction * 3.5f, centre.y - 4.5f,
                              centre.x - direction * 3.5f, centre.y + 4.5f);
            g.setColour(juce::Colours::black);
            g.fillPath(arrow);
        }
    }
private:
    Icon icon;
    bool reverseSense;
};

class BypassShade final : public juce::Component
{
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xffa6a6a6).withAlpha(0.66f));
    }
};

class Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
    using State = juce::AudioProcessorValueTreeState;
    using Color = juce::Colour;
    struct Control
    {
        std::unique_ptr<juce::Component> component;
        std::unique_ptr<State::SliderAttachment> slider;
        std::unique_ptr<State::ButtonAttachment> button;
        std::unique_ptr<State::ComboBoxAttachment> combo;
    };
    static constexpr int width = 1006, height = 334, visibleCount = 5;
    static constexpr Param enables[] {crusherOn, reduceOn, repeatOn, gateOn, pitchOn, reverseOn};
    static constexpr const char* titles[] {"CRUSHER", "", "REPEATER", "CHOPPER", "SHIFTER", "REVERSER"};

public:
    explicit Editor(Processor& owner) : AudioProcessorEditor(owner), processor(owner)
    {
        style.setColour(juce::Slider::thumbColourId, Color(0xff000080));
        style.setColour(juce::Slider::trackColourId, Color(0xff000080));
        style.setColour(juce::Slider::backgroundColourId, Color(0xff8a8a8a));
        style.setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
        style.setColour(juce::Slider::textBoxBackgroundColourId, Color(0xffeeeeee));
        style.setColour(juce::Slider::textBoxOutlineColourId, Color(0xff858585));
        style.setColour(juce::ComboBox::backgroundColourId, Color(0xffeeeeee));
        style.setColour(juce::ComboBox::outlineColourId, Color(0xff707070));
        style.setColour(juce::ComboBox::textColourId, juce::Colours::black);
        style.setColour(juce::ComboBox::arrowColourId, juce::Colours::black);
        style.setColour(juce::PopupMenu::backgroundColourId, Color(0xffeeeeee));
        style.setColour(juce::PopupMenu::textColourId, juce::Colours::black);
        style.setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        style.setColour(juce::PopupMenu::highlightedBackgroundColourId, Color(0xff000080));
        style.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        style.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        setLookAndFeel(&style);
        canvas.setInterceptsMouseClicks(false, true);
        addAndMakeVisible(canvas);
        for (int i = 0; i < count; ++i)
            if ((i > reduceMix || i == bits || i == rate) && i != hpOn && i != lpOn)
                createControl(static_cast<Param>(i));
        presetBox.setName("Presets");
        presetBox.setTooltip("Open Factory and User preset folders");
        presetBox.onClick = [this] { showPresetMenu(); };
        previousPreset.setName("Previous preset");
        nextPreset.setName("Next preset");
        previousPreset.setTooltip("Previous factory preset");
        nextPreset.setTooltip("Next factory preset");
        previousPreset.onClick = [this] { stepPreset(-1); };
        nextPreset.onClick = [this] { stepPreset(1); };
        for (auto* item : {static_cast<juce::Component*>(&presetBox),
                           static_cast<juce::Component*>(&previousPreset),
                           static_cast<juce::Component*>(&nextPreset)})
            canvas.addAndMakeVisible(item);
        canvas.addChildComponent(bypassShade);
        auto& bypassButton = static_cast<juce::TextButton&>(*controls[bypass].component);
        bypassButton.toFront(false);
        bypassButton.onClick = [this] { bypassShade.setVisible(on(bypass)); };
        displayedRoute = processor.getRoute();
        canvas.setBounds(0, 0, width, height);
        setResizable(true, true);
        setResizeLimits(750, 249, 1811, 602);
        getConstrainer()->setFixedAspectRatio(double(width) / height);
        setSize(width, height);
        timerCallback();
        startTimerHz(15);
    }

    ~Editor() override
    {
        stopTimer();
        chooser.reset();
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Color(0xffc0c0c0));
        g.addTransform(juce::AffineTransform::scale(editorScale()));
        RackStyle::bevel(g, {0, 0, width, height}, Color(0xffc0c0c0));
        g.setColour(Color(0xff000080));
        g.setFont(juce::Font(juce::FontOptions(41.0f, juce::Font::bold | juce::Font::italic)));
        g.drawText("VOCAL DMG", juce::Rectangle<int>{18, 13, 360, 49}, juce::Justification::centredLeft);

        const char* leftLabels[] {"Bits", "Rate", "Rate", "Rate", "Pitch", "Length"};
        const auto route = visibleRoute();
        for (int slot = 0; slot < visibleCount; ++slot)
        {
            const int module = route[slot];
            const auto box = panel(slot);
            const int x = box.getX(), y = box.getY();
            RackStyle::bevel(g, box, Color(0xffe6e6e6), true);
            RackStyle::bevel(g, box.withHeight(29).reduced(1, 1),
                             Color(slot == dropSlot || slot == dragSlot ? 0xffaab7dc : 0xffc0c0c0));
            label(g, juce::String(slot + 1) + ".  " + titles[module], {x + 9, y + 3, 130, 22}, 13.0f, true, false);
            if (module == crusher)
            {
                label(g, "Bits", {x + 5, y + 49, 54, 16}, 12.0f, false, true);
                label(g, "Sample Rate", {x + 57, y + 49, 75, 16}, 11.0f, false, true);
                label(g, "Mix", {x + 137, y + 49, 48, 16}, 12.0f, false, true);
            }
            else
            {
                label(g, leftLabels[module], {x + 15, y + 49, 74, 16}, 12.0f, false, true);
                label(g, module == chopper ? "Depth" : "Mix", {x + 114, y + 49, 49, 16}, 12.0f, false, true);
            }
        }

        RackStyle::bevel(g, {16, 250, width - 32, 72}, Color(0xffc0c0c0), true);
        label(g, "In", {23, 255, 78, 13}, 12.0f, true, true);
        label(g, "Out", {105, 255, 78, 13}, 12.0f, true, true);
        label(g, "Mix", {925, 255, 51, 13}, 12.0f, true, true);
        label(g, "HP", {206, 256, 31, 24}, 12.0f, true, false);
        label(g, "LP", {206, 286, 31, 24}, 12.0f, true, false);
    }

    void resized() override
    {
        canvas.setTransform(juce::AffineTransform::scale(editorScale()));
        previousPreset.setBounds(667, 27, 29, 26);
        presetBox.setBounds(699, 27, 193, 26);
        nextPreset.setBounds(895, 27, 29, 26);
        place(bypass, {936, 27, 48, 26});
        bypassShade.setBounds(canvas.getLocalBounds());
        const Param left[] {bits, rate, repeatRate, gateRate, pitch, reverseRate};
        const Param right[] {crushMix, reduceMix, repeatMix, depth, pitchMix, reverseMix};
        const auto route = visibleRoute();
        for (int slot = 0; slot < visibleCount; ++slot)
        {
            const auto module = route[slot];
            const int x = panel(slot).getX(), y = panel(slot).getY();
            place(enables[module], {x + 140, y + 4, 40, 20});
            if (module == crusher)
            {

                place(bits, {x + 5, y + 65, 54, 60});
                place(rate, {x + 65, y + 65, 60, 60});
                place(crusherMix, {x + 137, y + 65, 48, 60});
                continue;
            }

            place(left[module], {x + 15, y + 65, 74, 60});
            place(right[module], {x + 114, y + 65, 49, 60});
            if (module == repeater)
            {
                place(repeatTrigger, {x + 25, y + 129, 65, 22});
                place(repeatRandom, {x + 98, y + 129, 65, 22});
            }
            if (module == chopper)
            {
                place(pattern, {x + 17, y + 129, 94, 22});
                place(chopperRandom, {x + 120, y + 129, 54, 22});
            }
            if (module == reverser)
            {
                place(reverseMode, {x + 5, y + 129, 77, 22});
                place(reverseTrigger, {x + 84, y + 129, 46, 22});
                place(reverseRandom, {x + 132, y + 129, 52, 22});
            }
        }
        place(input, {23, 269, 78, 47});
        place(output, {105, 269, 78, 47});
        place(mix, {925, 269, 51, 47});
        place(hp, {236, 256, 647, 24});
        place(lp, {236, 286, 647, 24});
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        setMouseCursor(headerSlot(designPoint(event)) >= 0 ? juce::MouseCursor::DraggingHandCursor
                                                           : juce::MouseCursor::NormalCursor);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isLeftButtonDown())
            dragSlot = headerSlot(designPoint(event));
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (dragSlot < 0)
            return;
        const auto position = designPoint(event);
        dropSlot = position.y >= 78 && position.y < 245 && position.x >= 16 && position.x < width - 16
            ? juce::jlimit(0, visibleCount - 1, (position.x - 16) / 196) : -1;
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragSlot >= 0 && dropSlot >= 0 && dragSlot != dropSlot)
        {
            auto route = visibleRoute();
            if (dragSlot < dropSlot)
                std::rotate(route.begin() + dragSlot, route.begin() + dragSlot + 1, route.begin() + dropSlot + 1);
            else
                std::rotate(route.begin() + dropSlot, route.begin() + dragSlot, route.begin() + dragSlot + 1);
            Route expanded{};
            auto routeOutput = expanded.begin();
            for (const auto module : route)
            {
                *routeOutput++ = module;
                if (module == crusher)
                    *routeOutput++ = reducer;
            }
            processor.setRoute(expanded);
            displayedRoute = processor.getRoute();
            resized();
        }
        dragSlot = dropSlot = -1;
        repaint();
    }

private:
    float editorScale() const
    {
        return std::min(float(getWidth()) / float(width), float(getHeight()) / float(height));
    }

    juce::Point<int> designPoint(const juce::MouseEvent& event) const
    {
        return (event.position / editorScale()).toInt();
    }

    std::array<int, visibleCount> visibleRoute() const
    {
        std::array<int, visibleCount> result{};
        std::copy_if(displayedRoute.begin(), displayedRoute.end(), result.begin(), [](int module) { return module != reducer; });
        return result;
    }

    void stepPreset(int direction)
    {
        const int total = processor.getNumPrograms();
        const int current = processor.program.load();
        const int next = current < 0 ? (direction > 0 ? 0 : total - 1) : (current + direction + total) % total;
        processor.setCurrentProgram(next);
        updatePresetLabel();
    }

    static juce::Rectangle<int> panel(int slot) { return {16 + slot * 196, 84, 190, 159}; }

    static int headerSlot(juce::Point<int> position)
    {
        for (int slot = 0; slot < visibleCount; ++slot)
            if (panel(slot).withHeight(29).contains(position))
                return slot;
        return -1;
    }

    void place(Param id, juce::Rectangle<int> bounds) { controls[id].component->setBounds(bounds); }
    static void label(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                      float size, bool bold, bool centered, float alpha = 1.0f)
    {
        g.setColour(juce::Colours::black.withAlpha(alpha));
        g.setFont(juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)));
        g.drawFittedText(text, area, centered ? juce::Justification::centred : juce::Justification::centredLeft, 1);
    }

    void createControl(Param id)
    {
        auto& control = controls[id];
        const auto& d = definitions[id];
        if (d.kind == Kind::toggle)
        {
            const bool trigger = id == repeatTrigger || id == reverseTrigger;
            const bool power = id == bypass;
            std::unique_ptr<juce::TextButton> button;
            if (power)
                button = std::make_unique<IconButton>(IconButton::power, id == bypass);
            else
                button = std::make_unique<juce::TextButton>(trigger ? "Trigger" : (id == repeatRandom || id == reverseRandom || id == chopperRandom) ? "Random" : "Off");
            button->setClickingTogglesState(true);
            if (id == reverseTrigger)
            {
                button->setClickingTogglesState(false);
                button->onStateChange = [raw = button.get()]
                {
                    raw->setToggleState(raw->isDown(), juce::sendNotification);
                };
            }
            button->setTooltip(id == repeatTrigger ? "Latch the most recent fragment. Click again to release."
                : id == reverseTrigger ? "Capture and reverse the next fragment in Trigger mode." : id == repeatRandom ? "Randomly repeats fragments on the selected note grid."
                : id == chopperRandom ? "Randomly chops selected steps. Depth controls the amount."
                : id == reverseRandom ? "Randomly triggers one-shot reverses on the selected note grid."
                : d.name);
            control.button = std::make_unique<State::ButtonAttachment>(processor.state, d.id, *button);
            control.component = std::move(button);
        }
        else if (id == pattern || id == reverseMode)
        {
            auto combo = std::make_unique<juce::ComboBox>();
            combo->addItemList(choices(d), 1);
            combo->setTooltip(d.name);
            control.combo = std::make_unique<State::ComboBoxAttachment>(processor.state, d.id, *combo);
            control.component = std::move(combo);
        }
        else
        {
            auto slider = std::make_unique<DmgSlider>();
            const bool horizontal = id == hp || id == lp;
            const bool narrow = id == crusherMix || id == mix || id == crushMix || id == reduceMix || id == repeatMix
                              || id == depth || id == pitchMix || id == reverseMix;
            slider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
            slider->setColour(juce::Slider::textBoxBackgroundColourId, Color(0xffeeeeee));
            slider->setSliderStyle(horizontal ? juce::Slider::LinearHorizontal : juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle(horizontal ? juce::Slider::TextBoxRight : juce::Slider::TextBoxBelow,
                                    false, horizontal ? 83 : narrow ? 42 : 43, narrow ? 15 : 17);
            slider->setDoubleClickReturnValue(true, d.initial);
            slider->setVelocityModeParameters(0.5, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
            if (horizontal)
                slider->setTooltip("Move off the default to engage the filter. Double-click to bypass.");
            else
                slider->setTooltip(juce::String(d.name) + "  |  Shift-drag to fine tune, right-click to enter, double-click to reset");
            control.slider = std::make_unique<State::SliderAttachment>(processor.state, d.id, *slider);
            if (d.kind == Kind::knob)
            {
                slider->textFromValueFunction = [d, id](double number)
                {
                    if (id == rate)
                        return number >= 1000.0 ? juce::String(number / 1000.0, 1) + "k"
                                                : juce::String(number, 0) + " Hz";
                    if (juce::String(d.unit) == "%")
                        return juce::String(number, number == std::round(number) ? 0 : 1) + "%";
                    return juce::String(number, d.step >= 1.0f ? 0 : 1) + " " + d.unit;
                };
                slider->valueFromTextFunction = [id](const juce::String& entry)
                {
                    return entry.getDoubleValue() * (id == rate && entry.containsIgnoreCase("k") ? 1000.0 : 1.0);
                };
                slider->updateText();
            }
            control.component = std::move(slider);
        }
        control.component->setName(d.name);
        canvas.addAndMakeVisible(*control.component);
    }

    bool on(Param id) const { return processor.state.getRawParameterValue(definitions[id].id)->load() > 0.5f; }

    void timerCallback() override
    {
        updatePresetLabel();
        bypassShade.setVisible(on(bypass));
        for (const auto module : visibleRoute())
        {
            const auto id = enables[module];
            static_cast<juce::TextButton&>(*controls[id].component).setButtonText(on(id) ? "On" : "Off");
        }
        const auto route = processor.getRoute();
        if (dragSlot < 0 && route != displayedRoute)
        {
            displayedRoute = route;
            resized();
            repaint();
        }
        const bool reverseRandomEnabled = on(reverseRandom);
        controls[reverseMode].component->setEnabled(!reverseRandomEnabled);
        controls[reverseTrigger].component->setEnabled(on(reverseMode) || reverseRandomEnabled);
    }

    void updatePresetLabel()
    {
        const auto selected = processor.program.load();
        const auto labelText = selected >= 0 ? processor.getProgramName(selected)
                              : userPresetName.isNotEmpty() ? userPresetName : "User preset";
        presetBox.setButtonText(juce::String::charToString(0x25be) + "  " + labelText);
    }

    static juce::File presetFolder()
    {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("VOCAL DMG Presets");
    }

    void showPresetMenu()
    {
        static constexpr int firstFactory = 1;
        static constexpr int saveId = 1000, browseId = 1001, resetId = 1002, firstUser = 2000;
        juce::PopupMenu menu, factory, user;
        menu.setLookAndFeel(&style);
        const int factoryCount = processor.getNumPrograms();
        for (int i = 0; i < factoryCount; ++i)
            factory.addItem(firstFactory + i, processor.getProgramName(i), true, processor.program.load() == i);

        auto files = presetFolder().findChildFiles(juce::File::findFiles, false, "*.vocaldmgpreset");
        files.sort();
        for (int i = 0; i < files.size(); ++i)
            user.addItem(firstUser + i, files[i].getFileNameWithoutExtension());
        if (!files.isEmpty())
            user.addSeparator();
        user.addItem(saveId, "Save User Preset...");
        user.addItem(browseId, "Load from File...");
        menu.addSubMenu("Factory", factory);
        menu.addSubMenu("User", user);
        menu.addSeparator();
        menu.addItem(resetId, "Reset to Default");

        auto safe = juce::Component::SafePointer<Editor>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBox)
                               .withMinimumWidth(presetBox.getScreenBounds().getWidth()),
                           [safe, files = std::move(files), factoryCount](int result)
        {
            if (!safe || result == 0)
                return;
            if (result >= firstFactory && result < firstFactory + factoryCount)
                safe->processor.setCurrentProgram(result - firstFactory);
            else if (result >= firstUser && result < firstUser + files.size())
                safe->loadUserPreset(files[result - firstUser]);
            else if (result == resetId)
                safe->processor.setCurrentProgram(0);
            else if (result == saveId || result == browseId)
                safe->choosePresetFile(result == saveId);
            safe->updatePresetLabel();
        });
    }

    void loadUserPreset(const juce::File& file)
    {
        juce::MemoryBlock bytes;
        if (file.getSize() > 0 && file.getSize() <= 1048576 && file.loadFileAsData(bytes)
            && processor.restore(bytes.getData(), int(bytes.getSize())))
        {
            processor.program = -1;
            userPresetName = file.getFileNameWithoutExtension();
            updatePresetLabel();
        }
        else
            showPresetError("Could not read a VOCAL DMG preset from this file.");
    }

    static void showPresetError(const juce::String& message)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "VOCAL DMG", message);
    }

    void choosePresetFile(bool save)
    {
        const auto folder = presetFolder();
        if (save && folder.createDirectory().failed())
        {
            showPresetError("Could not create the VOCAL DMG Presets folder.");
            return;
        }
        const auto base = folder.exists() ? folder : folder.getParentDirectory();
        chooser = std::make_unique<juce::FileChooser>(save ? "Save VOCAL DMG preset" : "Load VOCAL DMG preset",
            save ? base.getChildFile("VOCAL DMG.vocaldmgpreset") : base, "*.vocaldmgpreset");
        const auto chooserFlags = juce::FileBrowserComponent::canSelectFiles
                         | (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                                 : juce::FileBrowserComponent::openMode);
        const auto safe = juce::Component::SafePointer<Editor>(this);
        chooser->launchAsync(chooserFlags, [safe, save](const juce::FileChooser& selected)
        {
            if (!safe)
                return;
            const auto file = selected.getResult();
            if (file == juce::File{})
                return;
            if (!save)
            {
                safe->loadUserPreset(file);
                return;
            }
            juce::MemoryBlock bytes;
            safe->processor.getStateInformation(bytes);
            if (!file.withFileExtension(".vocaldmgpreset").replaceWithData(bytes.getData(), bytes.getSize()))
                showPresetError("Could not save the preset.");
        });
    }

    Processor& processor;
    RackStyle style;
    juce::Component canvas;
    BypassShade bypassShade;
    std::array<Control, count> controls;
    Route displayedRoute = decodeRoute(defaultRoute);
    int dragSlot = -1, dropSlot = -1;
    juce::TextButton presetBox;
    juce::String userPresetName;
    IconButton previousPreset{IconButton::previous}, nextPreset{IconButton::next};
    juce::TooltipWindow tooltip{this, 650};
    std::unique_ptr<juce::FileChooser> chooser;
};

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor(*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Processor; }






