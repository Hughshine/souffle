#ifndef QUERYMANAGER_H
#define QUERYMANAGER_H

#include "souffle/RamTypes.h"
#include "souffle/problog/Atom.h"
#include "souffle/problog/Query.h"
#include "souffle/problog/Rule.h"
#include <cassert>
#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

class QueryManager {
public:
    QueryManager(std::vector<Query> queries);
    void addQuery(Query query);
    std::vector<const Query*> getAllQuery() const;
private:
    std::unordered_map<std::size_t, Query> queries;
    std::size_t nextQueryId = 0;
};

#endif // QUERYMANAGER_H
