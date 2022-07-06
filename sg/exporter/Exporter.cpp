// Copyright 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Exporter.h"

namespace ospray {
  namespace sg {

  Exporter::Exporter()
  {
    createChild("file", "string", std::string(""));
  }

  NodeType Exporter::type() const
  {
    return NodeType::EXPORTER;
  }

  }  // namespace sg
} // namespace ospray
