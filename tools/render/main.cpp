// Captures PNG de la fenetre reelle de Strong Pickle (meme code que le plugin), pour l'apercu et la notice.
#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String out = argc > 1 ? argv[1] : ".";
    PickleAudioProcessor proc;
    proc.prepareToPlay (48000.0, 256);
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    auto* ped = static_cast<PickleAudioProcessorEditor*> (ed.get());
    std::printf ("taille par defaut : %d x %d\n", ed->getWidth(), ed->getHeight());

    auto pump = [] (int n) { for (int i = 0; i < n; ++i) MessageManager::getInstance()->runDispatchLoopUntil (40); };
    auto shot = [&] (const String& name, int preset, bool active, bool status, int height)
    {
        ped->setScaledHeight (height);
        proc.applyFactoryPreset (preset);
        proc.setParamValue ("active", active ? 1.f : 0.f);
        AudioBuffer<float> b (2, 256); MidiBuffer m;
        for (int i = 0; i < 20; ++i) { b.clear(); proc.processBlock (b, m); }
        pump (30);
        if (status) proc.setParamValue ("sustain", 6.4f);
        else pump (100);
        pump (12);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        File f (out + "/" + name + ".png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };
    shot ("pickle_default", 0, true,  false, PickleAudioProcessorEditor::defaultH);
    shot ("pickle_status",  0, true,  true,  PickleAudioProcessorEditor::baseH);
    shot ("pickle_crunch",  4, true,  false, PickleAudioProcessorEditor::baseH);
    shot ("pickle_scoop",   5, true,  false, PickleAudioProcessorEditor::baseH);
    shot ("pickle_bypass",  6, false, false, PickleAudioProcessorEditor::baseH);
    shot ("pickle_octave", 12, true,  false, PickleAudioProcessorEditor::baseH);

    // test pedale d'expression : course 2..116 sur CC 21 (sustain)
    {
        auto sendCC = [&] (int v)
        {
            AudioBuffer<float> b (2, 256); MidiBuffer m; m.addEvent (MidiMessage::controllerEvent (1, 21, v), 0);
            proc.processBlock (b, m);
            for (int k = 0; k < 20; ++k) { MidiBuffer e; b.clear(); proc.processBlock (b, e); }
            pump (3);
            return proc.raw ("sustain");
        };
        const float a = sendCC (2), c = sendCC (116), mid = sendCC (59);
        std::printf ("pedale 2..116 -> sustain %.2f / %.2f, mi-course %.2f\n", a, c, mid);
    }
    ed.reset();
    return 0;
}
