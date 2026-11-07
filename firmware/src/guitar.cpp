#include "guitar.h"

#include <cstring>

namespace {

const int8_t kOpenMidi[6] = {40, 45, 50, 55, 59, 64};  // E2 A2 D3 G3 B3 E4

constexpr int kMaxFret   = 12;
constexpr int kShortlist = 12;

// Hand-checked shapes for the twelve major and minor triads, in pitch-class
// order C C# D D# E F F# G G# A A# B. The search below still runs, these are
// only placed first so the familiar voicing is what the page opens with.
const int8_t kMajorShape[12][6] = {
    {-1, 3, 2, 0, 1, 0},  // C
    {-1, 4, 6, 6, 6, 4},  // C#
    {-1, -1, 0, 2, 3, 2}, // D
    {-1, 6, 8, 8, 8, 6},  // D#
    {0, 2, 2, 1, 0, 0},   // E
    {1, 3, 3, 2, 1, 1},   // F
    {2, 4, 4, 3, 2, 2},   // F#
    {3, 2, 0, 0, 0, 3},   // G
    {4, 6, 6, 5, 4, 4},   // G#
    {-1, 0, 2, 2, 2, 0},  // A
    {-1, 1, 3, 3, 3, 1},  // A#
    {-1, 2, 4, 4, 4, 2},  // B
};

const int8_t kMinorShape[12][6] = {
    {-1, 3, 5, 5, 4, 3},  // Cm
    {-1, 4, 6, 6, 5, 4},  // C#m
    {-1, -1, 0, 2, 3, 1}, // Dm
    {-1, 6, 8, 8, 7, 6},  // D#m
    {0, 2, 2, 0, 0, 0},   // Em
    {1, 3, 3, 1, 1, 1},   // Fm
    {2, 4, 4, 2, 2, 2},   // F#m
    {3, 5, 5, 3, 3, 3},   // Gm
    {4, 6, 6, 4, 4, 4},   // G#m
    {-1, 0, 2, 2, 1, 0},  // Am
    {-1, 1, 3, 3, 2, 1},  // A#m
    {-1, 2, 4, 4, 3, 2},  // Bm
};

int scoreOf(const Fingering& f);

struct Ctx {
    bool      tone[12];
    bool      need[12];
    int       rootPc;
    int8_t    cur[6];
    Fingering best[kShortlist];
    int       bestScore[kShortlist];
    int       nBest;
};

bool sameShape(const Fingering& a, const Fingering& b)
{
    for (int i = 0; i < 6; ++i) {
        if (a.fret[i] != b.fret[i]) return false;
    }
    return true;
}

// Score each playable shape and keep only the shortlist, so the search never
// has to store thousands of candidates.
void consider(Ctx& c, int sounding)
{
    if (sounding < 4) return;

    bool got[12] = {false};
    int  bass    = -1;
    for (int i = 0; i < 6; ++i) {
        if (c.cur[i] >= 0) {
            if (bass < 0) bass = i;
            got[(kOpenMidi[i] + c.cur[i]) % 12] = true;
        }
    }
    if ((kOpenMidi[bass] + c.cur[bass]) % 12 != c.rootPc) return;
    for (int pc = 0; pc < 12; ++pc) {
        if (c.need[pc] && !got[pc]) return;
    }

    Fingering f;
    for (int i = 0; i < 6; ++i) f.fret[i] = c.cur[i];

    for (int i = 0; i < c.nBest; ++i) {
        if (sameShape(c.best[i], f)) return;
    }
    const int sc = scoreOf(f);
    if (c.nBest == kShortlist && sc >= c.bestScore[kShortlist - 1]) return;

    int j = (c.nBest < kShortlist) ? c.nBest : kShortlist - 1;
    while (j > 0 && c.bestScore[j - 1] > sc) {
        c.bestScore[j] = c.bestScore[j - 1];
        c.best[j]      = c.best[j - 1];
        --j;
    }
    c.bestScore[j] = sc;
    c.best[j]      = f;
    if (c.nBest < kShortlist) ++c.nBest;
}

void dfs(Ctx& c, int s, int mn, int mx, int sounding)
{
    if (s == 6) {
        consider(c, sounding);
        return;
    }

    c.cur[s] = -1;
    dfs(c, s + 1, mn, mx, sounding);

    for (int f = 0; f <= kMaxFret; ++f) {
        if (!c.tone[(kOpenMidi[s] + f) % 12]) continue;
        int nmn = mn, nmx = mx;
        if (f > 0) {
            if (mn < 0) nmn = f;
            if (mx < 0) nmx = f;
            if (nmx - nmn > 3) continue;
        }
        c.cur[s] = (int8_t)f;
        dfs(c, s + 1, nmn, nmx, sounding + 1);
    }
    c.cur[s] = -1;
}

int scoreOf(const Fingering& f)
{
    int sounding = 0, opens = 0, mutes = 0, interior = 0;
    int mn = 99, mx = -1;
    int frets[6];
    int nf = 0;

    for (int i = 0; i < 6; ++i) {
        if (f.fret[i] >= 0) {
            ++sounding;
            if (f.fret[i] == 0) {
                ++opens;
            } else {
                frets[nf++] = f.fret[i];
                if (f.fret[i] < mn) mn = f.fret[i];
                if (f.fret[i] > mx) mx = f.fret[i];
            }
        } else {
            ++mutes;
        }
    }
    int first = -1, last = -1;
    for (int i = 0; i < 6; ++i) {
        if (f.fret[i] >= 0) {
            if (first < 0) first = i;
            last = i;
        }
    }
    for (int i = first; i <= last; ++i) {
        if (f.fret[i] < 0) ++interior;
    }

    int distinct = 0;
    for (int i = 0; i < nf; ++i) {
        bool seen = false;
        for (int j = 0; j < i; ++j) {
            if (frets[j] == frets[i]) seen = true;
        }
        if (!seen) ++distinct;
    }

    if (mx < 0) mx = 0;
    if (mn > mx) mn = 0;
    const int span = mx - mn;
    const int pos  = (opens > 0 && mx <= 3) ? 0 : mn;

    int sc = pos * 7 + span * 4 - sounding * 5 + mutes * 6 + interior * 40 + distinct * 2 - opens * 4;
    if (opens > 0 && mx > 3) sc += (mx - 3) * 10;
    if (opens == 0 && sounding >= 5 && span <= 3) sc -= 14;  // full barre
    return sc;
}

}  // namespace

int guitarShapes(mt::Note root, int typeIdx, Fingering* out, int maxShapes)
{
    if (maxShapes <= 0) return 0;

    const mt::ChordType& t   = mt::kChordTypes[typeIdx];
    const int            rpc = mt::pitchClass(root);

    static Ctx ctx;
    Ctx*       c = &ctx;

    memset(c->tone, 0, sizeof c->tone);
    memset(c->need, 0, sizeof c->need);
    for (int i = 0; i < t.n; ++i) {
        c->tone[(rpc + t.semis[i]) % 12] = true;
    }
    // Root and third define the chord; the seventh must be there too. The fifth
    // may be dropped, which is what guitarists do all the time.
    c->need[rpc] = true;
    c->need[(rpc + t.semis[1]) % 12] = true;
    if (t.n == 4) c->need[(rpc + t.semis[3]) % 12] = true;
    c->rootPc = rpc;
    c->nBest  = 0;

    dfs(*c, 0, -1, -1, 0);

    int count = 0;

    // a known-good shape first, then whatever the search liked best
    if (typeIdx == 0 || typeIdx == 1) {
        const int8_t* tbl = (typeIdx == 0) ? kMajorShape[rpc] : kMinorShape[rpc];
        for (int i = 0; i < 6; ++i) out[0].fret[i] = tbl[i];
        count = 1;
    }

    for (int i = 0; i < c->nBest && count < maxShapes; ++i) {
        const Fingering& cand = c->best[i];
        bool             dup  = false;
        for (int k = 0; k < count; ++k) {
            if (sameShape(out[k], cand)) {
                dup = true;
                break;
            }
        }
        if (dup) continue;
        out[count++] = cand;
    }

    return count;
}




