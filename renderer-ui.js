(function registerUiHelpers(app) {
  app.createBadge = function createBadge(label) {
    const badge = document.createElement('span');
    badge.className = 'badge';
    badge.textContent = label;
    return badge;
  };

  app.createTagChip = function createTagChip(label) {
    const badge = document.createElement('span');
    badge.className = 'tag-chip';
    badge.textContent = label;
    return badge;
  };

  app.renderAll = function renderAll() {
    app.renderSourceButtons();
    app.renderBoardTabs();
    app.renderGrid();
    app.renderLibrary();
    app.renderSettingsSnapshot();
    app.updateNowPlaying();
  };

  app.renderSourceButtons = function renderSourceButtons() {
    const youtubeUnavailable = !app.state.status.youtube.available;
    document.querySelectorAll('.source-btn').forEach((button) => {
      button.classList.toggle('active', button.dataset.source === app.ui.searchSource);
      if (button.dataset.source === 'youtube') {
        button.classList.toggle('warning', youtubeUnavailable);
        button.title = youtubeUnavailable ? app.state.status.youtube.message : 'YouTube';
      }
    });
  };

  app.renderBoardTabs = function renderBoardTabs() {
    const select = document.getElementById('board-select');
    const title = document.getElementById('board-title');
    const activeBoard = app.getActiveBoard();

    if (title) title.textContent = activeBoard?.name || 'Board';
    if (select) {
      select.innerHTML = '';
      app.getBoards().forEach((board) => {
        const option = document.createElement('option');
        option.value = board.id;
        option.textContent = board.hotkey ? `${board.name} - ${board.hotkey}` : board.name;
        select.appendChild(option);
      });
      if (activeBoard) select.value = activeBoard.id;
    }
  };

  app.renderGrid = function renderGrid() {
    const board = app.getActiveBoard();
    const container = document.getElementById('grid-container');
    container.innerHTML = '';
    if (!board) return;

    container.style.setProperty('--grid-cols', board.cols);
    container.style.setProperty('--grid-rows', board.rows);

    const sizeSelect = document.getElementById('grid-size-select');
    if (sizeSelect) sizeSelect.value = `${board.cols}x${board.rows}`;

    board.cells.forEach((cell, index) => {
      const tile = document.createElement('div');
      tile.className = 'grid-cell';

      if (cell?.soundId) {
        const sound = app.getSound(cell.soundId);
        const isPlaying = app.ui.activePlayers.some(
          (player) => !player.preview && player.boardId === board.id && player.cellIndex === index
        );

        tile.classList.add('filled');
        if (isPlaying) tile.classList.add('playing');
        tile.draggable = true;
        tile.innerHTML = `
          <span class="cell-emoji">${sound?.emoji || 'S'}</span>
          <span class="cell-label">${app.escapeHtml(sound?.displayName || 'Missing Sound')}</span>
        `;

        if (cell.hotkey) {
          const hotkey = document.createElement('span');
          hotkey.className = 'cell-hotkey';
          hotkey.textContent = cell.hotkey;
          if (app.ui.hotkeyFailures.has(cell.hotkey.toLowerCase())) hotkey.classList.add('invalid');
          tile.appendChild(hotkey);
        }

        tile.addEventListener('click', () => app.playCell(board.id, index));
        tile.addEventListener('contextmenu', (event) => app.showContextMenu(event, board.id, index, cell.soundId));
        tile.addEventListener('dragstart', (event) => {
          event.dataTransfer?.setData(
            'application/json',
            JSON.stringify({
              type: 'cell',
              boardId: board.id,
              cellIndex: index,
              soundId: cell.soundId,
            })
          );
        });
      } else {
        tile.classList.add('empty');
        tile.innerHTML = '<span class="cell-add-icon">+</span><span class="cell-empty-label">Drop sound here</span>';
        tile.addEventListener('click', app.focusSearch);
      }

      tile.addEventListener('dragover', (event) => {
        event.preventDefault();
        tile.classList.add('drop-target');
      });
      tile.addEventListener('dragleave', () => tile.classList.remove('drop-target'));
      tile.addEventListener('drop', (event) => {
        event.preventDefault();
        tile.classList.remove('drop-target');
        app.handleGridDrop(event, board.id, index);
      });

      container.appendChild(tile);
    });
  };

  app.renderLibrary = function renderLibrary() {
    const libraryList = document.getElementById('library-list');
    const filter = document.getElementById('library-filter-input').value.trim().toLowerCase();

    const sounds = Object.values(app.state.library)
      .filter((sound) => {
        if (!filter) return true;
        const haystack = `${sound.displayName} ${(sound.tags || []).join(' ')}`.toLowerCase();
        return haystack.includes(filter);
      })
      .sort((left, right) => {
        if (left.favorite !== right.favorite) return left.favorite ? -1 : 1;
        return left.displayName.localeCompare(right.displayName);
      });

    libraryList.innerHTML = '';
    document.getElementById('library-count-pill').textContent = String(Object.keys(app.state.library).length);

    if (!sounds.length) {
      const empty = document.createElement('div');
      empty.className = 'empty-state';
      empty.textContent = 'No sounds in the current library view.';
      libraryList.appendChild(empty);
      return;
    }

    sounds.forEach((sound) => {
      const item = document.createElement('div');
      item.className = 'library-item';
      item.draggable = true;
      item.addEventListener('dragstart', (event) => {
        event.dataTransfer?.setData(
          'application/json',
          JSON.stringify({ type: 'library', soundId: sound.soundId })
        );
      });

      const main = document.createElement('div');
      main.className = 'library-item-main';

      const header = document.createElement('div');
      header.className = 'library-item-header';
      header.innerHTML = `<span class="library-item-emoji">${sound.emoji || 'S'}</span>`;

      const text = document.createElement('div');
      text.className = 'library-item-text';
      text.innerHTML = `
        <div class="library-item-title">${app.escapeHtml(sound.displayName)}</div>
        <div class="library-item-meta">${sound.sourceType.toUpperCase()} · ${sound.playbackMode}</div>
      `;
      header.appendChild(text);
      main.appendChild(header);

      if (sound.tags?.length) {
        const tagRow = document.createElement('div');
        tagRow.className = 'tag-row';
        sound.tags.slice(0, 3).forEach((tag) => tagRow.appendChild(app.createTagChip(tag)));
        main.appendChild(tagRow);
      }

      const actions = document.createElement('div');
      actions.className = 'library-item-actions';

      const playButton = document.createElement('button');
      playButton.className = 'icon-btn small';
      playButton.textContent = app.isSoundPlaying(sound.soundId) ? '[]' : '>';
      playButton.title = 'Preview';
      playButton.addEventListener('click', (event) => {
        event.stopPropagation();
        if (app.isSoundPlaying(sound.soundId)) app.stopPlayersForSound(sound.soundId);
        else app.playLibrarySound(sound.soundId);
      });

      const editButton = document.createElement('button');
      editButton.className = 'icon-btn small';
      editButton.textContent = 'E';
      editButton.title = 'Edit';
      editButton.addEventListener('click', (event) => {
        event.stopPropagation();
        app.openSoundModal(sound.soundId);
      });

      const deleteButton = document.createElement('button');
      deleteButton.className = 'icon-btn small danger';
      deleteButton.textContent = 'X';
      deleteButton.title = 'Delete';
      deleteButton.addEventListener('click', (event) => {
        event.stopPropagation();
        app.confirmDeleteSound(sound.soundId);
      });

      actions.append(playButton, editButton, deleteButton);
      item.append(main, actions);
      libraryList.appendChild(item);
    });
  };

  app.renderQueue = function renderQueue() {
    const container = document.getElementById('queue-list');
    if (!container) return;
    container.innerHTML = '';
  };

  app.renderStatusStrip = function renderStatusStrip() {
    const strip = document.getElementById('status-strip');
    if (!strip) return;
    strip.innerHTML = '';
  };

  app.renderSettingsSnapshot = function renderSettingsSnapshot() {
    document.getElementById('freesound-api-key').value = app.state.appConfig.freesoundApiKey || '';
    document.getElementById('global-hotkeys-toggle').checked = Boolean(app.state.appConfig.globalHotkeysEnabled);

    const title = document.getElementById('youtube-status-title');
    const body = document.getElementById('youtube-status-body');
    title.textContent = app.state.status.youtube.available ? 'YouTube available' : 'YouTube disabled';
    body.textContent = app.state.status.youtube.message;
    document.getElementById('youtube-status-card').classList.toggle('warning', !app.state.status.youtube.available);
  };

  app.updateNowPlaying = function updateNowPlaying() {
    const activeSounds = app.ui.activePlayers.filter((player) => !player.preview);
    const name = document.getElementById('now-playing-name');
    const meta = document.getElementById('now-playing-meta');

    if (!activeSounds.length) {
      name.textContent = 'No sound active';
      meta.textContent = '0 active playbacks';
      return;
    }

    const last = activeSounds[activeSounds.length - 1];
    const sound = app.getSound(last.soundId);
    name.textContent = `${sound?.emoji || 'S'} ${sound?.displayName || 'Sound'}`;
    meta.textContent = `${activeSounds.length} active playbacks`;
  };

  app.showSearchResults = function showSearchResults() {
    document.getElementById('search-results').classList.add('visible');
  };

  app.hideSearchResults = function hideSearchResults() {
    document.getElementById('search-results').classList.remove('visible');
    app.stopActivePreview();
  };

  app.setSearchLoading = function setSearchLoading(isLoading) {
    document.getElementById('search-loading').classList.toggle('visible', isLoading);
  };

  app.renderSearchWarnings = function renderSearchWarnings(warnings) {
    const container = document.getElementById('search-warnings');
    container.innerHTML = '';

    warnings.forEach((warning) => {
      const banner = document.createElement('div');
      banner.className = 'warning-banner';
      banner.textContent = `${warning.source}: ${warning.message}`;
      container.appendChild(banner);
    });
  };

  app.renderSearchEmpty = function renderSearchEmpty(message) {
    const resultsList = document.getElementById('search-results-list');
    resultsList.innerHTML = '';
    const empty = document.createElement('div');
    empty.className = 'empty-state';
    empty.textContent = message;
    resultsList.appendChild(empty);
  };

  app.createSearchResultBase = function createSearchResultBase(config) {
    const item = document.createElement('div');
    item.className = 'search-result-item';

    const previewButton = document.createElement('button');
    previewButton.className = 'result-preview-btn';
    previewButton.textContent = '>';

    const info = document.createElement('div');
    info.className = 'result-info';
    info.innerHTML = `
      <div class="result-title">${app.escapeHtml(config.title)}</div>
      <div class="result-meta">${app.escapeHtml(config.meta)}</div>
    `;

    const sourceLogo = document.createElement('img');
    sourceLogo.className = 'result-source-badge';
    sourceLogo.src = config.logo;
    sourceLogo.alt = config.logoTitle;
    sourceLogo.title = config.logoTitle;

    const addButton = document.createElement('button');
    addButton.className = 'result-add-btn';
    addButton.textContent = '+';

    item.append(previewButton, info, sourceLogo, addButton);
    return item;
  };

  app.createYouTubeResultItem = function createYouTubeResultItem(result) {
    const item = app.createSearchResultBase({
      title: result.title,
      meta: `${result.channel || 'YouTube'} · ${app.formatDuration(result.duration)}`,
      logo: 'assets/youtube-logo.svg',
      logoTitle: 'YouTube',
    });

    const previewButton = item.querySelector('.result-preview-btn');
    previewButton.addEventListener('click', async (event) => {
      event.stopPropagation();
      if (app.ui.activePreview?.button === previewButton) {
        app.stopActivePreview();
        return;
      }

      previewButton.textContent = '...';
      previewButton.disabled = true;
      const url = result.url || `https://www.youtube.com/watch?v=${result.id}`;
      const response = await window.api.youtubeGetStreamUrl(url);
      previewButton.disabled = false;

      if (!response.success) {
        previewButton.textContent = '>';
        app.showToast(response.error || 'YouTube preview unavailable.');
        return;
      }

      app.startPreview(response.url, previewButton);
    });

    item.querySelector('.result-add-btn').addEventListener('click', async (event) => {
      event.stopPropagation();
      const operationId = app.createOperation({
        label: `Download - ${result.title}`,
        source: 'youtube',
        status: 'queued',
        percent: 0,
      });

      const response = await window.api.youtubeDownload({
        url: result.url || `https://www.youtube.com/watch?v=${result.id}`,
        filenameBase: app.sanitizeFilenameBase(result.title),
        operationId,
      });

      if (!response.success) {
        app.updateOperation(operationId, {
          status: 'failed',
          statusText: response.error || 'Failed',
          percent: 0,
        });
        app.showToast(response.error || 'YouTube download failed.');
        return;
      }

      app.updateOperation(operationId, {
        status: 'done',
        statusText: 'Imported',
        percent: 100,
      });

      app.addImportedSound({
        filename: response.filename,
        displayName: result.title,
        sourceType: 'youtube',
        sourceUrl: result.url,
        tags: ['youtube'],
      });
    });

    return item;
  };

  app.createFreesoundResultItem = function createFreesoundResultItem(result) {
    const item = app.createSearchResultBase({
      title: result.name,
      meta: `Freesound · ${app.formatDuration(result.duration)}${result.tags?.length ? ` · ${result.tags.join(', ')}` : ''}`,
      logo: 'assets/freesound-logo.svg',
      logoTitle: 'Freesound',
    });

    const previewButton = item.querySelector('.result-preview-btn');
    previewButton.addEventListener('click', (event) => {
      event.stopPropagation();
      if (!result.preview) {
        app.showToast('No Freesound preview available.');
        return;
      }

      if (app.ui.activePreview?.button === previewButton) {
        app.stopActivePreview();
        return;
      }

      app.startPreview(result.preview, previewButton);
    });

    item.querySelector('.result-add-btn').addEventListener('click', async (event) => {
      event.stopPropagation();
      const operationId = app.createOperation({
        label: `Download - ${result.name}`,
        source: 'freesound',
        status: 'queued',
        percent: 0,
      });

      const response = await window.api.freesoundDownload({
        previewUrl: result.preview,
        filenameBase: app.sanitizeFilenameBase(result.name),
        operationId,
      });

      if (!response.success) {
        app.updateOperation(operationId, {
          status: 'failed',
          statusText: response.error || 'Failed',
          percent: 0,
        });
        app.showToast(response.error || 'Freesound download failed.');
        return;
      }

      app.updateOperation(operationId, {
        status: 'done',
        statusText: 'Imported',
        percent: 100,
      });

      app.addImportedSound({
        filename: response.filename,
        displayName: result.name,
        sourceType: 'freesound',
        sourceUrl: result.preview,
        tags: ['freesound', ...(result.tags || [])],
      });
    });

    return item;
  };

  app.showContextMenu = function showContextMenu(event, boardId, cellIndex, soundId) {
    event.preventDefault();
    event.stopPropagation();
    app.ui.contextTarget = { boardId, cellIndex, soundId };
    const menu = document.getElementById('context-menu');
    menu.style.left = `${event.clientX}px`;
    menu.style.top = `${event.clientY}px`;
    menu.classList.add('visible');
  };

  app.hideContextMenu = function hideContextMenu() {
    document.getElementById('context-menu').classList.remove('visible');
    app.ui.contextTarget = null;
  };

  app.openSettingsModal = async function openSettingsModal() {
    await app.populateAudioDevices();
    app.renderSettingsSnapshot();
    app.openModal('settings-modal');
  };

  app.closeSettingsModal = function closeSettingsModal() {
    app.closeModal('settings-modal');
  };

  app.openSoundModal = function openSoundModal(soundId) {
    const sound = app.getSound(soundId);
    if (!sound) return;

    app.ui.soundModalSoundId = soundId;
    document.getElementById('sound-modal-title').textContent = `Edit sound - ${sound.displayName}`;
    document.getElementById('sound-name-input').value = sound.displayName;
    document.getElementById('sound-emoji-input').value = sound.emoji || '';
    document.getElementById('sound-tags-input').value = (sound.tags || []).join(', ');
    document.getElementById('sound-favorite-toggle').checked = Boolean(sound.favorite);
    document.getElementById('sound-gain-slider').value = Math.round((sound.gain || 1) * 100);
    document.getElementById('sound-trim-start-input').value = sound.trimStartMs || 0;
    document.getElementById('sound-trim-end-input').value = sound.trimEndMs || 0;
    document.getElementById('sound-loop-toggle').checked = Boolean(sound.loop);
    document.getElementById('sound-playback-mode').value = sound.playbackMode || 'interrupt';
    document.getElementById('sound-trigger-mode').value = sound.triggerMode || 'oneshot';
    app.updateSoundGainLabel();
    app.openModal('sound-modal');
  };

  app.closeSoundModal = function closeSoundModal() {
    app.ui.soundModalSoundId = null;
    app.closeModal('sound-modal');
  };

  app.updateSoundGainLabel = function updateSoundGainLabel() {
    const gain = Number(document.getElementById('sound-gain-slider').value || 100);
    document.getElementById('sound-gain-label').textContent = `${gain}%`;
  };

  app.saveSoundModal = function saveSoundModal() {
    const sound = app.getSound(app.ui.soundModalSoundId);
    if (!sound) return;

    sound.displayName = document.getElementById('sound-name-input').value.trim() || sound.displayName;
    sound.emoji = document.getElementById('sound-emoji-input').value.trim() || app.guessEmoji(sound.displayName);
    sound.tags = document
      .getElementById('sound-tags-input')
      .value.split(',')
      .map((tag) => tag.trim())
      .filter(Boolean);
    sound.favorite = document.getElementById('sound-favorite-toggle').checked;
    sound.gain = Number(document.getElementById('sound-gain-slider').value || 100) / 100;
    sound.trimStartMs = Math.max(0, Number(document.getElementById('sound-trim-start-input').value || 0));
    sound.trimEndMs = Math.max(0, Number(document.getElementById('sound-trim-end-input').value || 0));
    sound.loop = document.getElementById('sound-loop-toggle').checked;
    sound.playbackMode = document.getElementById('sound-playback-mode').value;
    sound.triggerMode = document.getElementById('sound-trigger-mode').value;

    app.closeSoundModal();
    app.renderAll();
    app.queuePersist({ syncHotkeys: false });
  };

  app.openBoardModal = function openBoardModal(mode, board = null) {
    app.ui.boardModal.mode = mode;
    app.ui.boardModal.boardId = board?.id || null;
    app.ui.boardModal.hotkey = board?.hotkey || null;
    document.getElementById('board-modal-title').textContent =
      mode === 'create' ? 'New board' : `Edit board - ${board?.name || ''}`;
    document.getElementById('board-name-input').value = mode === 'create' ? '' : board?.name || '';
    document.getElementById('board-hotkey-display').textContent = board?.hotkey || 'None';
    app.openModal('board-modal');
  };

  app.closeBoardModal = function closeBoardModal() {
    app.closeModal('board-modal');
  };

  app.saveBoardModal = function saveBoardModal() {
    const name = document.getElementById('board-name-input').value.trim() || 'Board';
    const hotkey = app.ui.boardModal.hotkey || null;

    if (app.ui.boardModal.mode === 'create') {
      const active = app.getActiveBoard();
      const board = app.createBoardRecord({
        name,
        hotkey,
        cols: active?.cols || 4,
        rows: active?.rows || 3,
      });
      app.state.boards.boards.push(board);
      app.state.boards.activeBoardId = board.id;
      app.closeBoardModal();
      app.renderAll();
      app.queuePersist({ syncHotkeys: true, showHotkeyErrors: true });
      app.syncHotkeys({ showErrors: true });
      return;
    }

    const board = app.getBoardById(app.ui.boardModal.boardId);
    if (!board) return;
    board.name = name;
    board.hotkey = hotkey;
    app.closeBoardModal();
    app.renderAll();
    app.queuePersist({ syncHotkeys: true, showHotkeyErrors: true });
    app.syncHotkeys({ showErrors: true });
  };

  app.openConfirmModal = function openConfirmModal(config) {
    document.getElementById('confirm-title').textContent = config.title;
    document.getElementById('confirm-body').textContent = config.body;
    document.getElementById('confirm-submit-btn').textContent = config.confirmLabel || 'Confirm';
    app.ui.confirmAction = config.action || null;
    app.openModal('confirm-modal');
  };

  app.closeConfirmModal = function closeConfirmModal() {
    app.closeModal('confirm-modal');
  };

  app.openHotkeyModal = function openHotkeyModal(target) {
    app.ui.hotkeyModal.target = target;
    app.ui.hotkeyModal.value = target?.value || null;
    document.getElementById('hotkey-captured-display').textContent =
      app.ui.hotkeyModal.value || 'Press a key...';
    document.getElementById('hotkey-error').textContent = '';
    app.openModal('hotkey-modal');
    document.getElementById('hotkey-capture-area').focus();
    app.updateHotkeyConflictState();
  };

  app.closeHotkeyModal = function closeHotkeyModal() {
    app.ui.hotkeyModal.target = null;
    app.ui.hotkeyModal.value = null;
    app.closeModal('hotkey-modal');
  };

  app.updateHotkeyConflictState = function updateHotkeyConflictState() {
    const conflict = app.findHotkeyConflict(app.ui.hotkeyModal.value, app.ui.hotkeyModal.target);
    document.getElementById('hotkey-error').textContent = conflict || '';
  };

  app.saveHotkeyModal = function saveHotkeyModal() {
    const value = app.ui.hotkeyModal.value;
    const conflict = app.findHotkeyConflict(value, app.ui.hotkeyModal.target);
    if (conflict) {
      document.getElementById('hotkey-error').textContent = conflict;
      return;
    }

    const target = app.ui.hotkeyModal.target;
    if (!target) return;

    if (target.type === 'cell') {
      const board = app.getBoardById(target.boardId);
      if (board?.cells[target.cellIndex]) {
        board.cells[target.cellIndex].hotkey = value || null;
        app.renderAll();
        app.queuePersist({ syncHotkeys: true, showHotkeyErrors: true });
        app.syncHotkeys({ showErrors: true });
      }
    }

    if (target.type === 'board-draft') {
      app.ui.boardModal.hotkey = value || null;
      document.getElementById('board-hotkey-display').textContent = value || 'None';
    }

    app.closeHotkeyModal();
  };

  app.openWizard = function openWizard() {
    app.goToWizardStep(1);
    app.openModal('wizard-overlay');
  };

  app.goToWizardStep = function goToWizardStep(step) {
    document.querySelectorAll('.wizard-step').forEach((element) => {
      element.classList.toggle('active', element.id === `wizard-step-${step}`);
    });
  };

  app.finishWizard = async function finishWizard() {
    app.state.appConfig.firstRunComplete = true;
    app.closeModal('wizard-overlay');
    await app.persistState({ syncHotkeys: false });
  };

  app.focusSearch = function focusSearch() {
    document.getElementById('search-input').focus();
  };
})(window.SoundboardApp);
