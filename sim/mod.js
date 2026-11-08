// Modulation helper for the simulator.
// Mirrors firmware/src/modulation.cpp - keep the two in sync.

const MOD = (() => {
  const QUAL_MAJOR = [0, 1, 1, 0, 0, 1, 2];
  const QUAL_MINOR = [1, 2, 0, 1, 1, 0, 0];

  const analyse = (fromSlot, toSlot, toMinor) => {
    const fromTonic = MT.majorTonic(fromSlot);
    let toTonic = MT.majorTonic(toSlot);
    if (toMinor) toTonic = MT.relativeMinorOf(toTonic);

    let diff = ((toSlot - fromSlot) % 12 + 12) % 12;
    if (diff > 6) diff -= 12;

    let relation = 'FAR';
    if (diff === 0) relation = toMinor ? 'REL MINOR' : 'SAME KEY';
    else if (diff === 1) relation = 'DOMINANT';
    else if (diff === -1) relation = 'SUBDOMINANT';
    else if (diff === 2) relation = 'UP A 2ND';
    else if (diff === -2) relation = 'DOWN A 2ND';

    const fromScale = MT.scaleOf(fromTonic, false);
    const toScale = MT.scaleOf(toTonic, toMinor);
    const pivots = [];
    for (let a = 0; a < 7; a++) {
      for (let b = 0; b < 7; b++) {
        if (MT.pitchClass(fromScale[a]) !== MT.pitchClass(toScale[b])) continue;
        if (QUAL_MAJOR[a] !== (toMinor ? QUAL_MINOR[b] : QUAL_MAJOR[b])) continue;
        pivots.push({ root: toScale[b], qual: toMinor ? QUAL_MINOR[b] : QUAL_MAJOR[b], fromDeg: a, toDeg: b });
      }
    }

    return {
      distance: diff,
      relation,
      toTonic,
      toMinor,
      pivots,
      domRoot: toScale[4],
      iiRoot: toScale[1],
    };
  };

  const chordName = (root, qual) => MT.noteName(root) + ['', 'm', 'dim'][qual % 3];

  return { analyse, chordName };
})();

if (typeof module !== 'undefined') module.exports = MOD;
