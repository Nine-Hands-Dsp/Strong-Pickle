// Captures PNG de la fenetre reelle de Strong Pickle (meme code que le plugin), pour l'apercu et la notice.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
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
        ped->forceRenderScale (2.0f);
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
    shot ("pickle_status",  0, true,  true,  PickleAudioProcessorEditor::refH);
    shot ("pickle_crunch",  4, true,  false, PickleAudioProcessorEditor::refH);
    shot ("pickle_scoop",   5, true,  false, PickleAudioProcessorEditor::refH);
    shot ("pickle_bypass",  6, false, false, PickleAudioProcessorEditor::refH);
    shot ("pickle_octave", 12, true,  false, PickleAudioProcessorEditor::refH);           // Fat (haut)
    {
        proc.applyFactoryPreset (12); proc.setParamValue ("octave", 2.0f); pump (20);         // Bright (bas)
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        File f (out + "/pickle_octave_bright.png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
    }

    // rendu exact de la pedale seule (1235 x 1800), effet actif, octave Fat
    auto exact = [&] (const String& name, int preset, bool active)
    {
        ped->setScaledHeight (900);
        ped->forceRenderScale (2.0f);
        proc.applyFactoryPreset (preset);
        proc.setParamValue ("active", active ? 1.f : 0.f);
        AudioBuffer<float> b (2, 256); MidiBuffer m;
        for (int i = 0; i < 20; ++i) { b.clear(); proc.processBlock (b, m); }
        pump (140);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1800.0f / (float) ed->getHeight());
        File f (out + "/" + name + ".png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };
    exact ("pickle_exact", 12, true);
    exact ("pickle_exact_bypass", 0, false);

    // animation des inverseurs : chaque image (haut, milieu pendant la bascule, bas) capturee dans la meme fenetre
    {
        ped->setScaledHeight (900); ped->forceRenderScale (2.0f);
        auto cap = [&] (const String& name)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1800.0f / (float) ed->getHeight());
            File f (out + "/" + name + ".png"); f.deleteFile();
            FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        };
        proc.setParamValue ("octave", 1.0f); proc.setParamValue ("crunch", 1.0f); pump (10); cap ("anim_0_haut");
        proc.setParamValue ("octave", 2.0f); proc.setParamValue ("crunch", 0.0f); cap ("anim_1_milieu");   // avant les 55 ms
        pump (10); cap ("anim_2_bas");
        proc.setParamValue ("octave", 0.0f); pump (10); cap ("anim_3_octave_off");
    }

    // menu clic droit (design Octopus, NineHandsMenu.h) : rendu hors ecran avec le meme LookAndFeel que le plugin
    {
        NineHandsMenu::MenuLnf lnf;
        lnf.f = { Typeface::createSystemTypefaceFor (BinaryData::PoppinsRegular_ttf, BinaryData::PoppinsRegular_ttfSize),
                  Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize),
                  Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize) };
        struct It { String text; bool sep, sub, ticked, active, hi; };
        auto render = [&] (const String& name, const String& title, const String& subtitle, int ico, const std::vector<It>& items, int hiIndex)
        {
            NineHandsMenu::Header hd (lnf.f, ico, Colour (0xff8ee06a), title, subtitle);
            int w = 0, h = 0; hd.getIdealSize (w, h);
            const int border = lnf.getPopupMenuBorderSize();
            int totalH = border * 2 + h + 11, maxW = w;
            for (auto& it : items) { int iw, ih; lnf.getIdealPopupMenuItemSize (it.text, it.sep, 0, iw, ih); totalH += ih; maxW = jmax (maxW, iw + (it.sub ? 20 : 0)); }
            maxW += border * 2;
            Image img (Image::ARGB, maxW * 2, totalH * 2, true);
            Graphics g (img); g.addTransform (AffineTransform::scale (2.0f));
            lnf.drawPopupMenuBackground (g, maxW, totalH);
            int y = border;
            hd.setSize (maxW - border * 2, h);
            { Graphics::ScopedSaveState ss (g); g.setOrigin (border, y); hd.paint (g); }
            y += h;
            lnf.drawPopupMenuItem (g, { border, y, maxW - border * 2, 11 }, true, true, false, false, false, {}, {}, nullptr, nullptr); y += 11;
            for (size_t i = 0; i < items.size(); ++i)
            {
                const auto& it = items[i]; int iw, ih; lnf.getIdealPopupMenuItemSize (it.text, it.sep, 0, iw, ih);
                lnf.drawPopupMenuItem (g, { border, y, maxW - border * 2, ih }, it.sep, it.active, (int) i == hiIndex, it.ticked, it.sub, it.text, {}, nullptr, nullptr);
                y += ih;
            }
            File f (out + "/" + name + ".png"); f.deleteFile();
            FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
            std::printf ("%s\n", f.getFullPathName().toRawUTF8());
        };
        render ("pickle_menu", "Strong Pickle", "Jumbo fuzz  v1.5.0", NineHandsMenu::icoPedal,
                { { "Plugin size", false, true, false, true }, { "", true }, { "Factory presets", false, true, false, true },
                  { "Load user preset", false, true, false, true }, { "Save user preset", false, true, false, true }, { "", true },
                  { "Noise gate", false, true, false, true }, { "Settings", false, true, false, true }, { "MIDI", false, true, false, true },
                  { "", true }, { "Strong Pickle v1.5.0  NINE HANDS | DSP", false, false, false, false } }, 2);
        render ("pickle_menu_knob", "Sustain", "MIDI: not assigned", NineHandsMenu::icoKnob,
                { { "MIDI Learn: Sustain", false, false, false, true }, { "Clear MIDI: Sustain", false, false, false, false }, { "", true },
                  { "Plugin size", false, true, false, true }, { "", true }, { "Factory presets", false, true, false, true } }, 0);
        render ("pickle_menu_size", "Plugin size", "Window", NineHandsMenu::icoSize,
                { { "60 %  (default)", false, false, true, true }, { "75 %", false, false, false, true }, { "100 %", false, false, false, true },
                  { "", true }, { "Default size (new windows)", false, true, false, true } }, 1);
    }

    // test pedale d'expression : course 2..116 sur CC 21 (sustain)
    {
        proc.ccMap[1].store (21);                       // assignation faite comme par le MIDI Learn
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
