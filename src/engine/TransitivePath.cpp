// Copyright 2019, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Florian Kramer (florian.kramer@neptun.uni-freiburg.de)

#include "TransitivePath.h"

#include <limits>

#include "../util/Exception.h"
#include "CallFixedSize.h"

// _____________________________________________________________________________
TransitivePath::TransitivePath(
    QueryExecutionContext* qec, std::shared_ptr<QueryExecutionTree> child,
    bool leftIsVar, bool rightIsVar, size_t leftSubCol, size_t rightSubCol,
    size_t leftValue, size_t rightValue, const std::string& leftColName,
    const std::string& rightColName, size_t minDist, size_t maxDist)
    : Operation(qec),
      _leftSideTree(nullptr),
      _leftSideCol(-1),
      _rightSideTree(nullptr),
      _rightSideCol(-1),
      _resultWidth(2),
      _subtree(child),
      _leftIsVar(leftIsVar),
      _rightIsVar(rightIsVar),
      _leftSubCol(leftSubCol),
      _rightSubCol(rightSubCol),
      _leftValue(leftValue),
      _rightValue(rightValue),
      _leftColName(leftColName),
      _rightColName(rightColName),
      _minDist(minDist),
      _maxDist(maxDist) {
  _variableColumns[_leftColName] = 0;
  _variableColumns[_rightColName] = 1;
  if (! (leftIsVar || rightIsVar)) {
    throw std::runtime_error{"Tried to instantiate a transitive path where neither the left or the right side are a variable. This is currently forbidden"};
  }

  auto x = _subtree->resultSortedOn();
  if (x.empty() || (x[0] != leftSubCol && x[0] != rightSubCol)) {
    throw std::runtime_error{"Transitive Path input has to be sorted! This should never happen, please report it"};
  }
  if (x[0] == leftSubCol) {
    _childOrdered = ChildOrdered::Left;
  } else {
    _childOrdered = ChildOrdered::Right;
  }
}

template <int SUB_WIDTH, int LEFT_WIDTH, int RES_WIDTH>
void TransitivePath::computeTransitivePathLeftBound(
    IdTable* dynRes, const IdTable& dynSub, const IdTable& dynLeft,
    size_t leftSideCol, bool rightIsVar, size_t leftSubCol, size_t rightSubCol,
    Id rightValue, size_t minDist, size_t maxDist, size_t resWidth) {
  using Map = ad_utility::HashMap<Id, std::shared_ptr<ad_utility::HashSet<Id>>>;
  using MapIt = Map::iterator;

  const IdTableStatic<SUB_WIDTH> sub = dynSub.asStaticView<SUB_WIDTH>();
  const IdTableStatic<LEFT_WIDTH> left = dynLeft.asStaticView<LEFT_WIDTH>();
  IdTableStatic<RES_WIDTH> res = dynRes->moveToStatic<RES_WIDTH>();

  // Used to map entries in the left column to entries they have connection with
  Map edges;

  // initialize the map from the subresult
  for (size_t i = 0; i < sub.size(); i++) {
    size_t l = sub(i, leftSubCol);
    size_t r = sub(i, rightSubCol);
    MapIt it = edges.find(l);
    if (it == edges.end()) {
      std::shared_ptr<ad_utility::HashSet<Id>> s =
          std::make_shared<ad_utility::HashSet<Id>>();
      s->insert(r);
      edges[l] = s;
    } else {
      // If r is not in the vector insert it
      it->second->insert(r);
    }
  }

  // For every node do a dfs on the graph

  // Stores nodes we already have a path to. This avoids cycles.
  ad_utility::HashSet<Id> marks;

  // The stack used to store the dfs' progress
  std::vector<ad_utility::HashSet<Id>::const_iterator> positions;
  // Used to store all edges leading away from a node for every level.
  // Reduces access to the hashmap, and is safe as the map will not
  // be modified after this point.
  std::vector<std::shared_ptr<const ad_utility::HashSet<Id>>> edgeCache;

  size_t last_elem = std::numeric_limits<size_t>::max();
  size_t last_result_begin = 0;
  size_t last_result_end = 0;
  for (size_t i = 0; i < left.size(); i++) {
    if (left[i][leftSideCol] == last_elem) {
      // We can repeat the last output
      size_t num_new = last_result_end - last_result_begin;
      size_t res_row = res.size();
      res.resize(res.size() + num_new);
      for (size_t j = 0; j < num_new; j++) {
        for (size_t c = 0; c < resWidth; c++) {
          res(res_row + j, c) = res(last_result_begin + j, c);
        }
      }
      continue;
    }
    last_elem = left[i][leftSideCol];
    last_result_begin = res.size();
    MapIt rootEdges = edges.find(last_elem);
    if (rootEdges != edges.end()) {
      positions.push_back(rootEdges->second->begin());
      edgeCache.push_back(rootEdges->second);
    }
    if (minDist == 0) {
      AD_THROW(ad_semsearch::Exception::NOT_YET_IMPLEMENTED,
               "The TransitivePath operation does not support a minimum "
               "distance of 0 (use at least one instead).");
    }

    // While we have not found the entire transitive hull and have not reached
    // the max step limit
    while (!positions.empty()) {
      size_t stackIndex = positions.size() - 1;
      // Process the next child of the node at the top of the stack
      ad_utility::HashSet<Id>::const_iterator& pos = positions[stackIndex];
      const ad_utility::HashSet<Id>* nodeEdges = edgeCache.back().get();

      if (pos == nodeEdges->end()) {
        // We finished processing this node
        positions.pop_back();
        edgeCache.pop_back();
        continue;
      }

      size_t child = *pos;
      ++pos;
      size_t childDepth = positions.size();
      if (childDepth <= maxDist && marks.count(child) == 0) {
        // process the child
        if (childDepth >= minDist) {
          marks.insert(child);
          if (rightIsVar || child == rightValue) {
            size_t row = res.size();
            res.emplace_back();
            res(row, 0) = last_elem;
            res(row, 1) = child;
            for (size_t k = 2; k < resWidth + 1; k++) {
              if (k - 2 < leftSideCol) {
                res(row, k) = left(i, k - 2);
              } else if (k - 2 > leftSideCol) {
                res(row, k - 1) = left(i, k - 2);
              }
            }
          }
        }
        // Add the child to the stack
        MapIt it = edges.find(child);
        if (it != edges.end()) {
          positions.push_back(it->second->begin());
          edgeCache.push_back(it->second);
        }
      }
    }

    if (i + 1 < left.size()) {
      // reset everything for the next iteration
      marks.clear();
    }

    last_result_end = res.size();
  }

  *dynRes = res.moveToDynamic();
}

// _____________________________________________________________________________
std::string TransitivePath::asString(size_t indent) const {
  std::ostringstream os;
  for (size_t i = 0; i < indent; ++i) {
    os << " ";
  }
  os << "TransitivePath leftCol " << _leftSubCol << " rightCol "
     << _rightSubCol;

  if (!_leftIsVar) {
    os << " leftValue " << _leftValue;
  }
  if (!_rightIsVar) {
    os << " rightValue " << _rightValue;
  }
  os << " minDist " << _minDist << " maxDist " << _maxDist << "\n";
  os << _subtree->asString(indent) << "\n";
  if (_leftSideTree != nullptr) {
    os << "Left subtree:\n";
    os << _leftSideTree->asString(indent) << "\n";
  }
  os << _subtree->asString(indent) << "\n";
  if (_rightSideTree != nullptr) {
    os << "Right subtree:\n";
    os << _rightSideTree->asString(indent) << "\n";
  }
  return os.str();
}

// _____________________________________________________________________________
std::string TransitivePath::getDescriptor() const {
  std::ostringstream os;
  if (_leftSideTree != nullptr) {
    os << "TransitivePath left is subtree, rightCol " << _rightSubCol;
  } else if (_rightSideTree != nullptr) {
    os << "TransitivePath leftCol " << _leftSubCol << " right is subtree";
  } else {
    os << "TransitivePath leftCol " << _leftSubCol << " rightCol "
       << _rightSubCol;
  }
  if (!_leftIsVar) {
    os << " leftValue " << _leftValue;
  }
  if (!_rightIsVar) {
    os << " rightValue " << _rightValue;
  }
  os << " minDist " << _minDist << " maxDist " << _maxDist << "\n";
  return os.str();
}

// _____________________________________________________________________________
size_t TransitivePath::getResultWidth() const { return _resultWidth; }

// _____________________________________________________________________________
vector<size_t> TransitivePath::resultSortedOn() const {
  const std::vector<size_t>& subSortedOn =
      _subtree->getRootOperation()->getResultSortedOn();
  if (_leftSideTree == nullptr && _rightSideTree == nullptr && _leftIsVar &&
      _rightIsVar && subSortedOn.size() > 0 && subSortedOn[0] == _leftSubCol) {
    // This operation preserves the order of the _leftCol of the subtree.
    return {0};
  }
  if (_leftSideTree != nullptr) {
    const std::vector<size_t>& leftSortedOn =
        _leftSideTree->getRootOperation()->getResultSortedOn();
    if (leftSortedOn.size() > 0 && leftSortedOn[0] == _leftSideCol) {
      return {0};
    }
  }
  if (_rightSideTree != nullptr) {
    const std::vector<size_t>& rightSortedOn =
        _rightSideTree->getRootOperation()->getResultSortedOn();
    if (rightSortedOn.size() > 0 && rightSortedOn[0] == _rightSideCol) {
      return {1};
    }
  }
  return {};
}

// _____________________________________________________________________________
ad_utility::HashMap<std::string, size_t> TransitivePath::getVariableColumns()
    const {
  return _variableColumns;
}

// _____________________________________________________________________________
void TransitivePath::setTextLimit(size_t limit) {
  _subtree->setTextLimit(limit);
  if (_leftSideTree != nullptr) {
    _leftSideTree->setTextLimit(limit);
  }
  if (_rightSideTree != nullptr) {
    _rightSideTree->setTextLimit(limit);
  }
}

// _____________________________________________________________________________
bool TransitivePath::knownEmptyResult() { return _subtree->knownEmptyResult(); }

// _____________________________________________________________________________
float TransitivePath::getMultiplicity(size_t col) {
  (void)col;
  // The multiplicities are not known.
  return 1;
}

// _____________________________________________________________________________
size_t TransitivePath::getSizeEstimate() {
  if (_leftSideTree != nullptr) {
    return _leftSideTree->getSizeEstimate();
  }
  if (_rightSideTree != nullptr) {
    return _rightSideTree->getSizeEstimate();
  }
  // TODO(Florian): this is not necessarily a good estimator
  if (_leftIsVar) {
    return _subtree->getSizeEstimate() / _subtree->getMultiplicity(_leftSubCol);
  }
  return _subtree->getSizeEstimate();
}

// _____________________________________________________________________________
size_t TransitivePath::getCostEstimate() { return getSizeEstimate(); }

// _____________________________________________________________________________
template <int SUB_WIDTH>
void TransitivePath::computeTransitivePath(IdTable* res, const IdTable& sub,
                                           bool leftIsVar, bool rightIsVar,
                                           size_t leftSubCol,
                                           size_t rightSubCol, Id leftValue,
                                           Id rightValue, size_t minDist,
                                           size_t maxDist) {
  if (leftIsVar) {
    if (rightIsVar) {
      computeTransitivePath<SUB_WIDTH, true, true>(
          res, sub, leftSubCol, rightSubCol, leftValue, rightValue, minDist,
          maxDist);
    } else {
      computeTransitivePath<SUB_WIDTH, true, false>(
          res, sub, leftSubCol, rightSubCol, leftValue, rightValue, minDist,
          maxDist);
    }
  } else {
    if (rightIsVar) {
      computeTransitivePath<SUB_WIDTH, false, true>(
          res, sub, leftSubCol, rightSubCol, leftValue, rightValue, minDist,
          maxDist);
    } else {
      computeTransitivePath<SUB_WIDTH, false, false>(
          res, sub, leftSubCol, rightSubCol, leftValue, rightValue, minDist,
          maxDist);
    }
  }
}

// This instantiantion is needed by the unit tests
template void TransitivePath::computeTransitivePath<2>(
    IdTable* res, const IdTable& sub, bool leftIsVar, bool rightIsVar,
    size_t leftSubCol, size_t rightSubCol, Id leftValue, Id rightValue,
    size_t minDist, size_t maxDist);

// _____________________________________________________________________________
template <int SUB_WIDTH, bool leftIsVar, bool rightIsVar>
void TransitivePath::computeTransitivePath(IdTable* dynRes,
                                           const IdTable& dynSub,
                                           size_t leftSubCol,
                                           size_t rightSubCol, Id leftValue,
                                           Id rightValue, size_t minDist,
                                           size_t maxDist) {
  using Map = ad_utility::HashMap<Id, std::shared_ptr<ad_utility::HashSet<Id>>>;
  using MapIt = Map::iterator;

  if constexpr (!leftIsVar && !rightIsVar) {
    return;
  }

  const IdTableStatic<SUB_WIDTH> sub = dynSub.asStaticView<SUB_WIDTH>();
  IdTableStatic<2> res = dynRes->moveToStatic<2>();

  // Used to map entries in the left column to entries they have connection with
  Map edges;
  // All nodes on the graph from which an edge leads to another node
  std::vector<Id> nodes;

  // initialize the map from the subresult
  if constexpr (rightIsVar) {
    (void)rightValue;
    if (!leftIsVar) {
      nodes.push_back(leftValue);
    }
    for (size_t i = 0; i < sub.size(); i++) {
      size_t l = sub(i, leftSubCol);
      size_t r = sub(i, rightSubCol);
      MapIt it = edges.find(l);
      if (it == edges.end()) {
        if constexpr (leftIsVar) {
          nodes.push_back(l);
        }
        std::shared_ptr<ad_utility::HashSet<Id>> s =
            std::make_shared<ad_utility::HashSet<Id>>();
        s->insert(r);
        edges[l] = s;
      } else {
        // If r is not in the vector insert it
        it->second->insert(r);
      }
    }
  } else {
    (void)leftValue;
    nodes.push_back(rightValue);
    for (size_t i = 0; i < sub.size(); i++) {
      // Use the inverted edges
      size_t l = sub(i, leftSubCol);
      size_t r = sub(i, rightSubCol);
      MapIt it = edges.find(r);
      if (it == edges.end()) {
        std::shared_ptr<ad_utility::HashSet<Id>> s =
            std::make_shared<ad_utility::HashSet<Id>>();
        s->insert(l);
        edges[r] = s;
      } else {
        // If r is not in the vector insert it
        it->second->insert(l);
      }
    }
  }

  // For every node do a dfs on the graph

  // Stores nodes we already have a path to. This avoids cycles.
  ad_utility::HashSet<Id> marks;

  // The stack used to store the dfs' progress
  std::vector<ad_utility::HashSet<Id>::const_iterator> positions;

  // Used to store all edges leading away from a node for every level.
  // Reduces access to the hashmap, and is safe as the map will not
  // be modified after this point.
  std::vector<std::shared_ptr<const ad_utility::HashSet<Id>>> edgeCache;

  for (size_t i = 0; i < nodes.size(); i++) {
    MapIt rootEdges = edges.find(nodes[i]);
    if (rootEdges != edges.end()) {
      positions.push_back(rootEdges->second->begin());
      edgeCache.push_back(rootEdges->second);
    }
    if (minDist == 0) {
      AD_THROW(ad_semsearch::Exception::NOT_YET_IMPLEMENTED,
               "The TransitivePath operation does not support a minimum "
               "distance of 0 (use at least one instead).");
    }

    // While we have not found the entire transitive hull and have not reached
    // the max step limit
    while (!positions.empty()) {
      size_t stackIndex = positions.size() - 1;
      // Process the next child of the node at the top of the stack
      ad_utility::HashSet<Id>::const_iterator& pos = positions[stackIndex];
      const ad_utility::HashSet<Id>* nodeEdges = edgeCache.back().get();

      if (pos == nodeEdges->end()) {
        // We finished processing this node
        positions.pop_back();
        edgeCache.pop_back();
        continue;
      }

      size_t child = *pos;
      ++pos;
      size_t childDepth = positions.size();
      if (childDepth <= maxDist && marks.count(child) == 0) {
        // process the child
        if (childDepth >= minDist) {
          marks.insert(child);
          if constexpr (rightIsVar) {
            res.push_back({nodes[i], child});
          } else {
            res.push_back({child, nodes[i]});
          }
        }
        // Add the child to the stack
        MapIt it = edges.find(child);
        if (it != edges.end()) {
          positions.push_back(it->second->begin());
          edgeCache.push_back(it->second);
        }
      }
    }

    if (i + 1 < nodes.size()) {
      // reset everything for the next iteration
      marks.clear();
    }
  }

  *dynRes = res.moveToDynamic();
}

template <int SUB_WIDTH, int LEFT_WIDTH, int RES_WIDTH>
void TransitivePath::computeTransitivePathLeftBoundRightIsVar(
    IdTable* resTable, const IdTable& subTable, const IdTable& leftTable,
    size_t leftSideCol, bool rightIsVar, size_t leftSubCol, size_t rightSubCol,
    Id rightValue, size_t minDist, size_t maxDist, size_t resWidth) {

  struct X {
    Id id;
    size_t distance;
  };

  using Map = ad_utility::HashMap<Id, ad_utility::HashMap<Id, size_t>>;


  const IdTableStatic<SUB_WIDTH> sub = subTable.asStaticView<SUB_WIDTH>();
  const IdTableStatic<LEFT_WIDTH> left = leftTable.asStaticView<LEFT_WIDTH>();
  IdTableStatic<RES_WIDTH> res = resTable->moveToStatic<RES_WIDTH>();

  // Used to map entries in the left column to entries they have connection with
  //Map edges;
  Map results;

  if (minDist == 0) {
    AD_THROW(ad_semsearch::Exception::NOT_YET_IMPLEMENTED,
             "The TransitivePath operation does not support a minimum "
             "distance of 0 (use at least one instead).");
  }

  LOG(DEBUG) << "Starting leftBound transitivePath" << std::endl;

  auto compRow = [&](){
    if constexpr (SUB_WIDTH == 0) {
      return typename decltype(sub)::Row{sub.cols()};
    } else {
      return typename decltype(sub)::Row{};
    }
  }();

  LOG(DEBUG) << "Set up the comp row" << std::endl;

  auto getAdjacent = [&](Id id) {
    compRow[leftSubCol] = id;
    return std::equal_range(sub.begin(), sub.end(), compRow, [leftSubCol](const auto& a, const auto& b){return a[leftSubCol] < b[leftSubCol];});
  };

  auto calculateReachable = [&] (Id id) {
    auto [beg, end] = getAdjacent(id);
    ad_utility::HashMap<Id, size_t> reachable;
    ad_utility::HashMap<Id, size_t> toBeChecked;
    for (; beg != end; ++beg) {
      reachable.insert({(*beg)[rightSubCol], 1});
      toBeChecked.insert({(*beg)[rightSubCol], 1});
    }
    size_t dist = 1;
    while (!toBeChecked.empty() && dist < maxDist) {
      ad_utility::HashMap<Id, size_t> newFound;
      for (const auto&  [nextId, distSoFar] : toBeChecked) {
        if (results.contains(nextId)) {
          const auto& nextRes = results[nextId];
          for (const auto& [id, nextDist] : nextRes) {
            if (nextDist + distSoFar > maxDist) {
              continue;
            }
            if (!reachable.count(id) || reachable[id] < distSoFar + nextDist) {
              newFound[id] = distSoFar + nextDist;
              reachable[id] = distSoFar + nextDist;
            }
          }
        } else {
          auto [fst, lst] = getAdjacent(nextId);
          for (; fst < lst; ++fst) {
            Id next = (*fst)[rightSubCol];
            if (!reachable.count(next) || reachable[next] > dist) {
              newFound[next] = dist;
              reachable[next] = dist;
            }
          }

        }
      }
      toBeChecked = std::move(newFound);
      ++dist;
      if (dist == minDist) {
        toBeChecked = std::move(reachable);
        reachable.clear();
      }
    }
    return reachable;
  };

  size_t numWritten = 0;
  LOG(DEBUG) << "Starting inner TransPath for " << left.size() << "elements" << std::endl;
  for (size_t i = 0; i < left.size(); ++i) {
    if (i == 1 || !i%10000 || true) {
      LOG(DEBUG) << "Started " << i << " out of " << left.size() << std::endl;
    }

    Id cur = left[i][leftSideCol];
    if (!results.contains(cur)) {
      results.insert({cur, calculateReachable(cur)});
    }
    const auto& reachables = results[cur];
    auto res_row = res.size();
    for (const auto& [id, dist] : reachables) {
      (void) dist;
      res.emplace_back();
      if (!numWritten % 10000) {
        LOG(DEBUG) << "Wrote " << numWritten << " outputs of transPath " << std::endl;
      }

      res(res_row, 0) = cur;
      res(res_row, 1) = id;
      for (size_t k = 2; k < resWidth + 1; k++) {
        if (k - 2 < leftSideCol) {
          res(res_row, k) = left(i, k - 2);
        } else if (k - 2 > leftSideCol) {
          res(res_row, k - 1) = left(i, k - 2);
        }
      }
      ++res_row;
      ++numWritten;
    }
  }
  *resTable = res.moveToDynamic();
}

// _____________________________________________________________________________
template <int SUB_WIDTH, int LEFT_WIDTH, int RES_WIDTH>
void TransitivePath::computeTransitivePathRightBound(
    IdTable* dynRes, const IdTable& dynSub, const IdTable& dynRight,
    size_t rightSideCol, bool leftIsVar, size_t leftSubCol, size_t rightSubCol,
    Id leftValue, size_t minDist, size_t maxDist, size_t resWidth) {
  // Do the discovery from the right instead of the left.

  using Map = ad_utility::HashMap<Id, std::shared_ptr<ad_utility::HashSet<Id>>>;
  using MapIt = Map::iterator;

  const IdTableStatic<SUB_WIDTH> sub = dynSub.asStaticView<SUB_WIDTH>();
  const IdTableStatic<LEFT_WIDTH> right = dynRight.asStaticView<LEFT_WIDTH>();
  IdTableStatic<RES_WIDTH> res = dynRes->moveToStatic<RES_WIDTH>();

  // Used to map entries in the left column to entries they have connection with
  Map edges;

  // initialize the map from the subresult
  for (size_t i = 0; i < sub.size(); i++) {
    size_t l = sub(i, leftSubCol);
    size_t r = sub(i, rightSubCol);
    MapIt it = edges.find(r);
    if (it == edges.end()) {
      std::shared_ptr<ad_utility::HashSet<Id>> s =
          std::make_shared<ad_utility::HashSet<Id>>();
      s->insert(l);
      edges[r] = s;
    } else {
      it->second->insert(l);
    }
  }

  // For every node do a dfs on the graph

  // Stores nodes we already have a path to. This avoids cycles.
  ad_utility::HashSet<Id> marks;

  // The stack used to store the dfs' progress
  std::vector<ad_utility::HashSet<Id>::const_iterator> positions;

  // Used to store all edges leading away from a node for every level.
  // Reduces access to the hashmap, and is safe as the map will not
  // be modified after this point.
  std::vector<std::shared_ptr<const ad_utility::HashSet<Id>>> edgeCache;

  size_t last_elem = std::numeric_limits<size_t>::max();
  size_t last_result_begin = 0;
  size_t last_result_end = 0;
  for (size_t i = 0; i < right.size(); i++) {
    if (right[i][rightSideCol] == last_elem) {
      // We can repeat the last output
      size_t num_new = last_result_end - last_result_begin;
      size_t res_row = res.size();
      res.resize(res.size() + num_new);
      for (size_t j = 0; j < num_new; j++) {
        for (size_t c = 0; c < resWidth; c++) {
          res(res_row + j, c) = res(last_result_begin + j, c);
        }
      }
      continue;
    }
    last_elem = right(i, rightSideCol);
    last_result_begin = res.size();
    MapIt rootEdges = edges.find(last_elem);
    if (rootEdges != edges.end()) {
      positions.push_back(rootEdges->second->begin());
      edgeCache.push_back(rootEdges->second);
    }
    if (minDist == 0) {
      AD_THROW(ad_semsearch::Exception::NOT_YET_IMPLEMENTED,
               "The TransitivePath operation does not support a minimum "
               "distance of 0 (use at least one instead).");
    }

    // While we have not found the entire transitive hull and have not reached
    // the max step limit
    while (!positions.empty()) {
      size_t stackIndex = positions.size() - 1;
      // Process the next child of the node at the top of the stack
      ad_utility::HashSet<Id>::const_iterator& pos = positions[stackIndex];
      const ad_utility::HashSet<Id>* nodeEdges = edgeCache.back().get();

      if (pos == nodeEdges->end()) {
        // We finished processing this node
        positions.pop_back();
        edgeCache.pop_back();
        continue;
      }

      size_t child = *pos;
      ++pos;
      size_t childDepth = positions.size();
      if (childDepth <= maxDist && marks.count(child) == 0) {
        // process the child
        if (childDepth >= minDist) {
          marks.insert(child);
          if (leftIsVar || child == leftValue) {
            size_t row = res.size();
            res.emplace_back();
            res(row, 0) = child;
            res(row, 1) = last_elem;
            for (size_t k = 2; k < resWidth + 1; k++) {
              if (k - 2 < rightSideCol) {
                res(row, k) = right(i, k - 2);
              } else if (k - 2 > rightSideCol) {
                res(row, k - 1) = right(i, k - 2);
              }
            }
          }
        }
        // Add the child to the stack
        MapIt it = edges.find(child);
        if (it != edges.end()) {
          positions.push_back(it->second->begin());
          edgeCache.push_back(it->second);
        }
      }
    }

    if (i + 1 < right.size()) {
      // reset everything for the next iteration
      marks.clear();
    }

    last_result_end = res.size();
  }

  *dynRes = res.moveToDynamic();
}

// _____________________________________________________________________________
void TransitivePath::computeResult(ResultTable* result) {
  LOG(DEBUG) << "TransitivePath result computation..." << std::endl;
  shared_ptr<const ResultTable> subRes = _subtree->getResult();
  LOG(DEBUG) << "TransitivePath subresult computation done." << std::endl;

  RuntimeInformation& runtimeInfo = getRuntimeInfo();
  runtimeInfo.addChild(_subtree->getRootOperation()->getRuntimeInfo());

  result->_sortedBy = resultSortedOn();
  if (_leftIsVar || _leftSideTree != nullptr) {
    result->_resultTypes.push_back(subRes->getResultType(_leftSubCol));
  } else {
    result->_resultTypes.push_back(ResultTable::ResultType::KB);
  }
  if (_rightIsVar || _rightSideTree != nullptr) {
    result->_resultTypes.push_back(subRes->getResultType(_rightSubCol));
  } else {
    result->_resultTypes.push_back(ResultTable::ResultType::KB);
  }
  result->_data.setCols(getResultWidth());

  int subWidth = subRes->_data.cols();
  if (_leftSideTree != nullptr) {
    shared_ptr<const ResultTable> leftRes = _leftSideTree->getResult();
    for (size_t c = 0; c < leftRes->_data.cols(); c++) {
      if (c != _leftSideCol) {
        result->_resultTypes.push_back(leftRes->getResultType(c));
      }
    }
    runtimeInfo.addChild(_leftSideTree->getRootOperation()->getRuntimeInfo());
    int leftWidth = leftRes->_data.cols();
    // TODO<joka921> do a thorough evaluation of what is faster...
    if (_rightIsVar && subRes->size() > leftRes->size() * 3) {
      CALL_FIXED_SIZE_3(subWidth, leftWidth, _resultWidth,
                        computeTransitivePathLeftBoundRightIsVar, &result->_data,
                        subRes->_data, leftRes->_data, _leftSideCol, _rightIsVar,
                        _leftSubCol, _rightSubCol, _rightValue, _minDist,
                        _maxDist, _resultWidth);

    } else {
      CALL_FIXED_SIZE_3(subWidth, leftWidth, _resultWidth,
                        computeTransitivePathLeftBound, &result->_data,
                        subRes->_data, leftRes->_data, _leftSideCol,
                        _rightIsVar, _leftSubCol, _rightSubCol, _rightValue,
                        _minDist, _maxDist, _resultWidth);
    }
  } else if (_rightSideTree != nullptr) {
    shared_ptr<const ResultTable> rightRes = _rightSideTree->getResult();
    for (size_t c = 0; c < rightRes->_data.cols(); c++) {
      if (c != _rightSideCol) {
        result->_resultTypes.push_back(rightRes->getResultType(c));
      }
    }
    runtimeInfo.addChild(_rightSideTree->getRootOperation()->getRuntimeInfo());
    int rightWidth = rightRes->_data.cols();
    CALL_FIXED_SIZE_3(subWidth, rightWidth, _resultWidth,
                      computeTransitivePathRightBound, &result->_data,
                      subRes->_data, rightRes->_data, _rightSideCol, _leftIsVar,
                      _leftSubCol, _rightSubCol, _leftValue, _minDist, _maxDist,
                      _resultWidth);
  } else {
    CALL_FIXED_SIZE_1(subWidth, computeTransitivePath, &result->_data,
                      subRes->_data, _leftIsVar, _rightIsVar, _leftSubCol,
                      _rightSubCol, _leftValue, _rightValue, _minDist,
                      _maxDist);
  }

  LOG(DEBUG) << "TransitivePath result computation done." << std::endl;
}

// _____________________________________________________________________________
std::shared_ptr<TransitivePath> TransitivePath::bindLeftSide(
    std::shared_ptr<QueryExecutionTree> leftop, size_t inputCol) const {
  AD_CHECK(_childOrdered == ChildOrdered::Left || _rightIsVar);
  // Create a copy of this
  std::shared_ptr<TransitivePath> p = std::make_shared<TransitivePath>(*this);
  p->_leftSideTree = leftop;
  p->_leftSideCol = inputCol;
  const ad_utility::HashMap<string, size_t>& var = leftop->getVariableColumns();
  for (auto col : var) {
    if (col.second != inputCol) {
      if (col.second > inputCol) {
        p->_variableColumns[col.first] = col.second + 1;
      } else {
        p->_variableColumns[col.first] = col.second + 2;
      }
      p->_resultWidth++;
    }
  }
  return p;
}

// _____________________________________________________________________________
std::shared_ptr<TransitivePath> TransitivePath::bindRightSide(
    std::shared_ptr<QueryExecutionTree> rightop, size_t inputCol) const {
  // Create a copy of this
  AD_CHECK(_childOrdered == ChildOrdered::Right || _leftIsVar);
  std::shared_ptr<TransitivePath> p = std::make_shared<TransitivePath>(*this);
  p->_rightSideTree = rightop;
  p->_rightSideCol = inputCol;
  const ad_utility::HashMap<string, size_t>& var =
      rightop->getVariableColumns();
  for (auto col : var) {
    if (col.second != inputCol) {
      if (col.second > inputCol) {
        p->_variableColumns[col.first] = col.second + 1;
      } else {
        p->_variableColumns[col.first] = col.second + 2;
      }
      p->_resultWidth++;
    }
  }
  return p;
}

// _____________________________________________________________________________
bool TransitivePath::isBound() const {
  return _leftSideTree != nullptr || _rightSideTree != nullptr || !_leftIsVar ||
         !_rightIsVar;
}
