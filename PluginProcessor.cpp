#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Presets d'usine - reglages notes comme sur la pedale (0 a 10, 5 = midi)
//==============================================================================
static const PicklePreset factoryPresets[] =
{
    //  nom                      loud  sust  filt  crunch scoop octave
    { "Classic Pickle",          5.0f, 5.0f, 5.0f, 0, 0 },   // les deux inverseurs en bas : la Swollen Pickle a trois potards
    { "Siamese Dream",           5.0f, 7.0f, 5.5f, 0, 0 },   // Big Muff op-amp de Corgan : sustain vers 14 h, tone a midi, mediums a peine creuses, compresse
    { "Smooth Low Fuzz",         6.0f, 2.5f, 5.0f, 0, 0 },   // sustain vers 9 h : fuzz lisse
    { "Sputter Zone",            5.5f, 5.0f, 5.5f, 0, 0 },   // sustain a midi : les fins de notes crachotent
    { "Open Crunch",             4.5f, 6.0f, 6.0f, 1, 0 },
    { "Scooped Power Chords",    5.0f, 7.0f, 5.0f, 0, 1 },
    { "Dark Wool",               6.0f, 6.0f, 2.0f, 0, 0 },
    { "Bright Lead",             5.0f, 8.5f, 7.5f, 1, 0 },
    { "Amp Pusher",              8.0f, 1.5f, 6.5f, 1, 0 },   // peu de fuzz, beaucoup de volume devant un ampli deja sature
    { "Bass Pickle",             5.0f, 4.0f, 3.5f, 0, 0 },
    { "Doom Sludge",             6.0f, 10.f, 3.0f, 0, 1 },
    { "7 Words",                 5.0f, 6.0f, 6.0f, 1, 0 },   // Deftones, intro de 7 Words (Adrenaline) : riff hache, epais et net, mediums presents
    { "Octave Lead",             5.0f, 7.5f, 6.5f, 0, 0, 1 },// OCTAVE : octave au-dessus facon Ultimate Octave, micro manche apres la 12e case
};

int PickleAudioProcessor::getNumFactoryPresets() { return (int) std::size (factoryPresets); }

const PicklePreset& PickleAudioProcessor::getFactoryPreset (int i)
{
    return factoryPresets[(size_t) jlimit (0, getNumFactoryPresets() - 1, i)];
}

const char* PickleAudioProcessor::knobId (int i)
{
    static const char* ids[numKnobs] = { "loudness", "sustain", "filter" };
    return ids[jlimit (0, numKnobs - 1, i)];
}

const PickleAudioProcessor::LearnDef& PickleAudioProcessor::learnDef (int i)
{
    static const LearnDef defs[numLearn] =
    {
        { "loudness", "Loudness",   ccLoudness, learnContinuous },
        { "sustain",  "Sustain",    ccSustain,  learnContinuous },
        { "filter",   "Filter",     ccFilter,   learnContinuous },
        { "crunch",   "Crunch",     ccCrunch,   learnToggle },      // >= 64 : haut
        { "scoop",    "Scoop",      ccScoop,    learnToggle },      // >= 64 : haut
        { "active",   "Footswitch", ccBypassFs, learnFootswitch },
        { "octave",   "Octave",     ccOctave,   learnToggle },      // >= 64 : octave
    };
    return defs[jlimit (0, numLearn - 1, i)];
}

void PickleAudioProcessor::resetMidiMap()
{
    for (int i = 0; i < numLearn; ++i) ccMap[i].store (learnDef (i).defaultCC);
}

//==============================================================================
PickleAudioProcessor::PickleAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STRONGPICKLE", createLayout())
{
    setLatencySamples (pickle::Engine::latencyInt());
    for (int i = 0; i < numRaw; ++i) rawPtr[i] = apvts.getRawParameterValue (rawIds[i]);
    for (int i = 0; i < numQueued; ++i) { pendingValue[i] = 0.0f; pendingSet[i] = false; }
    resetMidiMap();
    startTimerHz (60);
}

AudioProcessorValueTreeState::ParameterLayout PickleAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const auto& d = factoryPresets[0];

    auto knob = [] (const String& id, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return String (roundToInt (v * 10.0f)) + " %"; }));
    };

    p.push_back (knob ("loudness", "Loudness", d.loudness));
    p.push_back (knob ("sustain",  "Sustain",  d.sustain));
    p.push_back (knob ("filter",   "Filter",   d.filter));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "crunch", 1 }, "Crunch", d.crunch != 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "scoop", 1 },  "Scoop",  d.scoop != 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "active", 1 }, "Footswitch", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "midiFsMode", 1 }, "MIDI Footswitch Mode",
                                                         StringArray { "Latching (127 / 0)", "Momentary (toggle)" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "gate", 1 }, "Noise Gate", gateNames(), 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "octave", 1 }, "Octave", d.octave != 0));
    return { p.begin(), p.end() };
}

bool PickleAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return in == out;
}

void PickleAudioProcessor::setParamValue (const String& id, float plain)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

// Anti-gel : jamais d'appel a l'hote (setValueNotifyingHost) ni d'allocation depuis le thread audio.
void PickleAudioProcessor::setParamFromAudio (int qi, float plain)
{
    if (qi < 0 || qi >= numQueued) return;
    pendingValue[qi].store (plain, std::memory_order_relaxed);
    pendingSet[qi].store (true, std::memory_order_release);
}

void PickleAudioProcessor::timerCallback()
{
    for (int i = 0; i < numQueued; ++i)
        if (pendingSet[i].exchange (false, std::memory_order_acquire))
            if (auto* prm = apvts.getParameter (learnDef (i).id))
                prm->setValueNotifyingHost (prm->convertTo0to1 (pendingValue[i].load (std::memory_order_relaxed)));
}

//==============================================================================
void PickleAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare (sr, jmax (1, getTotalNumOutputChannels()));
    for (auto& r : ramps) { r.active = false; r.left = 0; }
    setLatencySamples (pickle::Engine::latencyInt());
}

//==============================================================================
// MIDI
//==============================================================================
float PickleAudioProcessor::mapCC (int k, float raw01)
{
    auto& r = ramps[k];
    if (raw01 < r.lo) r.lo = raw01;
    if (raw01 > r.hi) r.hi = raw01;
    float v = raw01;
    if (r.hi - r.lo > 0.4f) v = (raw01 - r.lo) / (r.hi - r.lo);
    constexpr float dz = 0.02f;
    return jlimit (0.0f, 1.0f, (v - dz) / (1.0f - 2.0f * dz));
}

void PickleAudioProcessor::handleMidi (MidiBuffer& midi)
{
    const bool latching = rawAt (rMidiFsMode) < 0.5f;

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const juce::int64 when = sampleClock + meta.samplePosition;

        if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            bool known = true;

            auto knobFromCC = [&] (int k, float raw01)
            {
                auto& r = ramps[k];
                const float target = mapCC (k, raw01) * 10.0f;
                if (! r.active) r.value = rawAt (k);
                const double interval = (double) (when - r.lastTime);
                r.left = (int) jlimit (0.002 * sr, 0.040 * sr, interval);
                r.lastTime = when;
                r.target = target;
                r.step = (r.target - r.value) / (float) r.left;
                r.active = true;
                setParamFromAudio (k, target);
                currentPreset = -1;
            };

            // MIDI Learn : le premier CC recu est affecte a la commande visee (il n'agit pas sur le son)
            const int lt = learnTarget.load();
            if (lt >= 0 && lt < numLearn && cc < 120)
            {
                for (int j = 0; j < numLearn; ++j)
                    if (j != lt && ccMap[j].load() == cc) ccMap[j].store (-1);
                ccMap[lt].store (cc);
                if (lt < numKnobs) ramps[lt].msb = -1;
                if (learnDef (lt).type == learnFootswitch) prevFsCC[lt] = v;
                lastLearned.store (lt);
                learnTarget.store (-1);
                lastMidiCC = cc; lastMidiValue = v; ++midiCounter;
                continue;
            }

            int ctl = -1;
            for (int j = 0; j < numLearn; ++j)
                if (ccMap[j].load (std::memory_order_relaxed) == cc) { ctl = j; break; }
            int lsbOf = -1;
            if (ctl < 0)
                for (int k = 0; k < numKnobs; ++k)
                {
                    const int msbCC = ccMap[k].load (std::memory_order_relaxed);
                    if (msbCC >= 0 && msbCC < 32 && cc == msbCC + 32) { lsbOf = k; break; }
                }

            if (ctl >= 0 && ctl < numKnobs)
            {
                ramps[ctl].msb = v;
                knobFromCC (ctl, (float) v / 127.0f);
            }
            else if (lsbOf >= 0)
            {
                if (ramps[lsbOf].msb >= 0)
                    knobFromCC (lsbOf, (float) (ramps[lsbOf].msb * 128 + v) / 16383.0f);   // CC 14 bits
            }
            else if (ctl == 3 || ctl == 4 || ctl == 6)
            {
                setParamFromAudio (ctl, v >= 64 ? 1.0f : 0.0f);
                currentPreset = -1;
            }
            else if (ctl == 5)
            {
                int& prev = prevFsCC[ctl];
                const float cur = pendingSet[ctl].load() ? pendingValue[ctl].load() : rawAt (rActive);
                if (latching)                   setParamFromAudio (ctl, v >= 64 ? 1.0f : 0.0f);
                else if (v >= 64 && prev < 64)  setParamFromAudio (ctl, cur > 0.5f ? 0.0f : 1.0f);
                prev = v;
            }
            else known = false;

            if (known) { lastMidiCC = cc; lastMidiValue = v; ++midiCounter; }
        }
        else if (m.isProgramChange())
        {
            const int pc = m.getProgramChangeNumber();
            if (pc < getNumFactoryPresets()) { pendingPreset = pc; triggerAsyncUpdate(); }
            lastMidiProgram = pc; lastMidiCC = -2; ++midiCounter;
        }
    }
    midi.clear();
}

void PickleAudioProcessor::handleAsyncUpdate()
{
    const int p = pendingPreset.exchange (-1);
    if (p >= 0) applyFactoryPreset (p);
}

//==============================================================================
void PickleAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    handleMidi (midi);
    if (n == 0) return;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    pickle::Params p;
    float* knobs[numKnobs] = { &p.loudness, &p.sustain, &p.filter };
    for (int k = 0; k < numKnobs; ++k)
    {
        auto& r = ramps[k];
        if (r.active)
        {
            const int adv = jmin (r.left, n);
            r.value += r.step * (float) adv;
            r.left -= adv;
            if (r.left <= 0) { r.value = r.target; r.active = false; }
            *knobs[k] = r.value;
        }
        else
            *knobs[k] = rawAt (k);
    }
    p.crunch = rawAt (rCrunch) > 0.5f;
    p.scoop  = rawAt (rScoop) > 0.5f;
    p.octave = rawAt (rOctave) > 0.5f;
    p.active = rawAt (rActive) > 0.5f;
    p.gate   = jlimit (0, 3, roundToInt (rawAt (rGate)));

    const int nCh = jmin (2, buffer.getNumChannels());
    float* io[2] = { buffer.getWritePointer (0), nCh > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.process (io, n, p);
    sampleClock += n;
}

//==============================================================================
// Presets
//==============================================================================
int PickleAudioProcessor::getCurrentProgram()
{
    const int c = currentPreset.load();
    return (c >= 0 && c < getNumFactoryPresets()) ? c : 0;
}

void PickleAudioProcessor::applyFactoryPreset (int index)
{
    const auto& p = getFactoryPreset (index);
    setParamValue ("loudness", p.loudness); setParamValue ("sustain", p.sustain); setParamValue ("filter", p.filter);
    setParamValue ("crunch", (float) p.crunch); setParamValue ("scoop", (float) p.scoop);
    setParamValue ("octave", (float) p.octave);
    currentPreset = jlimit (0, getNumFactoryPresets() - 1, index);
    ++presetCounter;
}

File PickleAudioProcessor::getUserPresetFile()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
             .getChildFile ("Studio Wiga").getChildFile ("Strong Pickle").getChildFile ("UserPresets.xml");
}

static const char* presetIds[] = { "loudness", "sustain", "filter", "crunch", "scoop", "octave" };

bool PickleAudioProcessor::saveUserPreset (int slot)
{
    if (slot < 1 || slot > 9) return false;
    auto file = getUserPresetFile();
    file.getParentDirectory().createDirectory();
    std::unique_ptr<XmlElement> root;
    if (file.existsAsFile()) root = XmlDocument::parse (file);
    if (root == nullptr || ! root->hasTagName ("UserPresets")) root = std::make_unique<XmlElement> ("UserPresets");
    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
        if (e->getIntAttribute ("slot") == slot) { root->removeChildElement (e, true); break; }
    auto* e = root->createNewChildElement ("Preset");
    e->setAttribute ("slot", slot);
    for (auto* id : presetIds) e->setAttribute (id, (double) raw (id));
    const bool ok = root->writeTo (file);
    if (ok) { currentPreset = 100 + slot; ++presetCounter; }
    return ok;
}

bool PickleAudioProcessor::loadUserPreset (int slot)
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile()) return false;
    auto root = XmlDocument::parse (file);
    if (root == nullptr) return false;
    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
        if (e->getIntAttribute ("slot") == slot)
        {
            for (auto* id : presetIds)
                if (e->hasAttribute (id)) setParamValue (id, (float) e->getDoubleAttribute (id));
                else if (std::strcmp (id, "octave") == 0) setParamValue (id, 0.0f);   // presets d'avant la v1.3 : sans octave
            currentPreset = 100 + slot; ++presetCounter;
            return true;
        }
    return false;
}

bool PickleAudioProcessor::hasUserPreset (int slot) const
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile()) return false;
    if (auto root = XmlDocument::parse (file))
        for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
            if (e->getIntAttribute ("slot") == slot) return true;
    return false;
}

//==============================================================================
void PickleAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", currentPreset.load(), nullptr);
    for (int i = 0; i < numLearn; ++i)
        state.setProperty (String ("cc_") + learnDef (i).id, ccMap[i].load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PickleAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);
            currentPreset = (int) state.getProperty ("currentPreset", 0);
            for (int i = 0; i < numLearn; ++i)
                ccMap[i].store (jlimit (-1, 127, (int) state.getProperty (String ("cc_") + learnDef (i).id, learnDef (i).defaultCC)));
            apvts.replaceState (state);
            ++presetCounter;
        }
}

//==============================================================================
AudioProcessorEditor* PickleAudioProcessor::createEditor() { return new PickleAudioProcessorEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PickleAudioProcessor(); }
