// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Vdb.h"
#include <sstream>
#include <string>

namespace ospray {
  namespace sg {

  // VdbVolume definitions /////////////////////////////////////////////////////

  VdbVolume::VdbVolume() : Volume("vdb")
  {
  }

#if USE_OPENVDB
#define VKL_VDB_NUM_LEVELS 4
  /*
   * This builder can traverse the OpenVDB tree, generating
   * nodes for us along the way.
   */
  template <typename VdbNodeType, typename Enable = void>
  struct Builder
  {
    using ChildNodeType = typename VdbNodeType::ChildNodeType;
    static constexpr uint32_t level{VKL_VDB_NUM_LEVELS - 1 -
                                    VdbNodeType::LEVEL};
    static constexpr uint32_t nextLevel{level + 1};

    static void visit(const VdbNodeType &vdbNode,
                      std::vector<float> &tiles,
                      std::vector<uint32_t> &bufLevel,
                      std::vector<vec3i> &bufOrigin,
                      std::vector<float> &bufData,
                      std::vector<uint32_t> &bufFormat)
    {
      for (auto it = vdbNode.cbeginValueOn(); it; ++it) {
        const auto &coord = it.getCoord();
        bufLevel.emplace_back(nextLevel);
        bufOrigin.emplace_back(coord[0], coord[1], coord[2]);
        // Note: we can use a pointer to a value we just pushed back because
        //       we preallocated using reserve!
        tiles.push_back(*it);
        bufFormat.push_back(OSP_VOLUME_FORMAT_TILE);
      }

      for (auto it = vdbNode.cbeginChildOn(); it; ++it)
        Builder<ChildNodeType>::visit(*it, tiles, bufLevel, bufOrigin, bufData, bufFormat);
    }
  };

  template <typename VdbNodeType, class Enable>
  constexpr uint32_t Builder<VdbNodeType, Enable>::nextLevel;

  /*
   * We stop the recursion one level above the leaf level.
   * By default, this is level 2.
   */
  template <typename VdbNodeType>
  struct Builder<VdbNodeType,
                 typename std::enable_if<(VdbNodeType::LEVEL == 1)>::type>
  {
    static constexpr uint32_t level{VKL_VDB_NUM_LEVELS - 1 -
                                    VdbNodeType::LEVEL};
    static constexpr uint32_t nextLevel{level + 1};
    static_assert(nextLevel == VKL_VDB_NUM_LEVELS - 1,
                  "OpenVKL is not compiled to match OpenVDB::FloatTree");

    static void visit(const VdbNodeType &vdbNode,
                      std::vector<float> &tiles,
                      std::vector<uint32_t> &bufLevel,
                      std::vector<vec3i> &bufOrigin,
                      std::vector<float> &bufData,
                      std::vector<uint32_t> &bufFormat)
    {
      const uint32_t storageRes = 16;
      const uint32_t childRes   = 8;
      const auto *vdbVoxels     = vdbNode.getTable();
      const vec3i origin =
          vec3i(vdbNode.origin()[0], vdbNode.origin()[1], vdbNode.origin()[2]);

      // Note: OpenVdb stores data in z-major order!
      uint64_t vIdx = 0;
      for (uint32_t x = 0; x < storageRes; ++x)
        for (uint32_t y = 0; y < storageRes; ++y)
          for (uint32_t z = 0; z < storageRes; ++z, ++vIdx) {
            const bool isTile  = vdbNode.isValueMaskOn(vIdx);
            const bool isChild = vdbNode.isChildMaskOn(vIdx);

            if (!(isTile || isChild))
              continue;

            const auto &nodeUnion   = vdbVoxels[vIdx];
            const vec3i childOrigin = origin + childRes * vec3i(x, y, z);
            if (isTile) {
              bufLevel.emplace_back(nextLevel);
              bufOrigin.emplace_back(childOrigin);
              // Note: we can use a pointer to a value we just pushed back because
              //       we preallocated using reserve!
              tiles.push_back(nodeUnion.getValue());
              bufFormat.push_back(OSP_VOLUME_FORMAT_TILE);
            } else if (isChild) {
              bufLevel.emplace_back(nextLevel);
              bufOrigin.emplace_back(childOrigin);
              auto dataStart = nodeUnion.getChild()->buffer().data();
              auto dataEnd = dataStart + (childRes*childRes*childRes);
              std::copy(dataStart, dataEnd, std::back_inserter(bufData));
              bufFormat.push_back(OSP_VOLUME_FORMAT_DENSE_ZYX);
            }
          }
    }
  };

  template <typename VdbNodeType>
  constexpr uint32_t Builder<VdbNodeType,
                 typename std::enable_if<(VdbNodeType::LEVEL == 1)>::type>::nextLevel;

#endif  // USE_OPENVDB

  ///! \brief file name of the xml doc when the node was loaded from xml
  /*! \detailed we need this to properly resolve relative file names */
  void VdbVolume::load(const FileName &fileNameAbs)
  {
#if USE_OPENVDB
    auto vdbData = generateVDBData(fileNameAbs);
    createChildData("node.level", vdbData.level);
    createChildData("node.origin", vdbData.origin);
    if (vdbData.data.size())
      createChildData("nodesPackedDense", vdbData.data);
    if (tiles.size())
      createChildData("nodesPackedTile", tiles);
    createChildData("node.format", vdbData.format);
    createChildData("indexToObject", vdbData.bufI2o);

    fileLoaded = true;

#endif  // USE_OPENVDB
  }

  VDBData VdbVolume::generateVDBData(const FileName &fileNameAbs)
  {
    VDBData vdbData;
#if USE_OPENVDB
    if (!fileLoaded) {
      openvdb::initialize();  // Must initialize first! It's ok to do this
                              // multiple times.

      try {
        openvdb::io::File file(fileNameAbs.c_str());
        std::cout << "loading " << fileNameAbs << std::endl;
        file.open();

        std::vector<std::string> gridNames;

        // Locate valid grid names within the file
        for (auto nameIter = file.beginName(); nameIter != file.endName();
             ++nameIter)
          gridNames.push_back(nameIter.gridName());
 
        if (gridNames.size() > 1) {
          std::cout << "  multiple grids" << std::endl;
          for (auto &n : gridNames)
            std::cout << "    : " << n << std::endl;
        }

        auto gridNum = gridNames.size() - 1;

        // XXX at some point, add the ability to select which grid(s). For now,
        // just choose the first.
        std::cout << "  loading grid: " << gridNames[gridNum] << std::endl;
        grid = file.readGrid(gridNames[gridNum]);
        file.close();
      } catch (const std::exception &e) {
        const std::string err = (std::string("Error loading ")
          + fileNameAbs.c_str()) 
          + std::string(": ")
          + e.what();
        throw std::runtime_error(err);
      }

      // We only support the default topology in this loader.
      if (grid->type() != std::string("Tree_float_5_4_3"))
        throw std::runtime_error(std::string("Incorrect tree type: ") +
                                 grid->type());

      // Preallocate memory for all leaves; this makes loading the tree much
      // faster.
      openvdb::FloatGrid::Ptr vdb =
          openvdb::gridPtrCast<openvdb::FloatGrid>(grid);

      const auto &indexToObject = grid->transform().baseMap();
      if (!indexToObject->isLinear())
        throw std::runtime_error(
            "OpenVKL only supports linearly transformed volumes");

      // Transpose; OpenVDB stores column major (and a full 4x4 matrix).
      const auto &ri2o = indexToObject->getAffineMap()->getMat4();
      const auto *i2o  = ri2o.asPointer();

      // Preallocate buffers!
      const size_t numTiles  = vdb->tree().activeTileCount();
      const size_t numLeaves = vdb->tree().leafCount();
      const size_t numNodes  = numTiles + numLeaves; // level size

      tiles.reserve(numTiles);

      vdbData.level.reserve(numNodes);
      vdbData.origin.reserve(numNodes);
      vdbData.format.reserve(numNodes);

      const auto &root = vdb->tree().root();
      for (auto it = root.cbeginChildOn(); it; ++it)
        Builder<openvdb::FloatTree::RootNodeType::ChildNodeType>::visit(
            *it, tiles, vdbData.level, vdbData.origin, vdbData.data, vdbData.format);

      vdbData.bufI2o = {static_cast<float>(i2o[0]),
                        static_cast<float>(i2o[4]),
                        static_cast<float>(i2o[8]),
                        static_cast<float>(i2o[1]),
                        static_cast<float>(i2o[5]),
                        static_cast<float>(i2o[9]),
                        static_cast<float>(i2o[2]),
                        static_cast<float>(i2o[6]),
                        static_cast<float>(i2o[10]),
                        static_cast<float>(i2o[12]),
                        static_cast<float>(i2o[13]),
                        static_cast<float>(i2o[14])};

      // Query VDB voxel value range and set volume range
      range1f valueRange;
      vdb->evalMinMax(valueRange.lower, valueRange.upper);
      child("valueRange") = valueRange;
    }
#endif //USE_OPENVDB

    return vdbData;
  }

    OSP_REGISTER_SG_NODE_NAME(VdbVolume, volume_vdb);

  }  // namespace sg
} // namespace ospray
