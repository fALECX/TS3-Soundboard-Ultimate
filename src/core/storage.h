#pragma once

#include <QString>

#include "src/core/models.h"

namespace rpsu {

class StorageManager {
 public:
  StorageManager();

  const QString& baseDir() const { return baseDir_; }
  QString soundsDir() const;

  AppState loadState();
  bool saveState(const AppState& state) const;
  QString importSoundFile(const QString& sourcePath, AppState& state) const;
  bool deleteSound(const QString& soundId, AppState& state) const;

 private:
  QString baseDir_;
  QString libraryPath_;
  QString boardsPath_;
  QString configPath_;

  void ensureDirectories() const;
  void migrateLegacyElectronDataIfNeeded() const;
  QStringList legacyCandidateDirs() const;
};

}  // namespace rpsu
