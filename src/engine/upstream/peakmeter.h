#pragma once

#include <math.h>

class PeakMeter {
 private:
  static constexpr float minOutput = 0.001f;

 public:
  PeakMeter(float alpha, float beta, int hold) : alpha(alpha), beta(beta), hold(hold), output(minOutput), timer(0) {}

  inline float process(float sample) {
    const float absSample = fabs(sample);
    if (absSample > output) {
      output = alpha * absSample + (1.0f - alpha) * output;
      timer = hold;
    } else if (timer == 0) {
      output = (1.0f - beta) * output;
    } else {
      timer--;
    }
    if (output < minOutput) {
      output = minOutput;
    }
    return output;
  }

  inline short limit(float sample, float threshold) const {
    if (output > threshold) {
      sample *= threshold / output;
    }
    return static_cast<short>(sample + 0.5f);
  }

  inline float getOutput() const {
    return output;
  }

  inline void reset() {
    output = minOutput;
    timer = 0;
  }

 private:
  const float alpha;
  const float beta;
  const int hold;
  float output;
  int timer;
};
