// Modulation helper for the simulator.
// Mirrors firmware/src/modulation.cpp - keep the two in sync.

const MOD = (() => {
  const QUAL_MAJOR = [0, 1, 1, 0, 0, 1, 2];
  const QUAL_MINOR = [1, 2, 0, 1, 1, 0, 0];

  const analyse = (fromSlot, fromMinor, toSlot, toMinor) => {
    let fromTonic = MT.majorTonic(fromSlot);
    if (fromMinor) fromTonic = MT.relativeMinorOf(fromTonic);
    let toTonic = MT.majorTonic(toSlot);
    if (toMinor) toTonic = MT.relativeMinorOf(toTonic);

    let diff = ((toSlot - fromSlot) % 12 + 12) % 12;
    if (diff > 6) diff -= 12;

    let relation = 'FAR';
    if (diff === 0) relation = fromMinor === toMinor ? 'SAME KEY' : toMinor ? 'REL MINOR' : 'REL MAJOR';
    else if (diff === 1) relation = 'DOMINANT';
    else if (diff === -1) relation = 'SUBDOMINANT';
    else if (diff === 2) relation = 'UP A 2ND';
    else if (diff === -2) relation = 'DOWN A 2ND';

    const fromScale = MT.scaleOf(fromTonic, fromMinor);
    const toScale = MT.scaleOf(toTonic, toMinor);
    const pivots = [];
    for (let a = 0; a < 7; a++) {
      for (let b = 0; b < 7; b++) {
        if (MT.pitchClass(fromScale[a]) !== MT.pitchClass(toScale[b])) continue;
        const fromQ = fromMinor ? QUAL_MINOR[a] : QUAL_MAJOR[a];
        const toQ = toMinor ? QUAL_MINOR[b] : QUAL_MAJOR[b];
        if (fromQ !== toQ) continue;
        pivots.push({ root: toScale[b], qual: toQ, fromDeg: a, toDeg: b });
      }
    }

    return {
      distance: diff,
      relation,
      fromTonic,
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
