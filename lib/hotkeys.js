function buildHotkeyEntries(boards = []) {
  const entries = [];

  boards.forEach((board) => {
    if (board.hotkey) {
      entries.push({
        type: 'board',
        accelerator: board.hotkey,
        boardId: board.id,
        label: board.name,
      });
    }

    board.cells.forEach((cell, index) => {
      if (cell?.hotkey) {
        entries.push({
          type: 'cell',
          accelerator: cell.hotkey,
          boardId: board.id,
          cellIndex: index,
          label: `${board.name} #${index + 1}`,
        });
      }
    });
  });

  return entries;
}

function validateHotkeyEntries(entries = []) {
  const seen = new Map();
  const conflicts = [];

  entries.forEach((entry) => {
    const accelerator = String(entry.accelerator || '').trim();
    if (!accelerator) return;
    const key = accelerator.toLowerCase();

    if (seen.has(key)) {
      conflicts.push({
        accelerator,
        first: seen.get(key),
        second: entry,
      });
      return;
    }

    seen.set(key, entry);
  });

  return {
    valid: conflicts.length === 0,
    conflicts,
  };
}

module.exports = {
  buildHotkeyEntries,
  validateHotkeyEntries,
};
