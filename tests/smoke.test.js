const test = require('node:test');
const assert = require('node:assert/strict');

const {
  createBoardRecord,
  createSoundRecord,
  ensureUniqueFilename,
  migrateLegacyData,
  normalizeBoardsConfig,
  resizeBoard,
} = require('../lib/data-model');
const { buildHotkeyEntries, validateHotkeyEntries } = require('../lib/hotkeys');

test('ensureUniqueFilename appends numeric suffixes', () => {
  const existing = new Set(['radio.mp3', 'radio (2).mp3']);
  assert.equal(ensureUniqueFilename(existing, 'radio.mp3'), 'radio (3).mp3');
});

test('migrateLegacyData preserves repeated filename references', () => {
  const legacy = {
    gridCols: 4,
    gridRows: 3,
    cells: [
      { filename: 'radio.wav', label: 'Radio', hotkey: 'Ctrl+1' },
      null,
      { filename: 'radio.wav', label: 'Radio Again', hotkey: 'Ctrl+2' },
    ],
  };

  const migrated = migrateLegacyData({
    legacyGridConfig: legacy,
    soundFiles: ['radio.wav'],
  });

  const board = migrated.boards.boards[0];
  assert.ok(board.cells[0]);
  assert.ok(board.cells[2]);
  assert.equal(board.cells[0].soundId, board.cells[2].soundId);
});

test('resizeBoard moves truncated cells into the unassigned pool', () => {
  const board = createBoardRecord({
    cols: 4,
    rows: 3,
    cells: [
      { soundId: 'a' },
      { soundId: 'b' },
      { soundId: 'c' },
      { soundId: 'd' },
      { soundId: 'e' },
      { soundId: 'f' },
      { soundId: 'g' },
      { soundId: 'h' },
      { soundId: 'i' },
      { soundId: 'j' },
      { soundId: 'k' },
      { soundId: 'l' },
    ],
  });

  const { board: resized } = resizeBoard(board, 3, 2);
  assert.equal(resized.cells.length, 6);
  assert.deepEqual(resized.unassignedSoundIds, ['g', 'h', 'i', 'j', 'k', 'l']);
});

test('normalizeBoardsConfig drops references to missing sounds', () => {
  const library = {
    keep: createSoundRecord({ soundId: 'keep', filename: 'keep.wav' }),
  };
  const config = {
    activeBoardId: 'one',
    boards: [
      {
        id: 'one',
        name: 'Main',
        cols: 2,
        rows: 1,
        cells: [{ soundId: 'keep' }, { soundId: 'missing' }],
      },
    ],
  };

  const normalized = normalizeBoardsConfig(config, library);
  assert.ok(normalized.boards[0].cells[0]);
  assert.equal(normalized.boards[0].cells[1], null);
});

test('validateHotkeyEntries catches duplicates across boards and cells', () => {
  const boards = [
    createBoardRecord({
      id: 'board-1',
      name: 'One',
      hotkey: 'Ctrl+1',
      cells: [{ soundId: 'a', hotkey: 'Alt+1' }],
    }),
    createBoardRecord({
      id: 'board-2',
      name: 'Two',
      cells: [{ soundId: 'b', hotkey: 'Alt+1' }],
    }),
  ];

  const validation = validateHotkeyEntries(buildHotkeyEntries(boards));
  assert.equal(validation.valid, false);
  assert.equal(validation.conflicts.length, 1);
});
