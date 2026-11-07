// Music theory core for the simulator.
// Ported 1:1 from firmware/src/theory.cpp - keep the two in sync.

const MT = (() => {
  const LETTERS = 'CDEFGAB';
  const NAT = [0, 2, 4, 5, 7, 9, 11];

  const imod = (v, m) => {
    const r = v % m;
    return r < 0 ? r + m : r;
  };

  // ring slot -> fifths position (C at the top, practical spelling both ways)
  const SLOT_FIFTHS = [0, 1, 2, 3, 4, 5, 6, -5, -4, -3, -2, -1];

  const MAJOR_STEPS = [0, 2, 4, 5, 7, 9, 11];
  const MINOR_STEPS = [0, 2, 3, 5, 7, 8, 10];

  const CHORD_TYPES = [
    { suffix: '', label: 'major', n: 3, semis: [0, 4, 7], degrees: [0, 2, 4] },
    { suffix: 'm', label: 'minor', n: 3, semis: [0, 3, 7], degrees: [0, 2, 4] },
    { suffix: 'dim', label: 'dim', n: 3, semis: [0, 3, 6], degrees: [0, 2, 4] },
    { suffix: 'aug', label: 'aug', n: 3, semis: [0, 4, 8], degrees: [0, 2, 4] },
    { suffix: '7', label: 'dom 7', n: 4, semis: [0, 4, 7, 10], degrees: [0, 2, 4, 6] },
    { suffix: 'maj7', label: 'maj 7', n: 4, semis: [0, 4, 7, 11], degrees: [0, 2, 4, 6] },
    { suffix: 'm7', label: 'min 7', n: 4, semis: [0, 3, 7, 10], degrees: [0, 2, 4, 6] },
    { suffix: 'm7b5', label: 'half dim 7', n: 4, semis: [0, 3, 6, 10], degrees: [0, 2, 4, 6] },
    { suffix: 'dim7', label: 'dim 7', n: 4, semis: [0, 3, 6, 9], degrees: [0, 2, 4, 6] },
    { suffix: '6', label: 'maj 6', n: 4, semis: [0, 4, 7, 9], degrees: [0, 2, 4, 5] },
    { suffix: 'm6', label: 'min 6', n: 4, semis: [0, 3, 7, 9], degrees: [0, 2, 4, 5] },
    { suffix: 'sus4', label: 'sus 4', n: 3, semis: [0, 5, 7], degrees: [0, 3, 4] },
    { suffix: 'sus2', label: 'sus 2', n: 3, semis: [0, 2, 7], degrees: [0, 1, 4] },
    { suffix: '7sus4', label: '7 sus 4', n: 4, semis: [0, 5, 7, 10], degrees: [0, 3, 4, 6] },
  ];

  const SCALE_TYPES = [
    { name: 'MAJOR', n: 7, semis: [0, 2, 4, 5, 7, 9, 11], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'DORIAN', n: 7, semis: [0, 2, 3, 5, 7, 9, 10], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'PHRYGIAN', n: 7, semis: [0, 1, 3, 5, 7, 8, 10], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'LYDIAN', n: 7, semis: [0, 2, 4, 6, 7, 9, 11], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'MIXOLYDIAN', n: 7, semis: [0, 2, 4, 5, 7, 9, 10], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'AEOLIAN', n: 7, semis: [0, 2, 3, 5, 7, 8, 10], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'LOCRIAN', n: 7, semis: [0, 1, 3, 5, 6, 8, 10], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'HARM MIN', n: 7, semis: [0, 2, 3, 5, 7, 8, 11], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'MELOD MIN', n: 7, semis: [0, 2, 3, 5, 7, 9, 11], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'LYDIAN DOM', n: 7, semis: [0, 2, 4, 6, 7, 9, 10], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'PHRYG DOM', n: 7, semis: [0, 1, 4, 5, 7, 8, 10], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'HUNGARIAN', n: 7, semis: [0, 2, 3, 6, 7, 8, 11], degrees: [0, 1, 2, 3, 4, 5, 6] },
    { name: 'PENTATONIC', n: 5, semis: [0, 2, 4, 7, 9], degrees: [0, 1, 2, 4, 5] },
    { name: 'MIN PENTA', n: 5, semis: [0, 3, 5, 7, 10], degrees: [0, 2, 3, 4, 6] },
    { name: 'BLUES', n: 6, semis: [0, 3, 5, 6, 7, 10], degrees: [0, 2, 3, 3, 4, 6] },
    { name: 'BEBOP DOM', n: 8, semis: [0, 2, 4, 5, 7, 9, 10, 11], degrees: [0, 1, 2, 3, 4, 5, 6, 6] },
    { name: 'WHOLE TONE', n: 6, semis: [0, 2, 4, 6, 8, 10], degrees: [0, 1, 2, 3, 4, 5] },
    { name: 'DIMINISHED', n: 8, semis: [0, 2, 3, 5, 6, 8, 9, 11], degrees: [0, 1, 2, 3, 4, 5, 5, 6] },
    { name: 'HALF-WHOLE', n: 8, semis: [0, 1, 3, 4, 6, 7, 9, 10], degrees: [0, 1, 2, 2, 3, 4, 5, 6] },
  ];

  const note = (letter, acc) => ({ letter: imod(letter, 7), acc: acc | 0 });

  const pitchClass = (n) => imod(NAT[imod(n.letter, 7)] + n.acc, 12);

  const noteName = (n) => {
    let s = LETTERS[imod(n.letter, 7)];
    for (let i = 0; i < Math.abs(n.acc); i++) s += n.acc < 0 ? 'b' : '#';
    return s;
  };

  const majorTonic = (slot) => {
    const f = SLOT_FIFTHS[imod(slot, 12)];
    return note(imod(f * 4, 7), f >= 6 ? 1 : f <= -2 ? -1 : 0);
  };

  const signature = (slot) => SLOT_FIFTHS[imod(slot, 12)];

  const scaleOf = (tonic, minor) => {
    const steps = minor ? MINOR_STEPS : MAJOR_STEPS;
    const base = pitchClass(tonic);
    const out = [];
    for (let d = 0; d < 7; d++) {
      const letter = imod(tonic.letter + d, 7);
      const target = imod(base + steps[d], 12);
      let diff = target - NAT[letter];
      while (diff > 6) diff -= 12;
      while (diff < -6) diff += 12;
      out.push(note(letter, diff));
    }
    return out;
  };

  const diatonicChords = (tonic, minor) => {
    const qMajor = ['Maj', 'Min', 'Min', 'Maj', 'Maj', 'Min', 'Dim'];
    const qMinor = ['Min', 'Dim', 'Maj', 'Min', 'Min', 'Maj', 'Maj'];
    const q = minor ? qMinor : qMajor;
    const s = scaleOf(tonic, minor);
    return s.map((root, i) => ({ root, q: q[i] }));
  };

  const qualitySuffix = (q) => (q === 'Min' ? 'm' : q === 'Dim' ? 'dim' : q === 'Aug' ? 'aug' : '');
  const chordName = (c) => noteName(c.root) + qualitySuffix(c.q);

  const ROMAN_MAJOR = ['I', 'ii', 'iii', 'IV', 'V', 'vi', 'vii'];
  const ROMAN_MINOR = ['i', 'ii', 'III', 'iv', 'v', 'VI', 'VII'];
  const romanNumeral = (deg, minor) => (minor ? ROMAN_MINOR : ROMAN_MAJOR)[imod(deg, 7)];

  const relativeMinorOf = (tonic) => scaleOf(tonic, false)[5];
  const relativeMajorOf = (tonic) => scaleOf(tonic, true)[2];

  // ---- chord dictionary -------------------------------------------------

  const chordTypeFromSuffix = (s) => CHORD_TYPES.findIndex((t) => t.suffix === s);

  const spellChord = (root, ti) => {
    const t = CHORD_TYPES[ti];
    const base = pitchClass(root);
    const out = [];
    for (let i = 0; i < t.n; i++) {
      const letter = imod(root.letter + t.degrees[i], 7);
      const target = imod(base + t.semis[i], 12);
      let diff = target - NAT[letter];
      while (diff > 6) diff -= 12;
      while (diff < -6) diff += 12;
      out.push(note(letter, diff));
    }
    return out;
  };

  const parseChord = (text) => {
    const m = /^\s*([A-Ga-g])([#b]?)(.*)$/.exec(text);
    if (!m) return null;
    const letter = LETTERS.indexOf(m[1].toUpperCase());
    const acc = m[2] === '#' ? 1 : m[2] === 'b' ? -1 : 0;
    const ti = chordTypeFromSuffix(m[3].toLowerCase());
    if (ti < 0) return null;
    return { root: note(letter, acc), type: ti };
  };

  const formatChord = (root, ti) => noteName(root) + CHORD_TYPES[ti].suffix;

  const pcToNote = (pc, preferFlats) => {
    const sharpLetter = [0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6];
    const sharpAcc = [0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0];
    const flatLetter = [0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6];
    const flatAcc = [0, -1, 0, -1, 0, 0, -1, 0, -1, 0, -1, 0];
    const p = imod(pc, 12);
    return preferFlats ? note(flatLetter[p], flatAcc[p]) : note(sharpLetter[p], sharpAcc[p]);
  };

  const findChordInKeys = (root, ti) => {
    const t = CHORD_TYPES[ti];
    const rpc = pitchClass(root);
    const hits = [];
    for (let slot = 0; slot < 12; slot++) {
      for (let m = 0; m < 2; m++) {
        let tonic = majorTonic(slot);
        if (m) tonic = relativeMinorOf(tonic);
        const s = scaleOf(tonic, m !== 0);
        for (let d = 0; d < 7; d++) {
          const cn = [s[d], s[(d + 2) % 7], s[(d + 4) % 7]];
          if (t.n === 4) cn.push(s[(d + 6) % 7]);
          if (pitchClass(cn[0]) !== rpc) continue;
          let ok = true;
          for (let i = 0; i < t.n; i++) {
            if (imod(pitchClass(cn[i]) - rpc, 12) !== t.semis[i]) ok = false;
          }
          if (!ok) continue;
          const third = imod(pitchClass(cn[1]) - rpc, 12);
          const fifth = imod(pitchClass(cn[2]) - rpc, 12);
          let q = 0;
          if (third === 3 && fifth === 6) q = 2;
          else if (fifth === 8) q = 3;
          else if (third === 3) q = 1;
          hits.push({ slot, minor: m !== 0, degree: d, quality: q });
        }
      }
    }
    return hits;
  };

  const dominantTarget = (root, ti) => {
    if (ti !== 4 && ti !== 13) return null;
    return pcToNote(imod(pitchClass(root) + 5, 12), root.acc < 0);
  };

  const INTERVALS = ['1', 'b2', '2', 'b3', '3', '4', 'b5', '5', '#5', '6', 'b7', '7'];
  const intervalName = (s) => INTERVALS[imod(s, 12)];

  // ---- scales -----------------------------------------------------------

  const spellScale = (root, ti) => {
    const t = SCALE_TYPES[ti];
    const base = pitchClass(root);
    const out = [];
    for (let i = 0; i < t.n; i++) {
      const letter = imod(root.letter + t.degrees[i], 7);
      const target = imod(base + t.semis[i], 12);
      let diff = target - NAT[letter];
      while (diff > 6) diff -= 12;
      while (diff < -6) diff += 12;
      out.push(note(letter, diff));
    }
    return out;
  };

  const degreeLabel = (n, degOffset) => {
    let s = '';
    for (let i = 0; i < Math.abs(n.acc); i++) s += n.acc < 0 ? 'b' : '#';
    return s + (degOffset + 1);
  };

  return {
    imod, note, pitchClass, noteName, majorTonic, signature, scaleOf, diatonicChords,
    qualitySuffix, chordName, romanNumeral, relativeMinorOf, relativeMajorOf,
    CHORD_TYPES, SCALE_TYPES, spellChord, parseChord, formatChord, pcToNote,
    findChordInKeys, dominantTarget, intervalName, spellScale, degreeLabel,
  };
})();

if (typeof module !== 'undefined') module.exports = MT;

