// Copyright 2011, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Authors: Björn Buchhold <buchholb>,
//          Johannes Kalmbach<joka921> (johannes.kalmbach@gmail.com)

#pragma once

#include <fstream>
#include <iostream>

#include "../util/File.h"
#include "../util/HashMap.h"
#include "../util/HashSet.h"
#include "./ConstantsIndexCreation.h"
#include "./Vocabulary.h"

using std::string;

// _____________________________________________________________________________
template <class S, class C>
void Vocabulary<S, C>::readFromFile(const string& fileName,
                                    const string& extLitsFileName) {
  LOG(INFO) << "Reading vocabulary from file " << fileName << "\n";
  _words.clear();
  _words.reserve(_maxVocabularySize);
  std::fstream in(fileName.c_str(), std::ios_base::in);
  string line;
  [[maybe_unused]] bool first = true;
  std::string lastExpandedString;
  bool floatsStarted = false;
  size_t numWords = 0;
  while (std::getline(in, line)) {
    if constexpr (_isCompressed) {
      // when we read from file it means that all preprocessing has been done
      // and the prefixes are already stripped in the file
      auto compr = CompressedString::fromString(line);
      auto str = expandPrefix(compr);
      _words.emplace_back();
      if (ad_utility::startsWith(str, VALUE_FLOAT_PREFIX)) {
        try {
          _words.back().setFloat(ad_utility::convertIndexWordToFloat(str));
        } catch(...) {
          LOG(ERROR) << "Error converting index word " << str << '\n';
          throw;
        }
        if (!floatsStarted) {
          _lowerBoundFloat = numWords;
          floatsStarted = true;
        }
      } else {
        _words.back().setStr(compr);
        if (floatsStarted) {
          floatsStarted = false;
          _upperBoundFloat = numWords;
        }
      }
        if (!first) {
          if (!(_caseComparator.compare(lastExpandedString, str,
                                      SortLevel::TOTAL))) {
          LOG(ERROR) << "Vocabulary is not sorted in ascending order for words "
                     << lastExpandedString << " and " << str << std::endl;
          // AD_CHECK(false);
        }
      } else {
        first = false;
      }
      lastExpandedString = std::move(str);
    } else {
      _words.emplace_back();
      if (ad_utility::startsWith(line, VALUE_FLOAT_PREFIX)) {
        _words.back().setFloat(ad_utility::convertIndexWordToFloat(line));
        if (!floatsStarted) {
          _lowerBoundFloat = numWords;
          floatsStarted = true;
        }
      } else {
        _words.back().setStr(line);
        if (floatsStarted) {
          floatsStarted = false;
          _upperBoundFloat = numWords + 1;
        }
      }
    }
    numWords++;
  }
  in.close();
  LOG(INFO) << "Done reading vocabulary from file.\n";
  LOG(INFO) << "It contains " << _words.size() << " elements\n";
  if (extLitsFileName.size() > 0) {
    if (!_isCompressed) {
      LOG(INFO) << "ERROR: trying to load externalized literals to an "
                   "uncompressed vocabulary. This is not valid and a "
                   "programming error. Terminating\n";
      AD_CHECK(false);
    }

    LOG(INFO) << "Registering external vocabulary for literals.\n";
    _externalLiterals.initFromFile(extLitsFileName);
    LOG(INFO) << "Done registering external vocabulary for literals.\n";
  }
}

// _____________________________________________________________________________
template <class S, class C>
template <typename, typename>
void Vocabulary<S, C>::writeToFile(const string& fileName) const {
  LOG(INFO) << "Writing vocabulary to file " << fileName << "\n";
  std::ofstream out(fileName.c_str(), std::ios_base::out);
  // on disk we save the compressed version, so do not  expand prefixes
  if constexpr (!_isCompressed) {
    for (size_t i = 0; i + 1 < _words.size(); ++i) {
      out << at(i) << '\n';
    }
    if (_words.size() > 0) {
      out << at(_words.size() - 1);
    }
    out.close();
    LOG(INFO) << "Done writing vocabulary to file.\n";
  } else {
    throw std::runtime_error("Vocabulary writing to file for compressed vocabularies is illegal!");
  }
}

// _____________________________________________________________________________
template <class S, class C>
template <typename, typename>
void Vocabulary<S, C>::writeToBinaryFileForMerging(
    const string& fileName) const {
  LOG(INFO) << "Writing vocabulary to binary file " << fileName << "\n";
  std::ofstream out(fileName.c_str(),
                    std::ios_base::out | std::ios_base::binary);
  AD_CHECK(out.is_open());
  for (size_t i = 0; i < _words.size(); ++i) {
    // 32 bits should be enough for len of string
    std::string_view word = _words[i];
    uint32_t len = word.size();
    size_t zeros = 0;
    out.write((char*)&len, sizeof(len));
    out.write(word.data(), len);
    out.write((char*)&zeros, sizeof(zeros));
  }
  out.close();
  LOG(INFO) << "Done writing vocabulary to file.\n";
}

// _____________________________________________________________________________
template <class S, class C>
template <typename, typename>
void Vocabulary<S, C>::createFromSet(const ad_utility::HashSet<S>& set) {
  LOG(INFO) << "Creating vocabulary from set ...\n";
  //TODO<joka921> This is inefficient, but only used for the small text index;
  std::vector<S> words;
  words.clear();
  words.reserve(set.size());
  words.insert(begin(words), begin(set), end(set));
  LOG(INFO) << "... sorting ...\n";
  std::sort(begin(words), end(words), _caseComparator);
  _words.reserve(words.size());
  for (const auto& w : words) {
    _words.emplace_back();
    _words.back().setStr(w);
  }

  LOG(INFO) << "Done creating vocabulary.\n";
}

// _____________________________________________________________________________
// TODO<joka921> is this used? if no, throw out, if yes, transform to
// compressedString as key type
template <class S, class C>
template <typename, typename>
ad_utility::HashMap<string, Id> Vocabulary<S, C>::asMap() {
  ad_utility::HashMap<string, Id> map;
  for (size_t i = 0; i < _words.size(); ++i) {
    map[_words[i]] = i;
  }
  return map;
}

// _____________________________________________________________________________
template <class S, class C>
template <typename, typename>
void Vocabulary<S, C>::externalizeLiterals(const string& fileName) {
  LOG(INFO) << "Externalizing literals..." << std::endl;
  auto ext = std::lower_bound(_words.begin(), _words.end(),
                              EXTERNALIZED_LITERALS_PREFIX, _caseComparator);
  size_t nofInternal = ext - _words.begin();
  vector<string> extVocab;
  while (ext != _words.end()) {
    extVocab.push_back((*ext++).substr(1));
  }
  _words.resize(nofInternal);
  _externalLiterals.buildFromVector(extVocab, fileName);
  LOG(INFO) << "Done externalizing literals." << std::endl;
}

// _____________________________________________________________________________
template <class S, class C>
bool Vocabulary<S, C>::isLiteral(const string& word) {
  return word.size() > 0 && word[0] == '\"';
}

// _____________________________________________________________________________
template <class S, class C>
bool Vocabulary<S, C>::isExternalizedLiteral(const string& word) {
  return word.size() > 1 &&
         ad_utility::startsWith(word, EXTERNALIZED_LITERALS_PREFIX + '\"');
}

// _____________________________________________________________________________
template <class S, class C>
bool Vocabulary<S, C>::shouldBeExternalized(const string& word) const {
  for (const auto& p : _externalizedPrefixes) {
    if (ad_utility::startsWith(word, p)) {
      return true;
    }
  }
  if (!isLiteral(word)) {
    return false;
    //return shouldEntityBeExternalized(word);
  } else {
    return shouldLiteralBeExternalized(word);
  }
}

// ___________________________________________________________________
template <class S, class C>
bool Vocabulary<S, C>::shouldEntityBeExternalized(const string& word) const {
  for (const auto& p : _externalizedPrefixes) {
    if (ad_utility::startsWith(word, p)) {
      return true;
    }
  }
  return false;
}

// ___________________________________________________________________
template <class S, class C>
bool Vocabulary<S, C>::shouldLiteralBeExternalized(const string& word) const {
  if (word.size() > MAX_INTERNAL_LITERAL_BYTES) {
    return true;
  }

  const string lang = getLanguage(word);
  if (lang == "") {
    return false;
  }

  for (const auto& p : _internalizedLangs) {
    if (lang == p) {
      return false;
    }
  }
  return true;
}
// _____________________________________________________________________________
template <class S, class C>
string Vocabulary<S, C>::getLanguage(const string& literal) {
  auto lioAt = literal.rfind('@');
  if (lioAt != string::npos) {
    auto lioQ = literal.rfind('\"');
    if (lioQ != string::npos && lioQ < lioAt) {
      return literal.substr(lioAt + 1);
    }
  }
  return "";
}

// ____________________________________________________________________________
template <class S, class C>
template <typename, typename>
string Vocabulary<S, C>::expandPrefix(const CompressedString& word) const {
  assert(!word.empty());
  auto idx = static_cast<uint8_t>(word[0]) - MIN_COMPRESSION_PREFIX;
  if (idx >= 0 && idx < NUM_COMPRESSION_PREFIXES) {
    return _prefixMap[idx] + word.toStringView().substr(1);
  } else {
    return string(word.toStringView().substr(1));
  }
}

// _____________________________________________________________________________
template <class S, class C>
template <typename, typename>
CompressedString Vocabulary<S, C>::compressPrefix(const string& word) const {
  for (const auto& p : _prefixVec) {
    if (ad_utility::startsWith(word, p._fulltext)) {
      auto res = CompressedString::fromString(
          p._prefix + std::string_view(word).substr(p._fulltext.size()));
      return res;
    }
  }
  auto res = CompressedString::fromString(NO_PREFIX_CHAR + word);
  return res;
}

// _____________________________________________________________________________
template <class S, class C>
template <class StringRange, typename, typename>
void Vocabulary<S, C>::initializePrefixes(const StringRange& prefixes) {
  for (auto& el : _prefixMap) {
    el = "";
  }
  _prefixVec.clear();
  unsigned char prefixIdx = 0;
  for (const auto& fulltext : prefixes) {
    if (prefixIdx >= NUM_COMPRESSION_PREFIXES) {
      LOG(INFO) << "More than " << NUM_COMPRESSION_PREFIXES
                << " prefixes have been specified. Skipping the rest\n";
      break;
    }
    _prefixMap[prefixIdx] = fulltext;
    _prefixVec.emplace_back(prefixIdx + MIN_COMPRESSION_PREFIX, fulltext);
    prefixIdx++;
  }
  if (prefixIdx != NUM_COMPRESSION_PREFIXES) {
    LOG(WARN) << "less than " << NUM_COMPRESSION_PREFIXES
              << " prefixes specified.";
  }
  // if longest strings come first we correctly handle overlapping prefixes
  auto pred = [](const Prefix& a, const Prefix& b) {
    return a._fulltext.size() > b._fulltext.size();
  };
  std::sort(_prefixVec.begin(), _prefixVec.end(), pred);
}

// ______________________________________________________________________________
template <class S, class C>
template <class StringRange>
void Vocabulary<S, C>::initializeExternalizePrefixes(const StringRange& s) {
  _externalizedPrefixes.clear();
  for (const auto& el : s) {
    _externalizedPrefixes.push_back(el);
  }
}

// ______________________________________________________________________________
template <class S, class C>
template <class StringRange>
void Vocabulary<S, C>::initializeInternalizedLangs(const StringRange& s) {
  _internalizedLangs.clear();
  _internalizedLangs.insert(_internalizedLangs.begin(), s.begin(), s.end());
}

// _____________________________________________________
template <class S, class C>
template <typename, typename>
void Vocabulary<S, C>::prefixCompressFile(const string& infile,
                                          const string& outfile,
                                          const vector<string>& prefixes) {
  std::ifstream in(infile);
  std::ofstream out(outfile);
  AD_CHECK(in.is_open() && out.is_open());
  Vocabulary v;
  v.initializePrefixes(prefixes);
  std::string word;
  while (std::getline(in, word)) {
    out << v.compressPrefix(word).toStringView() << '\n';
  }
}
