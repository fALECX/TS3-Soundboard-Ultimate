window.SoundboardApp = {
  DEFAULT_APP_CONFIG: {
    firstRunComplete: false,
    freesoundApiKey: '',
    outputDevice: 'default',
    masterVolume: 0.8,
    globalHotkeysEnabled: true,
  },
  DEFAULT_BOARD_SIZES: [
    { cols: 3, rows: 2 },
    { cols: 4, rows: 3 },
    { cols: 4, rows: 4 },
    { cols: 5, rows: 4 },
  ],
  EMOJI_MAP: {
    radio: '📻',
    police: '🚨',
    siren: '🚨',
    handcuff: '🔒',
    lock: '🔒',
    money: '💰',
    cash: '💰',
    car: '🚗',
    engine: '🚗',
    med: '🩹',
    bone: '💀',
    smoke: '🚬',
    cigarette: '🚬',
    phone: '📱',
    gun: '🔫',
    rain: '🌧️',
    thunder: '⛈️',
    laugh: '😂',
    scream: '😱',
    door: '🚪',
    food: '🍔',
    beer: '🍺',
  },
  state: {
    appConfig: {},
    library: {},
    boards: {
      version: 2,
      activeBoardId: null,
      boards: [],
    },
    status: {
      youtube: {
        available: false,
        mode: 'missing',
        message: 'yt-dlp nicht gefunden.',
      },
    },
  },
  ui: {
    searchSource: 'all',
    libraryCollapsed: false,
    searchTimer: null,
    saveTimer: null,
    persistPromise: Promise.resolve(),
    soundUrlCache: new Map(),
    activePlayers: [],
    activePreview: null,
    lastPlayback: null,
    operations: [],
    contextTarget: null,
    soundModalSoundId: null,
    boardModal: {
      mode: 'create',
      boardId: null,
      hotkey: null,
    },
    confirmAction: null,
    hotkeyModal: {
      target: null,
      value: null,
    },
    hotkeyFailures: new Map(),
  },
};

(function registerStateHelpers(app) {
  app.createId = function createId(prefix) {
    return `${prefix}_${crypto.randomUUID()}`;
  };

  app.stripExtension = function stripExtension(filename) {
    return String(filename || '').replace(/\.[^.]+$/, '');
  };

  app.guessEmoji = function guessEmoji(name) {
    const lower = String(name || '').toLowerCase();
    for (const [keyword, emoji] of Object.entries(app.EMOJI_MAP)) {
      if (lower.includes(keyword)) return emoji;
    }
    return '🔊';
  };

  app.sanitizeFilenameBase = function sanitizeFilenameBase(name) {
    return (name || 'sound')
      .replace(/[<>:"/\\|?*\u0000-\u001f]/g, '_')
      .replace(/\s+/g, ' ')
      .trim()
      .slice(0, 80) || 'sound';
  };

  app.escapeHtml = function escapeHtml(value) {
    const div = document.createElement('div');
    div.textContent = value;
    return div.innerHTML;
  };

  app.clampVolume = function clampVolume(value) {
    return Math.max(0, Math.min(1, value));
  };

  app.formatDuration = function formatDuration(seconds) {
    if (!seconds) return '0:00';
    const total = Math.max(0, Math.round(seconds));
    const minutes = Math.floor(total / 60);
    const remainder = total % 60;
    return `${minutes}:${String(remainder).padStart(2, '0')}`;
  };

  app.showToast = function showToast(message) {
    const toast = document.getElementById('toast');
    document.getElementById('toast-message').textContent = message;
    toast.classList.add('visible');
    clearTimeout(app.showToast.timeoutId);
    app.showToast.timeoutId = setTimeout(() => {
      toast.classList.remove('visible');
    }, 3000);
  };

  app.openModal = function openModal(id) {
    document.getElementById(id).classList.add('visible');
  };

  app.closeModal = function closeModal(id) {
    document.getElementById(id).classList.remove('visible');
  };

  app.getBoards = function getBoards() {
    return app.state.boards.boards;
  };

  app.getBoardById = function getBoardById(boardId) {
    return app.getBoards().find((board) => board.id === boardId) || null;
  };

  app.getActiveBoard = function getActiveBoard() {
    return app.getBoardById(app.state.boards.activeBoardId);
  };

  app.getSound = function getSound(soundId) {
    return app.state.library[soundId] || null;
  };

  app.createBoardRecord = function createBoardRecord(input = {}) {
    const cols = Number.isFinite(input.cols) ? input.cols : 4;
    const rows = Number.isFinite(input.rows) ? input.rows : 3;
    const total = cols * rows;
    const cells = Array.isArray(input.cells) ? input.cells.slice(0, total) : [];
    while (cells.length < total) cells.push(null);

    return {
      id: input.id || app.createId('board'),
      name: input.name || 'Main Board',
      hotkey: input.hotkey || null,
      cols,
      rows,
      cells: cells.map((cell) => (cell?.soundId ? { soundId: cell.soundId, hotkey: cell.hotkey || null } : null)),
      unassignedSoundIds: Array.isArray(input.unassignedSoundIds)
        ? input.unassignedSoundIds.filter(Boolean)
        : [],
    };
  };

  app.createSoundRecord = function createSoundRecord(input = {}) {
    const timestamp = new Date().toISOString();
    return {
      soundId: input.soundId || app.createId('sound'),
      filename: input.filename,
      displayName: input.displayName || app.stripExtension(input.filename || 'Sound'),
      emoji: input.emoji || app.guessEmoji(input.displayName || input.filename || ''),
      color: input.color || null,
      sourceType: input.sourceType || 'local',
      sourceUrl: input.sourceUrl || '',
      tags: Array.isArray(input.tags) ? [...new Set(input.tags.filter(Boolean))] : [],
      favorite: Boolean(input.favorite),
      gain: Number.isFinite(input.gain) ? input.gain : 1,
      trimStartMs: Number.isFinite(input.trimStartMs) ? input.trimStartMs : 0,
      trimEndMs: Number.isFinite(input.trimEndMs) ? input.trimEndMs : 0,
      loop: Boolean(input.loop),
      playbackMode: ['interrupt', 'restart', 'mix'].includes(input.playbackMode)
        ? input.playbackMode
        : 'interrupt',
      triggerMode: ['oneshot', 'toggle'].includes(input.triggerMode)
        ? input.triggerMode
        : 'oneshot',
      createdAt: input.createdAt || timestamp,
      lastPlayedAt: input.lastPlayedAt || null,
      playCount: Number.isFinite(input.playCount) ? input.playCount : 0,
    };
  };

  app.applyLoadedState = function applyLoadedState(payload) {
    app.state.appConfig = { ...app.DEFAULT_APP_CONFIG, ...(payload?.appConfig || {}) };
    app.state.library = { ...(payload?.library || {}) };
    app.state.boards = payload?.boards || {
      version: 2,
      activeBoardId: null,
      boards: [],
    };
    app.state.status = payload?.status || app.state.status;
    app.normalizeClientState();
  };

  app.normalizeClientState = function normalizeClientState() {
    if (!Array.isArray(app.state.boards.boards)) app.state.boards.boards = [];
    app.state.boards.boards = app.state.boards.boards.map((board) => app.createBoardRecord(board));

    if (!app.state.boards.boards.length) {
      const board = app.createBoardRecord({ name: 'Main Board' });
      app.state.boards.boards = [board];
      app.state.boards.activeBoardId = board.id;
    }

    if (!app.state.boards.boards.some((board) => board.id === app.state.boards.activeBoardId)) {
      app.state.boards.activeBoardId = app.state.boards.boards[0].id;
    }
  };

  app.buildHotkeyEntries = function buildHotkeyEntries() {
    const entries = [];

    app.getBoards().forEach((board) => {
      if (board.hotkey) {
        entries.push({ type: 'board', accelerator: board.hotkey, boardId: board.id });
      }

      board.cells.forEach((cell, index) => {
        if (cell?.hotkey) {
          entries.push({
            type: 'cell',
            accelerator: cell.hotkey,
            boardId: board.id,
            cellIndex: index,
          });
        }
      });
    });

    return entries;
  };

  app.findHotkeyConflict = function findHotkeyConflict(accelerator, target) {
    if (!accelerator) return '';
    const normalized = accelerator.toLowerCase();

    for (const board of app.getBoards()) {
      if (board.hotkey && board.hotkey.toLowerCase() === normalized) {
        const ignore = target?.type === 'board-draft' && target.boardId === board.id;
        if (!ignore) return `Konflikt mit Board-Hotkey "${board.name}".`;
      }

      for (let index = 0; index < board.cells.length; index += 1) {
        const cell = board.cells[index];
        if (!cell?.hotkey) continue;
        if (cell.hotkey.toLowerCase() !== normalized) continue;
        const ignore =
          target?.type === 'cell' &&
          target.boardId === board.id &&
          target.cellIndex === index;
        if (!ignore) return `Konflikt mit ${board.name} · Slot ${index + 1}.`;
      }
    }

    return '';
  };

  app.syncHotkeys = async function syncHotkeys(options = {}) {
    app.ui.hotkeyFailures.clear();
    const response = await window.api.registerHotkeys({
      enabled: app.state.appConfig.globalHotkeysEnabled,
      entries: app.buildHotkeyEntries(),
    });

    if (response.conflicts?.length) {
      if (options.showErrors) app.showToast('Doppelte Hotkeys erkannt. Bitte Konflikte auflösen.');
      return response;
    }

    (response.results || []).forEach((result) => {
      if (!result.success) {
        app.ui.hotkeyFailures.set(result.accelerator.toLowerCase(), true);
      }
    });

    if (options.showErrors && app.ui.hotkeyFailures.size) {
      app.showToast('Einige Hotkeys konnten systemweit nicht registriert werden.');
    }

    return response;
  };

  app.persistState = async function persistState(options = {}) {
    const payload = {
      appConfig: app.state.appConfig,
      library: app.state.library,
      boards: app.state.boards,
    };

    app.ui.persistPromise = app.ui.persistPromise.then(async () => {
      const saved = await window.api.saveState(payload);
      app.applyLoadedState(saved);
      if (options.syncHotkeys) {
        await app.syncHotkeys({ showErrors: options.showHotkeyErrors });
      }
      app.renderAll();
    });

    return app.ui.persistPromise;
  };

  app.queuePersist = function queuePersist(options = {}) {
    clearTimeout(app.ui.saveTimer);
    app.ui.saveTimer = setTimeout(() => {
      app.persistState({
        syncHotkeys: options.syncHotkeys,
        showHotkeyErrors: options.showHotkeyErrors,
      });
    }, 150);
  };

  app.createOperation = function createOperation(operation) {
    const entry = {
      id: operation.id || app.createId('operation'),
      label: operation.label,
      source: operation.source,
      status: operation.status || 'queued',
      statusText: operation.statusText || operation.status || 'queued',
      percent: operation.percent || 0,
    };
    app.ui.operations.unshift(entry);
    app.renderQueue();
    return entry.id;
  };

  app.updateOperation = function updateOperation(operationId, patch) {
    const operation = app.ui.operations.find((entry) => entry.id === operationId);
    if (!operation) return;
    Object.assign(operation, patch);
    app.renderQueue();
  };

  app.handleDownloadProgress = function handleDownloadProgress(payload) {
    app.updateOperation(payload.operationId, {
      status: payload.status,
      statusText: payload.label || payload.status,
      percent: payload.percent ?? 0,
    });
  };

  app.readDragPayload = function readDragPayload(event) {
    try {
      const raw = event.dataTransfer?.getData('application/json');
      return raw ? JSON.parse(raw) : null;
    } catch (error) {
      return null;
    }
  };
})(window.SoundboardApp);
