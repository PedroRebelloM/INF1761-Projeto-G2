#include "spotlight.h"
#include <cmath>

SpotLight::SpotLight (float px, float py, float pz,
                      float dx, float dy, float dz,
                      float cutoffDegrees, float exponent,
                      const std::string& space)
  : Light(space)
{
  SetPosition(px, py, pz);
  SetDirection(dx, dy, dz);
  SetCutoff(cutoffDegrees);
  SetExponent(exponent);
}

SpotLightPtr SpotLight::Make (float px, float py, float pz,
                              float dx, float dy, float dz,
                              float cutoffDegrees, float exponent,
                              const std::string& space)
{
  return SpotLightPtr(new SpotLight(px, py, pz, dx, dy, dz, cutoffDegrees, exponent, space));
}

void SpotLight::SetPosition (float x, float y, float z)
{
  Set("light_position", Vec4{x, y, z, 1.0f});
}

void SpotLight::SetDirection (float x, float y, float z)
{
  Vec3 dir = Vec3Normalize(Vec3{x, y, z});
  Set("light_direction", dir);
}

void SpotLight::SetCutoff (float cutoffDegrees)
{
  float rad = Radians(cutoffDegrees);
  Set("light_cutoff", std::cos(rad));
}

void SpotLight::SetExponent (float exponent)
{
  Set("light_exponent", exponent);
}

std::map<std::string, UniformValue> SpotLight::Map (const Mat4& matrix) const
{
  Vec4 pos = std::get<Vec4>(Get("light_position", Vec4{0.0f, 0.0f, 0.0f, 1.0f}));
  Vec3 dir = std::get<Vec3>(Get("light_direction", Vec3{0.0f, -1.0f, 0.0f}));

  Vec4 mappedPos = Mat4MulVec4(matrix, pos);

  Vec4 mappedDir4 = Mat4MulVec4(matrix, Vec4{dir.x, dir.y, dir.z, 0.0f});
  Vec3 mappedDir = Vec3Normalize(Vec3{mappedDir4.x, mappedDir4.y, mappedDir4.z});

  return {
    {"light_position", mappedPos},
    {"light_direction", mappedDir}
  };
}
