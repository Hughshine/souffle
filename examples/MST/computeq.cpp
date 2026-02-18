#define SOUFFLE_GENERATOR_VERSION "efb25a3a4"
#include "souffle/CompiledSouffle.h"
#include "souffle/Derivation.h"
#include "souffle/SignalHandler.h"
#include "souffle/SouffleInterface.h"
#include "souffle/cli/Cli.h"
#include "souffle/datastructure/BTree.h"
#include "souffle/datastructure/BTreeDelete.h"
#include "souffle/io/IOSystem.h"
#include "souffle/problog/Atom.h"
#include "souffle/problog/Pipeline.h"
#include "souffle/problog/Query.h"
#include "souffle/problog/QueryManager.h"
#include "souffle/problog/Rule.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/debug/Debugger.h"
#include "souffle/utility/MiscUtil.h"
#include <any>
namespace functors {
extern "C" {
}
} //namespace functors
namespace souffle::t_btree_100_ii__0_1__11 {
using namespace souffle;
struct Type {
static constexpr Relation::arity_type Arity = 2;
using t_tuple = Tuple<RamDomain, 2>;
struct t_comparator_0{
 int operator()(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0])) ? -1 : (ramBitCast<RamSigned>(a[0]) > ramBitCast<RamSigned>(b[0])) ? 1 :((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1])) ? -1 : (ramBitCast<RamSigned>(a[1]) > ramBitCast<RamSigned>(b[1])) ? 1 :(0));
 }
bool less(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0]))|| ((ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0])) && ((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1]))));
 }
bool equal(const t_tuple& a, const t_tuple& b) const {
return (ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0]))&&(ramBitCast<RamSigned>(a[1]) == ramBitCast<RamSigned>(b[1]));
 }
};
using t_ind_0 = btree_delete_set<t_tuple,t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool erase(const t_tuple& t);
bool insert(const t_tuple& t);
bool insert(const t_tuple& t, context& h);
bool insert(const RamDomain* ramDomain);
bool insert(RamDomain a0,RamDomain a1);
bool contains(const t_tuple& t, context& h) const;
bool contains(const t_tuple& t) const;
std::size_t size() const;
iterator find(const t_tuple& t, context& h) const;
iterator find(const t_tuple& t) const;
range<iterator> lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;
range<iterator> lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */) const;
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_100_ii__0_1__11 
namespace souffle::t_btree_100_ii__0_1__11 {
using namespace souffle;
using t_ind_0 = Type::t_ind_0;
using iterator = Type::iterator;
using context = Type::context;
bool Type::erase(const t_tuple& t) {
if (ind_0.erase(t) > 0) {
return true;
} else return false;
}
bool Type::insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool Type::insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool Type::insert(const RamDomain* ramDomain) {
RamDomain data[2];
std::copy(ramDomain, ramDomain + 2, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool Type::insert(RamDomain a0,RamDomain a1) {
RamDomain data[2] = {a0,a1};
return insert(data);
}
bool Type::contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool Type::contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t Type::size() const {
return ind_0.size();
}
iterator Type::find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator Type::find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> Type::lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> Type::lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> Type::lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_11(lower,upper,h);
}
bool Type::empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> Type::partition() const {
return ind_0.getChunks(400);
}
void Type::purge() {
ind_0.clear();
}
iterator Type::begin() const {
return ind_0.begin();
}
iterator Type::end() const {
return ind_0.end();
}
void Type::printStatistics(std::ostream& o) const {
o << " arity 2 direct b-tree index 0 lex-order [0,1]\n";
ind_0.printStats(o);
}
} // namespace souffle::t_btree_100_ii__0_1__11 
namespace souffle::t_btree_000_ii__0_1__11 {
using namespace souffle;
struct Type {
static constexpr Relation::arity_type Arity = 2;
using t_tuple = Tuple<RamDomain, 2>;
struct t_comparator_0{
 int operator()(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0])) ? -1 : (ramBitCast<RamSigned>(a[0]) > ramBitCast<RamSigned>(b[0])) ? 1 :((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1])) ? -1 : (ramBitCast<RamSigned>(a[1]) > ramBitCast<RamSigned>(b[1])) ? 1 :(0));
 }
bool less(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0]))|| ((ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0])) && ((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1]))));
 }
bool equal(const t_tuple& a, const t_tuple& b) const {
return (ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0]))&&(ramBitCast<RamSigned>(a[1]) == ramBitCast<RamSigned>(b[1]));
 }
};
using t_ind_0 = btree_set<t_tuple,t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t);
bool insert(const t_tuple& t, context& h);
bool insert(const RamDomain* ramDomain);
bool insert(RamDomain a0,RamDomain a1);
bool contains(const t_tuple& t, context& h) const;
bool contains(const t_tuple& t) const;
std::size_t size() const;
iterator find(const t_tuple& t, context& h) const;
iterator find(const t_tuple& t) const;
range<iterator> lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;
range<iterator> lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */) const;
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_000_ii__0_1__11 
namespace souffle::t_btree_000_ii__0_1__11 {
using namespace souffle;
using t_ind_0 = Type::t_ind_0;
using iterator = Type::iterator;
using context = Type::context;
bool Type::insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool Type::insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool Type::insert(const RamDomain* ramDomain) {
RamDomain data[2];
std::copy(ramDomain, ramDomain + 2, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool Type::insert(RamDomain a0,RamDomain a1) {
RamDomain data[2] = {a0,a1};
return insert(data);
}
bool Type::contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool Type::contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t Type::size() const {
return ind_0.size();
}
iterator Type::find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator Type::find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> Type::lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> Type::lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> Type::lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_11(lower,upper,h);
}
bool Type::empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> Type::partition() const {
return ind_0.getChunks(400);
}
void Type::purge() {
ind_0.clear();
}
iterator Type::begin() const {
return ind_0.begin();
}
iterator Type::end() const {
return ind_0.end();
}
void Type::printStatistics(std::ostream& o) const {
o << " arity 2 direct b-tree index 0 lex-order [0,1]\n";
ind_0.printStats(o);
}
} // namespace souffle::t_btree_000_ii__0_1__11 
namespace souffle::t_btree_100_ii__1_0__11__01 {
using namespace souffle;
struct Type {
static constexpr Relation::arity_type Arity = 2;
using t_tuple = Tuple<RamDomain, 2>;
struct t_comparator_0{
 int operator()(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1])) ? -1 : (ramBitCast<RamSigned>(a[1]) > ramBitCast<RamSigned>(b[1])) ? 1 :((ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0])) ? -1 : (ramBitCast<RamSigned>(a[0]) > ramBitCast<RamSigned>(b[0])) ? 1 :(0));
 }
bool less(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1]))|| ((ramBitCast<RamSigned>(a[1]) == ramBitCast<RamSigned>(b[1])) && ((ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0]))));
 }
bool equal(const t_tuple& a, const t_tuple& b) const {
return (ramBitCast<RamSigned>(a[1]) == ramBitCast<RamSigned>(b[1]))&&(ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0]));
 }
};
using t_ind_0 = btree_delete_set<t_tuple,t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool erase(const t_tuple& t);
bool insert(const t_tuple& t);
bool insert(const t_tuple& t, context& h);
bool insert(const RamDomain* ramDomain);
bool insert(RamDomain a0,RamDomain a1);
bool contains(const t_tuple& t, context& h) const;
bool contains(const t_tuple& t) const;
std::size_t size() const;
iterator find(const t_tuple& t, context& h) const;
iterator find(const t_tuple& t) const;
range<iterator> lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;
range<iterator> lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */) const;
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const;
range<t_ind_0::iterator> lowerUpperRange_01(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_01(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_100_ii__1_0__11__01 
namespace souffle::t_btree_100_ii__1_0__11__01 {
using namespace souffle;
using t_ind_0 = Type::t_ind_0;
using iterator = Type::iterator;
using context = Type::context;
bool Type::erase(const t_tuple& t) {
if (ind_0.erase(t) > 0) {
return true;
} else return false;
}
bool Type::insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool Type::insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool Type::insert(const RamDomain* ramDomain) {
RamDomain data[2];
std::copy(ramDomain, ramDomain + 2, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool Type::insert(RamDomain a0,RamDomain a1) {
RamDomain data[2] = {a0,a1};
return insert(data);
}
bool Type::contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool Type::contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t Type::size() const {
return ind_0.size();
}
iterator Type::find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator Type::find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> Type::lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> Type::lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> Type::lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_11(lower,upper,h);
}
range<t_ind_0::iterator> Type::lowerUpperRange_01(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_01(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_01(lower,upper,h);
}
bool Type::empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> Type::partition() const {
return ind_0.getChunks(400);
}
void Type::purge() {
ind_0.clear();
}
iterator Type::begin() const {
return ind_0.begin();
}
iterator Type::end() const {
return ind_0.end();
}
void Type::printStatistics(std::ostream& o) const {
o << " arity 2 direct b-tree index 0 lex-order [1,0]\n";
ind_0.printStats(o);
}
} // namespace souffle::t_btree_100_ii__1_0__11__01 
namespace souffle::t_btree_100_ii__0_1__11__10 {
using namespace souffle;
struct Type {
static constexpr Relation::arity_type Arity = 2;
using t_tuple = Tuple<RamDomain, 2>;
struct t_comparator_0{
 int operator()(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0])) ? -1 : (ramBitCast<RamSigned>(a[0]) > ramBitCast<RamSigned>(b[0])) ? 1 :((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1])) ? -1 : (ramBitCast<RamSigned>(a[1]) > ramBitCast<RamSigned>(b[1])) ? 1 :(0));
 }
bool less(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0]))|| ((ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0])) && ((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1]))));
 }
bool equal(const t_tuple& a, const t_tuple& b) const {
return (ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0]))&&(ramBitCast<RamSigned>(a[1]) == ramBitCast<RamSigned>(b[1]));
 }
};
using t_ind_0 = btree_delete_set<t_tuple,t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool erase(const t_tuple& t);
bool insert(const t_tuple& t);
bool insert(const t_tuple& t, context& h);
bool insert(const RamDomain* ramDomain);
bool insert(RamDomain a0,RamDomain a1);
bool contains(const t_tuple& t, context& h) const;
bool contains(const t_tuple& t) const;
std::size_t size() const;
iterator find(const t_tuple& t, context& h) const;
iterator find(const t_tuple& t) const;
range<iterator> lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;
range<iterator> lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */) const;
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const;
range<t_ind_0::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_100_ii__0_1__11__10 
namespace souffle::t_btree_100_ii__0_1__11__10 {
using namespace souffle;
using t_ind_0 = Type::t_ind_0;
using iterator = Type::iterator;
using context = Type::context;
bool Type::erase(const t_tuple& t) {
if (ind_0.erase(t) > 0) {
return true;
} else return false;
}
bool Type::insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool Type::insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool Type::insert(const RamDomain* ramDomain) {
RamDomain data[2];
std::copy(ramDomain, ramDomain + 2, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool Type::insert(RamDomain a0,RamDomain a1) {
RamDomain data[2] = {a0,a1};
return insert(data);
}
bool Type::contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool Type::contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t Type::size() const {
return ind_0.size();
}
iterator Type::find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator Type::find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> Type::lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> Type::lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> Type::lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_11(lower,upper,h);
}
range<t_ind_0::iterator> Type::lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_10(lower,upper,h);
}
bool Type::empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> Type::partition() const {
return ind_0.getChunks(400);
}
void Type::purge() {
ind_0.clear();
}
iterator Type::begin() const {
return ind_0.begin();
}
iterator Type::end() const {
return ind_0.end();
}
void Type::printStatistics(std::ostream& o) const {
o << " arity 2 direct b-tree index 0 lex-order [0,1]\n";
ind_0.printStats(o);
}
} // namespace souffle::t_btree_100_ii__0_1__11__10 
namespace souffle::t_btree_100_i__0__1 {
using namespace souffle;
struct Type {
static constexpr Relation::arity_type Arity = 1;
using t_tuple = Tuple<RamDomain, 1>;
struct t_comparator_0{
 int operator()(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0])) ? -1 : (ramBitCast<RamSigned>(a[0]) > ramBitCast<RamSigned>(b[0])) ? 1 :(0);
 }
bool less(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0]));
 }
bool equal(const t_tuple& a, const t_tuple& b) const {
return (ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0]));
 }
};
using t_ind_0 = btree_delete_set<t_tuple,t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool erase(const t_tuple& t);
bool insert(const t_tuple& t);
bool insert(const t_tuple& t, context& h);
bool insert(const RamDomain* ramDomain);
bool insert(RamDomain a0);
bool contains(const t_tuple& t, context& h) const;
bool contains(const t_tuple& t) const;
std::size_t size() const;
iterator find(const t_tuple& t, context& h) const;
iterator find(const t_tuple& t) const;
range<iterator> lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;
range<iterator> lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */) const;
range<t_ind_0::iterator> lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_100_i__0__1 
namespace souffle::t_btree_100_i__0__1 {
using namespace souffle;
using t_ind_0 = Type::t_ind_0;
using iterator = Type::iterator;
using context = Type::context;
bool Type::erase(const t_tuple& t) {
if (ind_0.erase(t) > 0) {
return true;
} else return false;
}
bool Type::insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool Type::insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool Type::insert(const RamDomain* ramDomain) {
RamDomain data[1];
std::copy(ramDomain, ramDomain + 1, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool Type::insert(RamDomain a0) {
RamDomain data[1] = {a0};
return insert(data);
}
bool Type::contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool Type::contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t Type::size() const {
return ind_0.size();
}
iterator Type::find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator Type::find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> Type::lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> Type::lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> Type::lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_1(lower,upper,h);
}
bool Type::empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> Type::partition() const {
return ind_0.getChunks(400);
}
void Type::purge() {
ind_0.clear();
}
iterator Type::begin() const {
return ind_0.begin();
}
iterator Type::end() const {
return ind_0.end();
}
void Type::printStatistics(std::ostream& o) const {
o << " arity 1 direct b-tree index 0 lex-order [0]\n";
ind_0.printStats(o);
}
} // namespace souffle::t_btree_100_i__0__1 
namespace souffle::t_btree_000_i__0__1 {
using namespace souffle;
struct Type {
static constexpr Relation::arity_type Arity = 1;
using t_tuple = Tuple<RamDomain, 1>;
struct t_comparator_0{
 int operator()(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0])) ? -1 : (ramBitCast<RamSigned>(a[0]) > ramBitCast<RamSigned>(b[0])) ? 1 :(0);
 }
bool less(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0]));
 }
bool equal(const t_tuple& a, const t_tuple& b) const {
return (ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0]));
 }
};
using t_ind_0 = btree_set<t_tuple,t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t);
bool insert(const t_tuple& t, context& h);
bool insert(const RamDomain* ramDomain);
bool insert(RamDomain a0);
bool contains(const t_tuple& t, context& h) const;
bool contains(const t_tuple& t) const;
std::size_t size() const;
iterator find(const t_tuple& t, context& h) const;
iterator find(const t_tuple& t) const;
range<iterator> lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;
range<iterator> lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */) const;
range<t_ind_0::iterator> lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_000_i__0__1 
namespace souffle::t_btree_000_i__0__1 {
using namespace souffle;
using t_ind_0 = Type::t_ind_0;
using iterator = Type::iterator;
using context = Type::context;
bool Type::insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool Type::insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool Type::insert(const RamDomain* ramDomain) {
RamDomain data[1];
std::copy(ramDomain, ramDomain + 1, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool Type::insert(RamDomain a0) {
RamDomain data[1] = {a0};
return insert(data);
}
bool Type::contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool Type::contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t Type::size() const {
return ind_0.size();
}
iterator Type::find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator Type::find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> Type::lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> Type::lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> Type::lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_1(lower,upper,h);
}
bool Type::empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> Type::partition() const {
return ind_0.getChunks(400);
}
void Type::purge() {
ind_0.clear();
}
iterator Type::begin() const {
return ind_0.begin();
}
iterator Type::end() const {
return ind_0.end();
}
void Type::printStatistics(std::ostream& o) const {
o << " arity 1 direct b-tree index 0 lex-order [0]\n";
ind_0.printStats(o);
}
} // namespace souffle::t_btree_000_i__0__1 
namespace  souffle {
using namespace souffle;
class Stratum_a_f5cc2531020019db {
public:
 Stratum_a_f5cc2531020019db(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_ii__0_1__11__10::Type& rel_a_454ddee488c1ed6b,t_btree_100_ii__0_1__11::Type& rel_b_96694f8e93f5c77d,t_btree_100_ii__1_0__11__01::Type& rel_c_981811ba2479fc8d);
void run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret);
private:
SymbolTable& symTable;
RecordTable& recordTable;
ConcurrentCache<std::string,std::regex>& regexCache;
bool& pruneImdtRels;
bool& performIO;
SignalHandler*& signalHandler;
std::atomic<std::size_t>& iter;
std::atomic<RamDomain>& ctr;
std::string& inputDirectory;
std::string& outputDirectory;
t_btree_100_ii__0_1__11__10::Type* rel_a_454ddee488c1ed6b;
t_btree_100_ii__0_1__11::Type* rel_b_96694f8e93f5c77d;
t_btree_100_ii__1_0__11__01::Type* rel_c_981811ba2479fc8d;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_a_f5cc2531020019db::Stratum_a_f5cc2531020019db(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_ii__0_1__11__10::Type& rel_a_454ddee488c1ed6b,t_btree_100_ii__0_1__11::Type& rel_b_96694f8e93f5c77d,t_btree_100_ii__1_0__11__01::Type& rel_c_981811ba2479fc8d):
symTable(symTable),
recordTable(recordTable),
regexCache(regexCache),
pruneImdtRels(pruneImdtRels),
performIO(performIO),
signalHandler(signalHandler),
iter(iter),
ctr(ctr),
inputDirectory(inputDirectory),
outputDirectory(outputDirectory),
rel_a_454ddee488c1ed6b(&rel_a_454ddee488c1ed6b),
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d),
rel_c_981811ba2479fc8d(&rel_c_981811ba2479fc8d){
}

void Stratum_a_f5cc2531020019db::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(a(X,Y) :- 
   b(X,Z),
   c(Y,Z).
in file  [1:1-1:1])_");
if(!(rel_b_96694f8e93f5c77d->empty()) && !(rel_c_981811ba2479fc8d->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
for(const auto& env0 : *rel_b_96694f8e93f5c77d) {
auto range = rel_c_981811ba2479fc8d->lowerUpperRange_01(Tuple<RamDomain,2>{{ramBitCast<RamDomain>(MIN_RAM_SIGNED), ramBitCast(env0[1])}},Tuple<RamDomain,2>{{ramBitCast<RamDomain>(MAX_RAM_SIGNED), ramBitCast(env0[1])}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env1[0])}};
rel_a_454ddee488c1ed6b->insert(tuple,READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("a",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
varValues.emplace_back(ramBitCast(env0[1]));
varValues.emplace_back(ramBitCast(env1[0]));
RuleApplication ruleApplication{1, varValues};
ruleSet->insert(ruleApplication);
}
}
}
();}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_b_1bd76238ec74a612 {
public:
 Stratum_b_1bd76238ec74a612(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_ii__0_1__11::Type& rel_b_96694f8e93f5c77d);
void run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret);
private:
SymbolTable& symTable;
RecordTable& recordTable;
ConcurrentCache<std::string,std::regex>& regexCache;
bool& pruneImdtRels;
bool& performIO;
SignalHandler*& signalHandler;
std::atomic<std::size_t>& iter;
std::atomic<RamDomain>& ctr;
std::string& inputDirectory;
std::string& outputDirectory;
t_btree_100_ii__0_1__11::Type* rel_b_96694f8e93f5c77d;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_b_1bd76238ec74a612::Stratum_b_1bd76238ec74a612(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_ii__0_1__11::Type& rel_b_96694f8e93f5c77d):
symTable(symTable),
recordTable(recordTable),
regexCache(regexCache),
pruneImdtRels(pruneImdtRels),
performIO(performIO),
signalHandler(signalHandler),
iter(iter),
ctr(ctr),
inputDirectory(inputDirectory),
outputDirectory(outputDirectory),
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d){
}

void Stratum_b_1bd76238ec74a612::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X\tZ"},{"auxArity","0"},{"cache","true"},{"fact-dir","./input"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","b"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"X\", \"Z\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
if (!inputDirectory.empty()) {directiveMap["fact-dir"] = inputDirectory;}
{
FunctionTimer timer("reading relation rel_b_96694f8e93f5c77d");
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_b_96694f8e93f5c77d);
for (auto& tuple: *rel_b_96694f8e93f5c77d) {
auto untypedTuple = UntypedTuple::fromTypedTuple("b",tuple);
inputFactSet.insert(untypedTuple);
initialInputRelations["b"].insert(untypedTuple);
}
}
} catch (std::exception& e) {std::cerr << "Error loading b data: " << e.what() << '\n';
exit(1);
}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_c_9d76b7130e3957ed {
public:
 Stratum_c_9d76b7130e3957ed(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_ii__1_0__11__01::Type& rel_c_981811ba2479fc8d);
void run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret);
private:
SymbolTable& symTable;
RecordTable& recordTable;
ConcurrentCache<std::string,std::regex>& regexCache;
bool& pruneImdtRels;
bool& performIO;
SignalHandler*& signalHandler;
std::atomic<std::size_t>& iter;
std::atomic<RamDomain>& ctr;
std::string& inputDirectory;
std::string& outputDirectory;
t_btree_100_ii__1_0__11__01::Type* rel_c_981811ba2479fc8d;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_c_9d76b7130e3957ed::Stratum_c_9d76b7130e3957ed(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_ii__1_0__11__01::Type& rel_c_981811ba2479fc8d):
symTable(symTable),
recordTable(recordTable),
regexCache(regexCache),
pruneImdtRels(pruneImdtRels),
performIO(performIO),
signalHandler(signalHandler),
iter(iter),
ctr(ctr),
inputDirectory(inputDirectory),
outputDirectory(outputDirectory),
rel_c_981811ba2479fc8d(&rel_c_981811ba2479fc8d){
}

void Stratum_c_9d76b7130e3957ed::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","Y\tZ"},{"auxArity","0"},{"cache","true"},{"fact-dir","./input"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","c"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"Y\", \"Z\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
if (!inputDirectory.empty()) {directiveMap["fact-dir"] = inputDirectory;}
{
FunctionTimer timer("reading relation rel_c_981811ba2479fc8d");
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_c_981811ba2479fc8d);
for (auto& tuple: *rel_c_981811ba2479fc8d) {
auto untypedTuple = UntypedTuple::fromTypedTuple("c",tuple);
inputFactSet.insert(untypedTuple);
initialInputRelations["c"].insert(untypedTuple);
}
}
} catch (std::exception& e) {std::cerr << "Error loading c data: " << e.what() << '\n';
exit(1);
}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_res_3d4a75d2a12655c0 {
public:
 Stratum_res_3d4a75d2a12655c0(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_ii__0_1__11__10::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_res_f46f91340698127b);
void run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret);
private:
SymbolTable& symTable;
RecordTable& recordTable;
ConcurrentCache<std::string,std::regex>& regexCache;
bool& pruneImdtRels;
bool& performIO;
SignalHandler*& signalHandler;
std::atomic<std::size_t>& iter;
std::atomic<RamDomain>& ctr;
std::string& inputDirectory;
std::string& outputDirectory;
t_btree_100_ii__0_1__11__10::Type* rel_a_454ddee488c1ed6b;
t_btree_100_i__0__1::Type* rel_res_f46f91340698127b;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_res_3d4a75d2a12655c0::Stratum_res_3d4a75d2a12655c0(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_ii__0_1__11__10::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_res_f46f91340698127b):
symTable(symTable),
recordTable(recordTable),
regexCache(regexCache),
pruneImdtRels(pruneImdtRels),
performIO(performIO),
signalHandler(signalHandler),
iter(iter),
ctr(ctr),
inputDirectory(inputDirectory),
outputDirectory(outputDirectory),
rel_a_454ddee488c1ed6b(&rel_a_454ddee488c1ed6b),
rel_res_f46f91340698127b(&rel_res_f46f91340698127b){
}

void Stratum_res_3d4a75d2a12655c0::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(res(X) :- 
   a(X,Y),
   X = 1.
in file  [1:1-1:1])_");
if(!(rel_a_454ddee488c1ed6b->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
CREATE_OP_CONTEXT(rel_res_f46f91340698127b_op_ctxt,rel_res_f46f91340698127b->createContext());
auto range = rel_a_454ddee488c1ed6b->lowerUpperRange_10(Tuple<RamDomain,2>{{ramBitCast(RamSigned(1)), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,2>{{ramBitCast(RamSigned(1)), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt));
for(const auto& env0 : range) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_res_f46f91340698127b->insert(tuple,READ_OP_CONTEXT(rel_res_f46f91340698127b_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("res",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
varValues.emplace_back(ramBitCast(env0[1]));
RuleApplication ruleApplication{2, varValues};
ruleSet->insert(ruleApplication);
}
}
();}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Sf_computeq: public SouffleProgram {
public:
 Sf_computeq();
 ~Sf_computeq();
void run();
void runInc();
void runAll(std::string inputDirectoryArg = "",std::string outputDirectoryArg = "",bool performIOArg = true,bool pruneImdtRelsArg = false);
void runAllInc(std::string inputDirectoryArg = "",std::string outputDirectoryArg = "",bool performIOArg = true,bool pruneImdtRelsArg = false);
void printAll([[maybe_unused]] std::string outputDirectoryArg = "");
void loadAll([[maybe_unused]] std::string inputDirectoryArg = "");
void loadAllExcept([[maybe_unused]] std::string inputDirectoryArg = "");
void dumpInputs();
void dumpOutputs();
SymbolTable& getSymbolTable();
RecordTable& getRecordTable();
void setNumThreads(std::size_t numThreadsValue);
void executeSubroutine(std::string name,const std::vector<RamDomain>& args,std::vector<RamDomain>& ret);
private:
void runFunction(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg);
void runFunctionInc(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg);
SymbolTableImpl symTable;
SpecializedRecordTable<0> recordTable;
ConcurrentCache<std::string,std::regex> regexCache;
Own<t_btree_100_ii__0_1__11::Type> rel_b_96694f8e93f5c77d;
souffle::RelationWrapper<t_btree_100_ii__0_1__11::Type> wrapper_rel_b_96694f8e93f5c77d;
Own<t_btree_000_ii__0_1__11::Type> rel_old_b_38eb3a9d03faff34;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_derv_insert_b_420980c115f01b29;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_derv_insert_b_420980c115f01b29;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_derv_delete_b_ded826b2e75f35bb;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_derv_delete_b_ded826b2e75f35bb;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_tuple_delete_b_1b345b26cae73383;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_tuple_delete_b_1b345b26cae73383;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp_b_d86ac13254fca704;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp2_b_b058b32c7cd1b966;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp3_b_2e0d1bd98a1a5c51;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp4_b_d5b6cdda8ba346c1;
Own<t_btree_100_ii__0_1__11::Type> rel_inc_tuple_overdelete_b_f9480aa9bb17885d;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_derv_overdelete_b_2df625d9f0a591a4;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_new_derv_rederive_b_85af71cbeb32637e;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_tuple_rederive_b_29aee4d2a2657817;
Own<t_btree_000_ii__0_1__11::Type> rel_delta_tuple_delete_b_43dfed04dd4422a4;
Own<t_btree_000_ii__0_1__11::Type> rel_delta_tuple_insert_b_a88502da93017be6;
Own<t_btree_000_ii__0_1__11::Type> rel_new_derv_delete_b_4199ee6bbba1324f;
Own<t_btree_000_ii__0_1__11::Type> rel_new_derv_insert_b_8572f4c197f8bd21;
Own<t_btree_100_ii__1_0__11__01::Type> rel_c_981811ba2479fc8d;
souffle::RelationWrapper<t_btree_100_ii__1_0__11__01::Type> wrapper_rel_c_981811ba2479fc8d;
Own<t_btree_000_ii__0_1__11::Type> rel_old_c_3e6edc8484191be0;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_derv_insert_c_0d24c4484987ca3f;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_derv_insert_c_0d24c4484987ca3f;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_derv_delete_c_2409cf566a420e77;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_derv_delete_c_2409cf566a420e77;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_tuple_insert_c_70adcfa1afd70315;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_tuple_insert_c_70adcfa1afd70315;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_tuple_delete_c_43fa1164ffebfc11;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_tuple_delete_c_43fa1164ffebfc11;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp_c_b253e025804a1387;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp2_c_a683fa7e39954476;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp3_c_0914a1995ce884bc;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp4_c_7881dda484a7d59d;
Own<t_btree_100_ii__0_1__11::Type> rel_inc_tuple_overdelete_c_1ebbcf7f486522d8;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_derv_overdelete_c_b4a21678aced1fb3;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_new_derv_rederive_c_c3f4d0e8ef218854;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263;
Own<t_btree_000_ii__0_1__11::Type> rel_delta_tuple_delete_c_99c900b677e7ae7a;
Own<t_btree_000_ii__0_1__11::Type> rel_delta_tuple_insert_c_02751d71f015bc27;
Own<t_btree_000_ii__0_1__11::Type> rel_new_derv_delete_c_eb3a420fe21b36f5;
Own<t_btree_000_ii__0_1__11::Type> rel_new_derv_insert_c_92944d39136b7981;
Own<t_btree_100_ii__0_1__11__10::Type> rel_a_454ddee488c1ed6b;
souffle::RelationWrapper<t_btree_100_ii__0_1__11__10::Type> wrapper_rel_a_454ddee488c1ed6b;
Own<t_btree_000_ii__0_1__11::Type> rel_old_a_bd7865de58a1cd60;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_derv_insert_a_bbea135139bb89ae;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_derv_insert_a_bbea135139bb89ae;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_derv_delete_a_394b3623655cbdd0;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_derv_delete_a_394b3623655cbdd0;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_tuple_insert_a_88895cba4fbff038;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_tuple_insert_a_88895cba4fbff038;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_tuple_delete_a_58354ad400e6cd67;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_tuple_delete_a_58354ad400e6cd67;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp_a_619a41cbadd142a6;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp2_a_aedcf448adead01c;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp3_a_2c915cf5bb34ed3b;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp4_a_55c6aba43a5b815f;
Own<t_btree_100_ii__0_1__11::Type> rel_inc_tuple_overdelete_a_3943091945425515;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_derv_overdelete_a_45448aaf3278243c;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_new_derv_rederive_a_cf31a1985b633fe9;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7;
Own<t_btree_000_ii__0_1__11::Type> rel_delta_tuple_delete_a_249185740a160373;
Own<t_btree_000_ii__0_1__11::Type> rel_delta_tuple_insert_a_419a5d6ce8e56c01;
Own<t_btree_000_ii__0_1__11::Type> rel_new_derv_delete_a_f6265c4636f36293;
Own<t_btree_000_ii__0_1__11::Type> rel_new_derv_insert_a_cb7a1502e8af1907;
Own<t_btree_100_i__0__1::Type> rel_res_f46f91340698127b;
souffle::RelationWrapper<t_btree_100_i__0__1::Type> wrapper_rel_res_f46f91340698127b;
Own<t_btree_000_i__0__1::Type> rel_old_res_e69d7e45b3927b85;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_derv_insert_res_f04df169be2474c0;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_derv_insert_res_f04df169be2474c0;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_derv_delete_res_21fda861d986a27e;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_derv_delete_res_21fda861d986a27e;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_delete_res_cb088c722dc19c6b;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_tuple_delete_res_cb088c722dc19c6b;
Own<t_btree_000_i__0__1::Type> rel_tmp_res_e458f07e74639534;
Own<t_btree_000_i__0__1::Type> rel_tmp2_res_7a283ba7099f0a30;
Own<t_btree_000_i__0__1::Type> rel_tmp3_res_831769078ba4e773;
Own<t_btree_000_i__0__1::Type> rel_tmp4_res_24f88b22ff01c4a8;
Own<t_btree_100_i__0__1::Type> rel_inc_tuple_overdelete_res_07e98c324c9a10e0;
Own<t_btree_000_i__0__1::Type> rel_inc_derv_overdelete_res_0a6ec95635ed04b1;
Own<t_btree_000_i__0__1::Type> rel_inc_new_derv_rederive_res_9ebe504a89aba57f;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_rederive_res_e162dd1a879e05d3;
Own<t_btree_000_i__0__1::Type> rel_delta_tuple_delete_res_420bff0bb973a2ff;
Own<t_btree_000_i__0__1::Type> rel_delta_tuple_insert_res_737c4fc759657f34;
Own<t_btree_000_i__0__1::Type> rel_new_derv_delete_res_dfe2dd10cea29f96;
Own<t_btree_000_i__0__1::Type> rel_new_derv_insert_res_9b29542d654eb177;
Stratum_a_f5cc2531020019db stratum_a_f1f7b26a4b108ce3;
Stratum_b_1bd76238ec74a612 stratum_b_43b73774b68513f6;
Stratum_c_9d76b7130e3957ed stratum_c_b5ed19a85a4e3095;
Stratum_res_3d4a75d2a12655c0 stratum_res_889015e26521459c;
SignalHandler* signalHandler{SignalHandler::instance()};
std::atomic<RamDomain> ctr{};
std::atomic<std::size_t> iter{};
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Sf_computeq::Sf_computeq():
symTable(),
recordTable(),
regexCache(),
rel_b_96694f8e93f5c77d(mk<t_btree_100_ii__0_1__11::Type>()),
wrapper_rel_b_96694f8e93f5c77d(0, *rel_b_96694f8e93f5c77d, *this, "b", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Z"}}, 0),
rel_old_b_38eb3a9d03faff34(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_delta_derv_insert_b_420980c115f01b29(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_derv_insert_b_420980c115f01b29(1, *rel_inc_delta_derv_insert_b_420980c115f01b29, *this, "$inc_delta_derv_insert_b", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Z"}}, 0),
rel_inc_delta_derv_delete_b_ded826b2e75f35bb(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_derv_delete_b_ded826b2e75f35bb(2, *rel_inc_delta_derv_delete_b_ded826b2e75f35bb, *this, "$inc_delta_derv_delete_b", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Z"}}, 0),
rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46(3, *rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46, *this, "$inc_delta_tuple_insert_b", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Z"}}, 0),
rel_inc_delta_tuple_delete_b_1b345b26cae73383(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_tuple_delete_b_1b345b26cae73383(4, *rel_inc_delta_tuple_delete_b_1b345b26cae73383, *this, "$inc_delta_tuple_delete_b", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Z"}}, 0),
rel_tmp_b_d86ac13254fca704(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp2_b_b058b32c7cd1b966(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp3_b_2e0d1bd98a1a5c51(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp4_b_d5b6cdda8ba346c1(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_tuple_overdelete_b_f9480aa9bb17885d(mk<t_btree_100_ii__0_1__11::Type>()),
rel_inc_derv_overdelete_b_2df625d9f0a591a4(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_new_derv_rederive_b_85af71cbeb32637e(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_delta_tuple_rederive_b_29aee4d2a2657817(mk<t_btree_000_ii__0_1__11::Type>()),
rel_delta_tuple_delete_b_43dfed04dd4422a4(mk<t_btree_000_ii__0_1__11::Type>()),
rel_delta_tuple_insert_b_a88502da93017be6(mk<t_btree_000_ii__0_1__11::Type>()),
rel_new_derv_delete_b_4199ee6bbba1324f(mk<t_btree_000_ii__0_1__11::Type>()),
rel_new_derv_insert_b_8572f4c197f8bd21(mk<t_btree_000_ii__0_1__11::Type>()),
rel_c_981811ba2479fc8d(mk<t_btree_100_ii__1_0__11__01::Type>()),
wrapper_rel_c_981811ba2479fc8d(5, *rel_c_981811ba2479fc8d, *this, "c", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"Y","Z"}}, 0),
rel_old_c_3e6edc8484191be0(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_delta_derv_insert_c_0d24c4484987ca3f(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_derv_insert_c_0d24c4484987ca3f(6, *rel_inc_delta_derv_insert_c_0d24c4484987ca3f, *this, "$inc_delta_derv_insert_c", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"Y","Z"}}, 0),
rel_inc_delta_derv_delete_c_2409cf566a420e77(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_derv_delete_c_2409cf566a420e77(7, *rel_inc_delta_derv_delete_c_2409cf566a420e77, *this, "$inc_delta_derv_delete_c", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"Y","Z"}}, 0),
rel_inc_delta_tuple_insert_c_70adcfa1afd70315(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_tuple_insert_c_70adcfa1afd70315(8, *rel_inc_delta_tuple_insert_c_70adcfa1afd70315, *this, "$inc_delta_tuple_insert_c", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"Y","Z"}}, 0),
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_tuple_delete_c_43fa1164ffebfc11(9, *rel_inc_delta_tuple_delete_c_43fa1164ffebfc11, *this, "$inc_delta_tuple_delete_c", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"Y","Z"}}, 0),
rel_tmp_c_b253e025804a1387(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp2_c_a683fa7e39954476(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp3_c_0914a1995ce884bc(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp4_c_7881dda484a7d59d(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_tuple_overdelete_c_1ebbcf7f486522d8(mk<t_btree_100_ii__0_1__11::Type>()),
rel_inc_derv_overdelete_c_b4a21678aced1fb3(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_new_derv_rederive_c_c3f4d0e8ef218854(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263(mk<t_btree_000_ii__0_1__11::Type>()),
rel_delta_tuple_delete_c_99c900b677e7ae7a(mk<t_btree_000_ii__0_1__11::Type>()),
rel_delta_tuple_insert_c_02751d71f015bc27(mk<t_btree_000_ii__0_1__11::Type>()),
rel_new_derv_delete_c_eb3a420fe21b36f5(mk<t_btree_000_ii__0_1__11::Type>()),
rel_new_derv_insert_c_92944d39136b7981(mk<t_btree_000_ii__0_1__11::Type>()),
rel_a_454ddee488c1ed6b(mk<t_btree_100_ii__0_1__11__10::Type>()),
wrapper_rel_a_454ddee488c1ed6b(10, *rel_a_454ddee488c1ed6b, *this, "a", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Y"}}, 0),
rel_old_a_bd7865de58a1cd60(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_delta_derv_insert_a_bbea135139bb89ae(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_derv_insert_a_bbea135139bb89ae(11, *rel_inc_delta_derv_insert_a_bbea135139bb89ae, *this, "$inc_delta_derv_insert_a", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Y"}}, 0),
rel_inc_delta_derv_delete_a_394b3623655cbdd0(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_derv_delete_a_394b3623655cbdd0(12, *rel_inc_delta_derv_delete_a_394b3623655cbdd0, *this, "$inc_delta_derv_delete_a", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Y"}}, 0),
rel_inc_delta_tuple_insert_a_88895cba4fbff038(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_tuple_insert_a_88895cba4fbff038(13, *rel_inc_delta_tuple_insert_a_88895cba4fbff038, *this, "$inc_delta_tuple_insert_a", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Y"}}, 0),
rel_inc_delta_tuple_delete_a_58354ad400e6cd67(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_tuple_delete_a_58354ad400e6cd67(14, *rel_inc_delta_tuple_delete_a_58354ad400e6cd67, *this, "$inc_delta_tuple_delete_a", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Y"}}, 0),
rel_tmp_a_619a41cbadd142a6(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp2_a_aedcf448adead01c(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp3_a_2c915cf5bb34ed3b(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp4_a_55c6aba43a5b815f(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_tuple_overdelete_a_3943091945425515(mk<t_btree_100_ii__0_1__11::Type>()),
rel_inc_derv_overdelete_a_45448aaf3278243c(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_new_derv_rederive_a_cf31a1985b633fe9(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7(mk<t_btree_000_ii__0_1__11::Type>()),
rel_delta_tuple_delete_a_249185740a160373(mk<t_btree_000_ii__0_1__11::Type>()),
rel_delta_tuple_insert_a_419a5d6ce8e56c01(mk<t_btree_000_ii__0_1__11::Type>()),
rel_new_derv_delete_a_f6265c4636f36293(mk<t_btree_000_ii__0_1__11::Type>()),
rel_new_derv_insert_a_cb7a1502e8af1907(mk<t_btree_000_ii__0_1__11::Type>()),
rel_res_f46f91340698127b(mk<t_btree_100_i__0__1::Type>()),
wrapper_rel_res_f46f91340698127b(15, *rel_res_f46f91340698127b, *this, "res", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_old_res_e69d7e45b3927b85(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_derv_insert_res_f04df169be2474c0(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_insert_res_f04df169be2474c0(16, *rel_inc_delta_derv_insert_res_f04df169be2474c0, *this, "$inc_delta_derv_insert_res", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_derv_delete_res_21fda861d986a27e(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_delete_res_21fda861d986a27e(17, *rel_inc_delta_derv_delete_res_21fda861d986a27e, *this, "$inc_delta_derv_delete_res", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b(18, *rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b, *this, "$inc_delta_tuple_insert_res", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_tuple_delete_res_cb088c722dc19c6b(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_delete_res_cb088c722dc19c6b(19, *rel_inc_delta_tuple_delete_res_cb088c722dc19c6b, *this, "$inc_delta_tuple_delete_res", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_tmp_res_e458f07e74639534(mk<t_btree_000_i__0__1::Type>()),
rel_tmp2_res_7a283ba7099f0a30(mk<t_btree_000_i__0__1::Type>()),
rel_tmp3_res_831769078ba4e773(mk<t_btree_000_i__0__1::Type>()),
rel_tmp4_res_24f88b22ff01c4a8(mk<t_btree_000_i__0__1::Type>()),
rel_inc_tuple_overdelete_res_07e98c324c9a10e0(mk<t_btree_100_i__0__1::Type>()),
rel_inc_derv_overdelete_res_0a6ec95635ed04b1(mk<t_btree_000_i__0__1::Type>()),
rel_inc_new_derv_rederive_res_9ebe504a89aba57f(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_tuple_rederive_res_e162dd1a879e05d3(mk<t_btree_000_i__0__1::Type>()),
rel_delta_tuple_delete_res_420bff0bb973a2ff(mk<t_btree_000_i__0__1::Type>()),
rel_delta_tuple_insert_res_737c4fc759657f34(mk<t_btree_000_i__0__1::Type>()),
rel_new_derv_delete_res_dfe2dd10cea29f96(mk<t_btree_000_i__0__1::Type>()),
rel_new_derv_insert_res_9b29542d654eb177(mk<t_btree_000_i__0__1::Type>()),
stratum_a_f1f7b26a4b108ce3(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_a_454ddee488c1ed6b,*rel_b_96694f8e93f5c77d,*rel_c_981811ba2479fc8d),
stratum_b_43b73774b68513f6(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_b_96694f8e93f5c77d),
stratum_c_b5ed19a85a4e3095(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_c_981811ba2479fc8d),
stratum_res_889015e26521459c(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_a_454ddee488c1ed6b,*rel_res_f46f91340698127b){
addRelation("b", wrapper_rel_b_96694f8e93f5c77d, true, false);
addRelation("$inc_delta_derv_insert_b", wrapper_rel_inc_delta_derv_insert_b_420980c115f01b29, false, false);
addRelation("$inc_delta_derv_delete_b", wrapper_rel_inc_delta_derv_delete_b_ded826b2e75f35bb, false, false);
addRelation("$inc_delta_tuple_insert_b", wrapper_rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46, false, false);
addRelation("$inc_delta_tuple_delete_b", wrapper_rel_inc_delta_tuple_delete_b_1b345b26cae73383, false, false);
addRelation("c", wrapper_rel_c_981811ba2479fc8d, true, false);
addRelation("$inc_delta_derv_insert_c", wrapper_rel_inc_delta_derv_insert_c_0d24c4484987ca3f, false, false);
addRelation("$inc_delta_derv_delete_c", wrapper_rel_inc_delta_derv_delete_c_2409cf566a420e77, false, false);
addRelation("$inc_delta_tuple_insert_c", wrapper_rel_inc_delta_tuple_insert_c_70adcfa1afd70315, false, false);
addRelation("$inc_delta_tuple_delete_c", wrapper_rel_inc_delta_tuple_delete_c_43fa1164ffebfc11, false, false);
addRelation("a", wrapper_rel_a_454ddee488c1ed6b, false, false);
addRelation("$inc_delta_derv_insert_a", wrapper_rel_inc_delta_derv_insert_a_bbea135139bb89ae, false, false);
addRelation("$inc_delta_derv_delete_a", wrapper_rel_inc_delta_derv_delete_a_394b3623655cbdd0, false, false);
addRelation("$inc_delta_tuple_insert_a", wrapper_rel_inc_delta_tuple_insert_a_88895cba4fbff038, false, false);
addRelation("$inc_delta_tuple_delete_a", wrapper_rel_inc_delta_tuple_delete_a_58354ad400e6cd67, false, false);
addRelation("res", wrapper_rel_res_f46f91340698127b, false, false);
addRelation("$inc_delta_derv_insert_res", wrapper_rel_inc_delta_derv_insert_res_f04df169be2474c0, false, false);
addRelation("$inc_delta_derv_delete_res", wrapper_rel_inc_delta_derv_delete_res_21fda861d986a27e, false, false);
addRelation("$inc_delta_tuple_insert_res", wrapper_rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b, false, false);
addRelation("$inc_delta_tuple_delete_res", wrapper_rel_inc_delta_tuple_delete_res_cb088c722dc19c6b, false, false);
}

 Sf_computeq::~Sf_computeq(){
}

void Sf_computeq::runFunction(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){

    this->inputDirectory  = std::move(inputDirectoryArg);
    this->outputDirectory = std::move(outputDirectoryArg);
    this->performIO       = performIOArg;
    this->pruneImdtRels   = pruneImdtRelsArg;

    // set default threads (in embedded mode)
    // if this is not set, and omp is used, the default omp setting of number of cores is used.
#if defined(_OPENMP)
    if (0 < getNumThreads()) { omp_set_num_threads(static_cast<int>(getNumThreads())); }
#endif

    signalHandler->set();
// -- query evaluation --
{
FunctionTimer timer("stratum_b");
 std::vector<RamDomain> args, ret;
stratum_b_43b73774b68513f6.run(args, ret);
}
{
FunctionTimer timer("stratum_c");
 std::vector<RamDomain> args, ret;
stratum_c_b5ed19a85a4e3095.run(args, ret);
}
{
FunctionTimer timer("stratum_a");
 std::vector<RamDomain> args, ret;
stratum_a_f1f7b26a4b108ce3.run(args, ret);
}
{
FunctionTimer timer("stratum_res");
 std::vector<RamDomain> args, ret;
stratum_res_889015e26521459c.run(args, ret);
}

// -- relation hint statistics --
signalHandler->reset();
}

void Sf_computeq::runFunctionInc(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){

    this->inputDirectory  = std::move(inputDirectoryArg);
    this->outputDirectory = std::move(outputDirectoryArg);
    this->performIO       = performIOArg;
    this->pruneImdtRels   = pruneImdtRelsArg;

    // set default threads (in embedded mode)
    // if this is not set, and omp is used, the default omp setting of number of cores is used.
#if defined(_OPENMP)
    if (0 < getNumThreads()) { omp_set_num_threads(static_cast<int>(getNumThreads())); }
#endif

    signalHandler->set();
// -- query evaluation --

// -- relation hint statistics --
signalHandler->reset();
}

void Sf_computeq::run(){
runFunction("", "", false, false);
}

void Sf_computeq::runInc(){
runFunctionInc("", "", false, false);
}

void Sf_computeq::runAll(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){
runFunction(inputDirectoryArg, outputDirectoryArg, performIOArg, pruneImdtRelsArg);
}

void Sf_computeq::runAllInc(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){
runFunctionInc(inputDirectoryArg, outputDirectoryArg, performIOArg, pruneImdtRelsArg);
}

void Sf_computeq::printAll([[maybe_unused]] std::string outputDirectoryArg){
}

void Sf_computeq::loadAll([[maybe_unused]] std::string inputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X\tZ"},{"auxArity","0"},{"cache","true"},{"fact-dir","./input"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","b"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"X\", \"Z\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_b_96694f8e93f5c77d);
} catch (std::exception& e) {std::cerr << "Error loading b data: " << e.what() << '\n';
exit(1);
}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","Y\tZ"},{"auxArity","0"},{"cache","true"},{"fact-dir","./input"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","c"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"Y\", \"Z\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_c_981811ba2479fc8d);
} catch (std::exception& e) {std::cerr << "Error loading c data: " << e.what() << '\n';
exit(1);
}
}

void Sf_computeq::loadAllExcept([[maybe_unused]] std::string inputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X\tZ"},{"auxArity","0"},{"cache","true"},{"fact-dir","./input"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","b"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"X\", \"Z\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAllExcept(*rel_b_96694f8e93f5c77d, *rel_inc_delta_tuple_delete_b_1b345b26cae73383);
} catch (std::exception& e) {std::cerr << "Error loading with filterb data: " << e.what() << '\n';
exit(1);
}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","Y\tZ"},{"auxArity","0"},{"cache","true"},{"fact-dir","./input"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","c"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"Y\", \"Z\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAllExcept(*rel_c_981811ba2479fc8d, *rel_inc_delta_tuple_delete_c_43fa1164ffebfc11);
} catch (std::exception& e) {std::cerr << "Error loading with filterc data: " << e.what() << '\n';
exit(1);
}
}

void Sf_computeq::dumpInputs(){
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "b";
rwOperation["types"] = "{\"relation\": {\"arity\": 2, \"auxArity\": 0, \"types\": [\"i:number\", \"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_b_96694f8e93f5c77d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "c";
rwOperation["types"] = "{\"relation\": {\"arity\": 2, \"auxArity\": 0, \"types\": [\"i:number\", \"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_c_981811ba2479fc8d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

void Sf_computeq::dumpOutputs(){
}

SymbolTable& Sf_computeq::getSymbolTable(){
return symTable;
}

RecordTable& Sf_computeq::getRecordTable(){
return recordTable;
}

void Sf_computeq::setNumThreads(std::size_t numThreadsValue){
SouffleProgram::setNumThreads(numThreadsValue);
symTable.setNumLanes(getNumThreads());
recordTable.setNumLanes(getNumThreads());
regexCache.setNumLanes(getNumThreads());
}

void Sf_computeq::executeSubroutine(std::string name,const std::vector<RamDomain>& args,std::vector<RamDomain>& ret){
if (name == "a") {
stratum_a_f1f7b26a4b108ce3.run(args, ret);
return;}
if (name == "b") {
stratum_b_43b73774b68513f6.run(args, ret);
return;}
if (name == "c") {
stratum_c_b5ed19a85a4e3095.run(args, ret);
return;}
if (name == "res") {
stratum_res_889015e26521459c.run(args, ret);
return;}
fatal(("unknown subroutine " + name).c_str());
}

} // namespace  souffle
std::vector<std::pair<UntypedTuple,bool>> evidences;
namespace souffle {
SouffleProgram *newInstance_computeq(){return new  souffle::Sf_computeq;}
SymbolTable *getST_computeq(SouffleProgram *p){return &reinterpret_cast<souffle::Sf_computeq*>(p)->getSymbolTable();}
} // namespace souffle

#ifndef __EMBEDDED_SOUFFLE__
#include "souffle/CompiledOptions.h"
#include "souffle/problog/DerivationGraph.h"
int main(int argc, char** argv)
{
try{
souffle::CmdOptions opt(R"(test.dl)",
R"(./input)",
R"(./output)",
false,
R"()",
1, "log.txt", false,"inc-naive",false,false);
if (!opt.parse(argc,argv)) return 1;
DerivationGraph::setMergeBiImpEnabled(opt.isMergeBiImpEnabled());
souffle::Sf_computeq obj;
if (opt.getKnowledgeRepresentation() == "bdd") {
obj.setKnowledge(souffle::Knowledge::BDD);
} else if (opt.getKnowledgeRepresentation() == "sdd") {
obj.setKnowledge(souffle::Knowledge::SDD);
} else { std::cout << "opt.getKnowledgeRepresentation()" << opt.getKnowledgeRepresentation() << std::endl;
 assert(false && "unknown knowledge representation"); }
#if defined(_OPENMP) 
obj.setNumThreads(opt.getNumJobs());

#endif
debugger.startTurn();
debugger.startStage(StageKind::SEMINAIVE_FULL);
obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());
debugger.endStage();
try {
debugger.startStage(StageKind::IO_LOAD_FULL);
{
FunctionTimer timer("Reading fact probability from " + opt.getInputFileDir());
{
std::string rel = "b";
std::cout << "reading: " << opt.getInputFileDir() << "/" << rel << ".facts and " << opt.getInputFileDir() << "/" << rel << ".prob" << std::endl;
std::ifstream factFile(opt.getInputFileDir() + "/" + rel + ".facts");std::ifstream probFile(opt.getInputFileDir() + "/" + rel + ".prob");
if (!factFile.is_open()) {
    std::cerr << "Missing facts file for relation: " << rel << std::endl;
    assert(false && "facts file not found");
}
bool probExists = probFile.is_open();
if (!probExists) {
    std::cerr << "[Warning] Missing prob file for relation: " << rel << ", defaulting probabilities to 1.0" << std::endl;
}
std::string factLine, probLine;
while (std::getline(factFile, factLine)) {
std::istringstream fs(factLine);std::istringstream ps(probLine);    double prob = 1.0;
    if (probExists && std::getline(probFile, probLine)) {
        std::istringstream ps(probLine);
        if (!(ps >> prob) || prob < 0 || prob > 1) {
            std::cerr << "[Warning] Invalid probability in " << rel << ".prob, defaulting to 1.0" << std::endl;
            prob = 1.0;
        }
    }
souffle::RamDomain field;
std::vector<souffle::RamDomain> fields;
while (fs >> field) {fields.push_back(field);}
UntypedTuple tuple{rel, fields};
fact_prob[tuple] = prob;
}
}
{
std::string rel = "c";
std::cout << "reading: " << opt.getInputFileDir() << "/" << rel << ".facts and " << opt.getInputFileDir() << "/" << rel << ".prob" << std::endl;
std::ifstream factFile(opt.getInputFileDir() + "/" + rel + ".facts");std::ifstream probFile(opt.getInputFileDir() + "/" + rel + ".prob");
if (!factFile.is_open()) {
    std::cerr << "Missing facts file for relation: " << rel << std::endl;
    assert(false && "facts file not found");
}
bool probExists = probFile.is_open();
if (!probExists) {
    std::cerr << "[Warning] Missing prob file for relation: " << rel << ", defaulting probabilities to 1.0" << std::endl;
}
std::string factLine, probLine;
while (std::getline(factFile, factLine)) {
std::istringstream fs(factLine);std::istringstream ps(probLine);    double prob = 1.0;
    if (probExists && std::getline(probFile, probLine)) {
        std::istringstream ps(probLine);
        if (!(ps >> prob) || prob < 0 || prob > 1) {
            std::cerr << "[Warning] Invalid probability in " << rel << ".prob, defaulting to 1.0" << std::endl;
            prob = 1.0;
        }
    }
souffle::RamDomain field;
std::vector<souffle::RamDomain> fields;
while (fs >> field) {fields.push_back(field);}
UntypedTuple tuple{rel, fields};
fact_prob[tuple] = prob;
}
}
}
debugger.endStage();
debugger.startStage(StageKind::CONSTRUCT_RULE_FULL);
const Atom rule1_head = Atom("a", std::vector<SymbolicField>{SymbolicField::makeVariable("X"), SymbolicField::makeVariable("Y")});
const Atom atom_1_1 = Atom{"b", {SymbolicField::makeVariable("X"), SymbolicField::makeVariable("Z"), }};
const Atom atom_1_2 = Atom{"c", {SymbolicField::makeVariable("Y"), SymbolicField::makeVariable("Z"), }};
const Rule rule1 = Rule(1,rule1_head, {atom_1_1, atom_1_2}, {"X", "Z", "Y"}, 1.000000, 0, 0, false);
const Atom rule2_head = Atom("res", std::vector<SymbolicField>{SymbolicField::makeVariable("X")});
const Atom atom_2_1 = Atom{"a", {SymbolicField::makeVariable("X"), SymbolicField::makeVariable("Y"), }};
const Rule rule2 = Rule(2,rule2_head, {atom_2_1}, {"X", "Y"}, 1.000000, 0, 0, false);
ruleManager = RuleManager({rule1, rule2}, {});
const Query query_res_ = Query(Atom("res", {SymbolicField{1}}));
QueryManager queryManager = QueryManager({query_res_});
debugger.endStage();
souffle::problog::runPipeline(opt, obj, ruleManager, queryManager, fact_prob, evidences, false);
Debugger& debugger = Debugger::getInstance();
std::string logBase = basenameFromPath(opt.getLogFileName());
std::string reportFileName = generateFilename(logBase, ".json");
std::string reportFile = souffle::problog::makeOutputPath(opt, reportFileName);
std::ofstream ofs = std::ofstream(reportFile);
debugger.printReportJson(ofs);
std::cout << "[pipeline] debugger log: " << reportFileName << std::endl;
// debugger.printReport(std::cout);
} catch (std::exception& e) {std::cerr << "Problog calc failed" << e.what() << std::endl;}
return 0;
} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}
}
#endif

namespace  souffle {
using namespace souffle;
class factory_Sf_computeq: souffle::ProgramFactory {
public:
souffle::SouffleProgram* newInstance();
 factory_Sf_computeq();
private:
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
souffle::SouffleProgram* factory_Sf_computeq::newInstance(){
return new  souffle::Sf_computeq();
}

 factory_Sf_computeq::factory_Sf_computeq():
souffle::ProgramFactory("computeq"){
}

} // namespace  souffle
namespace souffle {

#ifdef __EMBEDDED_SOUFFLE__
extern "C" {
souffle::factory_Sf_computeq __factory_Sf_computeq_instance;
}
#endif
} // namespace souffle

