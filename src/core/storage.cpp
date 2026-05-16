#include "src/core/storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QProcessEnvironment>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

namespace rpsu {

namespace {

QJsonObject readJsonObject(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }

  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  return document.isObject() ? document.object() : QJsonObject{};
}

bool writeJsonObject(const QString& path, const QJsonObject& object) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
  return true;
}

bool boardConfigHasAssignedCells(const QJsonObject& boardsObject) {
  const QJsonArray boards = boardsObject.value(QStringLiteral("boards")).toArray();
  for (const QJsonValue& boardValue : boards) {
    if (!boardValue.isObject()) {
      continue;
    }

    const QJsonArray cells = boardValue.toObject().value(QStringLiteral("cells")).toArray();
    for (const QJsonValue& cellValue : cells) {
      if (cellValue.isObject() && !cellValue.toObject().value(QStringLiteral("soundId")).toString().isEmpty()) {
        return true;
      }
    }
  }
  return false;
}

bool hasUsableStateFiles(const QString& libraryPath, const QString& boardsPath) {
  const QJsonObject libraryObject = readJsonObject(libraryPath);
  if (!libraryObject.isEmpty()) {
    return true;
  }

  return boardConfigHasAssignedCells(readJsonObject(boardsPath));
}

QString copySound(const QString& sourcePath, const QString& targetDir, const QStringList& existingNames) {
  const QFileInfo info(sourcePath);
  const QString extension = info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix();
  const QString baseName = sanitizeFilenameBase(info.completeBaseName());
  const QString safeName = ensureUniqueFilename(existingNames, baseName + extension);
  const QString targetPath = QDir(targetDir).filePath(safeName);
  QFile::copy(sourcePath, targetPath);
  return safeName;
}

int probeSoundDurationMs(const QString& path) {
#ifdef _WIN32
  const QString alias = QStringLiteral("rpsu_meta_%1").arg(reinterpret_cast<quintptr>(&path));
  const QString normalizedPath = QDir::toNativeSeparators(path);
  auto sendCommand = [](const QString& command, QString* response = nullptr) {
    wchar_t buffer[256] = {0};
    const MCIERROR error = mciSendStringW(
      reinterpret_cast<LPCWSTR>(command.utf16()),
      response ? buffer : nullptr,
      response ? 256 : 0,
      NULL
    );
    if (error != 0) {
      return false;
    }
    if (response) {
      *response = QString::fromWCharArray(buffer);
    }
    return true;
  };

  if (!sendCommand(QStringLiteral("open \"%1\" alias %2").arg(normalizedPath, alias))) {
    return 0;
  }

  sendCommand(QStringLiteral("set %1 time format milliseconds").arg(alias));
  QString durationValue;
  const bool ok = sendCommand(QStringLiteral("status %1 length").arg(alias), &durationValue);
  sendCommand(QStringLiteral("close %1").arg(alias));
  return ok ? durationValue.toInt() : 0;
#else
  Q_UNUSED(path);
  return 0;
#endif
}

}  // namespace

StorageManager::StorageManager() {
  baseDir_ = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                 .filePath(QStringLiteral("RP Soundboard Ultimate"));
  libraryPath_ = QDir(baseDir_).filePath(QStringLiteral("library.json"));
  boardsPath_ = QDir(baseDir_).filePath(QStringLiteral("boards-config.json"));
  configPath_ = QDir(baseDir_).filePath(QStringLiteral("plugin-config.json"));
  ensureDirectories();
  migrateLegacyElectronDataIfNeeded();
}

QString StorageManager::soundsDir() const {
  return QDir(baseDir_).filePath(QStringLiteral("sounds"));
}

void StorageManager::ensureDirectories() const {
  QDir().mkpath(baseDir_);
  QDir().mkpath(soundsDir());
}

QStringList StorageManager::legacyCandidateDirs() const {
  QStringList roots;
  const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  if (env.contains(QStringLiteral("APPDATA"))) {
    roots.push_back(env.value(QStringLiteral("APPDATA")));
  }

  const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (!appData.isEmpty()) {
    QDir dir(appData);
    for (int i = 0; i < 3 && dir.cdUp(); ++i) {
      roots.push_back(dir.absolutePath());
    }
  }

  QStringList candidates;
  const QStringList names = {
    QStringLiteral("rp-soundboard-ultimate"),
    QStringLiteral("RP Soundboard Ultimate"),
    QStringLiteral("RP-Soundboard-Ultimate")
  };
  for (const QString& root : roots) {
    for (const QString& name : names) {
      const QString candidate = QDir(root).filePath(name);
      if (QDir(candidate).canonicalPath() != QDir(baseDir_).canonicalPath() && !candidates.contains(candidate)) {
        candidates.push_back(candidate);
      }
    }
  }
  return candidates;
}

void StorageManager::migrateLegacyElectronDataIfNeeded() const {
  if (hasUsableStateFiles(libraryPath_, boardsPath_)) {
    return;
  }

  for (const QString& candidate : legacyCandidateDirs()) {
    const QString legacyLibrary = QDir(candidate).filePath(QStringLiteral("library.json"));
    const QString legacyBoards = QDir(candidate).filePath(QStringLiteral("boards-config.json"));
    const QString legacyAppConfig = QDir(candidate).filePath(QStringLiteral("app-config.json"));
    if (!QFileInfo::exists(legacyLibrary) && !QFileInfo::exists(legacyBoards)) {
      continue;
    }

    const auto copyIfExists = [](const QString& src, const QString& dst) {
      if (QFileInfo::exists(src)) {
        QFile::remove(dst);
        QFile::copy(src, dst);
      }
    };
    copyIfExists(legacyLibrary, libraryPath_);
    copyIfExists(legacyBoards, boardsPath_);
    copyIfExists(legacyAppConfig, configPath_);

    const QString legacySounds = QDir(candidate).filePath(QStringLiteral("sounds"));
    if (QFileInfo::exists(legacySounds)) {
      const QDir sourceDir(legacySounds);
      const QStringList soundFiles = sourceDir.entryList(QDir::Files);
      for (const QString& filename : soundFiles) {
        const QString target = QDir(soundsDir()).filePath(filename);
        if (!QFileInfo::exists(target)) {
          QFile::copy(sourceDir.filePath(filename), target);
        }
      }
    }
    break;
  }
}

AppState StorageManager::loadState() {
  AppState state = stateFromJson(readJsonObject(libraryPath_), readJsonObject(boardsPath_), readJsonObject(configPath_));
  if (state.library.isEmpty()) {
    const QDir dir(soundsDir());
    const QStringList files = dir.entryList(QDir::Files);
    for (const QString& filename : files) {
      state.library.push_back(createSoundRecord(filename));
    }
  }
  bool metadataChanged = false;
  for (SoundRecord& sound : state.library) {
    if (sound.durationMs <= 0) {
      refreshSoundMetadata(sound);
      metadataChanged = metadataChanged || sound.durationMs > 0;
    }
  }
  if (state.boards.isEmpty()) {
    BoardRecord board = createBoardRecord();
    state.activeBoardId = board.id;
    state.boards.push_back(board);
  }
  if (metadataChanged) {
    saveState(state);
  }
  return state;
}

bool StorageManager::saveState(const AppState& state) const {
  QJsonObject libraryObject;
  for (const SoundRecord& sound : state.library) {
    libraryObject.insert(sound.soundId, toJson(sound));
  }

  QJsonArray boardsArray;
  for (const BoardRecord& board : state.boards) {
    boardsArray.push_back(toJson(board));
  }

  QJsonObject boardsObject;
  boardsObject.insert(QStringLiteral("activeBoardId"), state.activeBoardId);
  boardsObject.insert(QStringLiteral("boards"), boardsArray);

  return writeJsonObject(libraryPath_, libraryObject)
      && writeJsonObject(boardsPath_, boardsObject)
      && writeJsonObject(configPath_, toJson(state.config));
}

QString StorageManager::importSoundFile(const QString& sourcePath, AppState& state) const {
  QStringList existingNames;
  for (const SoundRecord& sound : state.library) {
    existingNames.push_back(sound.filename);
  }

  const QString filename = copySound(sourcePath, soundsDir(), existingNames);
  SoundRecord sound = createSoundRecord(filename);
  refreshSoundMetadata(sound);
  state.library.push_back(sound);
  return sound.soundId;
}

bool StorageManager::deleteSound(const QString& soundId, AppState& state) const {
  int index = -1;
  QString filename;
  for (int i = 0; i < state.library.size(); ++i) {
    if (state.library[i].soundId == soundId) {
      index = i;
      filename = state.library[i].filename;
      break;
    }
  }
  if (index < 0) {
    return false;
  }

  state.library.removeAt(index);
  for (BoardRecord& board : state.boards) {
    for (Cell& cell : board.cells) {
      if (cell.soundId == soundId) {
        cell.soundId.clear();
      }
    }
    board.unassignedSoundIds.removeAll(soundId);
  }

  QFile::remove(QDir(soundsDir()).filePath(filename));
  return true;
}

void StorageManager::refreshSoundMetadata(SoundRecord& sound) const {
  if (sound.filename.isEmpty()) {
    sound.durationMs = 0;
    return;
  }

  sound.durationMs = probeSoundDurationMs(QDir(soundsDir()).filePath(sound.filename));
}

}  // namespace rpsu
