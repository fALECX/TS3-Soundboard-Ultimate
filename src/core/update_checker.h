#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace rpsu {

class UpdateChecker : public QObject {
  Q_OBJECT
 public:
  explicit UpdateChecker(QObject* parent = nullptr);

  void checkAsync(const QString& currentVersion);

 Q_SIGNALS:
  void updateAvailable(const QString& latestVersion, const QString& releaseUrl);

 private:
  void onReplyFinished(QNetworkReply* reply, const QString& currentVersion);

  static bool isNewer(const QString& candidate, const QString& current);

  QNetworkAccessManager* nam_ = nullptr;
};

}  // namespace rpsu
