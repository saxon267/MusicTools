#include "modulation.h"

#include <cstring>

namespace {

uint8_t qualityOf(int degree, bool minor)
{
    static const uint8_t kMajor[7] = {0, 1, 1, 0, 0, 1, 2};
    static const uint8_t kMinor[7] = {1, 2, 0, 1, 1, 0, 0};
    return (minor ? kMinor : kMajor)[degree % 7];
}

}  // namespace

void modAnalyse(int fromSlot, int toSlot, bool toMinor, ModInfo& out)
{
    memset(&out, 0, sizeof out);

    const mt::Note fromTonic = mt::majorTonic(fromSlot);
    mt::Note       toTonic   = mt::majorTonic(toSlot);
    if (toMinor) toTonic = mt::relativeMinorOf(toTonic);

    int diff = mt::imod(toSlot - fromSlot, 12);
    if (diff > 6) diff -= 12;
    out.distance = diff;
    out.toTonic  = toTonic;
    out.toMinor  = toMinor;

    if (diff == 0) {
        out.relation = toMinor ? "REL MINOR" : "SAME KEY";
    } else if (diff == 1) {
        out.relation = "DOMINANT";
    } else if (diff == -1) {
        out.relation = "SUBDOMINANT";
    } else if (diff == 2) {
        out.relation = "UP A 2ND";
    } else if (diff == -2) {
        out.relation = "DOWN A 2ND";
    } else {
        out.relation = "FAR";
    }

    // pivot chords: triads that exist in both keys with the same quality
    const mt::Scale fromScale = mt::scaleOf(fromTonic, false);
    const mt::Scale toScale   = mt::scaleOf(toTonic, toMinor);

    for (int a = 0; a < 7; ++a) {
        for (int b = 0; b < 7; ++b) {
            if (mt::pitchClass(fromScale.notes[a]) != mt::pitchClass(toScale.notes[b])) continue;
            if (qualityOf(a, false) != qualityOf(b, toMinor)) continue;
            if (out.nPivot >= 7) break;
            Pivot& p   = out.pivots[out.nPivot++];
            p.root     = toScale.notes[b];
            p.qual     = qualityOf(b, toMinor);
            p.fromDeg  = (uint8_t)a;
            p.toDeg    = (uint8_t)b;
        }
    }

    // the two moves that always work, whatever the distance
    out.domRoot = toScale.notes[4];
    out.iiRoot  = toScale.notes[1];
}
