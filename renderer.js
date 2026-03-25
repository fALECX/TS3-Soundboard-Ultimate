(function bootstrap(app) {
  app.keyEventToAccelerator = function keyEventToAccelerator(event) {
    const ignored = ['Control', 'Shift', 'Alt', 'Meta', 'AltGraph'];
    if (ignored.includes(event.key)) return null;

    const parts = [];
    if (event.ctrlKey) parts.push('Ctrl');
    if (event.altKey) parts.push('Alt');
    if (event.shiftKey) parts.push('Shift');

    const specialKeys = {
      ' ': 'Space',
      ArrowUp: 'Up',
      ArrowDown: 'Down',
      ArrowLeft: 'Left',
      ArrowRight: 'Right',
      Escape: 'Escape',
      Enter: 'Return',
      Tab: 'Tab',
      Backspace: 'Backspace',
      Delete: 'Delete',
      Home: 'Home',
      End: 'End',
      PageUp: 'PageUp',
      PageDown: 'PageDown',
      Insert: 'Insert',
    };

    const key = specialKeys[event.key] || (/^F\d+$/.test(event.key) ? event.key : event.key.toUpperCase());
    parts.push(key);
    return parts.join('+');
  };

  app.populateAudioDevices = async function populateAudioDevices() {
    const select = document.getElementById('output-device');
    select.innerHTML = '';

    const defaultOption = document.createElement('option');
    defaultOption.value = 'default';
    defaultOption.textContent = 'Standard';
    select.appendChild(defaultOption);

    try {
      const devices = await navigator.mediaDevices?.enumerateDevices?.();
      const seen = new Set(['default']);
      (devices || [])
        .filter((device) => device.kind === 'audiooutput' && device.deviceId && !seen.has(device.deviceId))
        .forEach((device) => {
          seen.add(device.deviceId);
          const option = document.createElement('option');
          option.value = device.deviceId;
          option.textContent = device.label || `Audioausgabe ${device.deviceId.slice(0, 8)}`;
          select.appendChild(option);
        });
    } catch (error) {
      console.warn('Could not enumerate audio devices:', error);
    }

    select.value = app.state.appConfig.outputDevice || 'default';
  };

  app.setLibraryCollapsed = function setLibraryCollapsed(collapsed) {
    app.ui.libraryCollapsed = Boolean(collapsed);
    document.body.classList.toggle('library-collapsed', app.ui.libraryCollapsed);

    const button = document.getElementById('btn-toggle-library');
    if (button) {
      button.textContent = app.ui.libraryCollapsed ? '>' : '<';
      button.title = app.ui.libraryCollapsed ? 'Soundliste anzeigen' : 'Soundliste einklappen';
    }
  };

  app.handleGridDrop = function handleGridDrop(event, boardId, targetIndex) {
    const payload = app.readDragPayload(event);
    if (!payload) return;

    const board = app.getBoardById(boardId);
    if (!board) return;

    if (payload.type === 'library') {
      app.assignSoundToCell(board, targetIndex, payload.soundId);
    }

    if (payload.type === 'unassigned') {
      const sourceBoard = app.getBoardById(payload.boardId);
      if (!sourceBoard) return;
      if (typeof payload.unassignedIndex === 'number') sourceBoard.unassignedSoundIds.splice(payload.unassignedIndex, 1);
      else app.removeSingleUnassigned(sourceBoard, payload.soundId);
      app.assignSoundToCell(board, targetIndex, payload.soundId);
    }

    if (payload.type === 'cell') {
      const sourceBoard = app.getBoardById(payload.boardId);
      if (!sourceBoard) return;
      if (payload.boardId === boardId && payload.cellIndex === targetIndex) return;
      const sourceCell = sourceBoard.cells[payload.cellIndex];
      if (!sourceCell?.soundId) return;
      sourceBoard.cells[payload.cellIndex] = null;
      app.assignSoundToCell(board, targetIndex, sourceCell.soundId);
    }

    app.renderAll();
    app.queuePersist({ syncHotkeys: false });
  };

  app.handleUnassignedDrop = function handleUnassignedDrop(event) {
    const payload = app.readDragPayload(event);
    const board = app.getActiveBoard();
    if (!payload || !board) return;

    if (payload.type === 'cell') {
      const sourceBoard = app.getBoardById(payload.boardId);
      if (!sourceBoard) return;
      const sourceCell = sourceBoard.cells[payload.cellIndex];
      if (!sourceCell?.soundId) return;
      sourceBoard.cells[payload.cellIndex] = null;
      board.unassignedSoundIds.push(sourceCell.soundId);
    }

    app.renderAll();
    app.queuePersist({ syncHotkeys: false });
  };

  app.handleHotkeyTrigger = function handleHotkeyTrigger(payload) {
    if (payload.type === 'board') {
      app.state.boards.activeBoardId = payload.boardId;
      app.renderAll();
      app.showToast(`Board gewechselt: ${app.getBoardById(payload.boardId)?.name || 'Board'}`);
      return;
    }

    if (payload.type === 'cell') {
      app.state.boards.activeBoardId = payload.boardId;
      app.renderAll();
      app.playCell(payload.boardId, payload.cellIndex);
    }
  };

  app.deleteCurrentBoard = function deleteCurrentBoard() {
    const board = app.getActiveBoard();
    if (!board) return;
    if (app.state.boards.boards.length === 1) {
      app.showToast('Mindestens ein Board muss erhalten bleiben.');
      return;
    }

    app.openConfirmModal({
      title: 'Board loeschen?',
      body: `"${board.name}" wird entfernt. Die Sounds bleiben in der Library.`,
      confirmLabel: 'Board loeschen',
      action: async () => {
        app.stopPlayersForBoard(board.id);
        app.state.boards.boards = app.state.boards.boards.filter((entry) => entry.id !== board.id);
        app.state.boards.activeBoardId = app.state.boards.boards[0].id;
        app.renderAll();
        await app.persistState({ syncHotkeys: true, showHotkeyErrors: true });
      },
    });
  };

  app.doSearch = async function doSearch(query) {
    app.showSearchResults();
    app.setSearchLoading(true);
    app.renderSearchWarnings([]);
    document.getElementById('search-results-list').innerHTML = '';
    document.getElementById('search-results-title').textContent = `Suche · ${query}`;

    const warnings = [];
    let youtubeResults = [];
    let freesoundResults = [];

    try {
      if (app.ui.searchSource === 'youtube' || app.ui.searchSource === 'all') {
        const response = await window.api.youtubeSearch(query);
        youtubeResults = response.results || [];
        if (response.error) warnings.push({ source: 'YouTube', message: response.warning || response.error });
      }

      if (app.ui.searchSource === 'freesound' || app.ui.searchSource === 'all') {
        const response = await window.api.freesoundSearch(query);
        freesoundResults = response.results || [];
        if (response.noApiKey) warnings.push({ source: 'Freesound', message: 'API Key fehlt.' });
        if (response.invalidKey) warnings.push({ source: 'Freesound', message: 'API Key ungueltig.' });
        if (response.error) warnings.push({ source: 'Freesound', message: response.error });
      }
    } catch (error) {
      warnings.push({ source: 'Search', message: error.message });
    }

    app.setSearchLoading(false);
    app.renderSearchWarnings(warnings);

    const resultsList = document.getElementById('search-results-list');
    resultsList.innerHTML = '';

    if (app.ui.searchSource === 'youtube') {
      if (!youtubeResults.length) app.renderSearchEmpty('Keine YouTube-Ergebnisse gefunden.');
      youtubeResults.forEach((result) => resultsList.appendChild(app.createYouTubeResultItem(result)));
      return;
    }

    if (app.ui.searchSource === 'freesound') {
      if (!freesoundResults.length) app.renderSearchEmpty('Keine Freesound-Ergebnisse gefunden.');
      freesoundResults.forEach((result) => resultsList.appendChild(app.createFreesoundResultItem(result)));
      return;
    }

    const longest = Math.max(youtubeResults.length, freesoundResults.length);
    if (!longest) {
      app.renderSearchEmpty('Keine Ergebnisse gefunden.');
      return;
    }

    for (let index = 0; index < longest; index += 1) {
      if (youtubeResults[index]) resultsList.appendChild(app.createYouTubeResultItem(youtubeResults[index]));
      if (freesoundResults[index]) resultsList.appendChild(app.createFreesoundResultItem(freesoundResults[index]));
    }
  };

  app.bindStaticEvents = function bindStaticEvents() {
    document.getElementById('btn-minimize').addEventListener('click', () => window.api.minimize());
    document.getElementById('btn-maximize').addEventListener('click', () => window.api.maximize());
    document.getElementById('btn-close').addEventListener('click', () => window.api.close());

    document.querySelectorAll('.source-btn').forEach((button) => {
      button.addEventListener('click', () => {
        app.ui.searchSource = button.dataset.source;
        app.renderSourceButtons();
        const query = document.getElementById('search-input').value.trim();
        if (query.length >= 2) app.doSearch(query);
      });
    });

    const searchInput = document.getElementById('search-input');
    searchInput.addEventListener('input', () => {
      clearTimeout(app.ui.searchTimer);
      const query = searchInput.value.trim();
      if (query.length < 2) {
        app.hideSearchResults();
        return;
      }
      app.ui.searchTimer = setTimeout(() => app.doSearch(query), 350);
    });
    searchInput.addEventListener('keydown', (event) => {
      if (event.key === 'Enter') {
        clearTimeout(app.ui.searchTimer);
        const query = searchInput.value.trim();
        if (query.length >= 2) app.doSearch(query);
      }
      if (event.key === 'Escape') app.hideSearchResults();
    });

    document.getElementById('search-close').addEventListener('click', app.hideSearchResults);
    document.getElementById('btn-import').addEventListener('click', app.importLocalFiles);
    document.getElementById('btn-settings').addEventListener('click', app.openSettingsModal);
    document.getElementById('btn-toggle-library').addEventListener('click', () => {
      app.setLibraryCollapsed(!app.ui.libraryCollapsed);
    });
    document.getElementById('btn-open-freesound-api').addEventListener('click', () => {
      window.api.openExternal('https://freesound.org/apiv2/apply');
    });

    document.getElementById('library-filter-input').addEventListener('input', app.renderLibrary);

    document.getElementById('board-select').addEventListener('change', (event) => {
      app.state.boards.activeBoardId = event.target.value;
      app.renderAll();
      app.queuePersist({ syncHotkeys: false });
    });

    document.getElementById('grid-size-select').addEventListener('change', (event) => {
      const board = app.getActiveBoard();
      if (!board) return;
      const [cols, rows] = event.target.value.split('x').map((value) => Number(value));
      if (!Number.isFinite(cols) || !Number.isFinite(rows)) return;
      app.resizeBoard(board.id, cols, rows);
    });

    document.getElementById('btn-replay-last').addEventListener('click', app.replayLastSound);
    document.getElementById('btn-stop-active').addEventListener('click', app.stopAllPlayers);

    const volumeSlider = document.getElementById('volume-slider');
    volumeSlider.addEventListener('input', () => {
      app.state.appConfig.masterVolume = Number(volumeSlider.value) / 100;
      app.updateMasterVolumeLabel();
      app.refreshPlayerVolumes();
      app.queuePersist({ syncHotkeys: false });
    });

    document.getElementById('settings-close').addEventListener('click', app.closeSettingsModal);
    document.getElementById('settings-modal').addEventListener('click', (event) => {
      if (event.target.id === 'settings-modal') app.closeSettingsModal();
    });

    document.getElementById('output-device').addEventListener('change', (event) => {
      app.state.appConfig.outputDevice = event.target.value;
      app.queuePersist({ syncHotkeys: false });
      app.refreshPlayerOutputDevices();
    });

    document.getElementById('global-hotkeys-toggle').addEventListener('change', async (event) => {
      app.state.appConfig.globalHotkeysEnabled = event.target.checked;
      app.queuePersist({ syncHotkeys: true, showHotkeyErrors: true });
      await app.syncHotkeys({ showErrors: true });
      app.renderAll();
    });

    document.getElementById('freesound-api-key').addEventListener('change', () => {
      app.state.appConfig.freesoundApiKey = document.getElementById('freesound-api-key').value.trim();
      app.queuePersist({ syncHotkeys: false });
      app.showToast(app.state.appConfig.freesoundApiKey ? 'Freesound API Key gespeichert.' : 'Freesound API Key entfernt.');
    });

    document.getElementById('sound-modal-close').addEventListener('click', app.closeSoundModal);
    document.getElementById('sound-cancel-btn').addEventListener('click', app.closeSoundModal);
    document.getElementById('sound-modal').addEventListener('click', (event) => {
      if (event.target.id === 'sound-modal') app.closeSoundModal();
    });
    document.getElementById('sound-gain-slider').addEventListener('input', app.updateSoundGainLabel);
    document.getElementById('sound-save-btn').addEventListener('click', app.saveSoundModal);
    document.getElementById('sound-delete-btn').addEventListener('click', () => {
      if (!app.ui.soundModalSoundId) return;
      app.closeSoundModal();
      app.confirmDeleteSound(app.ui.soundModalSoundId);
    });

    document.getElementById('board-modal-close').addEventListener('click', app.closeBoardModal);
    document.getElementById('board-cancel-btn').addEventListener('click', app.closeBoardModal);
    document.getElementById('board-save-btn').addEventListener('click', app.saveBoardModal);
    document.getElementById('btn-board-hotkey').addEventListener('click', () => {
      app.openHotkeyModal({
        type: 'board-draft',
        boardId: app.ui.boardModal.boardId,
        value: app.ui.boardModal.hotkey,
      });
    });
    document.getElementById('board-modal').addEventListener('click', (event) => {
      if (event.target.id === 'board-modal') app.closeBoardModal();
    });

    document.getElementById('confirm-close').addEventListener('click', app.closeConfirmModal);
    document.getElementById('confirm-cancel-btn').addEventListener('click', app.closeConfirmModal);
    document.getElementById('confirm-submit-btn').addEventListener('click', async () => {
      const action = app.ui.confirmAction;
      app.ui.confirmAction = null;
      app.closeConfirmModal();
      if (action) await action();
    });
    document.getElementById('confirm-modal').addEventListener('click', (event) => {
      if (event.target.id === 'confirm-modal') app.closeConfirmModal();
    });

    document.getElementById('hotkey-modal-close').addEventListener('click', app.closeHotkeyModal);
    document.getElementById('hotkey-cancel-btn').addEventListener('click', app.closeHotkeyModal);
    document.getElementById('hotkey-clear-btn').addEventListener('click', () => {
      app.ui.hotkeyModal.value = null;
      document.getElementById('hotkey-captured-display').textContent = 'Kein Hotkey';
      document.getElementById('hotkey-error').textContent = '';
    });
    document.getElementById('hotkey-save-btn').addEventListener('click', app.saveHotkeyModal);
    document.getElementById('hotkey-modal').addEventListener('click', (event) => {
      if (event.target.id === 'hotkey-modal') app.closeHotkeyModal();
    });
    document.getElementById('hotkey-capture-area').addEventListener('keydown', (event) => {
      event.preventDefault();
      const accelerator = app.keyEventToAccelerator(event);
      if (!accelerator) return;
      app.ui.hotkeyModal.value = accelerator;
      document.getElementById('hotkey-captured-display').textContent = accelerator;
      app.updateHotkeyConflictState();
    });

    document.getElementById('wizard-next-1').addEventListener('click', () => app.goToWizardStep(2));
    document.getElementById('wizard-open-freesound').addEventListener('click', () => {
      window.api.openExternal('https://freesound.org/apiv2/apply');
    });
    document.getElementById('wizard-skip-2').addEventListener('click', () => app.goToWizardStep(3));
    document.getElementById('wizard-next-2').addEventListener('click', () => {
      const apiKey = document.getElementById('wizard-api-key-input').value.trim();
      if (apiKey) {
        app.state.appConfig.freesoundApiKey = apiKey;
        app.queuePersist({ syncHotkeys: false });
      }
      app.goToWizardStep(3);
    });
    document.getElementById('wizard-open-vbcable').addEventListener('click', () => {
      window.api.openExternal('https://vb-audio.com/Cable/');
    });
    document.getElementById('wizard-finish').addEventListener('click', app.finishWizard);

    document.addEventListener('click', () => app.hideContextMenu());
    document.querySelectorAll('.ctx-item').forEach((button) => {
      button.addEventListener('click', () => {
        const action = button.dataset.action;
        const target = app.ui.contextTarget;
        app.hideContextMenu();
        if (!target) return;

        if (action === 'play') app.playCell(target.boardId, target.cellIndex);
        if (action === 'edit') app.openSoundModal(target.soundId);
        if (action === 'hotkey') {
          app.openHotkeyModal({
            type: 'cell',
            boardId: target.boardId,
            cellIndex: target.cellIndex,
            value: app.getBoardById(target.boardId)?.cells[target.cellIndex]?.hotkey || null,
          });
        }
        if (action === 'remove') app.removeSoundFromBoard(target.boardId, target.cellIndex);
        if (action === 'delete') app.confirmDeleteSound(target.soundId);
      });
    });
  };

  app.initialize = async function initialize() {
    app.bindStaticEvents();
    app.setLibraryCollapsed(false);
    const loaded = await window.api.loadState();
    app.applyLoadedState(loaded);
    app.updateMasterVolumeLabel();
    app.renderAll();
    await app.populateAudioDevices();
    await app.syncHotkeys();

    if (!app.state.appConfig.firstRunComplete) {
      app.openWizard();
    }

    window.api.onHotkeyTrigger(app.handleHotkeyTrigger);
    window.api.onDownloadProgress(app.handleDownloadProgress);
  };

  document.addEventListener('DOMContentLoaded', app.initialize);
})(window.SoundboardApp);
