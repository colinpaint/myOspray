// Copyright 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "AdvancedMaterialEditor.h"
#include "FileBrowserWidget.h"
#include "GenerateImGuiWidgets.h" // TreeState
#include "ListBoxWidget.h"

#include "sg/NodeType.h"
#include "sg/texture/Texture2D.h"

#include "rkcommon/math/AffineSpace.h"
#include "rkcommon/os/FileName.h"

#include <string>
#include <vector>

#include <imgui.h>

using namespace rkcommon::math;

void AdvancedMaterialEditor::buildUI(
    NodePtr materialRegistry, NodePtr &selectedMat)
{
  auto matName = selectedMat->name();
  updateTextureNames(selectedMat);

  if (ImGui::Button("Copy")) {
    copiedMat = selectedMat;
  }
  if (clipboard()) {
    if (NodePtr s_copiedMat = copiedMat.lock()) {
      ImGui::SameLine();
      if (ImGui::Button("Paste")) {
        // actually create the copied material node here so we know what
        // name to give it
        auto newMat = copyMaterial(s_copiedMat, selectedMat->name());
        materialRegistry->add(newMat);
      }
      ImGui::SameLine();
      ImGui::TextDisabled("copied: '%s'", s_copiedMat->name().c_str());
    }
  }

  ImGui::Spacing();
  ImGui::Text("Replace material");
  static int currentMatType = 0;
  const char *matTypes[] = {"principled", "carPaint", "obj", "luminous"};
  ImGui::Combo("Material types", &currentMatType, matTypes, 4);
  if (ImGui::Button("Replace##material")) {
    auto newMat = ospray::sg::createNode(matName, matTypes[currentMatType]);
    materialRegistry->add(newMat);
  }

  ImGui::Spacing();
  ImGui::Text("Add texture");
  static bool showTextureFileBrowser = false;
  static rkcommon::FileName matTexFileName("");
  static char matTexParamName[64] = "";
  ImGui::InputTextWithHint("texture##material",
      "select...",
      (char *)matTexFileName.base().c_str(),
      0);
  if (ImGui::IsItemClicked())
    showTextureFileBrowser = true;

  // Leave the fileBrowser open until file is selected
  if (showTextureFileBrowser) {
    ospray_studio::FileList fileList = {};
    if (ospray_studio::fileBrowser(fileList, "Select Texture")) {
      showTextureFileBrowser = false;
      if (!fileList.empty()) {
        matTexFileName = fileList[0];
      }
    }
  }

  ImGui::InputText(
      "paramName##material", matTexParamName, sizeof(matTexParamName));
  if (ImGui::Button("Add##materialtexture")) {
    std::string paramStr(matTexParamName);
    if (!paramStr.empty()) {
      std::shared_ptr<ospray::sg::Texture2D> sgTex =
          std::static_pointer_cast<ospray::sg::Texture2D>(
              ospray::sg::createNode(paramStr, "texture_2d"));
      // If load fails, remove the texture node
      if (!sgTex->load(matTexFileName, true, false))
        sgTex = nullptr;
      else {
        auto newMat = copyMaterial(selectedMat, "", paramStr);
        newMat->add(sgTex, paramStr);
        newMat->createChild(paramStr + ".transform", "linear2f") =
            linear2f(one);
        newMat->createChild(paramStr + ".translation", "vec2f") = vec2f(0.f);
        newMat->child(paramStr + ".translation").setMinMax(-1.f, 1.f);
        materialRegistry->add(newMat);
      }
    }
  }

  ImGui::Spacing();
  ImGui::Text("Remove texture");
  static int removeIndex = 0;
  static std::string texToRemove = (currentMaterialTextureNames.size() > 0)
      ? currentMaterialTextureNames[removeIndex]
      : "";
  if (ImGui::BeginCombo("Texture##materialtexture", texToRemove.c_str(), 0)) {
    for (int i = 0; i < currentMaterialTextureNames.size(); i++) {
      const bool isSelected = (removeIndex == i);
      if (ImGui::Selectable(
              currentMaterialTextureNames[i].c_str(), isSelected)) {
        removeIndex = i;
        texToRemove = currentMaterialTextureNames[removeIndex];
      }
      if (isSelected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  if (ImGui::Button("Remove##materialtexturebutton")) {
    auto newMat = copyMaterial(selectedMat, "", texToRemove);
    materialRegistry->add(newMat);
    texToRemove = "";
  }
}
