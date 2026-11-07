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
    tuner: { mic: false, valid: false, freq: 0, note: 0, cents: 0, level: 0 },
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
    osc.frequency.value = accent ? 1568 : 1046;
    g.gain.setValueAtTime(accent ? 0.35 : 0.2, t);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.06);
    osc.connect(g);
    g.connect(ctx.destination);
    osc.start(t);
    osc.stop(t + 0.07);
  };

  const stopMetro = () => {
    if (metro.timer) clearInterval(metro.timer);
    metro.timer = null;
    st.metroOn = false;
    st.beat = -1;
  };

  const startMetro = () => {
    stopMetro();
    st.metroOn = true;
    st.beat = -1;
    const tick = () => {
      const p = UI.METRO[st.metroPattern];
      st.beat = (st.beat + 1) % p.beats;
      clickSound((p.accents >> st.beat) & 1);
      refresh();
    };
    tick();
    metro.timer = setInterval(tick, 60000 / st.bpm);
  };

  // ---- tuner -----------------------------------------------------------
  const tuner = { stream: null, analyser: null, buf: null, timer: null };

  const stopTuner = () => {
    if (tuner.timer) clearInterval(tuner.timer);
    tuner.timer = null;
    if (tuner.stream) {
      for (const tr of tuner.stream.getTracks()) tr.stop();
      tuner.stream = null;
    }
    tuner.analyser = null;
    st.tuner.mic = false;
  };

  const analyseTuner = () => {
    const an = tuner.analyser;
    if (!an) return;
    an.getFloatTimeDomainData(tuner.buf);

    // decimate by 4 so the autocorrelation stays cheap
    const raw = tuner.buf;
    const n = Math.floor(raw.length / 4);
    const x = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      x[i] = (raw[i * 4] + raw[i * 4 + 1] + raw[i * 4 + 2] + raw[i * 4 + 3]) / 4;
    }

    let energy = 0;
    for (let i = 0; i < n; i++) energy += x[i] * x[i];
    const rms = Math.sqrt(energy / n);
    st.tuner.level = Math.max(0, Math.min(100, Math.round(Math.log10(rms + 1) * 42)));

    if (energy < 1e-6) {
      st.tuner.valid = false;
      return;
    }

    const rate = ensureAudio().sampleRate / 4;
    const minLag = Math.max(2, Math.floor(rate / 1400));
    const maxLag = Math.floor(rate / 55);
    const corr = new Float32Array(maxLag + 2);
    let best = 0;
    for (let lag = minLag; lag <= maxLag; lag++) {
      let sum = 0;
      for (let i = 0; i + lag < n; i++) sum += x[i] * x[i + lag];
      sum /= n - lag;
      corr[lag] = sum;
      if (sum > best) best = sum;
    }
    if (best <= 0) {
      st.tuner.valid = false;
      return;
    }

    const thr = best * 0.87;
    let peak = 0;
    for (let lag = minLag + 1; lag < maxLag; lag++) {
      if (corr[lag] < thr) continue;
      if (corr[lag] < corr[lag - 1] || corr[lag] < corr[lag + 1]) continue;
      peak = lag;
      break;
    }
    if (!peak) {
      st.tuner.valid = false;
      return;
    }

    const y0 = corr[peak - 1];
    const y1 = corr[peak];
    const y2 = corr[peak + 1];
    const den = y0 - 2 * y1 + y2;
    let shift = 0;
    if (Math.abs(den) > 1e-9) shift = (0.5 * (y0 - y2)) / den;
    shift = Math.max(-0.5, Math.min(0.5, shift));

    const freq = rate / (peak + shift);
    if (!(freq > 40 && freq < 1400)) {
      st.tuner.valid = false;
      return;
    }
    const midi = 69 + 12 * Math.log2(freq / 440);
    const note = Math.round(midi);
    st.tuner.valid = true;
    st.tuner.freq = freq;
    st.tuner.note = note;
    st.tuner.cents = (midi - note) * 100;
  };

  const startTuner = async () => {
    if (tuner.stream) return;
    try {
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: { echoCancellation: false, autoGainControl: false, noiseSuppression: false },
      });
      const ctx = ensureAudio();
      const src = ctx.createMediaStreamSource(stream);
      const an = ctx.createAnalyser();
      an.fftSize = 2048;
      src.connect(an);
      tuner.stream = stream;
      tuner.analyser = an;
      tuner.buf = new Float32Array(an.fftSize);
      st.tuner.mic = true;
      tuner.timer = setInterval(() => {
        analyseTuner();
        refresh();
      }, 120);
    } catch (e) {
      st.tuner.mic = false;
      refresh();
    }
  };

  const gainOf = () => 0.22 * (st.volume / 100);

  const changeVolume = (delta) => {
    const v = Math.max(0, Math.min(100, st.volume + delta));
    if (v === st.volume) return;
    st.volume = v;
    st.showVolume = true;
    if (volTimer) clearTimeout(volTimer);
    volTimer = setTimeout(() => {
      st.showVolume = false;
      refresh();
    }, 1200);
    refresh();
  };

  const ensureAudio = () => {
    if (!audio.ctx) audio.ctx = new (window.AudioContext || window.webkitAudioContext)();
    if (audio.ctx.state === 'suspended') audio.ctx.resume();
    return audio.ctx;
  };

  const stopAudio = () => {
    if (audio.timer) {
      clearTimeout(audio.timer);
      audio.timer = null;
    }
    if (audio.master) {
      try {
        audio.master.gain.cancelScheduledValues(audio.ctx.currentTime);
        audio.master.gain.setTargetAtTime(0.0001, audio.ctx.currentTime, 0.01);
      } catch (e) {
        /* already gone */
      }
    }
    for (const n of audio.nodes) {
      try {
        n.stop(audio.ctx.currentTime + 0.05);
      } catch (e) {
        /* already stopped */
      }
    }
    audio.nodes = [];
    st.playing = false;
  };

  const playChord = (rootPc, typeIdx) => {
    stopAudio();
    const ctx = ensureAudio();
    const t = MT.CHORD_TYPES[typeIdx];
    const rootMidi = 48 + rootPc;

    const master = ctx.createGain();
    master.gain.value = gainOf();
    master.connect(ctx.destination);
    audio.master = master;

    const now = ctx.currentTime;
    master.gain.setValueAtTime(0.22, now);
    master.gain.setValueAtTime(0.22, now + 0.55);
    master.gain.exponentialRampToValueAtTime(0.0001, now + 0.85);

    const midis = [];
    for (let i = 0; i < t.n; i++) midis.push(rootMidi + t.semis[i]);
    midis.push(rootMidi + 12);

    for (const m of midis) {
      const osc = ctx.createOscillator();
      const g = ctx.createGain();
      osc.type = 'sine';
      osc.frequency.value = 440 * Math.pow(2, (m - 69) / 12);
      g.gain.value = 0.9 / midis.length;
      osc.connect(g);
      g.connect(master);
      osc.start(now);
      osc.stop(now + 0.9);
      audio.nodes.push(osc);
    }

    st.playing = true;
    audio.timer = setTimeout(() => {
      st.playing = false;
      audio.nodes = [];
      refresh();
    }, 900);
  };

  // ascending run, one note every 220 ms, the same as the firmware
  const playScale = (rootPc, typeIdx) => {
    stopAudio();
    const ctx = ensureAudio();
    const t = MT.SCALE_TYPES[typeIdx];

    const master = ctx.createGain();
    master.gain.value = gainOf();
    master.connect(ctx.destination);
    audio.master = master;

    const now = ctx.currentTime + 0.02;
    const per = 0.22;
    for (let i = 0; i < t.n; i++) {
      const midi = 60 + rootPc + t.semis[i];
      const t0 = now + i * per;
      const osc = ctx.createOscillator();
      const g = ctx.createGain();
      osc.type = 'sine';
      osc.frequency.value = 440 * Math.pow(2, (midi - 69) / 12);
      g.gain.setValueAtTime(0.0001, t0);
      g.gain.exponentialRampToValueAtTime(0.9 / t.n, t0 + 0.012);
      g.gain.exponentialRampToValueAtTime(0.0001, t0 + per * 0.95);
      osc.connect(g);
      g.connect(master);
      osc.start(t0);
      osc.stop(t0 + per);
      audio.nodes.push(osc);
    }

    st.playing = true;
    audio.timer = setTimeout(() => {
      st.playing = false;
      audio.nodes = [];
      refresh();
    }, (t.n * per + 0.15) * 1000);
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
    const prev = st.page;
    st.page = (st.page + 1) % 6;
    if (st.page === 1) seedChordFromSelection();
    if (st.page === 5) {
      stopMetro();
      startTuner();
    } else if (prev === 5) {
      stopTuner();
    }
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
    if (st.page === 5) return;

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
        break;
      case 'right':
        if (st.page === 0) moveSlot(1);
        else if (st.page === 1) transposeRoot(1);
        else if (st.page === 2) st.gtrPage = (st.gtrPage + 1) % 3;
        else if (st.page === 3) transposeScaleRoot(1);
        else if (st.page === 4) changePattern(1);
        break;
      case 'up':
        if (st.page === 0) moveDegree(-1);
        else if (st.page === 1 || st.page === 2) cycleType(-1);
        else if (st.page === 3) cycleScaleType(-1);
        else if (st.page === 4) changeBpm(1);
        break;
      case 'down':
        if (st.page === 0) moveDegree(1);
        else if (st.page === 1 || st.page === 2) cycleType(1);
        else if (st.page === 3) cycleScaleType(1);
        else if (st.page === 4) changeBpm(-1);
        break;
      case 'enter':
        togglePlay();
        break;
      case 'back':
        if (st.page === 1 || st.page === 2) deleteLastChar();
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

  const pageNames = ['五度圈', '和弦查询', '吉他指法', '音阶', '节拍器', '调音表'];

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
    } else {
      detail = st.tuner.valid
        ? `${st.tuner.freq.toFixed(1)} Hz · ${Math.round(st.tuner.cents)} cents`
        : st.tuner.mic
          ? '等待声音'
          : '无麦克风';
    }
    el.textContent = `${st.page + 1}/6 ${pageNames[st.page]} — ${detail}${st.playing ? ' · 播放中' : ''}`;
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
    if (h.has('page')) st.page = Math.max(0, Math.min(5, Number(h.get('page')) || 0));
    if (h.has('bpm')) st.bpm = Math.max(40, Math.min(240, Number(h.get('bpm')) || 100));
    if (h.has('key')) st.slot = MT.imod(Number(h.get('key')) || 0, 12);
    if (h.has('tuner')) {
      st.tuner = { mic: true, valid: true, freq: 440.0, note: 69, cents: 7, level: 62 };
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







