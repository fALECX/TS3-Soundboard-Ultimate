#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace rpsu {

struct SoundRecord {
  QString soundId;
  QString filename;
  QString displayName;
  QString icon;
  QString color;
  QString sourceType;
  QString sourceUrl;
  QStringList tags;
  bool favorite = false;
  double gain = 1.0;
  int trimStartMs = 0;
  int trimEndMs = 0;
  bool loop = false;
  QString playbackMode = "interrupt";
  QString triggerMode = "oneshot";
  QString createdAt;
  QString lastPlayedAt;
  int playCount = 0;
};

struct Cell {
  QString soundId;
  QString hotkey;
};

struct BoardRecord {
  QString id;
  QString name;
  QString hotkey;
  int cols = 4;
  int rows = 3;
  QVector<Cell> cells;
  QStringList unassignedSoundIds;
};

struct PluginConfig {
  bool firstRunComplete = false;
  QString freesoundApiKey;
  double masterVolume = 0.8;
  bool globalHotkeysEnabled = true;
};

struct AppState {
  PluginConfig config;
  QVector<SoundRecord> library;
  QVector<BoardRecord> boards;
  QString activeBoardId;
};

struct HotkeyBinding {
  QString keyword;
  QString displayValue;
  QString boardId;
  int cellIndex = -1;
  bool isBoard = false;
};

QString createId(const QString& prefix);
QString nowIso();
QString sanitizeFilenameBase(const QString& value);
QString ensureUniqueFilename(const QStringList& existingNames, const QString& desiredFilename);
QString extractDisplayName(const QString& filename);

SoundRecord createSoundRecord(const QString& filename);
BoardRecord createBoardRecord(const QString& name = QStringLiteral("Main Board"), int cols = 4, int rows = 3);
AppState createDefaultState();

QJsonObject toJson(const SoundRecord& sound);
QJsonObject toJson(const BoardRecord& board);
QJsonObject toJson(const PluginConfig& config);

SoundRecord soundFromJson(const QJsonObject& object);
BoardRecord boardFromJson(const QJsonObject& object);
PluginConfig configFromJson(const QJsonObject& object);
AppState stateFromJson(const QJsonObject& libraryObject, const QJsonObject& boardsObject, const QJsonObject& configObject);

QVector<HotkeyBinding> buildHotkeyBindings(const AppState& state);
QString validateHotkeyConflict(const AppState& state, const QString& hotkey, const QString& boardId, int cellIndex);

}  // namespace rpsu
