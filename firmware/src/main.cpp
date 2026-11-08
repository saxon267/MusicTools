// Circle of fifths + chord dictionary for M5Stack Cardputer (v1 and ADV).
//
// page 0: 12-slot wheel (major outside / relative minor inside) + the seven
//         diatonic chords of the selected key
// page 1: chord query - spelt tones and intervals on the left, every key that
//         contains the chord on the right
//
// Keys:
//   Tab                 switch page
//   Fn + , / .          page 0: previous / next key on the circle
//                       page 1: transpose the chord down / up a semitone
//   Fn + ; / /          page 0: previous / next diatonic degree
//                       page 1: previous / next chord type
//   1 .. 7              page 0: jump to a degree
//   a .. g 0-9 m # b    page 1: type the chord (e.g. g 7, g m 7 b 5)
//   backspace           page 1: delete the last character
//   Enter or Space      play the chord
#include <M5Cardputer.h>
#include <Preferences.h>

#include <cctype>
#include <cstring>

#include "audio.h"
#include "metronome.h"
#include "theory.h"
#include "ui.h"

namespace {

UiState g;
Preferences gPrefs;
uint32_t gVolShownUntil = 0;
uint32_t gVolSavedAt    = 0;
bool     gVolSaveDirty  = false;
uint32_t gNextBeatUs    = 0;
bool    gDirty = true;

struct RepeatKey {
    bool     down = false;
    uint32_t t0   = 0;
    uint32_t t1   = 0;
};

RepeatKey kLeft, kRight, kUp, kDown, kEnter, kSpace, kTab, kDel;

char gPrevKeys[32] = {0};
int  gPrevLen      = 0;

bool stepKey(bool cur, RepeatKey& k)
{
    const uint32_t now  = millis();
    bool           fire = false;
    if (cur && !k.down) {
        fire = true;
        k.t0 = now;
        k.t1 = now;
    } else if (cur) {
        if (now - k.t0 > 380 && now - k.t1 > 70) {
            fire = true;
            k.t1 = now;
        }
    }
    k.down = cur;
    return fire;
}

bool hasChar(const std::vector<char>& w, char c)
{
    for (char k : w) {
        if (k == c) return true;
    }
    return false;
}

// ---------------------------------------------------------------- circle page

void moveSlot(int delta)
{
    g.slot = (g.slot + delta + mt::kSlots) % mt::kSlots;
}

void moveDegree(int delta)
{
    g.selDegree = (g.selDegree + delta + 7) % 7;
}

void selectionChord(mt::Note& root, int& type)
{
    mt::Note tonic = mt::majorTonic(g.slot);
    if (g.minorView) tonic = mt::relativeMinorOf(tonic);

    mt::Chord chords[7];
    mt::diatonicChords(tonic, g.minorView, chords);

    root = chords[g.selDegree].root;
    switch (chords[g.selDegree].q) {
        case mt::Quality::Min: type = 1; break;
        case mt::Quality::Dim: type = 2; break;
        case mt::Quality::Aug: type = 3; break;
        default: type = 0; break;
    }
}

// ---------------------------------------------------------------- chord page

void rebuildText()
{
    mt::formatChord(g.chordRoot, g.chordType, g.chordText, (int)sizeof g.chordText);
    g.chordValid = true;
    g.gtrPage    = 0;
}

void reparse()
{
    mt::Note root;
    int      type;
    if (mt::parseChord(g.chordText, root, type)) {
        g.chordRoot  = root;
        g.chordType  = type;
        g.chordValid = true;
        g.gtrPage    = 0;
    } else {
        g.chordValid = false;
    }
}

void seedChordFromSelection()
{
    selectionChord(g.chordRoot, g.chordType);
    rebuildText();
}

void appendChar(char c)
{
    const int len = (int)strlen(g.chordText);
    if (len < (int)sizeof g.chordText - 1) {
        g.chordText[len]     = c;
        g.chordText[len + 1] = '\0';
        reparse();
    }
}

void startRoot(char upper)
{
    g.chordText[0] = upper;
    g.chordText[1] = '\0';
    reparse();
}

void transposeRoot(int delta)
{
    if (!g.chordValid) return;
    const int pc = mt::imod(mt::pitchClass(g.chordRoot) + delta, 12);
    g.chordRoot  = mt::pcToNote(pc, delta < 0);
    rebuildText();
}

void cycleType(int delta)
{
    if (!g.chordValid) return;
    g.chordType = mt::imod(g.chordType + delta, mt::kChordTypeCount);
    rebuildText();
}

void deleteLastChar()
{
    int len = (int)strlen(g.chordText);
    if (len <= 1) {
        strcpy(g.chordText, "C");
    } else {
        g.chordText[len - 1] = '\0';
    }
    reparse();
}

void editChord(char c)
{
    const int len = (int)strlen(g.chordText);

    if (c >= 'A' && c <= 'G') {  // shift+letter always starts a fresh root
        startRoot(c);
        return;
    }
    if (c >= 'a' && c <= 'g') {
        if (len == 1 && c == 'b') {  // "G" + b -> Gb
            appendChar('b');
            return;
        }
        if (len <= 1) {  // a bare root gets replaced
            startRoot((char)toupper((unsigned char)c));
            return;
        }
        appendChar(c);  // m7b5, maj7 ...
        return;
    }
    if (c == '#' || c == 'b' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')) {
        appendChar(c);
    }
}

// ---------------------------------------------------------------- shared

void playSelection()
{
    mt::Note root;
    int      type;
    selectionChord(root, type);
    audioPlayChord(root, type);
}

void playQuery()
{
    if (g.chordValid) audioPlayChord(g.chordRoot, g.chordType);
}

void transposeScaleRoot(int delta)
{
    const int pc = mt::imod(mt::pitchClass(g.scaleRoot) + delta, 12);
    g.scaleRoot  = mt::pcToNote(pc, delta < 0);
}

void cycleScaleType(int delta)
{
    g.scaleType = mt::imod(g.scaleType + delta, mt::kScaleTypeCount);
}

void editScaleRoot(char c)
{
    char up = c;
    if (up >= 'a' && up <= 'g') up = (char)(up - 'a' + 'A');
    if (up >= 'A' && up <= 'G') {
        static const char kLetters[] = "CDEFGAB";
        g.scaleRoot.letter = (int8_t)(strchr(kLetters, up) - kLetters);
        g.scaleRoot.acc    = 0;
    } else if (c == '#') {
        g.scaleRoot.acc = 1;
    }
}

void changeVolume(int delta)
{
    int v = g.volume + delta;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    if (v == g.volume) return;

    g.volume       = v;
    gVolShownUntil = millis() + 1200;
    gVolSaveDirty  = true;
    gVolSavedAt    = millis();
    audioSetVolume(v);
    gDirty = true;
}

// ---------------------------------------------------------------- progression

int progDiatonicQuality(int degree)
{
    static const uint8_t kQ[7] = {Q_MAJ, Q_MIN, Q_MIN, Q_MAJ, Q_MAJ, Q_MIN, Q_DIM};
    return kQ[degree % 7];
}

void progStop()
{
    g.progPlaying = false;
    audioStop();
    g.metroOn = false;
    g.beat    = -1;
}

void progStart()
{
    if (!g.progSteps || g.progLen <= 0) return;
    g.progPlaying = true;
    g.progPos     = 0;
    g.metroOn     = true;
    g.beat        = -1;
    gNextBeatUs   = micros();
}

void progLoadPreset(int style, int idx)
{
    const Progression& p = kProgStyles[style].items[idx];
    g.progSteps = p.steps;
    g.progLen   = p.len;
    g.progName  = p.name;
    g.progMinor = p.minor;
    g.progStyle = style;
    g.progSel   = idx;
    g.progView  = 2;
    g.progEdit  = false;
    g.progPos   = 0;
}

void progLoadCustom()
{
    g.progSteps = gCustomSteps;
    g.progLen   = kCustomLen;
    g.progName  = "CUSTOM";
    g.progMinor = false;
    g.progStyle = kProgStyleCount - 1;
    g.progView  = 2;
    g.progEdit  = true;
    g.progPos   = 0;
    g.progSlot  = 0;
}

void progBack()
{
    if (g.progView == 2) {
        progStop();
        g.progView = g.progEdit ? 0 : 1;
    } else if (g.progView == 1) {
        g.progView = 0;
    }
}

void progStep(int delta)
{
    ProgStep& s = gCustomSteps[g.progSlot];
    s.degree    = (uint8_t)((s.degree + delta + 7) % 7);
    s.acc       = 0;
    s.qual      = (uint8_t)progDiatonicQuality(s.degree);
}

void handleChar(char c)
{
    // global volume, on every page
    if (c == '-' || c == '_') {
        changeVolume(-10);
        return;
    }
    if (c == '=' || c == '+') {
        changeVolume(10);
        return;
    }

    if (g.page == 0) {
        if (c >= '1' && c <= '7') {
            g.selDegree = c - '1';
        } else if (c == 'm' || c == 'M') {
            g.minorView = !g.minorView;
        } else if (c == '[') {
            moveSlot(-1);
        } else if (c == ']') {
            moveSlot(1);
        }
    } else if (g.page == 3) {
        editScaleRoot(c);
    } else {
        editChord(c);
    }
}

bool pollTypedKeys()
{
    auto& ks = M5Cardputer.Keyboard.keysState();

    bool typed = false;
    char cur[32];
    int  n = 0;
    for (char c : ks.word) {
        if (n < (int)sizeof(cur) - 1) cur[n++] = c;
    }
    cur[n] = '\0';

    for (int i = 0; i < n; ++i) {
        bool seen = false;
        for (int j = 0; j < gPrevLen; ++j) {
            if (gPrevKeys[j] == cur[i]) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            handleChar(cur[i]);
            typed = true;
        }
    }

    memcpy(gPrevKeys, cur, (size_t)n + 1);
    gPrevLen = n;
    return typed;
}

}  // namespace

void setup()
{
    Serial.begin(115200);

    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    if (M5.Display.width() != 240) {
        M5.Display.setRotation(1);
    }
    M5.Display.setBrightness(140);

    audioInit();
    uiBegin();
    uiSplash();
    delay(1400);

    g.page       = 0;
    g.slot       = 0;
    g.minorView  = false;
    g.selDegree  = 4;  // the V chord is a useful default
    g.battery    = M5.Power.getBatteryLevel();
    metroInit();
    g.modTo       = 1;
    g.modMinor    = false;
    g.modFocus    = 1;

    g.progView    = 0;
    g.progStyle   = 0;
    g.progSel     = 0;
    g.progPos     = 0;
    g.progPlaying = false;
    g.progEdit    = false;
    g.progSlot    = 0;
    g.progMinor   = false;
    g.progSteps   = nullptr;
    g.progLen     = 0;
    g.progName    = "";

    g.bpm          = 100;
    g.metroPattern = 0;
    g.metroOn      = false;
    g.beat         = -1;

    gPrefs.begin("musictools", false);
    g.volume     = (int)gPrefs.getUChar("vol", 100);
    audioSetVolume(g.volume);
    g.showVolume = false;
    g.audioBusy  = false;
    g.chordType  = 0;
    g.scaleRoot  = mt::majorTonic(0);
    g.scaleType  = 0;
    g.chordRoot  = mt::majorTonic(0);
    g.chordValid = true;
    g.gtrPage    = 0;
    strcpy(g.chordText, "C");

    Serial.printf("[musictools] board=%d panel=%dx%d sprite=%d spk=%d\n", (int)M5.getBoard(),
                  (int)M5.Display.width(), (int)M5.Display.height(), uiHasCanvas() ? 1 : 0,
                  M5.Speaker.isEnabled() ? 1 : 0);

    uiRender(g);
}

void loop()
{
    static uint32_t lastBattery = 0;
    static uint32_t lastBusy    = 0;

    M5Cardputer.update();
    auto& ks = M5Cardputer.Keyboard.keysState();

    // This library revision exposes the Fn layer as raw characters, so the
    // arrows printed on the keycaps are Fn + ; , . /
    const bool up    = ks.fn && hasChar(ks.word, ';');
    const bool down  = ks.fn && hasChar(ks.word, '.');
    const bool left  = ks.fn && hasChar(ks.word, ',');
    const bool right = ks.fn && hasChar(ks.word, '/');

    bool fireLeft = stepKey(left, kLeft);
    bool fireRight = stepKey(right, kRight);
    bool fireUp    = stepKey(up, kUp);
    bool fireDown  = stepKey(down, kDown);
    bool fireTab   = stepKey(ks.tab, kTab);
    bool fireDel   = stepKey(ks.del, kDel);

    if (fireTab || fireLeft || fireRight || fireUp || fireDown || fireDel) gDirty = true;

    if (fireTab) {
        g.page = (g.page + 1) % 7;
        if (g.page == 1) seedChordFromSelection();
        if (g.page != 5 && g.progPlaying) progStop();
    }

    if (g.page == 0) {
        if (fireLeft) moveSlot(-1);
        if (fireRight) moveSlot(1);
        if (fireUp) moveDegree(-1);
        if (fireDown) moveDegree(1);
    } else if (g.page == 1) {
        if (fireLeft) transposeRoot(-1);
        if (fireRight) transposeRoot(1);
        if (fireUp) cycleType(-1);
        if (fireDown) cycleType(1);
        if (fireDel) deleteLastChar();
    } else if (g.page == 2) {
        if (fireLeft) g.gtrPage = (g.gtrPage + 2) % 3;
        if (fireRight) g.gtrPage = (g.gtrPage + 1) % 3;
        if (fireUp) cycleType(-1);
        if (fireDown) cycleType(1);
        if (fireDel) deleteLastChar();
    } else if (g.page == 3) {
        if (fireLeft) transposeScaleRoot(-1);
        if (fireRight) transposeScaleRoot(1);
        if (fireUp) cycleScaleType(-1);
        if (fireDown) cycleScaleType(1);
    } else if (g.page == 4) {
        if (fireLeft) g.metroPattern = (g.metroPattern + kMetroPatternCount - 1) % kMetroPatternCount;
        if (fireRight) g.metroPattern = (g.metroPattern + 1) % kMetroPatternCount;
        if (fireUp) g.bpm = g.bpm >= 240 ? 240 : g.bpm + 1;
        if (fireDown) g.bpm = g.bpm <= 40 ? 40 : g.bpm - 1;
        if (fireUp || fireDown) g.beat = -1;
    } else if (g.page == 5) {
        if (g.progView == 0) {
            if (fireUp) g.progStyle = (g.progStyle + kProgStyleCount - 1) % kProgStyleCount;
            if (fireDown) g.progStyle = (g.progStyle + 1) % kProgStyleCount;
        } else if (g.progView == 1) {
            const int n = kProgStyles[g.progStyle].count;
            if (n > 0) {
                if (fireUp) g.progSel = (g.progSel + n - 1) % n;
                if (fireDown) g.progSel = (g.progSel + 1) % n;
            }
        } else if (g.progEdit) {
            if (fireLeft) g.progSlot = (g.progSlot + g.progLen - 1) % g.progLen;
            if (fireRight) g.progSlot = (g.progSlot + 1) % g.progLen;
            if (fireUp) progStep(-1);
            if (fireDown) progStep(1);
        }
        if (fireDel) progBack();
    } else if (g.page == 6) {
        if (fireLeft || fireRight) {
            const int delta = fireRight ? 1 : 11;
            if (g.modFocus == 0) {
                g.slot = (g.slot + delta) % 12;
            } else {
                g.modTo = (g.modTo + delta) % 12;
            }
        }
        if (fireUp || fireDown) {
            if (g.modFocus == 0) {
                g.minorView = !g.minorView;
            } else {
                g.modMinor = !g.modMinor;
            }
        }
    }

    if (!ks.fn && pollTypedKeys()) gDirty = true;

    const bool fireEnter = stepKey(ks.enter, kEnter);
    const bool fireSpace = stepKey(ks.space, kSpace);
    if (fireEnter || fireSpace) {
        gDirty = true;
        if (g.page == 4) {
            // metronome: this key only starts and stops the click
            audioStop();
            g.metroOn   = !g.metroOn;
            g.beat      = -1;
            gNextBeatUs = micros();
        } else if (g.page == 6) {
            g.modFocus = 1 - g.modFocus;
        } else if (g.page == 5) {
            if (g.progView == 0) {
                if (g.progStyle == kProgStyleCount - 1) {
                    progLoadCustom();
                } else {
                    g.progView = 1;
                    g.progSel  = 0;
                }
            } else if (g.progView == 1) {
                progLoadPreset(g.progStyle, g.progSel);
            } else if (g.progPlaying) {
                progStop();
            } else {
                progStart();
            }
        } else if (audioBusy()) {
            audioStop();  // second press stops whatever is sounding
        } else if (g.page == 0) {
            playSelection();
        } else if (g.page <= 2) {
            playQuery();
        } else {
            audioPlayScale(g.scaleRoot, g.scaleType);
        }
    }

    // Hardware safety net: the side button still walks the circle if a future
    // board revision is not recognised by the keyboard scanner.
    if (M5.BtnA.wasClicked()) {
        if (g.page == 0) {
            moveSlot(1);
            playSelection();
            gDirty = true;
        }
    }

    const uint32_t now = millis();
    if (now - lastBattery > 20000) {
        lastBattery = now;
        int lvl     = M5.Power.getBatteryLevel();
        if (lvl != g.battery) {
            g.battery = lvl;
            gDirty    = true;
        }
    }

    if (now - lastBusy > 150) {
        lastBusy  = now;
        bool busy = audioBusy();
        if (busy != g.audioBusy) {
            g.audioBusy = busy;
            gDirty      = true;
        }
    }

    if (gVolShownUntil && now > gVolShownUntil) {
        gVolShownUntil = 0;
        gDirty         = true;
    }
    g.showVolume = (gVolShownUntil != 0);

    if (gVolSaveDirty && now - gVolSavedAt > 800) {
        gPrefs.putUChar("vol", (uint8_t)g.volume);
        gVolSaveDirty = false;
    }

    // metronome: schedule against micros() rather than the loop rate
    if (g.metroOn) {
        const uint32_t t = micros();
        if ((int32_t)(t - gNextBeatUs) >= 0) {
            const MetroPattern& p = kMetroPatterns[g.metroPattern];
            g.beat = (g.beat + 1) % p.beats;
            metroClick(((p.accents >> g.beat) & 1u) != 0);

            if (g.progPlaying && g.beat == 0 && g.progSteps && g.progLen > 0) {
                mt::Note tonic = mt::majorTonic(g.slot);
                if (g.progMinor) tonic = mt::relativeMinorOf(tonic);
                mt::Note root;
                int      ti = 0;
                progChord(tonic, g.progMinor, g.progSteps[g.progPos], root, ti);
                audioPlayChord(root, ti);
                g.progPos = (g.progPos + 1) % g.progLen;
            }
            const uint32_t interval = 60000000UL / (uint32_t)g.bpm;
            gNextBeatUs += interval;
            if ((int32_t)(t - gNextBeatUs) > 0) gNextBeatUs = t + interval;
            gDirty = true;
        }
    }

    if (gDirty) {
        uiRender(g);
        gDirty = false;
    }

    delay(g.metroOn ? 2 : 5);
}










