#include "src/engine/upstream/SoundInfo.h"

#include <algorithm>
#include <stdexcept>

SoundInfo::SoundInfo()
    : filename(QString()),
      customText(QString()),
      customColor(QColor(255, 255, 255, 0)),
      volume(0),
      cropEnabled(false),
      cropStartValue(0),
      cropStartUnit(0),
      cropStopAfterAt(0),
      cropStopValue(0),
      cropStopUnit(0) {}

double SoundInfo::getStartTime() const {
  if (!cropEnabled) {
    return 0.0;
  }
  return static_cast<double>(cropStartValue) * getTimeUnitFactor(cropStartUnit);
}

double SoundInfo::getPlayTime() const {
  if (!cropEnabled || cropStopValue <= 0) {
    return -1.0;
  }
  double t = static_cast<double>(cropStopValue) * getTimeUnitFactor(cropStopUnit);
  if (cropStopAfterAt == 1) {
    t -= getStartTime();
  }
  return std::max(t, 0.0);
}

double SoundInfo::getTimeUnitFactor(int unit) {
  switch (unit) {
    case 0:
      return 0.001;
    case 1:
      return 1.0;
    default:
      throw std::logic_error("No such unit");
  }
}
