//
// Created by johannes on 01.07.21.
//

#include "CompressedRelation.h"

#include "../engine/IdTable.h"
#include "../util/CompressionUsingZstd/ZstdWrapper.h"
#include "./Permutations.h"

template <class Permutation, typename IdTableImpl>
void CompressedRelationMetaData::scan(
    Id key, IdTableImpl* result, const Permutation& p,
    ad_utility::SharedConcurrentTimeoutTimer timer) {
  if (p._meta.relationExists(key)) {
    LOG(DEBUG) << "Entering actual scan routine\n";
    const auto& rmd = p._meta.getRmd(key);
    result->resize(rmd.getNofElements());
    Id* position = result->data();
    size_t spaceLeft = result->size() * result->cols();
#pragma omp parallel
#pragma omp single
    for (const auto& block : rmd._blocks) {
      LOG(DEBUG) << "Start preparing a block\n";
      std::vector<char> compressedBuffer;
      compressedBuffer.resize(block._compressedSize);
      p._file.read(compressedBuffer.data(), block._compressedSize,
                   block._offsetInFile);

      auto numElementsToRead = block._numberOfElements * 2;
      auto decompressLambda = [position, spaceLeft, numElementsToRead, &block,
                               compressedBuffer =
                                   std::move(compressedBuffer)]() {
        ad_utility::Timer t;
        t.start();
        auto numElementsActuallyRead = ZstdWrapper::decompressToBuffer(
            compressedBuffer.data(), compressedBuffer.size(), position,
            block._numberOfElements * 2);
        AD_CHECK(numElementsActuallyRead <= spaceLeft);
        AD_CHECK(numElementsToRead == numElementsActuallyRead);
        t.stop();
        LOG(INFO) << "Decompressed a block in " << t.usecs()
                  << "microseconds\n";
      };
      LOG(DEBUG) << "Finished preparing a block\n";

#pragma omp task
      { decompressLambda(); }

      spaceLeft -= numElementsToRead;
      position += numElementsToRead;
    }
    AD_CHECK(spaceLeft == 0);
  }
}

template void CompressedRelationMetaData::scan<Permutation::POS_T, IdTable>(
    Id key, IdTable* result, const Permutation::POS_T& p,
    ad_utility::SharedConcurrentTimeoutTimer timer);
template void CompressedRelationMetaData::scan<Permutation::PSO_T, IdTable>(
    Id key, IdTable* result, const Permutation::PSO_T& p,
    ad_utility::SharedConcurrentTimeoutTimer timer);

template <class PermutationInfo, typename IdTableImpl>
void CompressedRelationMetaData::scan(const Id relId, const Id& subjId,
                                      IdTableImpl* result,
                                      const PermutationInfo& p) {
  if (p._meta.relationExists(relId)) {
    const auto& rmd = p._meta.getRmd(relId);
    const auto [firstBlock, lastBlock] = rmd.getRelevantBlockIndices(subjId);

    size_t numElementsInBlocks = 0;
    for (auto begin = firstBlock; begin != lastBlock; ++begin) {
      numElementsInBlocks += begin->_numberOfElements;
    }
    using P = std::pair<Id, Id>;
    using Alloc = ad_utility::AllocatorWithLimit<P>;
    std::vector<P, Alloc> twoColumnBuffer(
        numElementsInBlocks, result->getAllocator().template as<P>());
    std::vector<char> compressedBuffer;
    Id* position = &(twoColumnBuffer.data()->first);
    size_t spaceLeft = numElementsInBlocks * 2;
    for (auto begin = firstBlock; begin != lastBlock; ++begin) {
      const auto& block = *begin;
      compressedBuffer.resize(block._compressedSize);
      p._file.read(compressedBuffer.data(), block._compressedSize,
                   block._offsetInFile);
      auto numElementsRead = ZstdWrapper::decompressToBuffer(
          compressedBuffer.data(), compressedBuffer.size(), position,
          block._numberOfElements * 2);
      AD_CHECK(numElementsRead <= spaceLeft);
      spaceLeft -= numElementsRead;
      position += numElementsRead;
    }
    AD_CHECK(spaceLeft == 0);

    P dummy{subjId, 0};

    auto [beg, end] = std::equal_range(
        twoColumnBuffer.begin(), twoColumnBuffer.end(), dummy,
        [](const auto& a, const auto& b) { return a.first < b.first; });
    auto resultStatic = result->template moveToStatic<1>();
    resultStatic.resize(end - beg);

    size_t i = 0;
    for (; beg != end; ++beg) {
      resultStatic(i++, 0) = beg->second;
    }

    *result = resultStatic.moveToDynamic();
  }
}

template void CompressedRelationMetaData::scan<Permutation::POS_T, IdTable>(
    const Id, const Id&, IdTable*, const Permutation::POS_T&);
template void CompressedRelationMetaData::scan<Permutation::PSO_T, IdTable>(
    const Id, const Id&, IdTable*, const Permutation::PSO_T&);
