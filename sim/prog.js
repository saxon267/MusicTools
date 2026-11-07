// Progression library for the simulator.
// Mirrors firmware/src/progression.cpp - keep the two in sync.

const PROG = (() => {
  const Q = { maj: '', min: 'm', dom7: '7', maj7: 'maj7', min7: 'm7', dim: 'dim' };

  const step = (degree, acc, qual) => ({ degree, acc, qual });

  const STYLES = [
    {
      name: 'POP',
      items: [
        { name: 'I-V-vi-IV', minor: false, steps: [step(0, 0, 'maj'), step(4, 0, 'maj'), step(5, 0, 'min'), step(3, 0, 'maj')] },
        { name: 'vi-IV-I-V', minor: false, steps: [step(5, 0, 'min'), step(3, 0, 'maj'), step(0, 0, 'maj'), step(4, 0, 'maj')] },
        { name: 'I-vi-IV-V', minor: false, steps: [step(0, 0, 'maj'), step(5, 0, 'min'), step(3, 0, 'maj'), step(4, 0, 'maj')] },
        { name: 'IV-I-V-vi', minor: false, steps: [step(3, 0, 'maj'), step(0, 0, 'maj'), step(4, 0, 'maj'), step(5, 0, 'min')] },
      ],
    },
    {
      name: 'ROCK',
      items: [
        { name: 'I-bVII-IV', minor: false, steps: [step(0, 0, 'maj'), step(6, -1, 'maj'), step(3, 0, 'maj')] },
        { name: 'i-bVI-bIII-bVII', minor: true, steps: [step(0, 0, 'min'), step(5, 0, 'maj'), step(2, 0, 'maj'), step(6, 0, 'maj')] },
        { name: 'I-IV-V', minor: false, steps: [step(0, 0, 'maj'), step(3, 0, 'maj'), step(4, 0, 'maj')] },
      ],
    },
    {
      name: 'JAZZ',
      items: [
        { name: 'ii-V-I', minor: false, steps: [step(1, 0, 'min'), step(4, 0, 'dom7'), step(0, 0, 'maj7')] },
        { name: 'I-vi-ii-V', minor: false, steps: [step(0, 0, 'maj7'), step(5, 0, 'min7'), step(1, 0, 'min7'), step(4, 0, 'dom7')] },
        { name: 'iii-vi-ii-V', minor: false, steps: [step(2, 0, 'min7'), step(5, 0, 'min7'), step(1, 0, 'min7'), step(4, 0, 'dom7')] },
        { name: 'i-iv-V7-i', minor: true, steps: [step(0, 0, 'min'), step(3, 0, 'min'), step(4, 0, 'dom7'), step(0, 0, 'min')] },
      ],
    },
    {
      name: 'BLUES',
      items: [
        {
          name: '12-bar blues',
          minor: false,
          steps: [
            step(0, 0, 'dom7'), step(0, 0, 'dom7'), step(0, 0, 'dom7'), step(0, 0, 'dom7'),
            step(3, 0, 'dom7'), step(3, 0, 'dom7'), step(0, 0, 'dom7'), step(0, 0, 'dom7'),
            step(4, 0, 'dom7'), step(3, 0, 'dom7'), step(0, 0, 'dom7'), step(4, 0, 'dom7'),
          ],
        },
        {
          name: '8-bar blues',
          minor: false,
          steps: [
            step(0, 0, 'dom7'), step(0, 0, 'dom7'), step(3, 0, 'dom7'), step(0, 0, 'dom7'),
            step(4, 0, 'dom7'), step(3, 0, 'dom7'), step(0, 0, 'dom7'), step(4, 0, 'dom7'),
          ],
        },
      ],
    },
    {
      name: 'CLASSICAL',
      items: [
        { name: 'I-IV-V-I', minor: false, steps: [step(0, 0, 'maj'), step(3, 0, 'maj'), step(4, 0, 'maj'), step(0, 0, 'maj')] },
        { name: 'i-iv-V-i', minor: true, steps: [step(0, 0, 'min'), step(3, 0, 'min'), step(4, 0, 'maj'), step(0, 0, 'min')] },
        {
          name: 'Pachelbel',
          minor: false,
          steps: [
            step(0, 0, 'maj'), step(4, 0, 'maj'), step(5, 0, 'min'), step(2, 0, 'min'),
            step(3, 0, 'maj'), step(0, 0, 'maj'), step(3, 0, 'maj'), step(4, 0, 'maj'),
          ],
        },
      ],
    },
    {
      name: 'FOLK',
      items: [
        { name: 'i-VII-VI-VII', minor: true, steps: [step(0, 0, 'min'), step(6, 0, 'maj'), step(5, 0, 'maj'), step(6, 0, 'maj')] },
        { name: 'i-VI-III-VII', minor: true, steps: [step(0, 0, 'min'), step(5, 0, 'maj'), step(2, 0, 'maj'), step(6, 0, 'maj')] },
        { name: 'I-ii-I-V', minor: false, steps: [step(0, 0, 'maj'), step(1, 0, 'min'), step(0, 0, 'maj'), step(4, 0, 'maj')] },
      ],
    },
    { name: 'CUSTOM', items: [] },
  ];

  const DIATONIC = ['maj', 'min', 'min', 'maj', 'maj', 'min', 'dim'];

  const custom = [
    { degree: 0, acc: 0, qual: 'maj' },
    { degree: 4, acc: 0, qual: 'maj' },
    { degree: 5, acc: 0, qual: 'min' },
    { degree: 3, acc: 0, qual: 'maj' },
  ];

  const chordOf = (keyTonic, keyMinor, s) => {
    const scale = MT.scaleOf(keyTonic, keyMinor);
    const root = scale[s.degree % 7];
    const n = MT.note(root.letter, root.acc + s.acc);
    return MT.noteName(n) + Q[s.qual];
  };

  const romanOf = (s, keyMinor) => {
    const r = MT.romanNumeral(s.degree % 7, keyMinor);
    return (s.acc < 0 ? 'b' : s.acc > 0 ? '#' : '') + r;
  };

  return { STYLES, custom, chordOf, romanOf, DIATONIC, Q };
})();

if (typeof module !== 'undefined') module.exports = PROG;

