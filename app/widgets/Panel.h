// Copyright 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>
#include <vector>

#ifndef PANEL_INTERFACE
  #ifdef _WIN32
    #define PANEL_INTERFACE __declspec(dllexport)
  #else
    #define PANEL_INTERFACE
  #endif
#endif

class StudioContext;

namespace ospray {

struct PANEL_INTERFACE Panel
{
  Panel() = default;
  virtual ~Panel() = default;

  // Function called by MainWindow to construct the desired ImGui widgets //

  virtual void buildUI(void* ImGuiCtx) = 0;

  // Controls to show/hide the panel in the app //

  void setShown(bool shouldBeShown);
  void toggleShown();
  bool isShown() const;

  // Panel name controls //

  void setName(const std::string &newName);
  std::string name() const;

 protected:
  // Constructor to be used by child classes
  Panel(const std::string &_name, std::shared_ptr<StudioContext> _context)
      : context(_context), currentName(_name)
  {}
  std::shared_ptr<StudioContext> context;
  bool show = false;

 private:
  // Properties //

  std::string currentName{"<unnamed panel>"};
};

using PanelList = std::vector<std::unique_ptr<Panel>>;

// Inlined members //////////////////////////////////////////////////////////

inline void Panel::setShown(bool shouldBeShown)
{
  show = shouldBeShown;
}

inline void Panel::toggleShown()
{
  show = !show;
}

inline bool Panel::isShown() const
{
  return show;
}

inline void Panel::setName(const std::string &newName)
{
  currentName = newName;
}

inline std::string Panel::name() const
{
  return currentName;
}

} // namespace ospray
