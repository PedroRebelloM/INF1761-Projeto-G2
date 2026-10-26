#include <memory>
class SpotLight;
using SpotLightPtr = std::shared_ptr<SpotLight>;

#ifndef SPOTLIGHT_H
#define SPOTLIGHT_H

#include "light.h"

// Spot light defined by its position, direction, cutoff angle (theta),
// and concentration exponent (sexp).
class SpotLight : public Light {
protected:
  SpotLight (float px, float py, float pz,
             float dx, float dy, float dz,
             float cutoffDegrees, float exponent,
             const std::string& space);

public:
  static SpotLightPtr Make (float px, float py, float pz,
                            float dx, float dy, float dz,
                            float cutoffDegrees, float exponent = 10.0f,
                            const std::string& space = "world");

  void SetPosition (float x, float y, float z);
  void SetDirection (float x, float y, float z);
  void SetCutoff (float cutoffDegrees);
  void SetExponent (float exponent);

  std::map<std::string, UniformValue> Map (const Mat4& matrix) const override;
};

#endif
