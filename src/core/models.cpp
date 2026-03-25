#include "src/core/models.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUuid>

namespace rpsu {

namespace {

Cell cellFromJson(const QJsonValue& value) {
  if (!value.isObject()) {
    return {};
  }

  const QJsonObject object = value.toObject();
  Cell cell;
  cell.soundId = object.value(QStringLiteral("soundId")).toString();
  cell.hotkey = object.value(QStringLiteral("hotkey")).toString();
  return cell;
}

QJsonObject cellToJson(const Cell& cell) {
  QJsonObject object;
  object.insert(QStringLiteral("soundId"), cell.soundId);
  object.insert(QStringLiteral("hotkey"), cell.hotkey);
  return object;
}

}  // namespace

QString createId(const QString& prefix) {
  return prefix + QStringLiteral("_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString nowIso() {
  return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QString sanitizeFilenameBase(const QString& value) {
  QString sanitized = value.trimmed();
  if (sanitized.isEmpty()) {
    sanitized = QStringLiteral("sound");
  }

  static const QString illegal = QStringLiteral("<>:\"/\\|?*");
  for (const QChar ch : illegal) {
    sanitized.replace(ch, QChar('_'));
  }

  sanitized.replace(QRegularExpression(QStringLiteral("[\\x00-\\x1f]")), QStringLiteral("_"));
  return sanitized.left(80);
}

QString ensureUniqueFilename(const QStringList& existingNames, const QString& desiredFilename) {
  const QFileInfo info(desiredFilename);
  const QString base = info.completeBaseName();
  const QString suffix = info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix();
  QString candidate = desiredFilename;
  int counter = 2;

  auto containsName = [&](const QString& name) {
    for (const QString& existing : existingNames) {
      if (existing.compare(name, Qt::CaseInsensitive) == 0) {
        return true;
      }
    }
    return false;
  };

  while (containsName(candidate)) {
    candidate = QStringLiteral("%1 (%2)%3").arg(base, QString::number(counter), suffix);
    ++counter;
  }

  return candidate;
}

QString extractDisplayName(const QString& filename) {
  QString result = QFileInfo(filename).completeBaseName();
  result.replace(QRegularExpression(QStringLiteral("[_-]+")), QStringLiteral(" "));
  result = result.simplified();
  return result.isEmpty() ? QStringLiteral("Sound") : result;
}

SoundRecord createSoundRecord(const QString& filename) {
  SoundRecord sound;
  sound.soundId = createId(QStringLiteral("sound"));
  sound.filename = filename;
  sound.displayName = extractDisplayName(filename);
  sound.icon = QStringLiteral("S");
  sound.sourceType = QStringLiteral("local");
  sound.createdAt = nowIso();
  return sound;
}

BoardRecord createBoardRecord(const QString& name, int cols, int rows) {
  BoardRecord board;
  board.id = createId(QStringLiteral("board"));
  board.name = name;
  board.cols = cols;
  board.rows = rows;
  board.cells.resize(cols * rows);
  return board;
}

AppState createDefaultState() {
  AppState state;
  BoardRecord board = createBoardRecord();
  state.activeBoardId = board.id;
  state.boards.push_back(board);
  return state;
}

QJsonObject toJson(const SoundRecord& sound) {
  QJsonObject object;
  object.insert(QStringLiteral("soundId"), sound.soundId);
  object.insert(QStringLiteral("filename"), sound.filename);
  object.insert(QStringLiteral("displayName"), sound.displayName);
  object.insert(QStringLiteral("icon"), sound.icon);
  object.insert(QStringLiteral("color"), sound.color);
  object.insert(QStringLiteral("sourceType"), sound.sourceType);
  object.insert(QStringLiteral("sourceUrl"), sound.sourceUrl);
  object.insert(QStringLiteral("favorite"), sound.favorite);
  object.insert(QStringLiteral("gain"), sound.gain);
  object.insert(QStringLiteral("trimStartMs"), sound.trimStartMs);
  object.insert(QStringLiteral("trimEndMs"), sound.trimEndMs);
  object.insert(QStringLiteral("loop"), sound.loop);
  object.insert(QStringLiteral("playbackMode"), sound.playbackMode);
  object.insert(QStringLiteral("triggerMode"), sound.triggerMode);
  object.insert(QStringLiteral("createdAt"), sound.createdAt);
  object.insert(QStringLiteral("lastPlayedAt"), sound.lastPlayedAt);
  object.insert(QStringLiteral("playCount"), sound.playCount);

  QJsonArray tags;
  for (const QString& tag : sound.tags) {
    tags.push_back(tag);
  }
  object.insert(QStringLiteral("tags"), tags);
  return object;
}

QJsonObject toJson(const BoardRecord& board) {
  QJsonObject object;
  object.insert(QStringLiteral("id"), board.id);
  object.insert(QStringLiteral("name"), board.name);
  object.insert(QStringLiteral("hotkey"), board.hotkey);
  object.insert(QStringLiteral("cols"), board.cols);
  object.insert(QStringLiteral("rows"), board.rows);

  QJsonArray cells;
  for (const Cell& cell : board.cells) {
    if (cell.soundId.isEmpty()) {
      cells.push_back(QJsonValue::Null);
    } else {
      cells.push_back(cellToJson(cell));
    }
  }
  object.insert(QStringLiteral("cells"), cells);

  QJsonArray unassigned;
  for (const QString& soundId : board.unassignedSoundIds) {
    unassigned.push_back(soundId);
  }
  object.insert(QStringLiteral("unassignedSoundIds"), unassigned);
  return object;
}

QJsonObject toJson(const PluginConfig& config) {
  QJsonObject object;
  object.insert(QStringLiteral("firstRunComplete"), config.firstRunComplete);
  object.insert(QStringLiteral("freesoundApiKey"), config.freesoundApiKey);
  object.insert(QStringLiteral("masterVolume"), config.masterVolume);
  object.insert(QStringLiteral("globalHotkeysEnabled"), config.globalHotkeysEnabled);
  return object;
}

SoundRecord soundFromJson(const QJsonObject& object) {
  SoundRecord sound;
  sound.soundId = object.value(QStringLiteral("soundId")).toString(createId(QStringLiteral("sound")));
  sound.filename = object.value(QStringLiteral("filename")).toString();
  sound.displayName = object.value(QStringLiteral("displayName")).toString(extractDisplayName(sound.filename));
  sound.icon = object.value(QStringLiteral("icon")).toString(QStringLiteral("S"));
  sound.color = object.value(QStringLiteral("color")).toString();
  sound.sourceType = object.value(QStringLiteral("sourceType")).toString(QStringLiteral("local"));
  sound.sourceUrl = object.value(QStringLiteral("sourceUrl")).toString();
  sound.favorite = object.value(QStringLiteral("favorite")).toBool(false);
  sound.gain = object.value(QStringLiteral("gain")).toDouble(1.0);
  sound.trimStartMs = object.value(QStringLiteral("trimStartMs")).toInt(0);
  sound.trimEndMs = object.value(QStringLiteral("trimEndMs")).toInt(0);
  sound.loop = object.value(QStringLiteral("loop")).toBool(false);
  sound.playbackMode = object.value(QStringLiteral("playbackMode")).toString(QStringLiteral("interrupt"));
  sound.triggerMode = object.value(QStringLiteral("triggerMode")).toString(QStringLiteral("oneshot"));
  sound.createdAt = object.value(QStringLiteral("createdAt")).toString(nowIso());
  sound.lastPlayedAt = object.value(QStringLiteral("lastPlayedAt")).toString();
  sound.playCount = object.value(QStringLiteral("playCount")).toInt(0);

  const QJsonArray tags = object.value(QStringLiteral("tags")).toArray();
  for (const QJsonValue& tag : tags) {
    sound.tags.push_back(tag.toString());
  }
  return sound;
}

BoardRecord boardFromJson(const QJsonObject& object) {
  BoardRecord board;
  board.id = object.value(QStringLiteral("id")).toString(createId(QStringLiteral("board")));
  board.name = object.value(QStringLiteral("name")).toString(QStringLiteral("Main Board"));
  board.hotkey = object.value(QStringLiteral("hotkey")).toString();
  board.cols = object.value(QStringLiteral("cols")).toInt(4);
  board.rows = object.value(QStringLiteral("rows")).toInt(3);

  const int total = qMax(1, board.cols * board.rows);
  board.cells.resize(total);
  const QJsonArray cells = object.value(QStringLiteral("cells")).toArray();
  for (int index = 0; index < cells.size() && index < total; ++index) {
    board.cells[index] = cellFromJson(cells.at(index));
  }

  const QJsonArray unassigned = object.value(QStringLiteral("unassignedSoundIds")).toArray();
  for (const QJsonValue& value : unassigned) {
    board.unassignedSoundIds.push_back(value.toString());
  }
  return board;
}

PluginConfig configFromJson(const QJsonObject& object) {
  PluginConfig config;
  config.firstRunComplete = object.value(QStringLiteral("firstRunComplete")).toBool(false);
  config.freesoundApiKey = object.value(QStringLiteral("freesoundApiKey")).toString();
  config.masterVolume = object.value(QStringLiteral("masterVolume")).toDouble(0.8);
  config.globalHotkeysEnabled = object.value(QStringLiteral("globalHotkeysEnabled")).toBool(true);
  return config;
}

AppState stateFromJson(const QJsonObject& libraryObject, const QJsonObject& boardsObject, const QJsonObject& configObject) {
  AppState state = createDefaultState();
  state.config = configFromJson(configObject);
  state.library.clear();
  state.boards.clear();

  for (auto it = libraryObject.begin(); it != libraryObject.end(); ++it) {
    if (it.value().isObject()) {
      state.library.push_back(soundFromJson(it.value().toObject()));
    }
  }

  if (boardsObject.contains(QStringLiteral("boards")) && boardsObject.value(QStringLiteral("boards")).isArray()) {
    const QJsonArray boards = boardsObject.value(QStringLiteral("boards")).toArray();
    for (const QJsonValue& value : boards) {
      if (value.isObject()) {
        state.boards.push_back(boardFromJson(value.toObject()));
      }
    }
    state.activeBoardId = boardsObject.value(QStringLiteral("activeBoardId")).toString();
  }

  if (state.boards.isEmpty()) {
    state.boards.push_back(createBoardRecord());
  }
  if (state.activeBoardId.isEmpty()) {
    state.activeBoardId = state.boards.front().id;
  }
  return state;
}

QVector<HotkeyBinding> buildHotkeyBindings(const AppState& state) {
  QVector<HotkeyBinding> bindings;
  for (const BoardRecord& board : state.boards) {
    if (!board.hotkey.isEmpty()) {
      HotkeyBinding binding;
      binding.keyword = QStringLiteral("board:%1").arg(board.id);
      binding.displayValue = board.hotkey;
      binding.boardId = board.id;
      binding.isBoard = true;
      bindings.push_back(binding);
    }

    for (int index = 0; index < board.cells.size(); ++index) {
      if (board.cells[index].hotkey.isEmpty()) {
        continue;
      }
      HotkeyBinding binding;
      binding.keyword = QStringLiteral("cell:%1:%2").arg(board.id, QString::number(index));
      binding.displayValue = board.cells[index].hotkey;
      binding.boardId = board.id;
      binding.cellIndex = index;
      bindings.push_back(binding);
    }
  }
  return bindings;
}

QString validateHotkeyConflict(const AppState& state, const QString& hotkey, const QString& boardId, int cellIndex) {
  if (hotkey.trimmed().isEmpty()) {
    return QString();
  }

  const QString normalized = hotkey.trimmed().toLower();
  const QVector<HotkeyBinding> bindings = buildHotkeyBindings(state);
  for (const HotkeyBinding& binding : bindings) {
    if (binding.displayValue.trimmed().toLower() != normalized) {
      continue;
    }
    if (binding.boardId == boardId && binding.cellIndex == cellIndex) {
      continue;
    }
    return QStringLiteral("Hotkey already used by another binding");
  }
  return QString();
}

}  // namespace rpsu
