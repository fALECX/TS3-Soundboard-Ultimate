#include "src/core/update_checker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace rpsu {

static const char* kReleasesApiUrl =
  "https://api.github.com/repos/fALECX/TS3-Soundboard-Ultimate/releases/latest";

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {
  nam_ = new QNetworkAccessManager(this);
}

void UpdateChecker::checkAsync(const QString& currentVersion) {
  QNetworkRequest request{QUrl(QString::fromLatin1(kReleasesApiUrl))};
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setRawHeader("User-Agent", "TS3-Soundboard-Ultimate-UpdateCheck");

  QNetworkReply* reply = nam_->get(request);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, currentVersion]() {
    onReplyFinished(reply, currentVersion);
  });
}

void UpdateChecker::onReplyFinished(QNetworkReply* reply, const QString& currentVersion) {
  reply->deleteLater();
  if (reply->error() != QNetworkReply::NoError) {
    return;
  }

  const QByteArray body = reply->readAll();
  const QJsonDocument doc = QJsonDocument::fromJson(body);
  if (!doc.isObject()) {
    return;
  }

  QString tagName = doc.object().value(QStringLiteral("tag_name")).toString();
  if (tagName.isEmpty()) {
    return;
  }
  if (tagName.startsWith(QLatin1Char('v'))) {
    tagName = tagName.mid(1);
  }

  const QString htmlUrl = doc.object().value(QStringLiteral("html_url")).toString();

  if (isNewer(tagName, currentVersion)) {
    Q_EMIT updateAvailable(tagName, htmlUrl);
  }
}

// Returns true if candidate version is strictly greater than current.
// Compares the first three dot-separated numeric components.
bool UpdateChecker::isNewer(const QString& candidate, const QString& current) {
  auto parse = [](const QString& v) -> std::tuple<int, int, int> {
    const QStringList parts = v.split(QLatin1Char('.'));
    const int major = parts.size() > 0 ? parts[0].toInt() : 0;
    const int minor = parts.size() > 1 ? parts[1].toInt() : 0;
    const int patch = parts.size() > 2 ? parts[2].toInt() : 0;
    return {major, minor, patch};
  };

  const auto [caMaj, caMin, caPat] = parse(candidate);
  const auto [cuMaj, cuMin, cuPat] = parse(current);

  if (caMaj != cuMaj) return caMaj > cuMaj;
  if (caMin != cuMin) return caMin > cuMin;
  return caPat > cuPat;
}

}  // namespace rpsu
