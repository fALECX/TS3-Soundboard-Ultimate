const { randomUUID } = require('crypto');
const path = require('path');

const AUDIO_EXTENSIONS = new Set([
  '.mp3',
  '.wav',
  '.ogg',
  '.flac',
  '.m4a',
  '.aac',
  '.webm',
  '.opus',
  '.mp4',
]);

const STARTER_SAMPLES = [
  { filename: 'cigarette-light.wav', displayName: 'Cigarette Light', emoji: '🚬', tags: ['starter', 'roleplay', 'smoke'] },
  { filename: 'cigarette-inhale.wav', displayName: 'Cigarette Inhale', emoji: '🚬', tags: ['starter', 'roleplay', 'smoke'] },
  { filename: 'bone-crack.wav', displayName: 'Bone Crack', emoji: '💀', tags: ['starter', 'impact', 'body'] },
  { filename: 'handcuffs-click.wav', displayName: 'Handcuffs Click', emoji: '🔒', tags: ['starter', 'police', 'cuffs'] },
  { filename: 'medkit-open.wav', displayName: 'Medkit Open', emoji: '🩹', tags: ['starter', 'medical'] },
  { filename: 'police-radio-chatter.wav', displayName: 'Police Radio', emoji: '📻', tags: ['starter', 'police', 'radio'] },
  { filename: 'cash-count.wav', displayName: 'Cash Count', emoji: '💰', tags: ['starter', 'money'] },
  { filename: 'car-lock-beep.wav', displayName: 'Car Lock Beep', emoji: '🚗', tags: ['starter', 'vehicle'] },
];

const STARTER_SAMPLE_MAP = new Map(
  STARTER_SAMPLES.map((sample) => [sample.filename.toLowerCase(), sample])
);

function nowIso() {
  return new Date().toISOString();
}

function createSoundId() {
  return `sound_${randomUUID()}`;
}

function createBoardId() {
  return `board_${randomUUID()}`;
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function extractDisplayName(filename) {
  return path
    .basename(filename, path.extname(filename))
    .replace(/[_-]+/g, ' ')
    .replace(/\s+/g, ' ')
    .trim();
}

function sanitizeFilenameBase(name) {
  return (name || 'sound')
    .replace(/[<>:"/\\|?*\u0000-\u001f]/g, '_')
    .replace(/\s+/g, ' ')
    .trim()
    .slice(0, 80) || 'sound';
}

function ensureUniqueFilename(existingNames, desiredFilename) {
  const lowerExisting = new Set(
    Array.from(existingNames || []).map((name) => String(name).toLowerCase())
  );
  const ext = path.extname(desiredFilename);
  const base = path.basename(desiredFilename, ext);

  let candidate = `${base}${ext}`;
  let counter = 2;
  while (lowerExisting.has(candidate.toLowerCase())) {
    candidate = `${base} (${counter})${ext}`;
    counter += 1;
  }
  return candidate;
}

function createSoundRecord(input = {}) {
  const timestamp = input.createdAt || nowIso();
  return {
    soundId: input.soundId || createSoundId(),
    filename: input.filename,
    displayName: input.displayName || extractDisplayName(input.filename || ''),
    emoji: input.emoji || '🔊',
    color: input.color || null,
    sourceType: input.sourceType || 'local',
    sourceUrl: input.sourceUrl || '',
    tags: Array.isArray(input.tags) ? Array.from(new Set(input.tags.filter(Boolean))) : [],
    favorite: Boolean(input.favorite),
    gain: clamp(Number.isFinite(input.gain) ? input.gain : 1, 0, 2),
    trimStartMs: Math.max(0, Math.round(input.trimStartMs || 0)),
    trimEndMs: Math.max(0, Math.round(input.trimEndMs || 0)),
    loop: Boolean(input.loop),
    playbackMode: ['interrupt', 'restart', 'mix'].includes(input.playbackMode)
      ? input.playbackMode
      : 'interrupt',
    triggerMode: ['oneshot', 'toggle'].includes(input.triggerMode)
      ? input.triggerMode
      : 'oneshot',
    createdAt: timestamp,
    lastPlayedAt: input.lastPlayedAt || null,
    playCount: Number.isFinite(input.playCount) ? input.playCount : 0,
  };
}

function createBoardRecord(input = {}) {
  const cols = Number.isFinite(input.cols) ? input.cols : 4;
  const rows = Number.isFinite(input.rows) ? input.rows : 3;
  const total = cols * rows;
  const cells = Array.isArray(input.cells) ? input.cells.slice(0, total) : [];
  while (cells.length < total) cells.push(null);

  return {
    id: input.id || createBoardId(),
    name: input.name || 'Main Board',
    hotkey: input.hotkey || null,
    cols,
    rows,
    cells: cells.map((cell) => normalizeCell(cell)),
    unassignedSoundIds: Array.isArray(input.unassignedSoundIds)
      ? input.unassignedSoundIds.filter(Boolean)
      : [],
  };
}

function normalizeCell(cell) {
  if (!cell) return null;
  if (typeof cell === 'string') {
    return { soundId: cell, hotkey: null };
  }
  if (!cell.soundId) return null;
  return {
    soundId: cell.soundId,
    hotkey: cell.hotkey || null,
  };
}

function normalizeLibrary(library) {
  const result = {};
  if (!library) return result;

  if (Array.isArray(library)) {
    library.forEach((sound) => {
      if (sound?.filename) {
        const normalized = createSoundRecord(sound);
        result[normalized.soundId] = normalized;
      }
    });
    return result;
  }

  Object.values(library).forEach((sound) => {
    if (sound?.filename) {
      const normalized = createSoundRecord(sound);
      result[normalized.soundId] = normalized;
    }
  });
  return result;
}

function resizeBoard(board, cols, rows) {
  const normalizedBoard = createBoardRecord(board);
  const nextCols = Number.isFinite(cols) ? cols : normalizedBoard.cols;
  const nextRows = Number.isFinite(rows) ? rows : normalizedBoard.rows;
  const nextTotal = nextCols * nextRows;
  const overflowSoundIds = [];

  if (nextTotal < normalizedBoard.cells.length) {
    normalizedBoard.cells.slice(nextTotal).forEach((cell) => {
      if (cell?.soundId) overflowSoundIds.push(cell.soundId);
    });
  }

  const nextBoard = createBoardRecord({
    ...normalizedBoard,
    cols: nextCols,
    rows: nextRows,
    cells: normalizedBoard.cells.slice(0, nextTotal),
    unassignedSoundIds: normalizedBoard.unassignedSoundIds.concat(overflowSoundIds),
  });

  return { board: nextBoard, overflowSoundIds };
}

function buildInitialLibrary(soundFiles = [], legacyGridConfig = null) {
  const filenameToSoundId = new Map();
  const library = {};

  const addSound = (filename, overrides = {}) => {
    const key = filename.toLowerCase();
    if (filenameToSoundId.has(key)) return filenameToSoundId.get(key);

    const starter = STARTER_SAMPLE_MAP.get(key);
    const sound = createSoundRecord({
      filename,
      displayName: overrides.displayName || starter?.displayName || extractDisplayName(filename),
      emoji: overrides.emoji || starter?.emoji || '🔊',
      tags: overrides.tags || starter?.tags || [],
      sourceType: overrides.sourceType || 'local',
      sourceUrl: overrides.sourceUrl || '',
      hotkey: null,
    });
    library[sound.soundId] = sound;
    filenameToSoundId.set(key, sound.soundId);
    return sound.soundId;
  };

  soundFiles.forEach((filename) => addSound(filename));

  if (legacyGridConfig?.cells) {
    legacyGridConfig.cells.forEach((cell) => {
      if (cell?.filename) {
        addSound(cell.filename, {
          displayName: cell.label,
          emoji: cell.emoji,
          color: cell.color,
        });
      }
    });
  }

  return { library, filenameToSoundId };
}

function migrateLegacyData({ legacyGridConfig, soundFiles = [] }) {
  const { library, filenameToSoundId } = buildInitialLibrary(soundFiles, legacyGridConfig);
  const defaultBoard = createBoardRecord({
    name: 'Main Board',
    cols: legacyGridConfig?.gridCols || 4,
    rows: legacyGridConfig?.gridRows || 3,
  });

  const cells = Array.isArray(legacyGridConfig?.cells) ? legacyGridConfig.cells : [];
  cells.forEach((cell, index) => {
    if (!cell?.filename || index >= defaultBoard.cells.length) return;
    const soundId = filenameToSoundId.get(cell.filename.toLowerCase());
    if (!soundId) return;
    defaultBoard.cells[index] = {
      soundId,
      hotkey: cell.hotkey || null,
    };
  });

  return {
    library,
    boards: {
      version: 2,
      activeBoardId: defaultBoard.id,
      boards: [defaultBoard],
    },
  };
}

function normalizeBoardsConfig(config, library) {
  const libraryIds = new Set(Object.keys(library || {}));

  if (!config?.boards?.length) {
    const board = createBoardRecord();
    return {
      version: 2,
      activeBoardId: board.id,
      boards: [board],
    };
  }

  const boards = config.boards.map((board) => {
    const normalized = createBoardRecord(board);
    normalized.cells = normalized.cells.map((cell) =>
      cell?.soundId && libraryIds.has(cell.soundId) ? cell : null
    );
    normalized.unassignedSoundIds = normalized.unassignedSoundIds.filter((soundId) =>
      libraryIds.has(soundId)
    );
    return normalized;
  });

  const activeBoardId = boards.some((board) => board.id === config.activeBoardId)
    ? config.activeBoardId
    : boards[0].id;

  return {
    version: 2,
    activeBoardId,
    boards,
  };
}

function collectSoundFilesFromLibrary(library) {
  return new Set(Object.values(library || {}).map((sound) => sound.filename));
}

module.exports = {
  AUDIO_EXTENSIONS,
  STARTER_SAMPLES,
  clamp,
  collectSoundFilesFromLibrary,
  createBoardId,
  createBoardRecord,
  createSoundId,
  createSoundRecord,
  ensureUniqueFilename,
  extractDisplayName,
  migrateLegacyData,
  normalizeBoardsConfig,
  normalizeCell,
  normalizeLibrary,
  resizeBoard,
  sanitizeFilenameBase,
};
