#pragma once

#include <QColor>
#include <QString>

class SoundInfo {
 public:
  SoundInfo();
  double getStartTime() const;
  double getPlayTime() const;
  static double getTimeUnitFactor(int unit);

  QString filename;
  QString customText;
  QColor customColor;
  int volume;
  bool cropEnabled;
  int cropStartValue;
  int cropStartUnit;
  int cropStopAfterAt;
  int cropStopValue;
  int cropStopUnit;
};
