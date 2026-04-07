#pragma once

#include <QString>
#include <QVector>

namespace rpsu {

struct YouTubeSearchResult {
  QString id;
  QString title;
  QString url;
  QString channel;
  int durationSeconds = 0;
};

class YouTubeService {
 public:
  QVector<YouTubeSearchResult> search(const QString& query, int limit, QString* errorMessage) const;
  bool downloadAudio(
    const YouTubeSearchResult& result,
    const QString& soundsDir,
    const QStringList& existingNames,
    QString* importedFilename,
    QString* errorMessage
  ) const;

 private:
  QString resolveYtDlpPath(QString* errorMessage) const;
  QString resolveFfmpegPath(QString* errorMessage) const;
};

}  // namespace rpsu
