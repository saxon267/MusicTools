// Application state and input for the simulator.
// Mirrors the state machine in firmware/src/main.cpp.

const App = (() => {
  const st = {
    page: 0,
    slot: 0,
    minorView: false,
    selDegree: 4,
    chordText: 'C',
    chordRoot: MT.note(0, 0),
    chordType: 0,
    chordValid: true,
    gtrPage: 0,
    scaleRoot: MT.note(0, 0),
    scaleType: 0,
    battery: 72,
    splash: false,
    bpm: 100,
    metroPattern: 0,
    metroOn: false,
    beat: -1,
    progView: 0,
    progStyle: 0,
    progSel: 0,
    progPos: 0,
    progPlaying: false,
    progEdit: false,
    progSlot: 0,
    progMinor: false,
    progSteps: [],
    progName: '',
    modTo: 1,
    modMinor: false,

    volume: 100,
    showVolume: false,
    playing: false,
  };

  let canvas = null;

  // ---------------------------------------------------------------- audio

  const audio = { ctx: null, master: null, nodes: [], timer: null };
  let volTimer = null;

  // ---- metronome -------------------------------------------------------
  const metro = { timer: null };
  const clickSound = (accent) => {
    const ctx = ensureAudio();
    const t = ctx.currentTime;
    const osc = ctx.createOscillator();
    const g = ctx.createGain();
    osc.frequency.value = accent ? 2000 : 1350;
    g.gain.setValueAtTime(accent ? 0.22 : 0.14, t);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.05);
    osc.connect(g);
    g.connect(ctx.destination);
    osc.start(t);
    osc.stop(t + 0.06);
  };

  const stopMetro = () => {
    if (metro.timer) clearInterval(metro.timer);
    metro.timer = null;
    st.metroOn = false;
    st.beat = -1;
  };

  const progStop = () => {
    stopMetro();
    st.progPlaying = false;
    st.progPos = 0;
  };

  const progStart = () => {
    if (!st.progSteps.length) return;
    st.progPos = 0;
    st.progPlaying = true;
    startMetro();
  };

  const progLoadPreset = (style, idx) => {
    const item = PROG.STYLES[style].items[idx];
    st.progSteps = item.steps;
    st.progName = item.name;
    st.progMinor = item.minor;
    st.progStyle = style;
    st.progSel = idx;
    st.progView = 2;
    st.progEdit = false;
    st.progPos = 0;
  };

  const progLoadCustom = () => {
    st.progSteps = PROG.custom;
    st.progName = 'CUSTOM';
    st.progMinor = false;
    st.progStyle = PROG.STYLES.length - 1;
    st.progView = 2;
    st.progEdit = true;
    st.progPos = 0;
    st.progSlot = 0;
  };

  const progBack = () => {
    if (st.progView === 2) {
      progStop();
      st.progView = st.progEdit ? 0 : 1;
    } else if (st.progView === 1) {
      st.progView = 0;
    }
  };

  const progMove = (delta) => {
    if (st.progView === 0) {
      st.progStyle = (st.progStyle + delta + PROG.STYLES.length) % PROG.STYLES.length;
    } else if (st.progView === 1) {
      const n = PROG.STYLES[st.progStyle].items.length;
      if (n) st.progSel = (st.progSel + delta + n) % n;
    } else if (st.progEdit) {
      const step = PROG.custom[st.progSlot];
      step.degree = (step.degree + delta + 7) % 7;
      step.acc = 0;
      step.qual = PROG.DIATONIC[step.degree];
    }
    refresh();
  };

  const progEnter = () => {
    if (st.progView === 0) {
      if (st.progStyle === PROG.STYLES.length - 1) progLoadCustom();
      else {
        st.progView = 1;
        st.progSel = 0;
      }
    } else if (st.progView === 1) {
      progLoadPreset(st.progStyle, st.progSel);
    } else if (st.progPlaying) {
      progStop();
    } else {
      progStart();
    }
    refresh();
  };

  const playProgChord = (rootPc, typeIdx) => {
    const ctx = ensureAudio();
    const t = MT.CHORD_TYPES[typeIdx];
    const master = ctx.createGain();
    master.gain.value = gainOf() * 0.8;
    master.connect(ctx.destination);
    const now = ctx.currentTime;
    master.gain.setValueAtTime(gainOf() * 0.8, now);
    master.gain.setValueAtTime(gainOf() * 0.8, now + 0.55);
    master.gain.exponentialRampToValueAtTime(0.0001, now + 0.85);
    for (let i = 0; i < t.n; i++) {
      const midi = 48 + rootPc + t.semis[i];
      const osc = ctx.createOscillator();
      const g = ctx.createGain();
      osc.type = 'sine';
      osc.frequency.value = 440 * Math.pow(2, (midi - 69) / 12);
      g.gain.value = 0.9 / (t.n + 1);
      osc.connect(g);
      g.connect(master);
      osc.start(now);
      osc.stop(now + 0.9);
    }
  };

  const startMetro = () => {
    stopMetro();
    st.metroOn = true;
    st.beat = -1;
    const tick = () => {
      const p = UI.METRO[st.metroPattern];
      st.beat = (st.beat + 1) % p.beats;
      clickSound((p.accents >> st.beat) & 1);

      if (st.progPlaying && st.beat === 0 && st.progSteps.length) {
        let tonic = MT.majorTonic(st.slot);
        if (st.progMinor) tonic = MT.relativeMinorOf(tonic);
        const scale = MT.scaleOf(tonic, st.progMinor);
        const step = st.progSteps[st.progPos];
        const root = scale[step.degree % 7];
        const n = MT.note(root.letter, root.acc + step.acc);
        const typeIdx = { maj: 0, min: 1, dom7: 4, maj7: 5, min7: 6, dim: 2 }[step.qual];
        playProgChord(MT.pitchClass(n), typeIdx);
        st.progPos = (st.progPos + 1) % st.progSteps.length;
      }
      refresh();
    };
    tick();
    metro.timer = setInterval(tick, 60000 / st.bpm);
  };

  // ---------------------------------------------------------------- helpers

  const rebuildText = () => {
    st.chordText = MT.formatChord(st.chordRoot, st.chordType);
    st.chordValid = true;
    st.gtrPage = 0;
  };

  const reparse = () => {
    const p = MT.parseChord(st.chordText);
    if (p) {
      st.chordRoot = p.root;
      st.chordType = p.type;
      st.chordValid = true;
      st.gtrPage = 0;
    } else {
      st.chordValid = false;
    }
  };

  const selectionChord = () => {
    let tonic = MT.majorTonic(st.slot);
    if (st.minorView) tonic = MT.relativeMinorOf(tonic);
    const chords = MT.diatonicChords(tonic, st.minorView);
    const c = chords[st.selDegree];
    return { root: c.root, type: c.q === 'Min' ? 1 : c.q === 'Dim' ? 2 : c.q === 'Aug' ? 3 : 0 };
  };

  const seedChordFromSelection = () => {
    const c = selectionChord();
    st.chordRoot = c.root;
    st.chordType = c.type;
    rebuildText();
  };

  const transposeRoot = (delta) => {
    if (!st.chordValid) return;
    const p = MT.imod(MT.pitchClass(st.chordRoot) + delta, 12);
    st.chordRoot = MT.pcToNote(p, delta < 0);
    rebuildText();
  };

  const cycleType = (delta) => {
    if (!st.chordValid) return;
    st.chordType = MT.imod(st.chordType + delta, MT.CHORD_TYPES.length);
    rebuildText();
  };

  const deleteLastChar = () => {
    if (st.chordText.length <= 1) st.chordText = 'C';
    else st.chordText = st.chordText.slice(0, -1);
    reparse();
  };

  const startRoot = (up) => {
    st.chordText = up;
    reparse();
  };

  const appendChar = (c) => {
    if (st.chordText.length < 11) st.chordText += c;
    reparse();
  };

  const editChord = (c) => {
    const len = st.chordText.length;
    if (c >= 'A' && c <= 'G') return startRoot(c);
    if (c >= 'a' && c <= 'g') {
      if (len === 1 && c === 'b') return appendChar('b');
      if (len <= 1) return startRoot(c.toUpperCase());
      return appendChar(c);
    }
    if (c === '#' || c === 'b' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')) appendChar(c);
  };

  const transposeScaleRoot = (delta) => {
    const p = MT.imod(MT.pitchClass(st.scaleRoot) + delta, 12);
    st.scaleRoot = MT.pcToNote(p, delta < 0);
  };

  const cycleScaleType = (delta) => {
    st.scaleType = MT.imod(st.scaleType + delta, MT.SCALE_TYPES.length);
  };

  const editScaleRoot = (c) => {
    let up = c;
    if (up >= 'a' && up <= 'g') up = up.toUpperCase();
    if (up >= 'A' && up <= 'G') {
      st.scaleRoot = MT.note('CDEFGAB'.indexOf(up), 0);
    } else if (c === '#') st.scaleRoot = MT.note(st.scaleRoot.letter, 1);
    else if (c === '-') st.scaleRoot = MT.note(st.scaleRoot.letter, -1);
  };

  // ---------------------------------------------------------------- actions

  const nextPage = () => {
    st.page = (st.page + 1) % 7;
    if (st.page !== 5 && st.progPlaying) progStop();
    if (st.page === 1) seedChordFromSelection();
  };

  const moveSlot = (d) => {
    st.slot = (st.slot + d + 12) % 12;
  };

  const moveDegree = (d) => {
    st.selDegree = (st.selDegree + d + 7) % 7;
  };

  const changeBpm = (delta) => {
    st.bpm = Math.max(40, Math.min(240, st.bpm + delta));
    if (st.metroOn) startMetro();
  };

  const changePattern = (delta) => {
    st.metroPattern = (st.metroPattern + delta + UI.METRO.length) % UI.METRO.length;
    if (st.metroOn) startMetro();
  };

  const togglePlay = () => {
    if (st.page === 4) {
      if (st.metroOn) stopMetro();
      else startMetro();
      refresh();
      return;
    }
    if (st.playing) {
      stopAudio();
      return;
    }
    if (st.page === 0) {
      const c = selectionChord();
      playChord(MT.pitchClass(c.root), c.type);
    } else if (st.page <= 2 && st.chordValid) {
      playChord(MT.pitchClass(st.chordRoot), st.chordType);
    } else if (st.page === 3) {
      playScale(MT.pitchClass(st.scaleRoot), st.scaleType);
    }
  };

  const input = (action) => {
    switch (action) {
      case 'tab':
        nextPage();
        break;
      case 'left':
        if (st.page === 0) moveSlot(-1);
        else if (st.page === 1) transposeRoot(-1);
        else if (st.page === 2) st.gtrPage = (st.gtrPage + 2) % 3;
        else if (st.page === 3) transposeScaleRoot(-1);
        else if (st.page === 4) changePattern(-1);
        else if (st.page === 5 && st.progView === 2 && st.progEdit) {
          st.progSlot = (st.progSlot + st.progSteps.length - 1) % st.progSteps.length;
        } else if (st.page === 6) st.modTo = (st.modTo + 11) % 12;
        break;
      case 'right':
        if (st.page === 0) moveSlot(1);
        else if (st.page === 1) transposeRoot(1);
        else if (st.page === 2) st.gtrPage = (st.gtrPage + 1) % 3;
        else if (st.page === 3) transposeScaleRoot(1);
        else if (st.page === 4) changePattern(1);
        else if (st.page === 5 && st.progView === 2 && st.progEdit) {
          st.progSlot = (st.progSlot + 1) % st.progSteps.length;
        } else if (st.page === 6) st.modTo = (st.modTo + 1) % 12;
        break;
      case 'up':
        if (st.page === 0) moveDegree(-1);
        else if (st.page === 1 || st.page === 2) cycleType(-1);
        else if (st.page === 3) cycleScaleType(-1);
        else changeBpm(1);
        break;
      case 'down':
        if (st.page === 0) moveDegree(1);
        else if (st.page === 1 || st.page === 2) cycleType(1);
        else if (st.page === 3) cycleScaleType(1);
        else changeBpm(-1);
        break;
      case 'enter':
        if (st.page === 5) progEnter();
        else togglePlay();
        break;
      case 'back':
        if (st.page === 5) progBack();
        else if (st.page === 1 || st.page === 2) deleteLastChar();
        break;
      default:
        break;
    }
    refresh();
  };

  const typeChar = (c) => {
    if (st.page === 0) {
      if (c >= '1' && c <= '7') st.selDegree = Number(c) - 1;
      else if (c === 'm' || c === 'M') st.minorView = !st.minorView;
      else if (c === '[') moveSlot(-1);
      else if (c === ']') moveSlot(1);
    } else if (st.page === 3) editScaleRoot(c);
    else editChord(c);
    refresh();
  };

  // ---------------------------------------------------------------- view

  const pageNames = ['五度圈', '和弦查询', '吉他指法', '音阶', '节拍器', '和弦进行', '转调'];

  const refresh = () => {
    UI.render(st, canvas);
    const el = document.getElementById('status');
    if (!el) return;
    let detail = '';
    if (st.page === 0) {
      let tonic = MT.majorTonic(st.slot);
      if (st.minorView) tonic = MT.relativeMinorOf(tonic);
      detail = `${MT.noteName(tonic)}${st.minorView ? 'm' : ''} · 第 ${st.selDegree + 1} 级`;
    } else if (st.page === 1) {
      detail = st.chordValid ? `${st.chordText} · ${MT.CHORD_TYPES[st.chordType].label}` : `${st.chordText}?`;
    } else if (st.page === 2) {
      detail = `${st.chordText} · 把位 ${st.gtrPage + 1}`;
    } else if (st.page === 3) {
      detail = `${MT.noteName(st.scaleRoot)} ${MT.SCALE_TYPES[st.scaleType].name}`;
    } else if (st.page === 4) {
      detail = `${st.bpm} BPM · ${UI.METRO[st.metroPattern].name}${st.metroOn ? ' · 运行中' : ''}`;
    } else if (st.page === 5) {
      const view = ['风格', '进行', '播放'][st.progView];
      detail = `${view} · ${st.progName || PROG.STYLES[st.progStyle].name}`;
    } else {
      const info = MOD.analyse(st.slot, st.modTo, st.modMinor);
      detail = `${MT.noteName(info.toTonic)}${info.toMinor ? 'm' : ''} · ${info.relation}`;
    }
    el.textContent = `${st.page + 1}/7 ${pageNames[st.page]} — ${detail}${st.playing ? ' · 播放中' : ''}`;
  };

  const keyMap = {
    Tab: 'tab',
    ArrowLeft: 'left',
    ArrowRight: 'right',
    ArrowUp: 'up',
    ArrowDown: 'down',
    Enter: 'enter',
    ' ': 'enter',
    Backspace: 'back',
  };

  // deep links for testing: index.html#page=2&chord=G7&scale=11&key=7
  const applyHash = () => {
    const h = new URLSearchParams(location.hash.replace('#', ''));
    if (h.has('page')) st.page = Math.max(0, Math.min(6, Number(h.get('page')) || 0));
    if (h.has('bpm')) st.bpm = Math.max(40, Math.min(240, Number(h.get('bpm')) || 100));
    if (h.has('key')) st.slot = MT.imod(Number(h.get('key')) || 0, 12);
    if (h.has('mod')) st.modTo = MT.imod(Number(h.get('mod')) || 0, 12);
    if (h.has('prog')) {
      progLoadPreset(Number(h.get('prog')) || 0, Number(h.get('item')) || 0);
    }
    if (h.has('vol')) {
      st.volume = Math.max(0, Math.min(100, Number(h.get('vol')) || 0));
      st.showVolume = true;
    }
    if (h.has('minor')) st.minorView = true;
    if (h.has('scale')) st.scaleType = MT.imod(Number(h.get('scale')) || 0, MT.SCALE_TYPES.length);
    if (h.has('chord')) {
      st.chordText = h.get('chord');
      reparse();
      if (!st.chordValid) {
        st.chordText = 'C';
        reparse();
      }
    }
  };

  const init = (cv) => {
    canvas = cv;
    const deepLinked = location.hash.includes('page=');
    applyHash();
    if (!deepLinked) {
      st.splash = true;
      setTimeout(() => {
        st.splash = false;
        refresh();
      }, 1400);
    }
    window.addEventListener('hashchange', () => {
      applyHash();
      refresh();
    });
    document.addEventListener('keydown', (e) => {
      if (e.key === '-' || e.key === '_') {
        e.preventDefault();
        changeVolume(-10);
        return;
      }
      if (e.key === '=' || e.key === '+') {
        e.preventDefault();
        changeVolume(10);
        return;
      }
      if (keyMap[e.key]) {
        e.preventDefault();
        input(keyMap[e.key]);
        return;
      }
      if (e.key.length === 1) typeChar(e.key);
    });
    for (const btn of document.querySelectorAll('[data-action]')) {
      btn.addEventListener('click', () => input(btn.dataset.action));
    }
    refresh();
  };

  return { init, refresh, state: st };
})();












