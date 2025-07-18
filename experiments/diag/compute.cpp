#define SOUFFLE_GENERATOR_VERSION "e45b7b1c2"
#include "souffle/CompiledSouffle.h"
#include "souffle/Derivation.h"
#include "souffle/SignalHandler.h"
#include "souffle/SouffleInterface.h"
#include "souffle/cli/Cli.h"
#include "souffle/datastructure/BTree.h"
#include "souffle/datastructure/BTreeDelete.h"
#include "souffle/io/IOSystem.h"
#include "souffle/problog/Atom.h"
#include "souffle/problog/ForwardCompilation.h"
#include "souffle/problog/Rule.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/formula/CuddManager.h"
#include "souffle/problog/formula/SddManager.h"
#include "souffle/utility/MiscUtil.h"
#include <any>
namespace functors {
extern "C" {
}
} //namespace functors
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
namespace souffle::t_btree_000_ii__0_1__11__10 {
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
range<t_ind_0::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_000_ii__0_1__11__10 
namespace souffle::t_btree_000_ii__0_1__11__10 {
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
} // namespace souffle::t_btree_000_ii__0_1__11__10 
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
namespace  souffle {
using namespace souffle;
class Stratum_a_f5cc2531020019db {
public:
 Stratum_a_f5cc2531020019db(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b);
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
t_btree_100_i__0__1::Type* rel_a_454ddee488c1ed6b;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_a_f5cc2531020019db::Stratum_a_f5cc2531020019db(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b):
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
rel_a_454ddee488c1ed6b(&rel_a_454ddee488c1ed6b){
}

void Stratum_a_f5cc2531020019db::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"cache","true"},{"fact-dir","./input"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","a"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!inputDirectory.empty()) {directiveMap["fact-dir"] = inputDirectory;}
{
FunctionTimer timer("reading relation rel_a_454ddee488c1ed6b");
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_a_454ddee488c1ed6b);
for (auto& tuple: *rel_a_454ddee488c1ed6b) {
auto untypedTuple = UntypedTuple::fromTypedTuple("a",tuple);
inputFactSet.insert(untypedTuple);
}
dumpInputFacts();
}
} catch (std::exception& e) {std::cerr << "Error loading a data: " << e.what() << '\n';
exit(1);
}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_a_inc_935a746942ed3383 {
public:
 Stratum_a_inc_935a746942ed3383(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_a_58354ad400e6cd67,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_a_88895cba4fbff038,t_btree_000_i__0__1::Type& rel_old_a_bd7865de58a1cd60,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b);
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
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_a_58354ad400e6cd67;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_a_88895cba4fbff038;
t_btree_000_i__0__1::Type* rel_old_a_bd7865de58a1cd60;
t_btree_100_i__0__1::Type* rel_a_454ddee488c1ed6b;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_a_inc_935a746942ed3383::Stratum_a_inc_935a746942ed3383(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_a_58354ad400e6cd67,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_a_88895cba4fbff038,t_btree_000_i__0__1::Type& rel_old_a_bd7865de58a1cd60,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b):
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
rel_inc_delta_tuple_delete_a_58354ad400e6cd67(&rel_inc_delta_tuple_delete_a_58354ad400e6cd67),
rel_inc_delta_tuple_insert_a_88895cba4fbff038(&rel_inc_delta_tuple_insert_a_88895cba4fbff038),
rel_old_a_bd7865de58a1cd60(&rel_old_a_bd7865de58a1cd60),
rel_a_454ddee488c1ed6b(&rel_a_454ddee488c1ed6b){
}

void Stratum_a_inc_935a746942ed3383::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
rel_a_454ddee488c1ed6b->purge();
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt,rel_inc_delta_tuple_delete_a_58354ad400e6cd67->createContext());
CREATE_OP_CONTEXT(rel_old_a_bd7865de58a1cd60_op_ctxt,rel_old_a_bd7865de58a1cd60->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
for(const auto& env0 : *rel_old_a_bd7865de58a1cd60) {
if( !(rel_inc_delta_tuple_delete_a_58354ad400e6cd67->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt)))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_a_454ddee488c1ed6b->insert(tuple,READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt));
}
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_a_88895cba4fbff038_op_ctxt,rel_inc_delta_tuple_insert_a_88895cba4fbff038->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_insert_a_88895cba4fbff038) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_a_454ddee488c1ed6b->insert(tuple,READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt));
}
}
();}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_b_1bd76238ec74a612 {
public:
 Stratum_b_1bd76238ec74a612(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d);
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
t_btree_100_i__0__1::Type* rel_a_454ddee488c1ed6b;
t_btree_100_i__0__1::Type* rel_b_96694f8e93f5c77d;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_b_1bd76238ec74a612::Stratum_b_1bd76238ec74a612(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d):
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
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d){
}

void Stratum_b_1bd76238ec74a612::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(b(X) :- 
   a(X).
in file test.dl [17:6-17:19])_");
if(!(rel_a_454ddee488c1ed6b->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
for(const auto& env0 : *rel_a_454ddee488c1ed6b) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_b_96694f8e93f5c77d->insert(tuple,READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("b",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{1, varValues};
ruleSet->insert(ruleApplication);
}
}
();}
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","b"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
{
FunctionTimer timer("writing relation rel_b_96694f8e93f5c77d");
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_b_96694f8e93f5c77d);
}
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_b_inc_92e6ac324cc92315 {
public:
 Stratum_b_inc_92e6ac324cc92315(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_b_ded826b2e75f35bb,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_b_420980c115f01b29,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_a_58354ad400e6cd67,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_b_1b345b26cae73383,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_a_88895cba4fbff038,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,t_btree_000_i__0__1::Type& rel_old_b_38eb3a9d03faff34,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d);
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
t_btree_000_i__0__1::Type* rel_inc_delta_derv_delete_b_ded826b2e75f35bb;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_insert_b_420980c115f01b29;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_a_58354ad400e6cd67;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_b_1b345b26cae73383;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_a_88895cba4fbff038;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46;
t_btree_000_i__0__1::Type* rel_old_b_38eb3a9d03faff34;
t_btree_100_i__0__1::Type* rel_a_454ddee488c1ed6b;
t_btree_100_i__0__1::Type* rel_b_96694f8e93f5c77d;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_b_inc_92e6ac324cc92315::Stratum_b_inc_92e6ac324cc92315(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_b_ded826b2e75f35bb,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_b_420980c115f01b29,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_a_58354ad400e6cd67,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_b_1b345b26cae73383,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_a_88895cba4fbff038,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,t_btree_000_i__0__1::Type& rel_old_b_38eb3a9d03faff34,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d):
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
rel_inc_delta_derv_delete_b_ded826b2e75f35bb(&rel_inc_delta_derv_delete_b_ded826b2e75f35bb),
rel_inc_delta_derv_insert_b_420980c115f01b29(&rel_inc_delta_derv_insert_b_420980c115f01b29),
rel_inc_delta_tuple_delete_a_58354ad400e6cd67(&rel_inc_delta_tuple_delete_a_58354ad400e6cd67),
rel_inc_delta_tuple_delete_b_1b345b26cae73383(&rel_inc_delta_tuple_delete_b_1b345b26cae73383),
rel_inc_delta_tuple_insert_a_88895cba4fbff038(&rel_inc_delta_tuple_insert_a_88895cba4fbff038),
rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46(&rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46),
rel_old_b_38eb3a9d03faff34(&rel_old_b_38eb3a9d03faff34),
rel_a_454ddee488c1ed6b(&rel_a_454ddee488c1ed6b),
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d){
}

void Stratum_b_inc_92e6ac324cc92315::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(b(X) :- 
   a(X).
in file test.dl [17:6-17:19])_");
if(!(rel_inc_delta_tuple_insert_a_88895cba4fbff038->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_insert_b_420980c115f01b29_op_ctxt,rel_inc_delta_derv_insert_b_420980c115f01b29->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_a_88895cba4fbff038_op_ctxt,rel_inc_delta_tuple_insert_a_88895cba4fbff038->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_insert_a_88895cba4fbff038) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_insert_b_420980c115f01b29->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_insert_b_420980c115f01b29_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("b",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaInsertRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{1, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
();}
if(!(rel_inc_delta_tuple_delete_a_58354ad400e6cd67->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_b_ded826b2e75f35bb_op_ctxt,rel_inc_delta_derv_delete_b_ded826b2e75f35bb->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt,rel_inc_delta_tuple_delete_a_58354ad400e6cd67->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_delete_a_58354ad400e6cd67) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_delete_b_ded826b2e75f35bb->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_delete_b_ded826b2e75f35bb_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("b",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaDeleteRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{1, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
if (ruleManager.isInRecursiveStratum(ruleApplication.ruleId)) {
auto*& ruleSetComplete = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
for (const auto& ruleApp: *ruleSetComplete) {
if(ruleManager.isRecursive(ruleApp.ruleId)) {
ruleSet->insert(ruleApp);
ruleSet2->insert(ruleApp);
}
}
}
}
}
();}
for(const auto& tupleDeltaDervInsert: *rel_inc_delta_derv_insert_b_420980c115f01b29) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("b",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& tupleDeltaDervDelete: *rel_inc_delta_derv_delete_b_ded826b2e75f35bb) {
auto untypedDeltaDervTupleDelete = UntypedTuple::fromTypedTuple("b",tupleDeltaDervDelete);
auto*& untypedDeltaDervTupleDeleteRuleSet = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedDeltaDervTupleDelete];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleDelete];

for(const auto& deletedRuleAppl: *untypedDeltaDervTupleDeleteRuleSet) {
untypedDeltaDervTupleRuleSet->erase(deletedRuleAppl);

}
if (untypedDeltaDervTupleRuleSet->empty()) {
delete untypedDeltaDervTupleRuleSet;

DerivationManager::untypedTuple2RuleApplications.erase(untypedDeltaDervTupleDelete);

if(!isInputFact(untypedDeltaDervTupleDelete))
rel_inc_delta_tuple_delete_b_1b345b26cae73383->insert(tupleDeltaDervDelete);
 
}
}
DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
for(const auto& deletedTuple: *rel_inc_delta_tuple_delete_b_1b345b26cae73383) {

rel_b_96694f8e93f5c77d->erase(deletedTuple);

}

for(const auto& insertedTuple: *rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46) {

rel_b_96694f8e93f5c77d->insert(insertedTuple);

}

if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","b"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
{
FunctionTimer timer("writing relation rel_b_96694f8e93f5c77d");
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_b_96694f8e93f5c77d);
}
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_c_9d76b7130e3957ed {
public:
 Stratum_c_9d76b7130e3957ed(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_ii__0_1__11__10::Type& rel_d_c9260aec44573475);
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
t_btree_100_i__0__1::Type* rel_b_96694f8e93f5c77d;
t_btree_100_i__0__1::Type* rel_c_981811ba2479fc8d;
t_btree_100_ii__0_1__11__10::Type* rel_d_c9260aec44573475;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_c_9d76b7130e3957ed::Stratum_c_9d76b7130e3957ed(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_ii__0_1__11__10::Type& rel_d_c9260aec44573475):
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
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d),
rel_c_981811ba2479fc8d(&rel_c_981811ba2479fc8d),
rel_d_c9260aec44573475(&rel_d_c9260aec44573475){
}

void Stratum_c_9d76b7130e3957ed::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(c(X) :- 
   b(X),
   d(X,Y).
in file test.dl [18:6-18:27])_");
if(!(rel_b_96694f8e93f5c77d->empty()) && !(rel_d_c9260aec44573475->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
CREATE_OP_CONTEXT(rel_d_c9260aec44573475_op_ctxt,rel_d_c9260aec44573475->createContext());
for(const auto& env0 : *rel_b_96694f8e93f5c77d) {
auto range = rel_d_c9260aec44573475->lowerUpperRange_10(Tuple<RamDomain,2>{{ramBitCast(env0[0]), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,2>{{ramBitCast(env0[0]), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_d_c9260aec44573475_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_c_981811ba2479fc8d->insert(tuple,READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("c",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
varValues.emplace_back(ramBitCast(env1[1]));
RuleApplication ruleApplication{2, varValues};
ruleSet->insert(ruleApplication);
}
}
}
();}
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","c"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
{
FunctionTimer timer("writing relation rel_c_981811ba2479fc8d");
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_c_981811ba2479fc8d);
}
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_c_inc_cf2c4d7fafabff14 {
public:
 Stratum_c_inc_cf2c4d7fafabff14(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_c_2409cf566a420e77,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_c_0d24c4484987ca3f,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_b_1b345b26cae73383,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,t_btree_000_ii__0_1__11__10::Type& rel_inc_delta_tuple_delete_d_c806904ae7af9698,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_c_70adcfa1afd70315,t_btree_000_ii__0_1__11__10::Type& rel_inc_delta_tuple_insert_d_67f1c476cedabf8e,t_btree_000_i__0__1::Type& rel_old_c_3e6edc8484191be0,t_btree_000_ii__0_1__11__10::Type& rel_old_d_cb928d3acfdedff8,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_ii__0_1__11__10::Type& rel_d_c9260aec44573475);
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
t_btree_000_i__0__1::Type* rel_inc_delta_derv_delete_c_2409cf566a420e77;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_insert_c_0d24c4484987ca3f;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_b_1b345b26cae73383;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_c_43fa1164ffebfc11;
t_btree_000_ii__0_1__11__10::Type* rel_inc_delta_tuple_delete_d_c806904ae7af9698;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_c_70adcfa1afd70315;
t_btree_000_ii__0_1__11__10::Type* rel_inc_delta_tuple_insert_d_67f1c476cedabf8e;
t_btree_000_i__0__1::Type* rel_old_c_3e6edc8484191be0;
t_btree_000_ii__0_1__11__10::Type* rel_old_d_cb928d3acfdedff8;
t_btree_100_i__0__1::Type* rel_b_96694f8e93f5c77d;
t_btree_100_i__0__1::Type* rel_c_981811ba2479fc8d;
t_btree_100_ii__0_1__11__10::Type* rel_d_c9260aec44573475;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_c_inc_cf2c4d7fafabff14::Stratum_c_inc_cf2c4d7fafabff14(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_c_2409cf566a420e77,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_c_0d24c4484987ca3f,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_b_1b345b26cae73383,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,t_btree_000_ii__0_1__11__10::Type& rel_inc_delta_tuple_delete_d_c806904ae7af9698,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_c_70adcfa1afd70315,t_btree_000_ii__0_1__11__10::Type& rel_inc_delta_tuple_insert_d_67f1c476cedabf8e,t_btree_000_i__0__1::Type& rel_old_c_3e6edc8484191be0,t_btree_000_ii__0_1__11__10::Type& rel_old_d_cb928d3acfdedff8,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_ii__0_1__11__10::Type& rel_d_c9260aec44573475):
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
rel_inc_delta_derv_delete_c_2409cf566a420e77(&rel_inc_delta_derv_delete_c_2409cf566a420e77),
rel_inc_delta_derv_insert_c_0d24c4484987ca3f(&rel_inc_delta_derv_insert_c_0d24c4484987ca3f),
rel_inc_delta_tuple_delete_b_1b345b26cae73383(&rel_inc_delta_tuple_delete_b_1b345b26cae73383),
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11(&rel_inc_delta_tuple_delete_c_43fa1164ffebfc11),
rel_inc_delta_tuple_delete_d_c806904ae7af9698(&rel_inc_delta_tuple_delete_d_c806904ae7af9698),
rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46(&rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46),
rel_inc_delta_tuple_insert_c_70adcfa1afd70315(&rel_inc_delta_tuple_insert_c_70adcfa1afd70315),
rel_inc_delta_tuple_insert_d_67f1c476cedabf8e(&rel_inc_delta_tuple_insert_d_67f1c476cedabf8e),
rel_old_c_3e6edc8484191be0(&rel_old_c_3e6edc8484191be0),
rel_old_d_cb928d3acfdedff8(&rel_old_d_cb928d3acfdedff8),
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d),
rel_c_981811ba2479fc8d(&rel_c_981811ba2479fc8d),
rel_d_c9260aec44573475(&rel_d_c9260aec44573475){
}

void Stratum_c_inc_cf2c4d7fafabff14::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(c(X) :- 
   b(X),
   d(X,Y).
in file test.dl [18:6-18:27])_");
if(!(rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46->empty()) && !(rel_old_d_cb928d3acfdedff8->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_insert_c_0d24c4484987ca3f_op_ctxt,rel_inc_delta_derv_insert_c_0d24c4484987ca3f->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46_op_ctxt,rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46->createContext());
CREATE_OP_CONTEXT(rel_old_d_cb928d3acfdedff8_op_ctxt,rel_old_d_cb928d3acfdedff8->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46) {
auto range = rel_old_d_cb928d3acfdedff8->lowerUpperRange_10(Tuple<RamDomain,2>{{ramBitCast(env0[0]), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,2>{{ramBitCast(env0[0]), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_old_d_cb928d3acfdedff8_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_insert_c_0d24c4484987ca3f->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_insert_c_0d24c4484987ca3f_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("c",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaInsertRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
varValues.emplace_back(ramBitCast(env1[1]));
RuleApplication ruleApplication{2, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
();}
if(!(rel_inc_delta_tuple_delete_b_1b345b26cae73383->empty()) && !(rel_old_d_cb928d3acfdedff8->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_c_2409cf566a420e77_op_ctxt,rel_inc_delta_derv_delete_c_2409cf566a420e77->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt,rel_inc_delta_tuple_delete_b_1b345b26cae73383->createContext());
CREATE_OP_CONTEXT(rel_old_d_cb928d3acfdedff8_op_ctxt,rel_old_d_cb928d3acfdedff8->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_delete_b_1b345b26cae73383) {
auto range = rel_old_d_cb928d3acfdedff8->lowerUpperRange_10(Tuple<RamDomain,2>{{ramBitCast(env0[0]), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,2>{{ramBitCast(env0[0]), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_old_d_cb928d3acfdedff8_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_delete_c_2409cf566a420e77->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_delete_c_2409cf566a420e77_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("c",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaDeleteRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
varValues.emplace_back(ramBitCast(env1[1]));
RuleApplication ruleApplication{2, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
if (ruleManager.isInRecursiveStratum(ruleApplication.ruleId)) {
auto*& ruleSetComplete = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
for (const auto& ruleApp: *ruleSetComplete) {
if(ruleManager.isRecursive(ruleApp.ruleId)) {
ruleSet->insert(ruleApp);
ruleSet2->insert(ruleApp);
}
}
}
}
}
}
();}
if(!(rel_b_96694f8e93f5c77d->empty()) && !(rel_inc_delta_tuple_insert_d_67f1c476cedabf8e->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_insert_c_0d24c4484987ca3f_op_ctxt,rel_inc_delta_derv_insert_c_0d24c4484987ca3f->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_d_67f1c476cedabf8e_op_ctxt,rel_inc_delta_tuple_insert_d_67f1c476cedabf8e->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
for(const auto& env0 : *rel_b_96694f8e93f5c77d) {
auto range = rel_inc_delta_tuple_insert_d_67f1c476cedabf8e->lowerUpperRange_10(Tuple<RamDomain,2>{{ramBitCast(env0[0]), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,2>{{ramBitCast(env0[0]), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_inc_delta_tuple_insert_d_67f1c476cedabf8e_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_insert_c_0d24c4484987ca3f->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_insert_c_0d24c4484987ca3f_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("c",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaInsertRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
varValues.emplace_back(ramBitCast(env1[1]));
RuleApplication ruleApplication{2, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
();}
if(!(rel_b_96694f8e93f5c77d->empty()) && !(rel_inc_delta_tuple_delete_d_c806904ae7af9698->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_c_2409cf566a420e77_op_ctxt,rel_inc_delta_derv_delete_c_2409cf566a420e77->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_d_c806904ae7af9698_op_ctxt,rel_inc_delta_tuple_delete_d_c806904ae7af9698->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
for(const auto& env0 : *rel_b_96694f8e93f5c77d) {
auto range = rel_inc_delta_tuple_delete_d_c806904ae7af9698->lowerUpperRange_10(Tuple<RamDomain,2>{{ramBitCast(env0[0]), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,2>{{ramBitCast(env0[0]), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_inc_delta_tuple_delete_d_c806904ae7af9698_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_delete_c_2409cf566a420e77->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_delete_c_2409cf566a420e77_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("c",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaDeleteRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
varValues.emplace_back(ramBitCast(env1[1]));
RuleApplication ruleApplication{2, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
if (ruleManager.isInRecursiveStratum(ruleApplication.ruleId)) {
auto*& ruleSetComplete = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
for (const auto& ruleApp: *ruleSetComplete) {
if(ruleManager.isRecursive(ruleApp.ruleId)) {
ruleSet->insert(ruleApp);
ruleSet2->insert(ruleApp);
}
}
}
}
}
}
();}
for(const auto& tupleDeltaDervInsert: *rel_inc_delta_derv_insert_c_0d24c4484987ca3f) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("c",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_inc_delta_tuple_insert_c_70adcfa1afd70315->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& tupleDeltaDervDelete: *rel_inc_delta_derv_delete_c_2409cf566a420e77) {
auto untypedDeltaDervTupleDelete = UntypedTuple::fromTypedTuple("c",tupleDeltaDervDelete);
auto*& untypedDeltaDervTupleDeleteRuleSet = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedDeltaDervTupleDelete];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleDelete];

for(const auto& deletedRuleAppl: *untypedDeltaDervTupleDeleteRuleSet) {
untypedDeltaDervTupleRuleSet->erase(deletedRuleAppl);

}
if (untypedDeltaDervTupleRuleSet->empty()) {
delete untypedDeltaDervTupleRuleSet;

DerivationManager::untypedTuple2RuleApplications.erase(untypedDeltaDervTupleDelete);

if(!isInputFact(untypedDeltaDervTupleDelete))
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->insert(tupleDeltaDervDelete);
 
}
}
DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
for(const auto& deletedTuple: *rel_inc_delta_tuple_delete_c_43fa1164ffebfc11) {

rel_c_981811ba2479fc8d->erase(deletedTuple);

}

for(const auto& insertedTuple: *rel_inc_delta_tuple_insert_c_70adcfa1afd70315) {

rel_c_981811ba2479fc8d->insert(insertedTuple);

}

if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","c"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
{
FunctionTimer timer("writing relation rel_c_981811ba2479fc8d");
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_c_981811ba2479fc8d);
}
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_d_9c04d129c7123e52 {
public:
 Stratum_d_9c04d129c7123e52(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_ii__0_1__11__10::Type& rel_d_c9260aec44573475);
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
t_btree_100_ii__0_1__11__10::Type* rel_d_c9260aec44573475;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_d_9c04d129c7123e52::Stratum_d_9c04d129c7123e52(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_ii__0_1__11__10::Type& rel_d_c9260aec44573475):
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
rel_d_c9260aec44573475(&rel_d_c9260aec44573475){
}

void Stratum_d_9c04d129c7123e52::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X\tY"},{"auxArity","0"},{"cache","true"},{"fact-dir","./input"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","d"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"X\", \"Y\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
if (!inputDirectory.empty()) {directiveMap["fact-dir"] = inputDirectory;}
{
FunctionTimer timer("reading relation rel_d_c9260aec44573475");
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_d_c9260aec44573475);
for (auto& tuple: *rel_d_c9260aec44573475) {
auto untypedTuple = UntypedTuple::fromTypedTuple("d",tuple);
inputFactSet.insert(untypedTuple);
}
dumpInputFacts();
}
} catch (std::exception& e) {std::cerr << "Error loading d data: " << e.what() << '\n';
exit(1);
}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_d_inc_45062f42b27138f2 {
public:
 Stratum_d_inc_45062f42b27138f2(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_ii__0_1__11__10::Type& rel_inc_delta_tuple_delete_d_c806904ae7af9698,t_btree_000_ii__0_1__11__10::Type& rel_inc_delta_tuple_insert_d_67f1c476cedabf8e,t_btree_000_ii__0_1__11__10::Type& rel_old_d_cb928d3acfdedff8,t_btree_100_ii__0_1__11__10::Type& rel_d_c9260aec44573475);
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
t_btree_000_ii__0_1__11__10::Type* rel_inc_delta_tuple_delete_d_c806904ae7af9698;
t_btree_000_ii__0_1__11__10::Type* rel_inc_delta_tuple_insert_d_67f1c476cedabf8e;
t_btree_000_ii__0_1__11__10::Type* rel_old_d_cb928d3acfdedff8;
t_btree_100_ii__0_1__11__10::Type* rel_d_c9260aec44573475;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_d_inc_45062f42b27138f2::Stratum_d_inc_45062f42b27138f2(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_ii__0_1__11__10::Type& rel_inc_delta_tuple_delete_d_c806904ae7af9698,t_btree_000_ii__0_1__11__10::Type& rel_inc_delta_tuple_insert_d_67f1c476cedabf8e,t_btree_000_ii__0_1__11__10::Type& rel_old_d_cb928d3acfdedff8,t_btree_100_ii__0_1__11__10::Type& rel_d_c9260aec44573475):
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
rel_inc_delta_tuple_delete_d_c806904ae7af9698(&rel_inc_delta_tuple_delete_d_c806904ae7af9698),
rel_inc_delta_tuple_insert_d_67f1c476cedabf8e(&rel_inc_delta_tuple_insert_d_67f1c476cedabf8e),
rel_old_d_cb928d3acfdedff8(&rel_old_d_cb928d3acfdedff8),
rel_d_c9260aec44573475(&rel_d_c9260aec44573475){
}

void Stratum_d_inc_45062f42b27138f2::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
rel_d_c9260aec44573475->purge();
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_d_c806904ae7af9698_op_ctxt,rel_inc_delta_tuple_delete_d_c806904ae7af9698->createContext());
CREATE_OP_CONTEXT(rel_old_d_cb928d3acfdedff8_op_ctxt,rel_old_d_cb928d3acfdedff8->createContext());
CREATE_OP_CONTEXT(rel_d_c9260aec44573475_op_ctxt,rel_d_c9260aec44573475->createContext());
for(const auto& env0 : *rel_old_d_cb928d3acfdedff8) {
if( !(rel_inc_delta_tuple_delete_d_c806904ae7af9698->contains(Tuple<RamDomain,2>{{ramBitCast(env0[0]),ramBitCast(env0[1])}},READ_OP_CONTEXT(rel_inc_delta_tuple_delete_d_c806904ae7af9698_op_ctxt)))) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1])}};
rel_d_c9260aec44573475->insert(tuple,READ_OP_CONTEXT(rel_d_c9260aec44573475_op_ctxt));
}
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_d_67f1c476cedabf8e_op_ctxt,rel_inc_delta_tuple_insert_d_67f1c476cedabf8e->createContext());
CREATE_OP_CONTEXT(rel_d_c9260aec44573475_op_ctxt,rel_d_c9260aec44573475->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_insert_d_67f1c476cedabf8e) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1])}};
rel_d_c9260aec44573475->insert(tuple,READ_OP_CONTEXT(rel_d_c9260aec44573475_op_ctxt));
}
}
();}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_inc_table_update_8edbd431ea6611a3 {
public:
 Stratum_inc_table_update_8edbd431ea6611a3(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_a_45448aaf3278243c,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_b_2df625d9f0a591a4,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_c_b4a21678aced1fb3,t_btree_000_ii__0_1__11::Type& rel_inc_derv_overdelete_d_9037dc3d2e668938,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_res_0a6ec95635ed04b1,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_a_3943091945425515,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_b_f9480aa9bb17885d,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_c_1ebbcf7f486522d8,t_btree_100_ii__0_1__11::Type& rel_inc_tuple_overdelete_d_896eca9dd7f781a4,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_res_07e98c324c9a10e0,t_btree_000_i__0__1::Type& rel_old_a_bd7865de58a1cd60,t_btree_000_i__0__1::Type& rel_old_b_38eb3a9d03faff34,t_btree_000_i__0__1::Type& rel_old_c_3e6edc8484191be0,t_btree_000_ii__0_1__11__10::Type& rel_old_d_cb928d3acfdedff8,t_btree_000_i__0__1::Type& rel_old_res_e69d7e45b3927b85,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_ii__0_1__11__10::Type& rel_d_c9260aec44573475,t_btree_100_i__0__1::Type& rel_res_f46f91340698127b);
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
t_btree_000_i__0__1::Type* rel_inc_derv_overdelete_a_45448aaf3278243c;
t_btree_000_i__0__1::Type* rel_inc_derv_overdelete_b_2df625d9f0a591a4;
t_btree_000_i__0__1::Type* rel_inc_derv_overdelete_c_b4a21678aced1fb3;
t_btree_000_ii__0_1__11::Type* rel_inc_derv_overdelete_d_9037dc3d2e668938;
t_btree_000_i__0__1::Type* rel_inc_derv_overdelete_res_0a6ec95635ed04b1;
t_btree_100_i__0__1::Type* rel_inc_tuple_overdelete_a_3943091945425515;
t_btree_100_i__0__1::Type* rel_inc_tuple_overdelete_b_f9480aa9bb17885d;
t_btree_100_i__0__1::Type* rel_inc_tuple_overdelete_c_1ebbcf7f486522d8;
t_btree_100_ii__0_1__11::Type* rel_inc_tuple_overdelete_d_896eca9dd7f781a4;
t_btree_100_i__0__1::Type* rel_inc_tuple_overdelete_res_07e98c324c9a10e0;
t_btree_000_i__0__1::Type* rel_old_a_bd7865de58a1cd60;
t_btree_000_i__0__1::Type* rel_old_b_38eb3a9d03faff34;
t_btree_000_i__0__1::Type* rel_old_c_3e6edc8484191be0;
t_btree_000_ii__0_1__11__10::Type* rel_old_d_cb928d3acfdedff8;
t_btree_000_i__0__1::Type* rel_old_res_e69d7e45b3927b85;
t_btree_100_i__0__1::Type* rel_a_454ddee488c1ed6b;
t_btree_100_i__0__1::Type* rel_b_96694f8e93f5c77d;
t_btree_100_i__0__1::Type* rel_c_981811ba2479fc8d;
t_btree_100_ii__0_1__11__10::Type* rel_d_c9260aec44573475;
t_btree_100_i__0__1::Type* rel_res_f46f91340698127b;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_inc_table_update_8edbd431ea6611a3::Stratum_inc_table_update_8edbd431ea6611a3(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_a_45448aaf3278243c,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_b_2df625d9f0a591a4,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_c_b4a21678aced1fb3,t_btree_000_ii__0_1__11::Type& rel_inc_derv_overdelete_d_9037dc3d2e668938,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_res_0a6ec95635ed04b1,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_a_3943091945425515,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_b_f9480aa9bb17885d,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_c_1ebbcf7f486522d8,t_btree_100_ii__0_1__11::Type& rel_inc_tuple_overdelete_d_896eca9dd7f781a4,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_res_07e98c324c9a10e0,t_btree_000_i__0__1::Type& rel_old_a_bd7865de58a1cd60,t_btree_000_i__0__1::Type& rel_old_b_38eb3a9d03faff34,t_btree_000_i__0__1::Type& rel_old_c_3e6edc8484191be0,t_btree_000_ii__0_1__11__10::Type& rel_old_d_cb928d3acfdedff8,t_btree_000_i__0__1::Type& rel_old_res_e69d7e45b3927b85,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_ii__0_1__11__10::Type& rel_d_c9260aec44573475,t_btree_100_i__0__1::Type& rel_res_f46f91340698127b):
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
rel_inc_derv_overdelete_a_45448aaf3278243c(&rel_inc_derv_overdelete_a_45448aaf3278243c),
rel_inc_derv_overdelete_b_2df625d9f0a591a4(&rel_inc_derv_overdelete_b_2df625d9f0a591a4),
rel_inc_derv_overdelete_c_b4a21678aced1fb3(&rel_inc_derv_overdelete_c_b4a21678aced1fb3),
rel_inc_derv_overdelete_d_9037dc3d2e668938(&rel_inc_derv_overdelete_d_9037dc3d2e668938),
rel_inc_derv_overdelete_res_0a6ec95635ed04b1(&rel_inc_derv_overdelete_res_0a6ec95635ed04b1),
rel_inc_tuple_overdelete_a_3943091945425515(&rel_inc_tuple_overdelete_a_3943091945425515),
rel_inc_tuple_overdelete_b_f9480aa9bb17885d(&rel_inc_tuple_overdelete_b_f9480aa9bb17885d),
rel_inc_tuple_overdelete_c_1ebbcf7f486522d8(&rel_inc_tuple_overdelete_c_1ebbcf7f486522d8),
rel_inc_tuple_overdelete_d_896eca9dd7f781a4(&rel_inc_tuple_overdelete_d_896eca9dd7f781a4),
rel_inc_tuple_overdelete_res_07e98c324c9a10e0(&rel_inc_tuple_overdelete_res_07e98c324c9a10e0),
rel_old_a_bd7865de58a1cd60(&rel_old_a_bd7865de58a1cd60),
rel_old_b_38eb3a9d03faff34(&rel_old_b_38eb3a9d03faff34),
rel_old_c_3e6edc8484191be0(&rel_old_c_3e6edc8484191be0),
rel_old_d_cb928d3acfdedff8(&rel_old_d_cb928d3acfdedff8),
rel_old_res_e69d7e45b3927b85(&rel_old_res_e69d7e45b3927b85),
rel_a_454ddee488c1ed6b(&rel_a_454ddee488c1ed6b),
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d),
rel_c_981811ba2479fc8d(&rel_c_981811ba2479fc8d),
rel_d_c9260aec44573475(&rel_d_c9260aec44573475),
rel_res_f46f91340698127b(&rel_res_f46f91340698127b){
}

void Stratum_inc_table_update_8edbd431ea6611a3::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
rel_old_a_bd7865de58a1cd60->purge();
[&](){
CREATE_OP_CONTEXT(rel_old_a_bd7865de58a1cd60_op_ctxt,rel_old_a_bd7865de58a1cd60->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
for(const auto& env0 : *rel_a_454ddee488c1ed6b) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_old_a_bd7865de58a1cd60->insert(tuple,READ_OP_CONTEXT(rel_old_a_bd7865de58a1cd60_op_ctxt));
}
}
();rel_inc_tuple_overdelete_a_3943091945425515->purge();
rel_inc_derv_overdelete_a_45448aaf3278243c->purge();
rel_old_d_cb928d3acfdedff8->purge();
[&](){
CREATE_OP_CONTEXT(rel_old_d_cb928d3acfdedff8_op_ctxt,rel_old_d_cb928d3acfdedff8->createContext());
CREATE_OP_CONTEXT(rel_d_c9260aec44573475_op_ctxt,rel_d_c9260aec44573475->createContext());
for(const auto& env0 : *rel_d_c9260aec44573475) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1])}};
rel_old_d_cb928d3acfdedff8->insert(tuple,READ_OP_CONTEXT(rel_old_d_cb928d3acfdedff8_op_ctxt));
}
}
();rel_inc_tuple_overdelete_d_896eca9dd7f781a4->purge();
rel_inc_derv_overdelete_d_9037dc3d2e668938->purge();
rel_old_b_38eb3a9d03faff34->purge();
[&](){
CREATE_OP_CONTEXT(rel_old_b_38eb3a9d03faff34_op_ctxt,rel_old_b_38eb3a9d03faff34->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
for(const auto& env0 : *rel_b_96694f8e93f5c77d) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_old_b_38eb3a9d03faff34->insert(tuple,READ_OP_CONTEXT(rel_old_b_38eb3a9d03faff34_op_ctxt));
}
}
();rel_inc_tuple_overdelete_b_f9480aa9bb17885d->purge();
rel_inc_derv_overdelete_b_2df625d9f0a591a4->purge();
rel_old_c_3e6edc8484191be0->purge();
[&](){
CREATE_OP_CONTEXT(rel_old_c_3e6edc8484191be0_op_ctxt,rel_old_c_3e6edc8484191be0->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
for(const auto& env0 : *rel_c_981811ba2479fc8d) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_old_c_3e6edc8484191be0->insert(tuple,READ_OP_CONTEXT(rel_old_c_3e6edc8484191be0_op_ctxt));
}
}
();rel_inc_tuple_overdelete_c_1ebbcf7f486522d8->purge();
rel_inc_derv_overdelete_c_b4a21678aced1fb3->purge();
rel_old_res_e69d7e45b3927b85->purge();
[&](){
CREATE_OP_CONTEXT(rel_old_res_e69d7e45b3927b85_op_ctxt,rel_old_res_e69d7e45b3927b85->createContext());
CREATE_OP_CONTEXT(rel_res_f46f91340698127b_op_ctxt,rel_res_f46f91340698127b->createContext());
for(const auto& env0 : *rel_res_f46f91340698127b) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_old_res_e69d7e45b3927b85->insert(tuple,READ_OP_CONTEXT(rel_old_res_e69d7e45b3927b85_op_ctxt));
}
}
();rel_inc_tuple_overdelete_res_07e98c324c9a10e0->purge();
rel_inc_derv_overdelete_res_0a6ec95635ed04b1->purge();
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_res_3d4a75d2a12655c0 {
public:
 Stratum_res_3d4a75d2a12655c0(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_i__0__1::Type& rel_res_f46f91340698127b);
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
t_btree_100_i__0__1::Type* rel_b_96694f8e93f5c77d;
t_btree_100_i__0__1::Type* rel_c_981811ba2479fc8d;
t_btree_100_i__0__1::Type* rel_res_f46f91340698127b;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_res_3d4a75d2a12655c0::Stratum_res_3d4a75d2a12655c0(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_i__0__1::Type& rel_res_f46f91340698127b):
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
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d),
rel_c_981811ba2479fc8d(&rel_c_981811ba2479fc8d),
rel_res_f46f91340698127b(&rel_res_f46f91340698127b){
}

void Stratum_res_3d4a75d2a12655c0::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(res(X) :- 
   b(X),
   c(X).
in file test.dl [19:6-19:26])_");
if(!(rel_b_96694f8e93f5c77d->empty()) && !(rel_c_981811ba2479fc8d->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
CREATE_OP_CONTEXT(rel_res_f46f91340698127b_op_ctxt,rel_res_f46f91340698127b->createContext());
for(const auto& env0 : *rel_b_96694f8e93f5c77d) {
if( rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_res_f46f91340698127b->insert(tuple,READ_OP_CONTEXT(rel_res_f46f91340698127b_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("res",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{3, varValues};
ruleSet->insert(ruleApplication);
}
}
}
();}
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","res"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
{
FunctionTimer timer("writing relation rel_res_f46f91340698127b");
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_res_f46f91340698127b);
}
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_res_inc_9b4cbdaf094874bf {
public:
 Stratum_res_inc_9b4cbdaf094874bf(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_res_21fda861d986a27e,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_res_f04df169be2474c0,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_b_1b345b26cae73383,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_res_cb088c722dc19c6b,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_c_70adcfa1afd70315,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b,t_btree_000_i__0__1::Type& rel_old_c_3e6edc8484191be0,t_btree_000_i__0__1::Type& rel_old_res_e69d7e45b3927b85,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_i__0__1::Type& rel_res_f46f91340698127b);
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
t_btree_000_i__0__1::Type* rel_inc_delta_derv_delete_res_21fda861d986a27e;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_insert_res_f04df169be2474c0;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_b_1b345b26cae73383;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_c_43fa1164ffebfc11;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_res_cb088c722dc19c6b;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_c_70adcfa1afd70315;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b;
t_btree_000_i__0__1::Type* rel_old_c_3e6edc8484191be0;
t_btree_000_i__0__1::Type* rel_old_res_e69d7e45b3927b85;
t_btree_100_i__0__1::Type* rel_b_96694f8e93f5c77d;
t_btree_100_i__0__1::Type* rel_c_981811ba2479fc8d;
t_btree_100_i__0__1::Type* rel_res_f46f91340698127b;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_res_inc_9b4cbdaf094874bf::Stratum_res_inc_9b4cbdaf094874bf(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_res_21fda861d986a27e,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_res_f04df169be2474c0,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_b_1b345b26cae73383,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_res_cb088c722dc19c6b,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_c_70adcfa1afd70315,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b,t_btree_000_i__0__1::Type& rel_old_c_3e6edc8484191be0,t_btree_000_i__0__1::Type& rel_old_res_e69d7e45b3927b85,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_i__0__1::Type& rel_res_f46f91340698127b):
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
rel_inc_delta_derv_delete_res_21fda861d986a27e(&rel_inc_delta_derv_delete_res_21fda861d986a27e),
rel_inc_delta_derv_insert_res_f04df169be2474c0(&rel_inc_delta_derv_insert_res_f04df169be2474c0),
rel_inc_delta_tuple_delete_b_1b345b26cae73383(&rel_inc_delta_tuple_delete_b_1b345b26cae73383),
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11(&rel_inc_delta_tuple_delete_c_43fa1164ffebfc11),
rel_inc_delta_tuple_delete_res_cb088c722dc19c6b(&rel_inc_delta_tuple_delete_res_cb088c722dc19c6b),
rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46(&rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46),
rel_inc_delta_tuple_insert_c_70adcfa1afd70315(&rel_inc_delta_tuple_insert_c_70adcfa1afd70315),
rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b(&rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b),
rel_old_c_3e6edc8484191be0(&rel_old_c_3e6edc8484191be0),
rel_old_res_e69d7e45b3927b85(&rel_old_res_e69d7e45b3927b85),
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d),
rel_c_981811ba2479fc8d(&rel_c_981811ba2479fc8d),
rel_res_f46f91340698127b(&rel_res_f46f91340698127b){
}

void Stratum_res_inc_9b4cbdaf094874bf::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(res(X) :- 
   b(X),
   c(X).
in file test.dl [19:6-19:26])_");
if(!(rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46->empty()) && !(rel_old_c_3e6edc8484191be0->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_insert_res_f04df169be2474c0_op_ctxt,rel_inc_delta_derv_insert_res_f04df169be2474c0->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46_op_ctxt,rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46->createContext());
CREATE_OP_CONTEXT(rel_old_c_3e6edc8484191be0_op_ctxt,rel_old_c_3e6edc8484191be0->createContext());
CREATE_OP_CONTEXT(rel_res_f46f91340698127b_op_ctxt,rel_res_f46f91340698127b->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46) {
if( rel_old_c_3e6edc8484191be0->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_old_c_3e6edc8484191be0_op_ctxt))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_insert_res_f04df169be2474c0->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_insert_res_f04df169be2474c0_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("res",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaInsertRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{3, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
();}
if(!(rel_inc_delta_tuple_delete_b_1b345b26cae73383->empty()) && !(rel_old_c_3e6edc8484191be0->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_res_21fda861d986a27e_op_ctxt,rel_inc_delta_derv_delete_res_21fda861d986a27e->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt,rel_inc_delta_tuple_delete_b_1b345b26cae73383->createContext());
CREATE_OP_CONTEXT(rel_old_c_3e6edc8484191be0_op_ctxt,rel_old_c_3e6edc8484191be0->createContext());
CREATE_OP_CONTEXT(rel_res_f46f91340698127b_op_ctxt,rel_res_f46f91340698127b->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_delete_b_1b345b26cae73383) {
if( rel_old_c_3e6edc8484191be0->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_old_c_3e6edc8484191be0_op_ctxt))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_delete_res_21fda861d986a27e->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_delete_res_21fda861d986a27e_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("res",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaDeleteRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{3, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
if (ruleManager.isInRecursiveStratum(ruleApplication.ruleId)) {
auto*& ruleSetComplete = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
for (const auto& ruleApp: *ruleSetComplete) {
if(ruleManager.isRecursive(ruleApp.ruleId)) {
ruleSet->insert(ruleApp);
ruleSet2->insert(ruleApp);
}
}
}
}
}
}
();}
if(!(rel_b_96694f8e93f5c77d->empty()) && !(rel_inc_delta_tuple_insert_c_70adcfa1afd70315->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_insert_res_f04df169be2474c0_op_ctxt,rel_inc_delta_derv_insert_res_f04df169be2474c0->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_c_70adcfa1afd70315_op_ctxt,rel_inc_delta_tuple_insert_c_70adcfa1afd70315->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
CREATE_OP_CONTEXT(rel_res_f46f91340698127b_op_ctxt,rel_res_f46f91340698127b->createContext());
for(const auto& env0 : *rel_b_96694f8e93f5c77d) {
if( rel_inc_delta_tuple_insert_c_70adcfa1afd70315->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_inc_delta_tuple_insert_c_70adcfa1afd70315_op_ctxt))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_insert_res_f04df169be2474c0->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_insert_res_f04df169be2474c0_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("res",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaInsertRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{3, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
();}
if(!(rel_b_96694f8e93f5c77d->empty()) && !(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_res_21fda861d986a27e_op_ctxt,rel_inc_delta_derv_delete_res_21fda861d986a27e->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt,rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
CREATE_OP_CONTEXT(rel_res_f46f91340698127b_op_ctxt,rel_res_f46f91340698127b->createContext());
for(const auto& env0 : *rel_b_96694f8e93f5c77d) {
if( rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_delete_res_21fda861d986a27e->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_delete_res_21fda861d986a27e_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("res",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaDeleteRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{3, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
if (ruleManager.isInRecursiveStratum(ruleApplication.ruleId)) {
auto*& ruleSetComplete = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
for (const auto& ruleApp: *ruleSetComplete) {
if(ruleManager.isRecursive(ruleApp.ruleId)) {
ruleSet->insert(ruleApp);
ruleSet2->insert(ruleApp);
}
}
}
}
}
}
();}
for(const auto& tupleDeltaDervInsert: *rel_inc_delta_derv_insert_res_f04df169be2474c0) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("res",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& tupleDeltaDervDelete: *rel_inc_delta_derv_delete_res_21fda861d986a27e) {
auto untypedDeltaDervTupleDelete = UntypedTuple::fromTypedTuple("res",tupleDeltaDervDelete);
auto*& untypedDeltaDervTupleDeleteRuleSet = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedDeltaDervTupleDelete];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleDelete];

for(const auto& deletedRuleAppl: *untypedDeltaDervTupleDeleteRuleSet) {
untypedDeltaDervTupleRuleSet->erase(deletedRuleAppl);

}
if (untypedDeltaDervTupleRuleSet->empty()) {
delete untypedDeltaDervTupleRuleSet;

DerivationManager::untypedTuple2RuleApplications.erase(untypedDeltaDervTupleDelete);

if(!isInputFact(untypedDeltaDervTupleDelete))
rel_inc_delta_tuple_delete_res_cb088c722dc19c6b->insert(tupleDeltaDervDelete);
 
}
}
DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
for(const auto& deletedTuple: *rel_inc_delta_tuple_delete_res_cb088c722dc19c6b) {

rel_res_f46f91340698127b->erase(deletedTuple);

}

for(const auto& insertedTuple: *rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b) {

rel_res_f46f91340698127b->insert(insertedTuple);

}

if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","res"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
{
FunctionTimer timer("writing relation rel_res_f46f91340698127b");
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_res_f46f91340698127b);
}
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Sf_compute: public SouffleProgram {
public:
 Sf_compute();
 ~Sf_compute();
void run();
void runInc();
void runAll(std::string inputDirectoryArg = "",std::string outputDirectoryArg = "",bool performIOArg = true,bool pruneImdtRelsArg = false);
void runAllInc(std::string inputDirectoryArg = "",std::string outputDirectoryArg = "",bool performIOArg = true,bool pruneImdtRelsArg = false);
void printAll([[maybe_unused]] std::string outputDirectoryArg = "");
void loadAll([[maybe_unused]] std::string inputDirectoryArg = "");
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
Own<t_btree_100_i__0__1::Type> rel_a_454ddee488c1ed6b;
souffle::RelationWrapper<t_btree_100_i__0__1::Type> wrapper_rel_a_454ddee488c1ed6b;
Own<t_btree_000_i__0__1::Type> rel_old_a_bd7865de58a1cd60;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_derv_insert_a_bbea135139bb89ae;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_derv_insert_a_bbea135139bb89ae;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_derv_delete_a_394b3623655cbdd0;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_derv_delete_a_394b3623655cbdd0;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_insert_a_88895cba4fbff038;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_tuple_insert_a_88895cba4fbff038;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_delete_a_58354ad400e6cd67;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_tuple_delete_a_58354ad400e6cd67;
Own<t_btree_000_i__0__1::Type> rel_tmp_a_619a41cbadd142a6;
Own<t_btree_000_i__0__1::Type> rel_tmp2_a_aedcf448adead01c;
Own<t_btree_000_i__0__1::Type> rel_tmp3_a_2c915cf5bb34ed3b;
Own<t_btree_000_i__0__1::Type> rel_tmp4_a_55c6aba43a5b815f;
Own<t_btree_100_i__0__1::Type> rel_inc_tuple_overdelete_a_3943091945425515;
Own<t_btree_000_i__0__1::Type> rel_inc_derv_overdelete_a_45448aaf3278243c;
Own<t_btree_000_i__0__1::Type> rel_inc_new_derv_rederive_a_cf31a1985b633fe9;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7;
Own<t_btree_000_i__0__1::Type> rel_delta_tuple_delete_a_249185740a160373;
Own<t_btree_000_i__0__1::Type> rel_delta_tuple_insert_a_419a5d6ce8e56c01;
Own<t_btree_000_i__0__1::Type> rel_new_derv_delete_a_f6265c4636f36293;
Own<t_btree_000_i__0__1::Type> rel_new_derv_insert_a_cb7a1502e8af1907;
Own<t_btree_100_ii__0_1__11__10::Type> rel_d_c9260aec44573475;
souffle::RelationWrapper<t_btree_100_ii__0_1__11__10::Type> wrapper_rel_d_c9260aec44573475;
Own<t_btree_000_ii__0_1__11__10::Type> rel_old_d_cb928d3acfdedff8;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_derv_insert_d_e45c88965e47e71d;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_derv_insert_d_e45c88965e47e71d;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_derv_delete_d_2ad2dc9f2ce026fb;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_inc_delta_derv_delete_d_2ad2dc9f2ce026fb;
Own<t_btree_000_ii__0_1__11__10::Type> rel_inc_delta_tuple_insert_d_67f1c476cedabf8e;
souffle::RelationWrapper<t_btree_000_ii__0_1__11__10::Type> wrapper_rel_inc_delta_tuple_insert_d_67f1c476cedabf8e;
Own<t_btree_000_ii__0_1__11__10::Type> rel_inc_delta_tuple_delete_d_c806904ae7af9698;
souffle::RelationWrapper<t_btree_000_ii__0_1__11__10::Type> wrapper_rel_inc_delta_tuple_delete_d_c806904ae7af9698;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp_d_e2a93ffc9224fd12;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp2_d_f278ef1f27134152;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp3_d_9e58bcf1ee3b6076;
Own<t_btree_000_ii__0_1__11::Type> rel_tmp4_d_fcb3483b4bf1d94e;
Own<t_btree_100_ii__0_1__11::Type> rel_inc_tuple_overdelete_d_896eca9dd7f781a4;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_derv_overdelete_d_9037dc3d2e668938;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_new_derv_rederive_d_46523e75075c36ba;
Own<t_btree_000_ii__0_1__11::Type> rel_inc_delta_tuple_rederive_d_115e10960cc8605e;
Own<t_btree_000_ii__0_1__11::Type> rel_delta_tuple_delete_d_8d2ca2a67a0853b0;
Own<t_btree_000_ii__0_1__11::Type> rel_delta_tuple_insert_d_474e4f3ff1761652;
Own<t_btree_000_ii__0_1__11::Type> rel_new_derv_delete_d_4d2bc9988b469c52;
Own<t_btree_000_ii__0_1__11::Type> rel_new_derv_insert_d_b1bdeac08cb29501;
Own<t_btree_100_i__0__1::Type> rel_b_96694f8e93f5c77d;
souffle::RelationWrapper<t_btree_100_i__0__1::Type> wrapper_rel_b_96694f8e93f5c77d;
Own<t_btree_000_i__0__1::Type> rel_old_b_38eb3a9d03faff34;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_derv_insert_b_420980c115f01b29;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_derv_insert_b_420980c115f01b29;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_derv_delete_b_ded826b2e75f35bb;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_derv_delete_b_ded826b2e75f35bb;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_delete_b_1b345b26cae73383;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_tuple_delete_b_1b345b26cae73383;
Own<t_btree_000_i__0__1::Type> rel_tmp_b_d86ac13254fca704;
Own<t_btree_000_i__0__1::Type> rel_tmp2_b_b058b32c7cd1b966;
Own<t_btree_000_i__0__1::Type> rel_tmp3_b_2e0d1bd98a1a5c51;
Own<t_btree_000_i__0__1::Type> rel_tmp4_b_d5b6cdda8ba346c1;
Own<t_btree_100_i__0__1::Type> rel_inc_tuple_overdelete_b_f9480aa9bb17885d;
Own<t_btree_000_i__0__1::Type> rel_inc_derv_overdelete_b_2df625d9f0a591a4;
Own<t_btree_000_i__0__1::Type> rel_inc_new_derv_rederive_b_85af71cbeb32637e;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_rederive_b_29aee4d2a2657817;
Own<t_btree_000_i__0__1::Type> rel_delta_tuple_delete_b_43dfed04dd4422a4;
Own<t_btree_000_i__0__1::Type> rel_delta_tuple_insert_b_a88502da93017be6;
Own<t_btree_000_i__0__1::Type> rel_new_derv_delete_b_4199ee6bbba1324f;
Own<t_btree_000_i__0__1::Type> rel_new_derv_insert_b_8572f4c197f8bd21;
Own<t_btree_100_i__0__1::Type> rel_c_981811ba2479fc8d;
souffle::RelationWrapper<t_btree_100_i__0__1::Type> wrapper_rel_c_981811ba2479fc8d;
Own<t_btree_000_i__0__1::Type> rel_old_c_3e6edc8484191be0;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_derv_insert_c_0d24c4484987ca3f;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_derv_insert_c_0d24c4484987ca3f;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_derv_delete_c_2409cf566a420e77;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_derv_delete_c_2409cf566a420e77;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_insert_c_70adcfa1afd70315;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_tuple_insert_c_70adcfa1afd70315;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_delete_c_43fa1164ffebfc11;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_tuple_delete_c_43fa1164ffebfc11;
Own<t_btree_000_i__0__1::Type> rel_tmp_c_b253e025804a1387;
Own<t_btree_000_i__0__1::Type> rel_tmp2_c_a683fa7e39954476;
Own<t_btree_000_i__0__1::Type> rel_tmp3_c_0914a1995ce884bc;
Own<t_btree_000_i__0__1::Type> rel_tmp4_c_7881dda484a7d59d;
Own<t_btree_100_i__0__1::Type> rel_inc_tuple_overdelete_c_1ebbcf7f486522d8;
Own<t_btree_000_i__0__1::Type> rel_inc_derv_overdelete_c_b4a21678aced1fb3;
Own<t_btree_000_i__0__1::Type> rel_inc_new_derv_rederive_c_c3f4d0e8ef218854;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263;
Own<t_btree_000_i__0__1::Type> rel_delta_tuple_delete_c_99c900b677e7ae7a;
Own<t_btree_000_i__0__1::Type> rel_delta_tuple_insert_c_02751d71f015bc27;
Own<t_btree_000_i__0__1::Type> rel_new_derv_delete_c_eb3a420fe21b36f5;
Own<t_btree_000_i__0__1::Type> rel_new_derv_insert_c_92944d39136b7981;
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
Stratum_a_inc_935a746942ed3383 stratum_a_inc_a6264e86fb843b31;
Stratum_b_1bd76238ec74a612 stratum_b_43b73774b68513f6;
Stratum_b_inc_92e6ac324cc92315 stratum_b_inc_5bd3860cc23b7de3;
Stratum_c_9d76b7130e3957ed stratum_c_b5ed19a85a4e3095;
Stratum_c_inc_cf2c4d7fafabff14 stratum_c_inc_3a2e56b5626fb27c;
Stratum_d_9c04d129c7123e52 stratum_d_f581825f3af32bbd;
Stratum_d_inc_45062f42b27138f2 stratum_d_inc_3477ccbc77b30aeb;
Stratum_inc_table_update_8edbd431ea6611a3 stratum_inc_table_update_da18684048fa1d45;
Stratum_res_3d4a75d2a12655c0 stratum_res_889015e26521459c;
Stratum_res_inc_9b4cbdaf094874bf stratum_res_inc_6a3962e77bf04edf;
SignalHandler* signalHandler{SignalHandler::instance()};
std::atomic<RamDomain> ctr{};
std::atomic<std::size_t> iter{};
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Sf_compute::Sf_compute():
symTable(),
recordTable(),
regexCache(),
rel_a_454ddee488c1ed6b(mk<t_btree_100_i__0__1::Type>()),
wrapper_rel_a_454ddee488c1ed6b(0, *rel_a_454ddee488c1ed6b, *this, "a", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_old_a_bd7865de58a1cd60(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_derv_insert_a_bbea135139bb89ae(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_insert_a_bbea135139bb89ae(1, *rel_inc_delta_derv_insert_a_bbea135139bb89ae, *this, "$inc_delta_derv_insert_a", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_derv_delete_a_394b3623655cbdd0(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_delete_a_394b3623655cbdd0(2, *rel_inc_delta_derv_delete_a_394b3623655cbdd0, *this, "$inc_delta_derv_delete_a", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_tuple_insert_a_88895cba4fbff038(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_insert_a_88895cba4fbff038(3, *rel_inc_delta_tuple_insert_a_88895cba4fbff038, *this, "$inc_delta_tuple_insert_a", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_tuple_delete_a_58354ad400e6cd67(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_delete_a_58354ad400e6cd67(4, *rel_inc_delta_tuple_delete_a_58354ad400e6cd67, *this, "$inc_delta_tuple_delete_a", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_tmp_a_619a41cbadd142a6(mk<t_btree_000_i__0__1::Type>()),
rel_tmp2_a_aedcf448adead01c(mk<t_btree_000_i__0__1::Type>()),
rel_tmp3_a_2c915cf5bb34ed3b(mk<t_btree_000_i__0__1::Type>()),
rel_tmp4_a_55c6aba43a5b815f(mk<t_btree_000_i__0__1::Type>()),
rel_inc_tuple_overdelete_a_3943091945425515(mk<t_btree_100_i__0__1::Type>()),
rel_inc_derv_overdelete_a_45448aaf3278243c(mk<t_btree_000_i__0__1::Type>()),
rel_inc_new_derv_rederive_a_cf31a1985b633fe9(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7(mk<t_btree_000_i__0__1::Type>()),
rel_delta_tuple_delete_a_249185740a160373(mk<t_btree_000_i__0__1::Type>()),
rel_delta_tuple_insert_a_419a5d6ce8e56c01(mk<t_btree_000_i__0__1::Type>()),
rel_new_derv_delete_a_f6265c4636f36293(mk<t_btree_000_i__0__1::Type>()),
rel_new_derv_insert_a_cb7a1502e8af1907(mk<t_btree_000_i__0__1::Type>()),
rel_d_c9260aec44573475(mk<t_btree_100_ii__0_1__11__10::Type>()),
wrapper_rel_d_c9260aec44573475(5, *rel_d_c9260aec44573475, *this, "d", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Y"}}, 0),
rel_old_d_cb928d3acfdedff8(mk<t_btree_000_ii__0_1__11__10::Type>()),
rel_inc_delta_derv_insert_d_e45c88965e47e71d(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_derv_insert_d_e45c88965e47e71d(6, *rel_inc_delta_derv_insert_d_e45c88965e47e71d, *this, "$inc_delta_derv_insert_d", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Y"}}, 0),
rel_inc_delta_derv_delete_d_2ad2dc9f2ce026fb(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_inc_delta_derv_delete_d_2ad2dc9f2ce026fb(7, *rel_inc_delta_derv_delete_d_2ad2dc9f2ce026fb, *this, "$inc_delta_derv_delete_d", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Y"}}, 0),
rel_inc_delta_tuple_insert_d_67f1c476cedabf8e(mk<t_btree_000_ii__0_1__11__10::Type>()),
wrapper_rel_inc_delta_tuple_insert_d_67f1c476cedabf8e(8, *rel_inc_delta_tuple_insert_d_67f1c476cedabf8e, *this, "$inc_delta_tuple_insert_d", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Y"}}, 0),
rel_inc_delta_tuple_delete_d_c806904ae7af9698(mk<t_btree_000_ii__0_1__11__10::Type>()),
wrapper_rel_inc_delta_tuple_delete_d_c806904ae7af9698(9, *rel_inc_delta_tuple_delete_d_c806904ae7af9698, *this, "$inc_delta_tuple_delete_d", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"X","Y"}}, 0),
rel_tmp_d_e2a93ffc9224fd12(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp2_d_f278ef1f27134152(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp3_d_9e58bcf1ee3b6076(mk<t_btree_000_ii__0_1__11::Type>()),
rel_tmp4_d_fcb3483b4bf1d94e(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_tuple_overdelete_d_896eca9dd7f781a4(mk<t_btree_100_ii__0_1__11::Type>()),
rel_inc_derv_overdelete_d_9037dc3d2e668938(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_new_derv_rederive_d_46523e75075c36ba(mk<t_btree_000_ii__0_1__11::Type>()),
rel_inc_delta_tuple_rederive_d_115e10960cc8605e(mk<t_btree_000_ii__0_1__11::Type>()),
rel_delta_tuple_delete_d_8d2ca2a67a0853b0(mk<t_btree_000_ii__0_1__11::Type>()),
rel_delta_tuple_insert_d_474e4f3ff1761652(mk<t_btree_000_ii__0_1__11::Type>()),
rel_new_derv_delete_d_4d2bc9988b469c52(mk<t_btree_000_ii__0_1__11::Type>()),
rel_new_derv_insert_d_b1bdeac08cb29501(mk<t_btree_000_ii__0_1__11::Type>()),
rel_b_96694f8e93f5c77d(mk<t_btree_100_i__0__1::Type>()),
wrapper_rel_b_96694f8e93f5c77d(10, *rel_b_96694f8e93f5c77d, *this, "b", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_old_b_38eb3a9d03faff34(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_derv_insert_b_420980c115f01b29(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_insert_b_420980c115f01b29(11, *rel_inc_delta_derv_insert_b_420980c115f01b29, *this, "$inc_delta_derv_insert_b", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_derv_delete_b_ded826b2e75f35bb(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_delete_b_ded826b2e75f35bb(12, *rel_inc_delta_derv_delete_b_ded826b2e75f35bb, *this, "$inc_delta_derv_delete_b", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46(13, *rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46, *this, "$inc_delta_tuple_insert_b", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_tuple_delete_b_1b345b26cae73383(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_delete_b_1b345b26cae73383(14, *rel_inc_delta_tuple_delete_b_1b345b26cae73383, *this, "$inc_delta_tuple_delete_b", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_tmp_b_d86ac13254fca704(mk<t_btree_000_i__0__1::Type>()),
rel_tmp2_b_b058b32c7cd1b966(mk<t_btree_000_i__0__1::Type>()),
rel_tmp3_b_2e0d1bd98a1a5c51(mk<t_btree_000_i__0__1::Type>()),
rel_tmp4_b_d5b6cdda8ba346c1(mk<t_btree_000_i__0__1::Type>()),
rel_inc_tuple_overdelete_b_f9480aa9bb17885d(mk<t_btree_100_i__0__1::Type>()),
rel_inc_derv_overdelete_b_2df625d9f0a591a4(mk<t_btree_000_i__0__1::Type>()),
rel_inc_new_derv_rederive_b_85af71cbeb32637e(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_tuple_rederive_b_29aee4d2a2657817(mk<t_btree_000_i__0__1::Type>()),
rel_delta_tuple_delete_b_43dfed04dd4422a4(mk<t_btree_000_i__0__1::Type>()),
rel_delta_tuple_insert_b_a88502da93017be6(mk<t_btree_000_i__0__1::Type>()),
rel_new_derv_delete_b_4199ee6bbba1324f(mk<t_btree_000_i__0__1::Type>()),
rel_new_derv_insert_b_8572f4c197f8bd21(mk<t_btree_000_i__0__1::Type>()),
rel_c_981811ba2479fc8d(mk<t_btree_100_i__0__1::Type>()),
wrapper_rel_c_981811ba2479fc8d(15, *rel_c_981811ba2479fc8d, *this, "c", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_old_c_3e6edc8484191be0(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_derv_insert_c_0d24c4484987ca3f(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_insert_c_0d24c4484987ca3f(16, *rel_inc_delta_derv_insert_c_0d24c4484987ca3f, *this, "$inc_delta_derv_insert_c", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_derv_delete_c_2409cf566a420e77(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_delete_c_2409cf566a420e77(17, *rel_inc_delta_derv_delete_c_2409cf566a420e77, *this, "$inc_delta_derv_delete_c", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_tuple_insert_c_70adcfa1afd70315(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_insert_c_70adcfa1afd70315(18, *rel_inc_delta_tuple_insert_c_70adcfa1afd70315, *this, "$inc_delta_tuple_insert_c", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_delete_c_43fa1164ffebfc11(19, *rel_inc_delta_tuple_delete_c_43fa1164ffebfc11, *this, "$inc_delta_tuple_delete_c", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_tmp_c_b253e025804a1387(mk<t_btree_000_i__0__1::Type>()),
rel_tmp2_c_a683fa7e39954476(mk<t_btree_000_i__0__1::Type>()),
rel_tmp3_c_0914a1995ce884bc(mk<t_btree_000_i__0__1::Type>()),
rel_tmp4_c_7881dda484a7d59d(mk<t_btree_000_i__0__1::Type>()),
rel_inc_tuple_overdelete_c_1ebbcf7f486522d8(mk<t_btree_100_i__0__1::Type>()),
rel_inc_derv_overdelete_c_b4a21678aced1fb3(mk<t_btree_000_i__0__1::Type>()),
rel_inc_new_derv_rederive_c_c3f4d0e8ef218854(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263(mk<t_btree_000_i__0__1::Type>()),
rel_delta_tuple_delete_c_99c900b677e7ae7a(mk<t_btree_000_i__0__1::Type>()),
rel_delta_tuple_insert_c_02751d71f015bc27(mk<t_btree_000_i__0__1::Type>()),
rel_new_derv_delete_c_eb3a420fe21b36f5(mk<t_btree_000_i__0__1::Type>()),
rel_new_derv_insert_c_92944d39136b7981(mk<t_btree_000_i__0__1::Type>()),
rel_res_f46f91340698127b(mk<t_btree_100_i__0__1::Type>()),
wrapper_rel_res_f46f91340698127b(20, *rel_res_f46f91340698127b, *this, "res", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_old_res_e69d7e45b3927b85(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_derv_insert_res_f04df169be2474c0(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_insert_res_f04df169be2474c0(21, *rel_inc_delta_derv_insert_res_f04df169be2474c0, *this, "$inc_delta_derv_insert_res", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_derv_delete_res_21fda861d986a27e(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_delete_res_21fda861d986a27e(22, *rel_inc_delta_derv_delete_res_21fda861d986a27e, *this, "$inc_delta_derv_delete_res", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b(23, *rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b, *this, "$inc_delta_tuple_insert_res", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
rel_inc_delta_tuple_delete_res_cb088c722dc19c6b(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_delete_res_cb088c722dc19c6b(24, *rel_inc_delta_tuple_delete_res_cb088c722dc19c6b, *this, "$inc_delta_tuple_delete_res", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"X"}}, 0),
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
stratum_a_f1f7b26a4b108ce3(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_a_454ddee488c1ed6b),
stratum_a_inc_a6264e86fb843b31(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_inc_delta_tuple_delete_a_58354ad400e6cd67,*rel_inc_delta_tuple_insert_a_88895cba4fbff038,*rel_old_a_bd7865de58a1cd60,*rel_a_454ddee488c1ed6b),
stratum_b_43b73774b68513f6(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_a_454ddee488c1ed6b,*rel_b_96694f8e93f5c77d),
stratum_b_inc_5bd3860cc23b7de3(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_inc_delta_derv_delete_b_ded826b2e75f35bb,*rel_inc_delta_derv_insert_b_420980c115f01b29,*rel_inc_delta_tuple_delete_a_58354ad400e6cd67,*rel_inc_delta_tuple_delete_b_1b345b26cae73383,*rel_inc_delta_tuple_insert_a_88895cba4fbff038,*rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,*rel_old_b_38eb3a9d03faff34,*rel_a_454ddee488c1ed6b,*rel_b_96694f8e93f5c77d),
stratum_c_b5ed19a85a4e3095(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_b_96694f8e93f5c77d,*rel_c_981811ba2479fc8d,*rel_d_c9260aec44573475),
stratum_c_inc_3a2e56b5626fb27c(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_inc_delta_derv_delete_c_2409cf566a420e77,*rel_inc_delta_derv_insert_c_0d24c4484987ca3f,*rel_inc_delta_tuple_delete_b_1b345b26cae73383,*rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,*rel_inc_delta_tuple_delete_d_c806904ae7af9698,*rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,*rel_inc_delta_tuple_insert_c_70adcfa1afd70315,*rel_inc_delta_tuple_insert_d_67f1c476cedabf8e,*rel_old_c_3e6edc8484191be0,*rel_old_d_cb928d3acfdedff8,*rel_b_96694f8e93f5c77d,*rel_c_981811ba2479fc8d,*rel_d_c9260aec44573475),
stratum_d_f581825f3af32bbd(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_d_c9260aec44573475),
stratum_d_inc_3477ccbc77b30aeb(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_inc_delta_tuple_delete_d_c806904ae7af9698,*rel_inc_delta_tuple_insert_d_67f1c476cedabf8e,*rel_old_d_cb928d3acfdedff8,*rel_d_c9260aec44573475),
stratum_inc_table_update_da18684048fa1d45(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_inc_derv_overdelete_a_45448aaf3278243c,*rel_inc_derv_overdelete_b_2df625d9f0a591a4,*rel_inc_derv_overdelete_c_b4a21678aced1fb3,*rel_inc_derv_overdelete_d_9037dc3d2e668938,*rel_inc_derv_overdelete_res_0a6ec95635ed04b1,*rel_inc_tuple_overdelete_a_3943091945425515,*rel_inc_tuple_overdelete_b_f9480aa9bb17885d,*rel_inc_tuple_overdelete_c_1ebbcf7f486522d8,*rel_inc_tuple_overdelete_d_896eca9dd7f781a4,*rel_inc_tuple_overdelete_res_07e98c324c9a10e0,*rel_old_a_bd7865de58a1cd60,*rel_old_b_38eb3a9d03faff34,*rel_old_c_3e6edc8484191be0,*rel_old_d_cb928d3acfdedff8,*rel_old_res_e69d7e45b3927b85,*rel_a_454ddee488c1ed6b,*rel_b_96694f8e93f5c77d,*rel_c_981811ba2479fc8d,*rel_d_c9260aec44573475,*rel_res_f46f91340698127b),
stratum_res_889015e26521459c(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_b_96694f8e93f5c77d,*rel_c_981811ba2479fc8d,*rel_res_f46f91340698127b),
stratum_res_inc_6a3962e77bf04edf(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_inc_delta_derv_delete_res_21fda861d986a27e,*rel_inc_delta_derv_insert_res_f04df169be2474c0,*rel_inc_delta_tuple_delete_b_1b345b26cae73383,*rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,*rel_inc_delta_tuple_delete_res_cb088c722dc19c6b,*rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,*rel_inc_delta_tuple_insert_c_70adcfa1afd70315,*rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b,*rel_old_c_3e6edc8484191be0,*rel_old_res_e69d7e45b3927b85,*rel_b_96694f8e93f5c77d,*rel_c_981811ba2479fc8d,*rel_res_f46f91340698127b){
addRelation("a", wrapper_rel_a_454ddee488c1ed6b, true, false);
addRelation("$inc_delta_derv_insert_a", wrapper_rel_inc_delta_derv_insert_a_bbea135139bb89ae, false, false);
addRelation("$inc_delta_derv_delete_a", wrapper_rel_inc_delta_derv_delete_a_394b3623655cbdd0, false, false);
addRelation("$inc_delta_tuple_insert_a", wrapper_rel_inc_delta_tuple_insert_a_88895cba4fbff038, false, false);
addRelation("$inc_delta_tuple_delete_a", wrapper_rel_inc_delta_tuple_delete_a_58354ad400e6cd67, false, false);
addRelation("d", wrapper_rel_d_c9260aec44573475, true, false);
addRelation("$inc_delta_derv_insert_d", wrapper_rel_inc_delta_derv_insert_d_e45c88965e47e71d, false, false);
addRelation("$inc_delta_derv_delete_d", wrapper_rel_inc_delta_derv_delete_d_2ad2dc9f2ce026fb, false, false);
addRelation("$inc_delta_tuple_insert_d", wrapper_rel_inc_delta_tuple_insert_d_67f1c476cedabf8e, false, false);
addRelation("$inc_delta_tuple_delete_d", wrapper_rel_inc_delta_tuple_delete_d_c806904ae7af9698, false, false);
addRelation("b", wrapper_rel_b_96694f8e93f5c77d, false, true);
addRelation("$inc_delta_derv_insert_b", wrapper_rel_inc_delta_derv_insert_b_420980c115f01b29, false, false);
addRelation("$inc_delta_derv_delete_b", wrapper_rel_inc_delta_derv_delete_b_ded826b2e75f35bb, false, false);
addRelation("$inc_delta_tuple_insert_b", wrapper_rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46, false, false);
addRelation("$inc_delta_tuple_delete_b", wrapper_rel_inc_delta_tuple_delete_b_1b345b26cae73383, false, false);
addRelation("c", wrapper_rel_c_981811ba2479fc8d, false, true);
addRelation("$inc_delta_derv_insert_c", wrapper_rel_inc_delta_derv_insert_c_0d24c4484987ca3f, false, false);
addRelation("$inc_delta_derv_delete_c", wrapper_rel_inc_delta_derv_delete_c_2409cf566a420e77, false, false);
addRelation("$inc_delta_tuple_insert_c", wrapper_rel_inc_delta_tuple_insert_c_70adcfa1afd70315, false, false);
addRelation("$inc_delta_tuple_delete_c", wrapper_rel_inc_delta_tuple_delete_c_43fa1164ffebfc11, false, false);
addRelation("res", wrapper_rel_res_f46f91340698127b, false, true);
addRelation("$inc_delta_derv_insert_res", wrapper_rel_inc_delta_derv_insert_res_f04df169be2474c0, false, false);
addRelation("$inc_delta_derv_delete_res", wrapper_rel_inc_delta_derv_delete_res_21fda861d986a27e, false, false);
addRelation("$inc_delta_tuple_insert_res", wrapper_rel_inc_delta_tuple_insert_res_d1f9f96e9dad9c4b, false, false);
addRelation("$inc_delta_tuple_delete_res", wrapper_rel_inc_delta_tuple_delete_res_cb088c722dc19c6b, false, false);
}

 Sf_compute::~Sf_compute(){
}

void Sf_compute::runFunction(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){

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
FunctionTimer timer("stratum_a");
 std::vector<RamDomain> args, ret;
stratum_a_f1f7b26a4b108ce3.run(args, ret);
}
{
FunctionTimer timer("stratum_d");
 std::vector<RamDomain> args, ret;
stratum_d_f581825f3af32bbd.run(args, ret);
}
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
FunctionTimer timer("stratum_res");
 std::vector<RamDomain> args, ret;
stratum_res_889015e26521459c.run(args, ret);
}

// -- relation hint statistics --
signalHandler->reset();
}

void Sf_compute::runFunctionInc(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){

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
FunctionTimer timer("stratum_inc_table_update");
 std::vector<RamDomain> args, ret;
stratum_inc_table_update_da18684048fa1d45.run(args, ret);
}
{
FunctionTimer timer("stratum_a_inc");
 std::vector<RamDomain> args, ret;
stratum_a_inc_a6264e86fb843b31.run(args, ret);
}
{
FunctionTimer timer("stratum_d_inc");
 std::vector<RamDomain> args, ret;
stratum_d_inc_3477ccbc77b30aeb.run(args, ret);
}
{
FunctionTimer timer("stratum_b_inc");
 std::vector<RamDomain> args, ret;
stratum_b_inc_5bd3860cc23b7de3.run(args, ret);
}
{
FunctionTimer timer("stratum_c_inc");
 std::vector<RamDomain> args, ret;
stratum_c_inc_3a2e56b5626fb27c.run(args, ret);
}
{
FunctionTimer timer("stratum_res_inc");
 std::vector<RamDomain> args, ret;
stratum_res_inc_6a3962e77bf04edf.run(args, ret);
}

// -- relation hint statistics --
signalHandler->reset();
}

void Sf_compute::run(){
runFunction("", "", false, false);
}

void Sf_compute::runInc(){
runFunctionInc("", "", false, false);
}

void Sf_compute::runAll(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){
runFunction(inputDirectoryArg, outputDirectoryArg, performIOArg, pruneImdtRelsArg);
}

void Sf_compute::runAllInc(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){
runFunctionInc(inputDirectoryArg, outputDirectoryArg, performIOArg, pruneImdtRelsArg);
}

void Sf_compute::printAll([[maybe_unused]] std::string outputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","b"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_b_96694f8e93f5c77d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","c"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_c_981811ba2479fc8d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","res"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_res_f46f91340698127b);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","b"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_b_96694f8e93f5c77d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","c"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_c_981811ba2479fc8d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","res"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_res_f46f91340698127b);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

void Sf_compute::loadAll([[maybe_unused]] std::string inputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X"},{"auxArity","0"},{"cache","true"},{"fact-dir","./input"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","a"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"X\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_a_454ddee488c1ed6b);
} catch (std::exception& e) {std::cerr << "Error loading a data: " << e.what() << '\n';
exit(1);
}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","X\tY"},{"auxArity","0"},{"cache","true"},{"fact-dir","./input"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","d"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"X\", \"Y\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_d_c9260aec44573475);
} catch (std::exception& e) {std::cerr << "Error loading d data: " << e.what() << '\n';
exit(1);
}
}

void Sf_compute::dumpInputs(){
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "a";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_a_454ddee488c1ed6b);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "d";
rwOperation["types"] = "{\"relation\": {\"arity\": 2, \"auxArity\": 0, \"types\": [\"i:number\", \"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_d_c9260aec44573475);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

void Sf_compute::dumpOutputs(){
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "b";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_b_96694f8e93f5c77d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "c";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_c_981811ba2479fc8d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "res";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_res_f46f91340698127b);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "b";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_b_96694f8e93f5c77d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "c";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_c_981811ba2479fc8d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "res";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_res_f46f91340698127b);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

SymbolTable& Sf_compute::getSymbolTable(){
return symTable;
}

RecordTable& Sf_compute::getRecordTable(){
return recordTable;
}

void Sf_compute::setNumThreads(std::size_t numThreadsValue){
SouffleProgram::setNumThreads(numThreadsValue);
symTable.setNumLanes(getNumThreads());
recordTable.setNumLanes(getNumThreads());
regexCache.setNumLanes(getNumThreads());
}

void Sf_compute::executeSubroutine(std::string name,const std::vector<RamDomain>& args,std::vector<RamDomain>& ret){
if (name == "a") {
stratum_a_f1f7b26a4b108ce3.run(args, ret);
return;}
if (name == "a_inc") {
stratum_a_inc_a6264e86fb843b31.run(args, ret);
return;}
if (name == "b") {
stratum_b_43b73774b68513f6.run(args, ret);
return;}
if (name == "b_inc") {
stratum_b_inc_5bd3860cc23b7de3.run(args, ret);
return;}
if (name == "c") {
stratum_c_b5ed19a85a4e3095.run(args, ret);
return;}
if (name == "c_inc") {
stratum_c_inc_3a2e56b5626fb27c.run(args, ret);
return;}
if (name == "d") {
stratum_d_f581825f3af32bbd.run(args, ret);
return;}
if (name == "d_inc") {
stratum_d_inc_3477ccbc77b30aeb.run(args, ret);
return;}
if (name == "inc_table_update") {
stratum_inc_table_update_da18684048fa1d45.run(args, ret);
return;}
if (name == "res") {
stratum_res_889015e26521459c.run(args, ret);
return;}
if (name == "res_inc") {
stratum_res_inc_6a3962e77bf04edf.run(args, ret);
return;}
fatal(("unknown subroutine " + name).c_str());
}

} // namespace  souffle
std::vector<Evidence> evidences = {
    Evidence(UntypedTuple{"a", {1}}, true),
};
namespace souffle {
SouffleProgram *newInstance_compute(){return new  souffle::Sf_compute;}
SymbolTable *getST_compute(SouffleProgram *p){return &reinterpret_cast<souffle::Sf_compute*>(p)->getSymbolTable();}
} // namespace souffle

#ifndef __EMBEDDED_SOUFFLE__
#include "souffle/CompiledOptions.h"
int main(int argc, char** argv)
{
try{
souffle::CmdOptions opt(R"(test.dl)",
R"()",
R"()",
false,
R"()",
1);
if (!opt.parse(argc,argv)) return 1;
souffle::Sf_compute obj;
if (opt.getKnowledgeRepresentation() == "bdd") {
obj.setKnowledge(souffle::Knowledge::BDD);
} else if (opt.getKnowledgeRepresentation() == "sdd") {
obj.setKnowledge(souffle::Knowledge::SDD);
} else { std::cout << "opt.getKnowledgeRepresentation()" << opt.getKnowledgeRepresentation() << std::endl;
 assert(false && "unknown knowledge representation"); }
#if defined(_OPENMP) 
obj.setNumThreads(opt.getNumJobs());

#endif
obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());
try {
std::unordered_map<UntypedTuple, double> fact_prob;
{
FunctionTimer timer(" reading fact probability ");
{
std::string rel = "a";
std::ifstream factFile("input/" + rel + ".facts");std::ifstream probFile("input/" + rel + ".prob");std::string factLine, probLine;while (std::getline(factFile, factLine) && std::getline(probFile, probLine)) {std::istringstream fs(factLine);std::istringstream ps(probLine);double prob; ps >> prob;
assert (prob >= 0 && prob <= 1);
souffle::RamDomain field;
std::vector<souffle::RamDomain> fields;
while (fs >> field) {fields.push_back(field);}
UntypedTuple tuple{rel, fields};
fact_prob[tuple] = prob;
}
}
{
std::string rel = "d";
std::ifstream factFile("input/" + rel + ".facts");std::ifstream probFile("input/" + rel + ".prob");std::string factLine, probLine;while (std::getline(factFile, factLine) && std::getline(probFile, probLine)) {std::istringstream fs(factLine);std::istringstream ps(probLine);double prob; ps >> prob;
assert (prob >= 0 && prob <= 1);
souffle::RamDomain field;
std::vector<souffle::RamDomain> fields;
while (fs >> field) {fields.push_back(field);}
UntypedTuple tuple{rel, fields};
fact_prob[tuple] = prob;
}
}
}
const Atom rule1_head = Atom{"b", {SymbolicField::makeVariable("X"), }};
const Atom atom_1_1 = Atom{"a", {SymbolicField::makeVariable("X"), }};
const Rule rule1 = Rule(1,rule1_head, {atom_1_1}, {"X"}, 0.500000, 0, 0);
const Atom rule2_head = Atom{"c", {SymbolicField::makeVariable("X"), }};
const Atom atom_2_1 = Atom{"b", {SymbolicField::makeVariable("X"), }};
const Atom atom_2_2 = Atom{"d", {SymbolicField::makeVariable("X"), SymbolicField::makeVariable("Y"), }};
const Rule rule2 = Rule(2,rule2_head, {atom_2_1, atom_2_2}, {"X", "Y"}, 0.200000, 0, 0);
const Atom rule3_head = Atom{"res", {SymbolicField::makeVariable("X"), }};
const Atom atom_3_1 = Atom{"b", {SymbolicField::makeVariable("X"), }};
const Atom atom_3_2 = Atom{"c", {SymbolicField::makeVariable("X"), }};
const Rule rule3 = Rule(3,rule3_head, {atom_3_1, atom_3_2}, {"X"}, 0.900000, 0, 0);
ruleManager = RuleManager({rule1, rule2, rule3});
std::cout << ruleManager.toString();
std::cout << std::fixed << std::setprecision(10);
auto graph = IncrementalDerivationGraph::createFrom(DerivationManager::untypedTuple2RuleApplications, ruleManager, fact_prob, evidences);
graph->dumpStatistics(std::cout);
graph->dumpDot("before_prune.dot");

auto view = graph->prune(obj.getOutputRelations());

// Force set probability of evidence nodes to 1.0
for (const auto& node : graph->getNodes()) {
    if (node->hasEvidence()) {
        node->setProbability(1.0);
        std::cout << "Set probability=1 for evidence node " << node->getTuple().toString() << std::endl;
    }
}

view.dumpDot("after_prune.dot");
view.dumpStatisticsInc(std::cout);
view.dumpDot("after_prune.dot");

view.dumpStatisticsInc(std::cout);
if (obj.getKnowledge() == souffle::Knowledge::BDD) {
std::map<NodePtr, BddNodeRef> nodeFormulas;std::map<EdgePtr, BddNodeRef> edgeFormulas;WeightedBDDManager bddManager;
{

FunctionTimer timer(" building formulas ");
buildFormulasCyclewise(view, bddManager, nodeFormulas, edgeFormulas);
}
{
FunctionTimer timer(" wmc and output probability ");
std::cout << "nodeFormulas size: " << nodeFormulas.size() << std::flush;
std::cout << "edgeFormulas size: " << edgeFormulas.size() << std::flush;
for (const auto& [node, bdd] : nodeFormulas) {
//    std::cout << "Node" << node->getId() ;
//    std::cout << "Node" << node->getId() << " " << node->getTuple().toString() << ": ";
//    std::cout << bddManager.toString(bdd) << "\t";
    auto prob = bddManager.computeWeightedModelCount(bdd);
    probResult[node] = prob;
//    std::cout << "Probability: " << prob << std::endl;
}
dumpProbabilities(probResult, "./output");
IncrementalCLI cli(&obj, graph, &ruleManager, &bddManager, &nodeFormulas, &edgeFormulas);
cli.run();
}

}

else if (obj.getKnowledge() == souffle::Knowledge::SDD) {
std::map<NodePtr, SddNodeRef> nodeFormulas;std::map<EdgePtr, SddNodeRef> edgeFormulas;SddFormulaManager sddManager(view.getNodes().size() + view.getEdges().size());
{

FunctionTimer timer(" building formulas ");
buildFormulasCyclewise(view, sddManager, nodeFormulas, edgeFormulas);
}
{
FunctionTimer timer(" wmc and output probability ");
std::cout << "nodeFormulas size: " << nodeFormulas.size() << std::flush;
std::cout << "edgeFormulas size: " << edgeFormulas.size() << std::flush;
for (const auto& [node, sdd] : nodeFormulas) {
//    std::cout << "Node" << node->getId() ;
//    std::cout << "Node" << node->getId() << " " << node->getTuple().toString() << ": ";
//    std::cout << sddManager.toString(sdd) << "\t";
    auto prob = sddManager.computeWeightedModelCount(sdd);
    probResult[node] = prob;
//    std::cout << "Probability: " << prob << std::endl;
}
dumpProbabilities(probResult, "./output");
IncrementalCLI cli(&obj, graph, &ruleManager, &sddManager, &nodeFormulas, &edgeFormulas);
cli.run();
}

}
std::cout << "Done" << std::endl;
} catch (std::exception& e) {std::cerr << "Problog colc failed" << e.what() << std::endl;}
return 0;
} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}
}
#endif

namespace  souffle {
using namespace souffle;
class factory_Sf_compute: souffle::ProgramFactory {
public:
souffle::SouffleProgram* newInstance();
 factory_Sf_compute();
private:
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
souffle::SouffleProgram* factory_Sf_compute::newInstance(){
return new  souffle::Sf_compute();
}

 factory_Sf_compute::factory_Sf_compute():
souffle::ProgramFactory("compute"){
}

} // namespace  souffle
namespace souffle {

#ifdef __EMBEDDED_SOUFFLE__
extern "C" {
souffle::factory_Sf_compute __factory_Sf_compute_instance;
}
#endif
} // namespace souffle

