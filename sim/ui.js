// Canvas renderer for the simulator.
// Layout constants and drawing order mirror firmware/src/ui.cpp.

const UI = (() => {
  const W = 240;
  const H = 135;
  const RAD = Math.PI / 180;

  // older Safari lacks roundRect
  if (typeof CanvasRenderingContext2D !== 'undefined' && !CanvasRenderingContext2D.prototype.roundRect) {
    CanvasRenderingContext2D.prototype.roundRect = function (x, y, w, h, r) {
      const rad = Math.min(r, w / 2, h / 2);
      this.beginPath();
      this.moveTo(x + rad, y);
      this.arcTo(x + w, y, x + w, y + h, rad);
      this.arcTo(x + w, y + h, x, y + h, rad);
      this.arcTo(x, y + h, x, y, rad);
      this.arcTo(x, y, x + w, y, rad);
      this.closePath();
      return this;
    };
  }

  const COL = {
    bg: '#0e1116',
    fg: '#e8ecf1',
    dim: '#84909e',
    line: '#282f3a',
    selBg: '#262e3a',
    hub: '#14181e',
    ink: '#101216',
    chordDim: '#788494',
    accent: '#96b2d0',
    footer: '#687484',
    panelChord: '#d2dae4',
    minorLabel: '#e4e8ee',
  };

  const F_TINY = '8px "Lucida Console", Consolas, monospace'; // Font0
  const F_MID = '14px "Segoe UI", Arial, sans-serif'; // Font2
  const F_BIG = '25px "Segoe UI", Arial, sans-serif'; // Font4
  const F_CHORD = '12px "Segoe UI", Arial, sans-serif'; // FreeSans9pt7b

  const C = { ctx: null };

  const hsv = (h, s, v) => {
    const c = v * s;
    const hp = (h % 360) / 60;
    const x = c * (1 - Math.abs((hp % 2) - 1));
    let r = 0, g = 0, b = 0;
    switch (Math.floor(hp)) {
      case 0: r = c; g = x; break;
      case 1: r = x; g = c; break;
      case 2: g = c; b = x; break;
      case 3: g = x; b = c; break;
      case 4: r = x; b = c; break;
      default: r = c; b = x; break;
    }
    const m = v - c;
    const to = (q) => Math.round((q + m) * 255);
    return `rgb(${to(r)},${to(g)},${to(b)})`;
  };

  const fillSeg = (cx, cy, r0, r1, a0, a1, color) => {
    const d = C.ctx;
    d.beginPath();
    d.arc(cx, cy, r1, a0 * RAD, a1 * RAD);
    d.arc(cx, cy, r0, a1 * RAD, a0 * RAD, true);
    d.closePath();
    d.fillStyle = color;
    d.fill();
  };

  const outlineSeg = (cx, cy, r0, r1, a0, a1, color) => {
    const d = C.ctx;
    d.strokeStyle = color;
    d.lineWidth = 1.5;
    d.beginPath();
    d.arc(cx, cy, r1, a0 * RAD, a1 * RAD);
    d.stroke();
    d.beginPath();
    d.arc(cx, cy, r0, a0 * RAD, a1 * RAD);
    d.stroke();
    for (const a of [a0, a1]) {
      d.beginPath();
      d.moveTo(cx + r0 * Math.cos(a * RAD), cy + r0 * Math.sin(a * RAD));
      d.lineTo(cx + r1 * Math.cos(a * RAD), cy + r1 * Math.sin(a * RAD));
      d.stroke();
    }
  };

  const setFont = (f) => {
    C.ctx.font = f;
  };

  const textMid = (x, y, s, font, color) => {
    const d = C.ctx;
    d.font = font;
    d.fillStyle = color;
    d.textAlign = 'center';
    d.textBaseline = 'middle';
    d.fillText(s, x, y);
  };

  const textTL = (x, y, s, font, color) => {
    const d = C.ctx;
    d.font = font;
    d.fillStyle = color;
    d.textAlign = 'left';
    d.textBaseline = 'top';
    d.fillText(s, x, y);
  };

  const textML = (x, y, s, font, color) => {
    const d = C.ctx;
    d.font = font;
    d.fillStyle = color;
    d.textAlign = 'left';
    d.textBaseline = 'middle';
    d.fillText(s, x, y);
  };

  const measure = (s, font) => {
    const d = C.ctx;
    d.font = font;
    return d.measureText(s).width;
  };

  // ---------------------------------------------------------------- page 0

  const CX = 68, CY = 68, R_OUT = 53, R_MAJ_IN = 38, R_MIN_IN = 24, R_HUB = 22, PANEL_X = 132;

  const drawWheel = (st) => {
    const d = C.ctx;
    for (let i = 0; i < 12; i++) {
      const sel = i === st.slot;
      const a0 = -105 + 30 * i + 0.7;
      const a1 = a0 + 30 - 1.4;

      fillSeg(CX, CY, R_MAJ_IN, R_OUT, a0, a1, sel ? hsv(i * 30, 0.95, 0.96) : hsv(i * 30, 0.45, 0.9));
      fillSeg(CX, CY, R_MIN_IN, R_MAJ_IN, a0, a1, sel ? hsv(i * 30, 0.85, 0.6) : hsv(i * 30, 0.42, 0.5));
      if (sel) outlineSeg(CX, CY, R_MAJ_IN, R_OUT, a0, a1, '#ffffff');

      const am = ((a0 + a1) / 2) * RAD;
      const maj = MT.noteName(MT.majorTonic(i));
      textMid(CX + 45.5 * Math.cos(am), CY + 45.5 * Math.sin(am), maj, F_TINY, sel ? '#fff' : COL.ink);

      const min = MT.noteName(MT.relativeMinorOf(MT.majorTonic(i))).toLowerCase();
      textMid(CX + 31 * Math.cos(am), CY + 31 * Math.sin(am), min, F_TINY, sel ? '#fff' : COL.minorLabel);
    }

    d.fillStyle = COL.hub;
    d.beginPath();
    d.arc(CX, CY, R_HUB, 0, Math.PI * 2);
    d.fill();

    let tonic = MT.majorTonic(st.slot);
    if (st.minorView) tonic = MT.relativeMinorOf(tonic);
    let name = MT.noteName(tonic) + (st.minorView ? 'm' : '');
    textMid(CX, CY - 4, name, F_BIG, COL.fg);

    const sig = MT.signature(st.slot);
    textMid(CX, CY + 11, sig > 0 ? `${sig}#` : sig < 0 ? `${-sig}b` : '0', F_TINY, COL.dim);
  };

  const drawPanel = (st) => {
    const d = C.ctx;
    let tonic = MT.majorTonic(st.slot);
    if (st.minorView) tonic = MT.relativeMinorOf(tonic);
    const sc = MT.scaleOf(tonic, st.minorView);
    const chords = MT.diatonicChords(tonic, st.minorView);

    const header = `${MT.noteName(tonic)}${st.minorView ? 'm' : ''} ${st.minorView ? 'MINOR' : 'MAJOR'}`;
    textTL(PANEL_X + 2, 13, header, F_MID, COL.fg);

    // the tab bar costs 12 px, so the scale line lives on the scale page now
    for (let i = 0; i < 7; i++) {
      const y = 32 + i * 13;
      const sel = i === st.selDegree;
      if (sel) {
        d.fillStyle = COL.selBg;
        d.beginPath();
        d.roundRect(PANEL_X + 1, y - 1, 102, 12, 3);
        d.fill();
      }
      textTL(PANEL_X + 2, y + 1, MT.romanNumeral(i, st.minorView), F_TINY, sel ? COL.fg : COL.dim);

      if (chords[i].q === 'Dim') {
        const w = measure(MT.romanNumeral(i, st.minorView), F_TINY);
        d.strokeStyle = sel ? COL.fg : COL.dim;
        d.beginPath();
        d.arc(PANEL_X + 3 + w + 2, y + 4, 1.2, 0, Math.PI * 2);
        d.stroke();
      }

      textML(PANEL_X + 25, y + 6, MT.chordName(chords[i]), F_CHORD, sel ? COL.fg : COL.panelChord);

      const notes = [0, 1, 2].map((k) => MT.noteName(sc[(i + k * 2) % 7])).join(' ');
      textTL(PANEL_X + 58, y + 1, notes, F_TINY, COL.chordDim);
    }

    d.strokeStyle = COL.line;
    d.lineWidth = 1;
    d.beginPath();
    d.moveTo(PANEL_X - 4, 4);
    d.lineTo(PANEL_X - 4, 122);
    d.stroke();
  };

  // ---------------------------------------------------------------- page 1

  const drawChordPage = (st) => {
    const d = C.ctx;
    if (!st.chordValid) {
      textTL(6, 16, '?', F_BIG, COL.dim);
      textTL(6, 52, 'unknown chord', F_TINY, COL.dim);
      textTL(6, 64, 'keep typing', F_TINY, COL.dim);
      textTL(6, 100, 'ENTER: play', F_TINY, COL.footer);
      return;
    }

    const t = MT.CHORD_TYPES[st.chordType];
    const big = measure(st.chordText, F_BIG) <= 104;
    textTL(6, 13, st.chordText, big ? F_BIG : F_MID, COL.fg);
    textTL(6, big ? 46 : 38, t.label, F_TINY, COL.dim);

    const notes = MT.spellChord(st.chordRoot, st.chordType).map(MT.noteName);
    textTL(6, 58, notes.join('  '), F_TINY, COL.fg);
    textTL(6, 68, t.semis.map(MT.intervalName).join('  '), F_TINY, COL.dim);

    const target = MT.dominantTarget(st.chordRoot, st.chordType);
    if (target) textTL(6, 82, `V7 of ${MT.noteName(target)}`, F_TINY, COL.accent);

    textTL(6, 100, 'ENTER: play', F_TINY, COL.footer);
    textTL(6, 110, 'BACKSPACE: edit', F_TINY, '#545e6c');

    d.strokeStyle = COL.line;
    d.beginPath();
    d.moveTo(120, 14);
    d.lineTo(120, 122);
    d.stroke();

    textTL(124, 14, 'IN KEYS', F_TINY, COL.dim);

    const hits = MT.findChordInKeys(st.chordRoot, st.chordType);
    if (!hits.length) {
      textTL(124, 20, 'not diatonic', F_TINY, COL.dim);
      textTL(124, 30, 'in any key', F_TINY, COL.dim);
      return;
    }

    const shown = Math.min(hits.length, 7);
    for (let i = 0; i < shown; i++) {
      const y = 26 + i * 13;
      let tonic = MT.majorTonic(hits[i].slot);
      if (hits[i].minor) tonic = MT.relativeMinorOf(tonic);
      textTL(124, y, MT.noteName(tonic), F_TINY, COL.fg);
      textTL(142, y, hits[i].minor ? 'min' : 'maj', F_TINY, COL.dim);

      const rom = MT.romanNumeral(hits[i].degree, hits[i].minor);
      textTL(168, y, rom, F_TINY, COL.fg);
      let w = measure(rom, F_TINY);
      if (hits[i].quality === 2) {
        d.strokeStyle = COL.fg;
        d.beginPath();
        d.arc(168 + w + 2, y + 4, 1.2, 0, Math.PI * 2);
        d.stroke();
        w += 4;
      }
      if (t.n === 4) textTL(168 + w + 2, y, '7', F_TINY, COL.fg);
    }

    if (hits.length > shown) textTL(124, 16 + shown * 13, `+${hits.length - shown} more`, F_TINY, COL.dim);
  };

  // ---------------------------------------------------------------- page 2

  const drawChordBox = (x, y, frets) => {
    const d = C.ctx;
    const dx = 10, dy = 12, n = 5;
    let mn = 99, firstSounding = -1;
    for (let i = 0; i < 6; i++) {
      if (frets[i] >= 0 && firstSounding < 0) firstSounding = i;
      if (frets[i] > 0 && frets[i] < mn) mn = frets[i];
    }
    if (firstSounding < 0) return;

    const first = mn > 3 ? mn : 1;

    d.strokeStyle = '#404856';
    d.lineWidth = 1;
    for (let i = 0; i < 6; i++) {
      d.beginPath();
      d.moveTo(x + i * dx, y);
      d.lineTo(x + i * dx, y + n * dy);
      d.stroke();
    }
    for (let j = 0; j <= n; j++) {
      d.beginPath();
      d.moveTo(x, y + j * dy);
      d.lineTo(x + 5 * dx, y + j * dy);
      d.stroke();
    }

    if (first === 1) {
      d.fillStyle = '#96a0b0';
      d.fillRect(x - 1, y - 1, 5 * dx + 2, 3);
    } else {
      textTL(x - 15, y + 1, `${first}fr`, F_TINY, COL.dim);
    }

    for (let i = 0; i < 6; i++) {
      const cx = x + i * dx;
      if (frets[i] === 0) {
        d.strokeStyle = COL.dim;
        d.beginPath();
        d.arc(cx, y - 8, 2, 0, Math.PI * 2);
        d.stroke();
      } else if (frets[i] < 0) {
        d.strokeStyle = COL.dim;
        d.beginPath();
        d.moveTo(cx - 2, y - 10);
        d.lineTo(cx + 2, y - 6);
        d.moveTo(cx + 2, y - 10);
        d.lineTo(cx - 2, y - 6);
        d.stroke();
      }
    }

    const sounding = frets.filter((f) => f >= 0);
    const allAbove = sounding.every((f) => f >= mn);
    const atFret = frets.filter((f) => f === mn).length;
    if (mn > 0 && allAbove && atFret >= 2 && frets[firstSounding] === mn) {
      let last = firstSounding;
      for (let i = firstSounding; i < 6; i++) if (frets[i] >= 0) last = i;
      const cy = y + (mn - first) * dy + dy / 2;
      d.fillStyle = COL.fg;
      d.beginPath();
      d.roundRect(x + firstSounding * dx - 3, cy - 3, (last - firstSounding) * dx + 7, 7, 3);
      d.fill();
    }

    d.fillStyle = COL.fg;
    for (let i = 0; i < 6; i++) {
      if (frets[i] <= 0) continue;
      d.beginPath();
      d.arc(x + i * dx, y + (frets[i] - first) * dy + dy / 2, 3, 0, Math.PI * 2);
      d.fill();
    }
  };

  const shapeCache = { key: '', list: [] };

  const drawGuitarPage = (st) => {
    if (!st.chordValid) {
      textTL(6, 16, '?', F_BIG, COL.dim);
      textTL(6, 52, 'unknown chord', F_TINY, COL.dim);
      return;
    }
    const t = MT.CHORD_TYPES[st.chordType];
    const pc = MT.pitchClass(st.chordRoot);
    const key = `${pc}:${st.chordType}`;
    if (shapeCache.key !== key) {
      shapeCache.key = key;
      shapeCache.list = GUITAR.shapes(pc, st.chordType, 6);
    }
    const shapes = shapeCache.list;

    textTL(6, 13, st.chordText, F_MID, COL.fg);
    const sw = measure(st.chordText, F_MID);
    textTL(6 + sw + 6, 18, t.label, F_TINY, COL.dim);

    const perPage = 3;
    const pages = Math.ceil(shapes.length / perPage) || 1;
    const page = st.gtrPage % pages;
    if (shapes.length) {
      const from = page * perPage + 1;
      const to = Math.min(page * perPage + perPage, shapes.length);
      const vb = `${from}-${to}/${shapes.length}`;
      textTL(W - 5 - measure(vb, F_TINY), 17, vb, F_TINY, COL.dim);
    }

    if (!shapes.length) {
      textTL(30, 56, 'no playable shape', F_TINY, COL.dim);
      textTL(30, 68, 'for this chord', F_TINY, COL.dim);
      return;
    }

    const xs = [30, 98, 166];
    for (let k = 0; k < perPage; k++) {
      const i = page * perPage + k;
      if (i >= shapes.length) break;
      drawChordBox(xs[k], 38, shapes[i]);
    }

    const notes = MT.spellChord(st.chordRoot, st.chordType).map(MT.noteName).join('  ');
    textMid(W / 2, 110, notes, F_TINY, COL.dim);
  };

  // ---------------------------------------------------------------- page 3

  const drawScalePage = (st) => {
    const t = MT.SCALE_TYPES[st.scaleType];
    const notes = MT.spellScale(st.scaleRoot, st.scaleType);

    textTL(6, 13, `${MT.noteName(st.scaleRoot)} ${t.name}`, F_MID, COL.fg);
    const idx = `${st.scaleType + 1}/${MT.SCALE_TYPES.length}`;
    textTL(W - 5 - measure(idx, F_TINY), 17, idx, F_TINY, COL.dim);

    const colW = 220 / t.n;
    for (let i = 0; i < t.n; i++) {
      const cx = 10 + colW * i + colW / 2;
      // every pitch class owns a hue, so the scale reads as a slice of a rainbow
      textMid(cx, 44, MT.noteName(notes[i]), F_MID, hsv(MT.pitchClass(notes[i]) * 30, 0.55, 0.95));
      textMid(cx, 56, MT.degreeLabel(notes[i], t.degrees[i]), F_TINY, COL.dim);
    }

    let flats = 0, sharps = 0;
    for (const n of notes) {
      if (n.acc > 0) sharps += n.acc;
      else flats -= n.acc;
    }
    const prefFlat = flats > sharps;

    const d = C.ctx;
    for (let pc = 0; pc < 12; pc++) {
      const x = 10 + pc * 18;
      let deg = -1;
      for (let i = 0; i < t.n; i++) if (MT.pitchClass(notes[i]) === pc) deg = i;

      if (deg >= 0) {
        d.fillStyle = hsv(pc * 30, 0.82, 0.72);
        d.beginPath();
        d.roundRect(x, 62, 16, 26, 2);
        d.fill();
      } else {
        d.strokeStyle = hsv(pc * 30, 0.45, 0.2);
        d.lineWidth = 1;
        d.beginPath();
        d.roundRect(x, 62, 16, 26, 2);
        d.stroke();
      }

      const label = deg >= 0 ? MT.noteName(notes[deg]) : MT.noteName(MT.pcToNote(pc, prefFlat));
      textMid(x + 8, 70, label, F_TINY, deg >= 0 ? COL.ink : '#58626f');
      if (deg >= 0) textMid(x + 8, 81, MT.degreeLabel(notes[deg], t.degrees[deg]), '7px Consolas, monospace', COL.ink);
    }

    const steps = [];
    for (let i = 0; i < t.n; i++) {
      steps.push(i + 1 < t.n ? t.semis[i + 1] - t.semis[i] : 12 - t.semis[i]);
    }
    textMid(W / 2, 96, steps.join(' '), F_TINY, COL.dim);
  };

  // ----------------------------------------------------------------

  const drawTabs = (st) => {
    const names = ['CIRCLE', 'CHORD', 'GUITAR', 'SCALE'];
    const d = C.ctx;
    for (let i = 0; i < 4; i++) {
      const x = 4 + i * 58;
      const on = i === st.page;
      if (on) {
        d.fillStyle = COL.selBg;
        d.beginPath();
        d.roundRect(x, 2, 56, 9, 2);
        d.fill();
      }
      const w = measure(names[i], F_TINY);
      textTL(x + (56 - w) / 2, 3, names[i], F_TINY, on ? COL.fg : '#58626f');
    }
    d.strokeStyle = COL.line;
    d.beginPath();
    d.moveTo(0, 12);
    d.lineTo(W, 12);
    d.stroke();
  };

  const drawVolumeBar = (st) => {
    if (!st.showVolume) return;
    const d = C.ctx;
    d.fillStyle = '#1a202a';
    d.beginPath();
    d.roundRect(8, 98, 224, 20, 5);
    d.fill();
    d.strokeStyle = '#3c485a';
    d.lineWidth = 1;
    d.stroke();

    textTL(14, 104, 'VOL', F_TINY, COL.dim);

    d.fillStyle = '#12161c';
    d.beginPath();
    d.roundRect(46, 104, 130, 8, 4);
    d.fill();
    const w = Math.round((130 * st.volume) / 100);
    if (w > 0) {
      d.fillStyle = COL.accent;
      d.beginPath();
      d.roundRect(46, 104, w, 8, Math.min(4, w / 2));
      d.fill();
    }

    const b = `${st.volume}%`;
    textTL(W - 12 - measure(b, F_TINY), 104, b, F_TINY, COL.fg);
  };

  const drawFooter = (st) => {
    const hints = [
      'TAB chord  Fn+<> key  ^v  ENTER',
      'TAB guitar  Fn+<> root  ^v type',
      'TAB scale  Fn+<> voice  ^v type',
      'TAB circle  Fn+<> root ^v ENTER',
    ];
    const d = C.ctx;
    d.strokeStyle = COL.line;
    d.beginPath();
    d.moveTo(4, 124);
    d.lineTo(W - 4, 124);
    d.stroke();

    textTL(5, 125, hints[st.page % 4], F_TINY, COL.footer);

    const bat = st.battery < 0 ? '--' : `${st.battery}%`;
    const w = measure(bat, F_TINY);
    textTL(W - 5 - w, 125, bat, F_TINY, st.playing ? '#78c88c' : COL.footer);
  };

  const render = (st, canvas) => {
    if (!C.ctx) C.ctx = canvas.getContext('2d');
    const d = C.ctx;
    d.imageSmoothingEnabled = false;
    d.fillStyle = COL.bg;
    d.fillRect(0, 0, W, H);

    drawTabs(st);
    if (st.page === 0) {
      drawWheel(st);
      drawPanel(st);
    } else if (st.page === 1) drawChordPage(st);
    else if (st.page === 2) drawGuitarPage(st);
    else drawScalePage(st);

    drawVolumeBar(st);
    drawFooter(st);
  };

  return { render, W, H };
})();

if (typeof module !== 'undefined') module.exports = UI;









