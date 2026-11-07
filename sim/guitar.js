// Guitar fingering search for the simulator.
// Ported 1:1 from firmware/src/guitar.cpp - keep the two in sync.

const GUITAR = (() => {
  const OPEN_MIDI = [40, 45, 50, 55, 59, 64]; // E2 A2 D3 G3 B3 E4
  const MAX_FRET = 12;

  // C C# D D# E F F# G G# A A# B
  const MAJOR_SHAPE = [
    [-1, 3, 2, 0, 1, 0], [-1, 4, 6, 6, 6, 4], [-1, -1, 0, 2, 3, 2], [-1, 6, 8, 8, 8, 6],
    [0, 2, 2, 1, 0, 0], [1, 3, 3, 2, 1, 1], [2, 4, 4, 3, 2, 2], [3, 2, 0, 0, 0, 3],
    [4, 6, 6, 5, 4, 4], [-1, 0, 2, 2, 2, 0], [-1, 1, 3, 3, 3, 1], [-1, 2, 4, 4, 4, 2],
  ];
  const MINOR_SHAPE = [
    [-1, 3, 5, 5, 4, 3], [-1, 4, 6, 6, 5, 4], [-1, -1, 0, 2, 3, 1], [-1, 6, 8, 8, 7, 6],
    [0, 2, 2, 0, 0, 0], [1, 3, 3, 1, 1, 1], [2, 4, 4, 2, 2, 2], [3, 5, 5, 3, 3, 3],
    [4, 6, 6, 4, 4, 4], [-1, 0, 2, 2, 1, 0], [-1, 1, 3, 3, 2, 1], [-1, 2, 4, 4, 3, 2],
  ];

  function scoreOf(fret) {
    let sounding = 0, opens = 0, mutes = 0, interior = 0;
    let mn = 99, mx = -1;
    const frets = [];
    for (let i = 0; i < 6; i++) {
      const f = fret[i];
      if (f >= 0) {
        sounding++;
        if (f === 0) opens++;
        else {
          frets.push(f);
          if (f < mn) mn = f;
          if (f > mx) mx = f;
        }
      } else mutes++;
    }
    let first = -1, last = -1;
    for (let i = 0; i < 6; i++) {
      if (fret[i] >= 0) {
        if (first < 0) first = i;
        last = i;
      }
    }
    for (let i = first; i <= last; i++) if (fret[i] < 0) interior++;

    let distinct = 0;
    for (let i = 0; i < frets.length; i++) {
      let seen = false;
      for (let j = 0; j < i; j++) if (frets[j] === frets[i]) seen = true;
      if (!seen) distinct++;
    }

    if (mx < 0) mx = 0;
    if (mn > mx) mn = 0;
    const span = mx - mn;
    const pos = opens > 0 && mx <= 3 ? 0 : mn;

    let sc = pos * 7 + span * 4 - sounding * 5 + mutes * 6 + interior * 40 + distinct * 2 - opens * 4;
    if (opens > 0 && mx > 3) sc += (mx - 3) * 10;
    if (opens === 0 && sounding >= 5 && span <= 3) sc -= 14; // full barre
    return sc;
  }

  const sameShape = (a, b) => a.every((v, i) => v === b[i]);

  function search(rootPc, ti, maxShapes) {
    const t = MT.CHORD_TYPES[ti];
    const tone = new Array(12).fill(false);
    const need = new Array(12).fill(false);
    for (let i = 0; i < t.n; i++) tone[(rootPc + t.semis[i]) % 12] = true;
    need[rootPc] = true;
    need[(rootPc + t.semis[1]) % 12] = true;
    if (t.n === 4) need[(rootPc + t.semis[3]) % 12] = true;

    // The browser can afford to keep every candidate and sort once, which is
    // simpler than the firmware's bounded shortlist and gives the same order.
    const all = [];
    const cur = [-1, -1, -1, -1, -1, -1];

    function consider(sounding) {
      if (sounding < 4) return;
      const got = new Array(12).fill(false);
      let bass = -1;
      for (let i = 0; i < 6; i++) {
        if (cur[i] >= 0) {
          if (bass < 0) bass = i;
          got[(OPEN_MIDI[i] + cur[i]) % 12] = true;
        }
      }
      if ((OPEN_MIDI[bass] + cur[bass]) % 12 !== rootPc) return;
      for (let p = 0; p < 12; p++) if (need[p] && !got[p]) return;
      const f = cur.slice();
      all.push({ f, sc: scoreOf(f) });
    }

    function dfs(s, mn, mx, sounding) {
      if (s === 6) {
        consider(sounding);
        return;
      }
      cur[s] = -1;
      dfs(s + 1, mn, mx, sounding);
      for (let f = 0; f <= MAX_FRET; f++) {
        if (!tone[(OPEN_MIDI[s] + f) % 12]) continue;
        let nmn = mn, nmx = mx;
        if (f > 0) {
          if (mn < 0) nmn = f;
          if (mx < 0) nmx = f;
          if (nmx - nmn > 3) continue;
        }
        cur[s] = f;
        dfs(s + 1, nmn, nmx, sounding + 1);
      }
      cur[s] = -1;
    }

    dfs(0, -1, -1, 0);

    all.sort((a, b) => a.sc - b.sc);

    const out = [];
    if (ti === 0 || ti === 1) out.push((ti === 0 ? MAJOR_SHAPE : MINOR_SHAPE)[rootPc].slice());
    for (const { f: cand } of all) {
      if (out.length >= 6) break;
      if (out.some((o) => sameShape(o, cand))) continue;
      out.push(cand);
    }
    return out.slice(0, maxShapes || 6);
  }

  return { shapes: search, MAJOR_SHAPE, MINOR_SHAPE };
})();

if (typeof module !== 'undefined') module.exports = GUITAR;


