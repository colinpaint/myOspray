// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#ifdef _WIN32
#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3native.h>
#endif

#include "rkcommon/math/range.h"
#include "rkcommon/math/vec.h"
#include "sg/scene/transfer_function/TransferFunction.h"

using namespace rkcommon::math;

using ColorPoint   = vec4f;
using OpacityPoint = vec2f;

class TransferFunctionWidget
{
 public:
  TransferFunctionWidget(
      std::function<void(const range1f &, const std::vector<vec4f> &)>
          transferFunctionUpdatedCallback,
      const range1f &valueRange     = range1f(-1.f, 1.f),
      const std::string &widgetName = "Transfer Function");
  ~TransferFunctionWidget();

  // update UI and process any UI events
  void updateUI();

  // setters/getters for current transfer function data
  void setValueRange(const range1f &);
  void setColorsAndOpacities(const std::vector<vec4f> &);
  range1f getValueRange();
  std::vector<vec4f> getSampledColorsAndOpacities(int numSamples = 256);

 private:
  void loadDefaultMaps();
  void setMap(int);

  vec3f interpolateColor(const std::vector<ColorPoint> &controlPoints, float x);

  float interpolateOpacity(const std::vector<OpacityPoint> &controlPoints,
                           float x);

  void updateTfnPaletteTexture();

  void drawEditor();

  // callback called whenever transfer function is updated
  std::function<void(const range1f &, const std::vector<vec4f> &)>
      transferFunctionUpdatedCallback{nullptr};

  // all available transfer functions
  std::vector<ospray::sg::NodePtr> tfnsNodes;
  std::vector<std::string> tfnsNames;
  std::vector<std::vector<ColorPoint>> tfnsColorPoints;
  std::vector<std::vector<OpacityPoint>> tfnsOpacityPoints;
  std::vector<bool> tfnsEditable;

  // properties of currently selected transfer function
  int currentMap{0};
  std::vector<ColorPoint> *tfnColorPoints;
  std::vector<OpacityPoint> *tfnOpacityPoints;
  bool tfnEditable{true};

  // flag indicating transfer function has changed in UI
  bool tfnChanged{true};

  // scaling factor for generated opacities
  float globalOpacityScale{1.f};

  // domain (value range) of transfer function
  range1f valueRange{-1.f, 1.f};

  // texture for displaying transfer function color palette
  GLuint tfnPaletteTexture{0};

  // widget name (use different names to support multiple concurrent widgets)
  std::string widgetName;
};
