#include "src/core/storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

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

QString copySound(const QString& sourcePath, const QString& targetDir, const QStringList& existingNames) {
  const QFileInfo info(sourcePath);
  const QString extension = info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix();
  const QString baseName = sanitizeFilenameBase(info.completeBaseName());
  const QString safeName = ensureUniqueFilename(existingNames, baseName + extension);
  const QString targetPath = QDir(targetDir).filePath(safeName);
  QFile::copy(sourcePath, targetPath);
  return safeName;
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
  const QString roaming = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return {
    QDir(roaming).filePath(QStringLiteral("rp-soundboard-ultimate")),
    QDir(roaming).filePath(QStringLiteral("RP Soundboard Ultimate")),
    QDir(roaming).filePath(QStringLiteral("RP-Soundboard-Ultimate"))
  };
}

void StorageManager::migrateLegacyElectronDataIfNeeded() const {
  if (QFileInfo::exists(libraryPath_) || QFileInfo::exists(boardsPath_)) {
    return;
  }

  for (const QString& candidate : legacyCandidateDirs()) {
    const QString legacyLibrary = QDir(candidate).filePath(QStringLiteral("library.json"));
    const QString legacyBoards = QDir(candidate).filePath(QStringLiteral("boards-config.json"));
    const QString legacyAppConfig = QDir(candidate).filePath(QStringLiteral("app-config.json"));
    if (!QFileInfo::exists(legacyLibrary) && !QFileInfo::exists(legacyBoards)) {
      continue;
    }

    if (QFileInfo::exists(legacyLibrary)) {
      QFile::copy(legacyLibrary, libraryPath_);
    }
    if (QFileInfo::exists(legacyBoards)) {
      QFile::copy(legacyBoards, boardsPath_);
    }
    if (QFileInfo::exists(legacyAppConfig)) {
      QFile::copy(legacyAppConfig, configPath_);
    }

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
  if (state.boards.isEmpty()) {
    BoardRecord board = createBoardRecord();
    state.activeBoardId = board.id;
    state.boards.push_back(board);
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
        cell = {};
      }
    }
    board.unassignedSoundIds.removeAll(soundId);
  }

  QFile::remove(QDir(soundsDir()).filePath(filename));
  return true;
}

}  // namespace rpsu
