// Copyright 2018, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Johannes Kalmbach <johannes.kalmbach@gmail.com>

#include "./VocabularyGenerator.h"
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../util/Conversions.h"
#include "../util/Exception.h"
#include "../util/HashMap.h"
#include "../util/Log.h"
#include "./ConstantsIndexCreation.h"
#include "./Vocabulary.h"

struct QueueValue {
  QueueValue() = default;
  QueueValue(const string& v, size_t file, Id word)
      : _value(v), _partialFileId(file), _partialWordId(word) {}
  string _value;
  size_t _partialFileId;
  Id _partialWordId;
};

class QueueCompare {
 public:
  bool operator()(const QueueValue& p1, const QueueValue& p2) {
    return p1._value > p2._value;
  }
};

// ___________________________________________________________________
size_t mergeVocabulary(const std::string& basename, size_t numFiles) {
  std::vector<std::fstream> infiles;
  std::vector<IdPairMMapVec> idVecs;
  std::ofstream outfile(basename + ".vocabulary");
  AD_CHECK(outfile.is_open());
  std::ofstream outfileExternal(basename + EXTERNAL_LITS_TEXT_FILE_NAME);
  AD_CHECK(outfileExternal.is_open());
  std::vector<bool> endOfFile(numFiles, false);

  std::priority_queue<QueueValue, std::vector<QueueValue>, QueueCompare> queue;

  for (size_t i = 0; i < numFiles; i++) {
    infiles.emplace_back(basename + PARTIAL_VOCAB_FILE_NAME + std::to_string(i),
                         std::ios_base::in | std::ios_base::out);
    idVecs.emplace_back(0, basename + PARTIAL_MMAP_IDS + std::to_string(i));
    AD_CHECK(infiles.back().is_open());
    endOfFile[i] = true;

    uint32_t len;
    if (infiles[i].read((char*)&len, sizeof(len))) {
      std::string word(len, '\0');
      infiles[i].read(&(word[0]), len);
      Id id;
      infiles[i].read((char*)&id, sizeof(id));
      queue.push(QueueValue(word, i, id));
      endOfFile[i] = false;
    }
  }

  std::string lastWritten = "";
  size_t totalWritten = 0;

  // start k-way merge
  while (!queue.empty()) {
    auto top = queue.top();
    queue.pop();

    // avoid duplicates
    if (top._value != lastWritten) {
      lastWritten = top._value;

      if (top._value < string({EXTERNALIZED_LITERALS_PREFIX})) {
        outfile << top._value << std::endl;
      } else {
        // we have to strip the externalization character again
        outfileExternal << top._value.substr(1) << std::endl;
      }

      // write id to corresponding vec
      idVecs[top._partialFileId].push_back(
          std::make_pair(top._partialWordId, totalWritten));
      totalWritten++;
    } else {
      // this is a duplicate which already occured in another partial vocabulary
      // in the last step
      // we already have increased total written, so for the duplicate
      // we have to subtract one again
      size_t minusOne = totalWritten - 1;
      idVecs[top._partialFileId].push_back(
          std::make_pair(top._partialWordId, minusOne));
    }

    // add next word from the same infile to the priority queue
    if (endOfFile[top._partialFileId]) {
      continue;
    }  // file is exhausted, nothing to add

    size_t i = top._partialFileId;
    endOfFile[i] = true;
    uint32_t len;
    if (infiles[i].read((char*)&len, sizeof(len))) {
      std::string word(len, '\0');
      infiles[i].read(&(word[0]), len);
      Id id;
      infiles[i].read((char*)&id, sizeof(id));
      queue.push(QueueValue(word, i, id));
      endOfFile[i] = false;
    }
  }
  return totalWritten;
}

// ____________________________________________________________________________________________
google::sparse_hash_map<string, Id> vocabMapFromPartialIndexedFile(
    const string& partialFile) {
  std::ifstream file(partialFile, std::ios_base::binary);
  AD_CHECK(file.is_open());
  google::sparse_hash_map<string, Id> vocabMap;
  uint32_t len;
  while (file.read((char*)&len, sizeof(len))) {
    std::string word(len, '\0');
    file.read(&(word[0]), len);
    size_t idx;
    file.read((char*)&idx, sizeof(idx));
    vocabMap[word] = idx;
  }
  return vocabMap;
}

// ______________________________________________________________________________________________
void writePartialIdMapToBinaryFileForMerging(
    const google::sparse_hash_map<string, Id>& map, const string& fileName) {
  LOG(INFO) << "Creating partial vocabulary from set ...\n";
  std::vector<std::pair<string, Id>> els;
  els.reserve(map.size());
  els.insert(begin(els), begin(map), end(map));
  LOG(INFO) << "... sorting ...\n";
  std::sort(begin(els), end(els),
            [](const auto& p1, const auto& p2) { return p1.first < p2.first; });
  LOG(INFO) << "Done creating vocabulary.\n";

  LOG(INFO) << "Writing vocabulary to binary file " << fileName << "\n";
  std::ofstream out(fileName.c_str(),
                    std::ios_base::out | std::ios_base::binary);
  AD_CHECK(out.is_open());
  for (size_t i = 0; i < els.size(); ++i) {
    // 32 bits should be enough for len of string
    std::string_view word = els[i].first;
    uint32_t len = word.size();
    out.write((char*)&len, sizeof(len));
    out.write(word.data(), len);
    Id id = els[i].second;
    out.write((char*)&id, sizeof(id));
  }
  out.close();
  LOG(INFO) << "Done writing vocabulary to file.\n";
}

// _____________________________________________________________________
google::sparse_hash_map<Id, Id> IdMapFromPartialIdMapFile(
    const string& mmapFilename) {
  google::sparse_hash_map<Id, Id> res;
  IdPairMMapVecView vec(mmapFilename);
  for (const auto [partialId, globalId] : vec) {
    res[partialId] = globalId;
  }
  return res;
}

