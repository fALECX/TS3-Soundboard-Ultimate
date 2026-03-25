(function registerPlaybackHelpers(app) {
  app.updateMasterVolumeLabel = function updateMasterVolumeLabel() {
    const percent = Math.round((app.state.appConfig.masterVolume || 0) * 100);
    document.getElementById('volume-slider').value = String(percent);
    document.getElementById('volume-label').textContent = `${percent}%`;
  };

  app.getSoundUrl = async function getSoundUrl(filename) {
    if (app.ui.soundUrlCache.has(filename)) return app.ui.soundUrlCache.get(filename);
    const url = await window.api.getSoundFileUrl(filename);
    app.ui.soundUrlCache.set(filename, url);
    return url;
  };

  app.waitForLoadedMetadata = function waitForLoadedMetadata(audio) {
    if (audio.readyState >= 1) return Promise.resolve();

    return new Promise((resolve) => {
      const done = () => resolve();
      audio.addEventListener('loadedmetadata', done, { once: true });
      audio.addEventListener('error', done, { once: true });
      setTimeout(done, 1500);
    });
  };

  app.applyOutputDevice = async function applyOutputDevice(audio) {
    if (app.state.appConfig.outputDevice === 'default') return;
    if (typeof audio.setSinkId !== 'function') return;

    try {
      await audio.setSinkId(app.state.appConfig.outputDevice);
    } catch (error) {
      console.warn('Could not set sink ID:', error);
    }
  };

  app.refreshPlayerVolumes = function refreshPlayerVolumes() {
    app.ui.activePlayers.forEach((player) => {
      const sound = player.soundId ? app.getSound(player.soundId) : null;
      const gain = sound?.gain || 1;
      player.audio.volume = app.clampVolume(app.state.appConfig.masterVolume * gain);
    });
  };

  app.refreshPlayerOutputDevices = function refreshPlayerOutputDevices() {
    app.ui.activePlayers.forEach((player) => {
      app.applyOutputDevice(player.audio);
    });
    if (app.ui.activePreview?.audio) app.applyOutputDevice(app.ui.activePreview.audio);
  };

  app.stopActivePreview = function stopActivePreview() {
    if (!app.ui.activePreview) return;
    app.ui.activePreview.audio.pause();
    app.ui.activePreview.audio.src = '';
    if (app.ui.activePreview.button) {
      app.ui.activePreview.button.textContent = '▶';
      app.ui.activePreview.button.disabled = false;
    }
    app.ui.activePreview = null;
  };

  app.startPreview = function startPreview(url, button) {
    app.stopActivePreview();

    const audio = new Audio(url);
    app.applyOutputDevice(audio);
    audio.volume = app.clampVolume(app.state.appConfig.masterVolume);

    button.textContent = '⏸';
    app.ui.activePreview = { audio, button };

    audio.play().catch(() => {
      app.stopActivePreview();
      app.showToast('Vorschau konnte nicht abgespielt werden.');
    });

    audio.addEventListener(
      'ended',
      () => {
        app.stopActivePreview();
      },
      { once: true }
    );
  };

  app.stopPlayer = function stopPlayer(player) {
    if (!player || player.stopped) return;
    player.cleanup?.();
  };

  app.stopAllPlayers = function stopAllPlayers() {
    [...app.ui.activePlayers].forEach((player) => app.stopPlayer(player));
    app.renderAll();
  };

  app.stopPlayersForSound = function stopPlayersForSound(soundId) {
    app.ui.activePlayers
      .filter((player) => player.soundId === soundId)
      .forEach((player) => app.stopPlayer(player));
    app.renderAll();
  };

  app.stopPlayersForBoard = function stopPlayersForBoard(boardId) {
    app.ui.activePlayers
      .filter((player) => player.boardId === boardId)
      .forEach((player) => app.stopPlayer(player));
    app.renderAll();
  };

  app.stopPlayersForCell = function stopPlayersForCell(boardId, cellIndex) {
    app.ui.activePlayers
      .filter((player) => player.boardId === boardId && player.cellIndex === cellIndex)
      .forEach((player) => app.stopPlayer(player));
    app.renderAll();
  };

  app.isSoundPlaying = function isSoundPlaying(soundId) {
    return app.ui.activePlayers.some((player) => player.soundId === soundId);
  };

  app.playCell = async function playCell(boardId, cellIndex) {
    const board = app.getBoardById(boardId);
    const cell = board?.cells[cellIndex];
    const sound = cell?.soundId ? app.getSound(cell.soundId) : null;
    if (!sound) return;
    await app.playSound(sound, { boardId, cellIndex });
  };

  app.playLibrarySound = async function playLibrarySound(soundId) {
    const sound = app.getSound(soundId);
    if (!sound) return;
    await app.playSound(sound, { boardId: null, cellIndex: null, preview: true });
  };

  app.playSound = async function playSound(sound, context) {
    app.stopActivePreview();

    if (sound.triggerMode === 'toggle' && app.isSoundPlaying(sound.soundId)) {
      app.stopPlayersForSound(sound.soundId);
      return;
    }

    if (sound.playbackMode === 'interrupt') {
      app.stopAllPlayers();
    } else if (sound.playbackMode === 'restart') {
      app.stopPlayersForSound(sound.soundId);
    }

    const url = await app.getSoundUrl(sound.filename);
    const audio = new Audio(url);
    await app.applyOutputDevice(audio);
    audio.volume = app.clampVolume(app.state.appConfig.masterVolume * (sound.gain || 1));
    await app.waitForLoadedMetadata(audio);

    const startAt = Math.max(0, Number(sound.trimStartMs || 0) / 1000);
    const trimEnd = Math.max(0, Number(sound.trimEndMs || 0) / 1000);
    const hasFiniteDuration = Number.isFinite(audio.duration) && audio.duration > 0;
    const endAt = hasFiniteDuration ? Math.max(startAt, audio.duration - trimEnd) : null;

    if (startAt > 0 && (audio.duration >= startAt || !hasFiniteDuration)) {
      try {
        audio.currentTime = startAt;
      } catch (error) {
        console.warn('Could not apply trim start:', error);
      }
    }

    if (sound.loop && endAt === null) {
      audio.loop = true;
    }

    const player = {
      id: app.createId('player'),
      audio,
      soundId: sound.soundId,
      boardId: context.boardId,
      cellIndex: context.cellIndex,
      preview: Boolean(context.preview),
      startAt,
      endAt,
      loop: Boolean(sound.loop),
      stopped: false,
    };

    const cleanup = () => {
      if (player.stopped) return;
      player.stopped = true;
      audio.pause();
      audio.src = '';
      app.ui.activePlayers = app.ui.activePlayers.filter((entry) => entry.id !== player.id);
      app.renderAll();
    };

    const onTimeUpdate = () => {
      if (player.stopped || player.endAt === null) return;
      if (audio.currentTime < player.endAt - 0.04) return;

      if (player.loop) {
        try {
          audio.currentTime = player.startAt;
          audio.play().catch(() => cleanup());
        } catch (error) {
          cleanup();
        }
        return;
      }

      cleanup();
    };

    player.cleanup = cleanup;

    audio.addEventListener('timeupdate', onTimeUpdate);
    audio.addEventListener('ended', cleanup, { once: true });
    audio.addEventListener(
      'error',
      () => {
        cleanup();
        app.showToast(`"${sound.displayName}" konnte nicht abgespielt werden.`);
      },
      { once: true }
    );

    app.ui.activePlayers.push(player);
    app.ui.lastPlayback = {
      soundId: sound.soundId,
      boardId: context.boardId,
      cellIndex: context.cellIndex,
    };

    sound.playCount = (sound.playCount || 0) + 1;
    sound.lastPlayedAt = new Date().toISOString();
    app.queuePersist({ syncHotkeys: false });
    app.renderAll();

    try {
      await audio.play();
    } catch (error) {
      cleanup();
      app.showToast(`"${sound.displayName}" konnte nicht gestartet werden.`);
    }
  };

  app.replayLastSound = async function replayLastSound() {
    if (!app.ui.lastPlayback?.soundId) {
      app.showToast('Noch kein Sound abgespielt.');
      return;
    }

    if (app.ui.lastPlayback.boardId && Number.isFinite(app.ui.lastPlayback.cellIndex)) {
      await app.playCell(app.ui.lastPlayback.boardId, app.ui.lastPlayback.cellIndex);
      return;
    }

    await app.playLibrarySound(app.ui.lastPlayback.soundId);
  };

  app.assignSoundToCell = function assignSoundToCell(board, targetIndex, soundId) {
    const existing = board.cells[targetIndex];
    if (existing?.soundId) {
      board.unassignedSoundIds.push(existing.soundId);
    }
    board.cells[targetIndex] = { soundId, hotkey: existing?.hotkey || null };
  };

  app.removeSingleUnassigned = function removeSingleUnassigned(board, soundId) {
    const index = board.unassignedSoundIds.indexOf(soundId);
    if (index >= 0) board.unassignedSoundIds.splice(index, 1);
  };

  app.assignSoundToBoardOrUnassigned = function assignSoundToBoardOrUnassigned(soundId, boardId, options = {}) {
    const board = app.getBoardById(boardId) || app.getActiveBoard();
    if (!board) return 'library';

    const firstEmpty = board.cells.findIndex((cell) => cell === null);
    if (firstEmpty >= 0) {
      board.cells[firstEmpty] = { soundId, hotkey: null };
      if (options.consumeUnassigned) app.removeSingleUnassigned(board, soundId);
      return 'cell';
    }

    board.unassignedSoundIds.push(soundId);
    if (options.consumeUnassigned) app.removeSingleUnassigned(board, soundId);
    return 'unassigned';
  };

  app.addImportedSound = function addImportedSound(input) {
    const sound = app.createSoundRecord({
      filename: input.filename,
      displayName: input.displayName,
      sourceType: input.sourceType,
      sourceUrl: input.sourceUrl,
      tags: input.tags || [],
    });

    app.state.library[sound.soundId] = sound;
    app.renderAll();
    app.queuePersist({ syncHotkeys: false });
    app.showToast(`"${sound.displayName}" zur Library hinzugefuegt.`);
  };

  app.importLocalFiles = async function importLocalFiles() {
    const files = await window.api.importFiles();
    if (!files.length) return;

    files.forEach((file) => {
      const operationId = app.createOperation({
        label: `Import · ${file.displayName}`,
        source: 'local',
        status: 'done',
        statusText: 'Importiert',
        percent: 100,
      });
      app.updateOperation(operationId, {
        status: 'done',
        statusText: 'Importiert',
        percent: 100,
      });
      app.addImportedSound(file);
    });
  };

  app.resizeBoard = function resizeBoard(boardId, cols, rows) {
    const board = app.getBoardById(boardId);
    if (!board) return;

    const nextTotal = cols * rows;
    if (nextTotal < board.cells.length) {
      board.cells.slice(nextTotal).forEach((cell) => {
        if (cell?.soundId) board.unassignedSoundIds.push(cell.soundId);
      });
    }

    board.cols = cols;
    board.rows = rows;
    board.cells = board.cells.slice(0, nextTotal);
    while (board.cells.length < nextTotal) board.cells.push(null);

    app.renderAll();
    app.queuePersist({ syncHotkeys: true });
    app.syncHotkeys({ showErrors: true });
  };

  app.removeSoundFromBoard = function removeSoundFromBoard(boardId, cellIndex) {
    const board = app.getBoardById(boardId);
    if (!board?.cells[cellIndex]) return;
    app.stopPlayersForCell(boardId, cellIndex);
    board.cells[cellIndex] = null;
    app.renderAll();
    app.queuePersist({ syncHotkeys: true });
    app.syncHotkeys({ showErrors: true });
  };

  app.confirmDeleteSound = function confirmDeleteSound(soundId) {
    const sound = app.getSound(soundId);
    if (!sound) return;

    app.openConfirmModal({
      title: 'Sound aus Library löschen?',
      body: `"${sound.displayName}" wird aus allen Boards entfernt und die Datei gelöscht.`,
      confirmLabel: 'Sound löschen',
      action: async () => {
        app.stopPlayersForSound(soundId);
        const filename = sound.filename;
        delete app.state.library[soundId];

        app.getBoards().forEach((board) => {
          board.cells = board.cells.map((cell) => (cell?.soundId === soundId ? null : cell));
          board.unassignedSoundIds = board.unassignedSoundIds.filter((entry) => entry !== soundId);
        });

        app.ui.soundUrlCache.delete(filename);
        app.renderAll();
        await app.persistState({ syncHotkeys: true, showHotkeyErrors: true });
        await window.api.deleteSoundFile(filename);
        app.showToast(`"${sound.displayName}" gelöscht.`);
      },
    });
  };
})(window.SoundboardApp);
