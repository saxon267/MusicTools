// Circle of fifths reference tool for M5Stack Cardputer (v1 and ADV).
//
// Layout A: 12-slot wheel (major outside / relative minor inside) on the left,
// the seven diatonic chords of the selected key on the right.
//
// Keys:
//   Fn + , / .        previous / next key on the circle
//   Fn + ; / /        previous / next diatonic degree
//   1 .. 7            jump to a degree
//   m                 toggle major / relative minor view
//   Enter or Space    play the selected chord
#include <M5Cardputer.h>

#include <cstring>

#include "audio.h"
#include "theory.h"
#include "ui.h"

namespace {

UiState gState;
bool    gDirty = true;

struct RepeatKey {
    bool     down = false;
    uint32_t t0   = 0;
    uint32_t t1   = 0;
};

RepeatKey kLeft, kRight, kUp, kDown, kEnter, kSpace;

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

void moveSlot(int delta)
{
    gState.slot = (gState.slot + delta + mt::kSlots) % mt::kSlots;
    gDirty      = true;
}

void moveDegree(int delta)
{
    gState.selDegree = (gState.selDegree + delta + 7) % 7;
    gDirty           = true;
}

void playSelected()
{
    mt::Note tonic = mt::majorTonic(gState.slot);
    if (gState.minorView) tonic = mt::relativeMinorOf(tonic);

    mt::Chord chords[7];
    mt::diatonicChords(tonic, gState.minorView, chords);
    audioPlayChord(chords[gState.selDegree]);
    gDirty = true;
}

void handleChar(char c)
{
    if (c >= '1' && c <= '7') {
        gState.selDegree = c - '1';
        gDirty           = true;
    } else if (c == 'm' || c == 'M') {
        gState.minorView = !gState.minorView;
        gDirty           = true;
    } else if (c == '[') {
        moveSlot(-1);
    } else if (c == ']') {
        moveSlot(1);
    }
}

bool hasChar(const std::vector<char>& w, char c)
{
    for (char k : w) {
        if (k == c) return true;
    }
    return false;
}

void pollTypedKeys()
{
    auto& ks = M5Cardputer.Keyboard.keysState();

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
        if (!seen) handleChar(cur[i]);
    }

    memcpy(gPrevKeys, cur, (size_t)n + 1);
    gPrevLen = n;
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

    gState.slot      = 0;
    gState.minorView = false;
    gState.selDegree = 4;  // the V chord is a useful default
    gState.battery   = M5.Power.getBatteryLevel();
    gState.audioBusy = false;

    Serial.printf("[fifths] board=%d panel=%dx%d sprite=%d spk=%d\n", (int)M5.getBoard(),
                  (int)M5.Display.width(), (int)M5.Display.height(), uiHasCanvas() ? 1 : 0,
                  M5.Speaker.isEnabled() ? 1 : 0);

    uiRender(gState);
    gDirty = false;
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

    if (stepKey(left, kLeft)) moveSlot(-1);
    if (stepKey(right, kRight)) moveSlot(1);
    if (stepKey(up, kUp)) moveDegree(-1);
    if (stepKey(down, kDown)) moveDegree(1);

    if (!ks.fn) pollTypedKeys();

    const bool fireEnter = stepKey(ks.enter, kEnter);
    const bool fireSpace = stepKey(ks.space, kSpace);
    if (fireEnter || fireSpace) {
        playSelected();
    }

    // Hardware safety net: the side button still walks the circle if a future
    // board revision is not recognised by the keyboard scanner.
    if (M5.BtnA.wasClicked()) {
        moveSlot(1);
        playSelected();
    }

    const uint32_t now = millis();
    if (now - lastBattery > 20000) {
        lastBattery = now;
        int lvl     = M5.Power.getBatteryLevel();
        if (lvl != gState.battery) {
            gState.battery = lvl;
            gDirty         = true;
        }
    }

    if (now - lastBusy > 150) {
        lastBusy  = now;
        bool busy = audioBusy();
        if (busy != gState.audioBusy) {
            gState.audioBusy = busy;
            gDirty           = true;
        }
    }

    if (gDirty) {
        uiRender(gState);
        gDirty = false;
    }

    delay(5);
}
