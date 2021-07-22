//
// Created by johannes on 01.07.21.
//

#ifndef QLEVER_COMPRESSEDRELATION_H
#define QLEVER_COMPRESSEDRELATION_H

#include <algorithm>
#include <vector>

#include "../global/Id.h"
#include "../util/File.h"
#include "../util/Serializer/SerializeVector.h"

struct CompressedBlockMetaData {
  off_t _offsetInFile;
  size_t _compressedSize;
  size_t _numberOfElements;
  // TODO<joka921> Do we actually need the lastRHS aka value?
  Id _firstLhs;
  Id _firstRhs;
  Id _lastLhs;
  Id _lastRhs;
};

template <typename Serializer>
void serialize(Serializer& s, CompressedBlockMetaData& b) {
  s | b._offsetInFile;
  s | b._compressedSize;
  s | b._numberOfElements;
  s | b._firstLhs;
  s | b._firstRhs;
  s | b._lastLhs;
  s | b._lastRhs;
}

struct CompressedRelationMetaData {
  Id _relId;
  size_t _numberOfElements;
  double _multiplicityColumn1;
  double _multiplicityColumn2;
  bool _isFunctional;
  std::vector<CompressedBlockMetaData> _blocks;

  size_t getNofElements() const { return _numberOfElements; }

  size_t getTotalNumberOfBytes() const {
    return std::accumulate(
        _blocks.begin(), _blocks.end(), 0ull,
        [](const auto& a, const auto& b) { return a + b._compressedSize; });
  }

  double getCol1Multiplicity() const { return _multiplicityColumn1; }
  double getCol2Multiplicity() const { return _multiplicityColumn2; }
  void setCol1Multiplicity(double mult) { _multiplicityColumn1 = mult; }
  void setCol2Multiplicity(double mult) { _multiplicityColumn2 = mult; }

  auto getRelevantBlockIndices(Id key) const {
    auto l = std::lower_bound(
        _blocks.begin(), _blocks.end(), key,
        [](const auto& a, const auto& b) { return a._lastLhs < b; });
    auto h = std::upper_bound(
        _blocks.begin(), _blocks.end(), key,
        [](const auto& a, const auto& b) { return a < b._firstLhs; });
    return std::pair{l, h};
  }

  /**
   * @brief Perform a scan for one key i.e. retrieve all YZ from the XYZ
   * permutation for a specific key value of X
   * @tparam Permutation The permutations Index::POS()... have different types
   * @param key The key (in Id space) for which to search, e.g. fixed value for
   * O in OSP permutation.
   * @param result The Id table to which we will write. Must have 2 columns.
   * @param p The Permutation to use (in particularly POS(), SOP,... members of
   * Index class).
   */
  // The IdTable is a rather expensive type, so we don't include it here.
  // but we can also not forward declare it b.c. it is actually an Alias.
  template <class Permutation, typename IdTableImpl>
  static void scan(Id key, IdTableImpl* result, const Permutation& p,
                   ad_utility::SharedConcurrentTimeoutTimer timer = nullptr);

  /**
   * @brief Perform a scan for two keys i.e. retrieve all Z from the XYZ
   * permutation for specific key values of X and Y.
   * @tparam Permutation The permutations Index::POS()... have different types
   * @param keyFirst The first key (as a raw string that is yet to be
   * transformed to index space) for which to search, e.g. fixed value for O in
   * OSP permutation.
   * @param keySecond The second key (as a raw string that is yet to be
   * transformed to index space) for which to search, e.g. fixed value for S in
   * OSP permutation.
   * @param result The Id table to which we will write. Must have 2 columns.
   * @param p The Permutation to use (in particularly POS(), SOP,... members of
   * Index class).
   */
  template <class PermutationInfo, typename IdTableImpl>
  static void scan(const Id relId, const Id& subjId, IdTableImpl* result,
                   const PermutationInfo& p);

  // __________________________________________________________________
  bool isFunctional() const { return _isFunctional; }
};

template <class Serializer>
void serialize(Serializer& s, CompressedRelationMetaData& c) {
  s | c._relId;
  s | c._numberOfElements;
  s | c._multiplicityColumn1;
  s | c._multiplicityColumn2;
  s | c._isFunctional;
  s | c._blocks;
}

#endif  // QLEVER_COMPRESSEDRELATION_H
