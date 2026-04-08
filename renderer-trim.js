(function registerTrimModal(app) {
  app.ui.trimModal = {
    soundId: null,
    audioBuffer: null,
    audioCtx: null,
    durationSec: 0,
    startSec: 0,
    endSec: 0,
    dragging: null,
    previewSource: null,
  };

  function drawWaveform(canvas, audioBuffer, startSec, endSec) {
    const W = canvas.offsetWidth || 560;
    const H = 128;
    canvas.width = W;
    canvas.height = H;

    const ctx = canvas.getContext('2d');
    const duration = audioBuffer.duration;

    // Mix to mono
    let samples;
    if (audioBuffer.numberOfChannels >= 2) {
      const ch0 = audioBuffer.getChannelData(0);
      const ch1 = audioBuffer.getChannelData(1);
      samples = new Float32Array(ch0.length);
      for (let i = 0; i < ch0.length; i++) {
        samples[i] = (ch0[i] + ch1[i]) * 0.5;
      }
    } else {
      samples = audioBuffer.getChannelData(0);
    }

    const total = samples.length;

    // Background
    ctx.fillStyle = '#0f1620';
    ctx.fillRect(0, 0, W, H);

    // Centre line
    ctx.fillStyle = 'rgba(255,255,255,0.06)';
    ctx.fillRect(0, H / 2 - 0.5, W, 1);

    const startPx = (startSec / duration) * W;
    const endPx = (endSec / duration) * W;

    // Waveform bars
    for (let px = 0; px < W; px++) {
      const sA = Math.floor((px / W) * total);
      const sB = Math.min(Math.floor(((px + 1) / W) * total), total - 1);
      let lo = 0, hi = 0;
      for (let s = sA; s <= sB; s++) {
        if (samples[s] < lo) lo = samples[s];
        if (samples[s] > hi) hi = samples[s];
      }
      const inRange = px >= startPx && px <= endPx;
      ctx.fillStyle = inRange ? 'rgba(255,171,82,0.88)' : 'rgba(156,173,197,0.22)';
      const yTop = Math.round((0.5 - hi * 0.46) * H);
      const yBot = Math.round((0.5 - lo * 0.46) * H);
      ctx.fillRect(px, yTop, 1, Math.max(1, yBot - yTop));
    }

    // Dim out-of-range regions
    ctx.fillStyle = 'rgba(15,22,32,0.52)';
    if (startPx > 0) ctx.fillRect(0, 0, startPx, H);
    if (endPx < W) ctx.fillRect(endPx, 0, W - endPx, H);

    // Handle lines
    const sPx = Math.round(startPx);
    const ePx = Math.round(endPx);
    ctx.fillStyle = '#ffab52';
    ctx.fillRect(sPx - 1, 0, 2, H);
    ctx.fillRect(ePx - 1, 0, 2, H);

    // Handle diamond grips at top
    function diamond(x) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x + 8, 10);
      ctx.lineTo(x, 20);
      ctx.lineTo(x - 8, 10);
      ctx.closePath();
      ctx.fillStyle = '#ffab52';
      ctx.fill();
      ctx.strokeStyle = 'rgba(0,0,0,0.4)';
      ctx.lineWidth = 1;
      ctx.stroke();
    }
    diamond(sPx);
    diamond(ePx);
  }

  function fmt(sec) {
    if (sec < 60) return `${sec.toFixed(3)}s`;
    const m = Math.floor(sec / 60);
    const s = (sec % 60).toFixed(3).padStart(6, '0');
    return `${m}:${s}`;
  }

  // ── Open ────────────────────────────────────────────────────────

  app.openTrimModal = async function openTrimModal(soundId) {
    const sound = app.getSound(soundId);
    if (!sound) return;

    const modal = app.ui.trimModal;
    modal.soundId = soundId;

    document.getElementById('trim-modal-title').textContent = `Trim: ${sound.displayName}`;
    app.openModal('trim-modal');

    // Loading placeholder
    const canvas = document.getElementById('trim-waveform-canvas');
    canvas.width = canvas.offsetWidth || 560;
    canvas.height = 128;
    const ctx2d = canvas.getContext('2d');
    ctx2d.fillStyle = '#0f1620';
    ctx2d.fillRect(0, 0, canvas.width, canvas.height);
    ctx2d.fillStyle = 'rgba(156,173,197,0.45)';
    ctx2d.font = '13px Inter, sans-serif';
    ctx2d.textAlign = 'center';
    ctx2d.fillText('Loading waveform…', canvas.width / 2, canvas.height / 2 + 5);

    try {
      const url = await app.getSoundUrl(sound.filename);
      const response = await fetch(url);
      const arrayBuf = await response.arrayBuffer();

      if (modal.audioCtx) {
        try { modal.audioCtx.close(); } catch (_) {}
      }
      modal.audioCtx = new AudioContext();
      const audioBuf = await modal.audioCtx.decodeAudioData(arrayBuf);

      modal.audioBuffer = audioBuf;
      modal.durationSec = audioBuf.duration;

      modal.startSec = Math.max(0, (sound.trimStartMs || 0) / 1000);
      const trimEndSec = Math.max(0, (sound.trimEndMs || 0) / 1000);
      modal.endSec = Math.min(modal.durationSec, modal.durationSec - trimEndSec);
      if (modal.endSec <= modal.startSec) {
        modal.startSec = 0;
        modal.endSec = modal.durationSec;
      }

      app.renderTrimModal();
    } catch (err) {
      console.error('Trim modal load error:', err);
      app.showToast('Could not load audio for trim.');
      app.closeTrimModal();
    }
  };

  // ── Close ───────────────────────────────────────────────────────

  app.closeTrimModal = function closeTrimModal() {
    const modal = app.ui.trimModal;

    if (modal.previewSource) {
      try { modal.previewSource.stop(); } catch (_) {}
      modal.previewSource = null;
    }
    if (modal.audioCtx) {
      try { modal.audioCtx.close(); } catch (_) {}
      modal.audioCtx = null;
    }

    modal.soundId = null;
    modal.audioBuffer = null;
    modal.dragging = null;
    document.getElementById('trim-preview-btn').textContent = '▶ Preview';
    app.closeModal('trim-modal');
  };

  // ── Render ──────────────────────────────────────────────────────

  app.renderTrimModal = function renderTrimModal() {
    const modal = app.ui.trimModal;
    if (!modal.audioBuffer) return;

    const canvas = document.getElementById('trim-waveform-canvas');
    drawWaveform(canvas, modal.audioBuffer, modal.startSec, modal.endSec);

    document.getElementById('trim-time-start').textContent = fmt(modal.startSec);
    document.getElementById('trim-time-end').textContent = fmt(modal.endSec);
    document.getElementById('trim-time-duration').textContent = fmt(modal.endSec - modal.startSec);
    document.getElementById('trim-total-duration').textContent = fmt(modal.durationSec);
  };

  // ── Save ────────────────────────────────────────────────────────

  app.saveTrimModal = async function saveTrimModal() {
    const modal = app.ui.trimModal;
    if (!modal.soundId) return;

    const sound = app.getSound(modal.soundId);
    if (!sound) return;

    sound.trimStartMs = Math.round(modal.startSec * 1000);
    sound.trimEndMs = Math.round((modal.durationSec - modal.endSec) * 1000);

    app.closeTrimModal();
    await app.persistState({ syncHotkeys: false });
    app.showToast(`Trim saved for "${sound.displayName}".`);
  };

  // ── Preview ─────────────────────────────────────────────────────

  app.previewTrimSection = function previewTrimSection() {
    const modal = app.ui.trimModal;
    const btn = document.getElementById('trim-preview-btn');

    if (modal.previewSource) {
      try { modal.previewSource.stop(); } catch (_) {}
      modal.previewSource = null;
      btn.textContent = '▶ Preview';
      return;
    }

    if (!modal.audioBuffer || !modal.audioCtx) return;

    if (modal.audioCtx.state === 'suspended') modal.audioCtx.resume();

    const source = modal.audioCtx.createBufferSource();
    source.buffer = modal.audioBuffer;
    source.connect(modal.audioCtx.destination);

    modal.previewSource = source;
    btn.textContent = '⏸ Stop';

    source.onended = () => {
      if (modal.previewSource === source) {
        modal.previewSource = null;
        btn.textContent = '▶ Preview';
      }
    };

    source.start(0, modal.startSec, modal.endSec - modal.startSec);
  };

  // ── Events ──────────────────────────────────────────────────────

  app.bindTrimModalEvents = function bindTrimModalEvents() {
    document.getElementById('trim-modal-close').addEventListener('click', app.closeTrimModal);
    document.getElementById('trim-cancel-btn').addEventListener('click', app.closeTrimModal);
    document.getElementById('trim-save-btn').addEventListener('click', app.saveTrimModal);
    document.getElementById('trim-preview-btn').addEventListener('click', app.previewTrimSection);
    document.getElementById('trim-modal').addEventListener('click', (event) => {
      if (event.target.id === 'trim-modal') app.closeTrimModal();
    });

    const canvas = document.getElementById('trim-waveform-canvas');
    const GRAB = 14; // px grab zone radius

    function relX(event) {
      const rect = canvas.getBoundingClientRect();
      return ((event.clientX - rect.left) / rect.width) * canvas.width;
    }

    function xToSec(x) {
      const m = app.ui.trimModal;
      return Math.max(0, Math.min(m.durationSec, (x / canvas.width) * m.durationSec));
    }

    canvas.addEventListener('mousedown', (event) => {
      const m = app.ui.trimModal;
      if (!m.audioBuffer) return;
      const x = relX(event);
      const sPx = (m.startSec / m.durationSec) * canvas.width;
      const ePx = (m.endSec / m.durationSec) * canvas.width;
      const dS = Math.abs(x - sPx);
      const dE = Math.abs(x - ePx);
      if (dS <= GRAB && dS <= dE) m.dragging = 'start';
      else if (dE <= GRAB) m.dragging = 'end';
      event.preventDefault();
    });

    canvas.addEventListener('mousemove', (event) => {
      const m = app.ui.trimModal;
      if (!m.audioBuffer) return;

      const x = relX(event);

      if (!m.dragging) {
        const sPx = (m.startSec / m.durationSec) * canvas.width;
        const ePx = (m.endSec / m.durationSec) * canvas.width;
        canvas.style.cursor =
          Math.abs(x - sPx) <= GRAB || Math.abs(x - ePx) <= GRAB ? 'ew-resize' : 'default';
        return;
      }

      const sec = xToSec(x);
      const MIN = 0.05;
      if (m.dragging === 'start') {
        m.startSec = Math.max(0, Math.min(sec, m.endSec - MIN));
      } else {
        m.endSec = Math.min(m.durationSec, Math.max(sec, m.startSec + MIN));
      }
      app.renderTrimModal();
    });

    const stopDrag = () => { app.ui.trimModal.dragging = null; };
    canvas.addEventListener('mouseup', stopDrag);
    document.addEventListener('mouseup', stopDrag);
  };
})(window.SoundboardApp);
