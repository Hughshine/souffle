#define SOUFFLE_GENERATOR_VERSION "2.4.1-54-g8b2657a77"
#include "souffle/CompiledSouffle.h"
#include "souffle/SignalHandler.h"
#include "souffle/SouffleInterface.h"
#include "souffle/datastructure/BTree.h"
#include "souffle/datastructure/Info.h"
#include "souffle/io/IOSystem.h"
#include "souffle/provenance/Explain.h"
#include "souffle/utility/MiscUtil.h"
#include <any>
#include <mutex>
namespace functors {
extern "C" {
}
} //namespace functors
namespace souffle::t_btree_011_iiii__0_1_3_2__1000__1111__1100 {
using namespace souffle;
struct Type {
static constexpr Relation::arity_type Arity = 4;
using t_tuple = Tuple<RamDomain, 4>;
struct updater {
bool update(t_tuple& old_t, const t_tuple& new_t) {
bool changed = false;
if (ramBitCast<RamSigned>(new_t[3]) < ramBitCast<RamSigned>(old_t[3]) || (ramBitCast<RamSigned>(new_t[3]) == ramBitCast<RamSigned>(old_t[3]) && ramBitCast<RamSigned>(new_t[2]) < ramBitCast<RamSigned>(old_t[2]))) {
    old_t[2] = new_t[2];
    old_t[3] = new_t[3];
    changed = true;
}
return changed;
}
};
struct t_comparator_0{
 int operator()(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0])) ? -1 : (ramBitCast<RamSigned>(a[0]) > ramBitCast<RamSigned>(b[0])) ? 1 :((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1])) ? -1 : (ramBitCast<RamSigned>(a[1]) > ramBitCast<RamSigned>(b[1])) ? 1 :((ramBitCast<RamSigned>(a[3]) < ramBitCast<RamSigned>(b[3])) ? -1 : (ramBitCast<RamSigned>(a[3]) > ramBitCast<RamSigned>(b[3])) ? 1 :((ramBitCast<RamSigned>(a[2]) < ramBitCast<RamSigned>(b[2])) ? -1 : (ramBitCast<RamSigned>(a[2]) > ramBitCast<RamSigned>(b[2])) ? 1 :(0))));
 }
bool less(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0]))|| ((ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0])) && ((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1]))|| ((ramBitCast<RamSigned>(a[1]) == ramBitCast<RamSigned>(b[1])) && ((ramBitCast<RamSigned>(a[3]) < ramBitCast<RamSigned>(b[3]))|| ((ramBitCast<RamSigned>(a[3]) == ramBitCast<RamSigned>(b[3])) && ((ramBitCast<RamSigned>(a[2]) < ramBitCast<RamSigned>(b[2]))))))));
 }
bool equal(const t_tuple& a, const t_tuple& b) const {
return (ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0]))&&(ramBitCast<RamSigned>(a[1]) == ramBitCast<RamSigned>(b[1]))&&(ramBitCast<RamSigned>(a[3]) == ramBitCast<RamSigned>(b[3]))&&(ramBitCast<RamSigned>(a[2]) == ramBitCast<RamSigned>(b[2]));
 }
};
struct t_comparator_0_aux{
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
using t_ind_0 = btree_set<t_tuple,t_comparator_0,std::allocator<t_tuple>,256,typename souffle::detail::default_strategy<t_tuple>::type,t_comparator_0_aux,updater>;
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
bool insert(RamDomain a0,RamDomain a1,RamDomain a2,RamDomain a3);
bool contains(const t_tuple& t, context& h) const;
bool contains(const t_tuple& t) const;
std::size_t size() const;
iterator find(const t_tuple& t, context& h) const;
iterator find(const t_tuple& t) const;
range<iterator> lowerUpperRange_0000(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;
range<iterator> lowerUpperRange_0000(const t_tuple& /* lower */, const t_tuple& /* upper */) const;
range<t_ind_0::iterator> lowerUpperRange_1000(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_1000(const t_tuple& lower, const t_tuple& upper) const;
range<t_ind_0::iterator> lowerUpperRange_1111(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_1111(const t_tuple& lower, const t_tuple& upper) const;
range<t_ind_0::iterator> lowerUpperRange_1100(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_1100(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_011_iiii__0_1_3_2__1000__1111__1100 
namespace souffle::t_btree_011_iiii__0_1_3_2__1000__1111__1100 {
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
RamDomain data[4];
std::copy(ramDomain, ramDomain + 4, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool Type::insert(RamDomain a0,RamDomain a1,RamDomain a2,RamDomain a3) {
RamDomain data[4] = {a0,a1,a2,a3};
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
range<iterator> Type::lowerUpperRange_0000(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> Type::lowerUpperRange_0000(const t_tuple& /* lower */, const t_tuple& /* upper */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> Type::lowerUpperRange_1000(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_1000(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_1000(lower,upper,h);
}
range<t_ind_0::iterator> Type::lowerUpperRange_1111(const t_tuple& lower, const t_tuple& upper, context& h) const {
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
range<t_ind_0::iterator> Type::lowerUpperRange_1111(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_1111(lower,upper,h);
}
range<t_ind_0::iterator> Type::lowerUpperRange_1100(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_1100(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_1100(lower,upper,h);
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
o << " arity 4 direct b-tree index 0 lex-order [0,1,3,2]\n";
ind_0.printStats(o);
}
} // namespace souffle::t_btree_011_iiii__0_1_3_2__1000__1111__1100 
namespace souffle::t_btree_011_iii__0_2_1__100__111 {
using namespace souffle;
struct Type {
static constexpr Relation::arity_type Arity = 3;
using t_tuple = Tuple<RamDomain, 3>;
struct updater {
bool update(t_tuple& old_t, const t_tuple& new_t) {
bool changed = false;
if (ramBitCast<RamSigned>(new_t[2]) < ramBitCast<RamSigned>(old_t[2]) || (ramBitCast<RamSigned>(new_t[2]) == ramBitCast<RamSigned>(old_t[2]) && ramBitCast<RamSigned>(new_t[1]) < ramBitCast<RamSigned>(old_t[1]))) {
    old_t[1] = new_t[1];
    old_t[2] = new_t[2];
    changed = true;
}
return changed;
}
};
struct t_comparator_0{
 int operator()(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0])) ? -1 : (ramBitCast<RamSigned>(a[0]) > ramBitCast<RamSigned>(b[0])) ? 1 :((ramBitCast<RamSigned>(a[2]) < ramBitCast<RamSigned>(b[2])) ? -1 : (ramBitCast<RamSigned>(a[2]) > ramBitCast<RamSigned>(b[2])) ? 1 :((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1])) ? -1 : (ramBitCast<RamSigned>(a[1]) > ramBitCast<RamSigned>(b[1])) ? 1 :(0)));
 }
bool less(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0]))|| ((ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0])) && ((ramBitCast<RamSigned>(a[2]) < ramBitCast<RamSigned>(b[2]))|| ((ramBitCast<RamSigned>(a[2]) == ramBitCast<RamSigned>(b[2])) && ((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1]))))));
 }
bool equal(const t_tuple& a, const t_tuple& b) const {
return (ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0]))&&(ramBitCast<RamSigned>(a[2]) == ramBitCast<RamSigned>(b[2]))&&(ramBitCast<RamSigned>(a[1]) == ramBitCast<RamSigned>(b[1]));
 }
};
struct t_comparator_0_aux{
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
using t_ind_0 = btree_set<t_tuple,t_comparator_0,std::allocator<t_tuple>,256,typename souffle::detail::default_strategy<t_tuple>::type,t_comparator_0_aux,updater>;
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
bool insert(RamDomain a0,RamDomain a1,RamDomain a2);
bool contains(const t_tuple& t, context& h) const;
bool contains(const t_tuple& t) const;
std::size_t size() const;
iterator find(const t_tuple& t, context& h) const;
iterator find(const t_tuple& t) const;
range<iterator> lowerUpperRange_000(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;
range<iterator> lowerUpperRange_000(const t_tuple& /* lower */, const t_tuple& /* upper */) const;
range<t_ind_0::iterator> lowerUpperRange_100(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_100(const t_tuple& lower, const t_tuple& upper) const;
range<t_ind_0::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_011_iii__0_2_1__100__111 
namespace souffle::t_btree_011_iii__0_2_1__100__111 {
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
RamDomain data[3];
std::copy(ramDomain, ramDomain + 3, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool Type::insert(RamDomain a0,RamDomain a1,RamDomain a2) {
RamDomain data[3] = {a0,a1,a2};
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
range<iterator> Type::lowerUpperRange_000(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> Type::lowerUpperRange_000(const t_tuple& /* lower */, const t_tuple& /* upper */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> Type::lowerUpperRange_100(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_100(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_100(lower,upper,h);
}
range<t_ind_0::iterator> Type::lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper, context& h) const {
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
range<t_ind_0::iterator> Type::lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_111(lower,upper,h);
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
o << " arity 3 direct b-tree index 0 lex-order [0,2,1]\n";
ind_0.printStats(o);
}
} // namespace souffle::t_btree_011_iii__0_2_1__100__111 
namespace souffle::t_btree_011_iii__0_2_1__111 {
using namespace souffle;
struct Type {
static constexpr Relation::arity_type Arity = 3;
using t_tuple = Tuple<RamDomain, 3>;
struct updater {
bool update(t_tuple& old_t, const t_tuple& new_t) {
bool changed = false;
if (ramBitCast<RamSigned>(new_t[2]) < ramBitCast<RamSigned>(old_t[2]) || (ramBitCast<RamSigned>(new_t[2]) == ramBitCast<RamSigned>(old_t[2]) && ramBitCast<RamSigned>(new_t[1]) < ramBitCast<RamSigned>(old_t[1]))) {
    old_t[1] = new_t[1];
    old_t[2] = new_t[2];
    changed = true;
}
return changed;
}
};
struct t_comparator_0{
 int operator()(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0])) ? -1 : (ramBitCast<RamSigned>(a[0]) > ramBitCast<RamSigned>(b[0])) ? 1 :((ramBitCast<RamSigned>(a[2]) < ramBitCast<RamSigned>(b[2])) ? -1 : (ramBitCast<RamSigned>(a[2]) > ramBitCast<RamSigned>(b[2])) ? 1 :((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1])) ? -1 : (ramBitCast<RamSigned>(a[1]) > ramBitCast<RamSigned>(b[1])) ? 1 :(0)));
 }
bool less(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0]))|| ((ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0])) && ((ramBitCast<RamSigned>(a[2]) < ramBitCast<RamSigned>(b[2]))|| ((ramBitCast<RamSigned>(a[2]) == ramBitCast<RamSigned>(b[2])) && ((ramBitCast<RamSigned>(a[1]) < ramBitCast<RamSigned>(b[1]))))));
 }
bool equal(const t_tuple& a, const t_tuple& b) const {
return (ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0]))&&(ramBitCast<RamSigned>(a[2]) == ramBitCast<RamSigned>(b[2]))&&(ramBitCast<RamSigned>(a[1]) == ramBitCast<RamSigned>(b[1]));
 }
};
struct t_comparator_0_aux{
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
using t_ind_0 = btree_set<t_tuple,t_comparator_0,std::allocator<t_tuple>,256,typename souffle::detail::default_strategy<t_tuple>::type,t_comparator_0_aux,updater>;
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
bool insert(RamDomain a0,RamDomain a1,RamDomain a2);
bool contains(const t_tuple& t, context& h) const;
bool contains(const t_tuple& t) const;
std::size_t size() const;
iterator find(const t_tuple& t, context& h) const;
iterator find(const t_tuple& t) const;
range<iterator> lowerUpperRange_000(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;
range<iterator> lowerUpperRange_000(const t_tuple& /* lower */, const t_tuple& /* upper */) const;
range<t_ind_0::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_011_iii__0_2_1__111 
namespace souffle::t_btree_011_iii__0_2_1__111 {
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
RamDomain data[3];
std::copy(ramDomain, ramDomain + 3, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool Type::insert(RamDomain a0,RamDomain a1,RamDomain a2) {
RamDomain data[3] = {a0,a1,a2};
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
range<iterator> Type::lowerUpperRange_000(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> Type::lowerUpperRange_000(const t_tuple& /* lower */, const t_tuple& /* upper */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> Type::lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper, context& h) const {
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
range<t_ind_0::iterator> Type::lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_111(lower,upper,h);
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
o << " arity 3 direct b-tree index 0 lex-order [0,2,1]\n";
ind_0.printStats(o);
}
} // namespace souffle::t_btree_011_iii__0_2_1__111 
namespace  souffle {
using namespace souffle;
class Stratum_3_59fd9dde26237834 {
public:
 Stratum_3_59fd9dde26237834(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_info<5>& rel_A_info_1_952fc2026eec70f1);
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
t_info<5>* rel_A_info_1_952fc2026eec70f1;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_3_59fd9dde26237834::Stratum_3_59fd9dde26237834(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_info<5>& rel_A_info_1_952fc2026eec70f1):
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
rel_A_info_1_952fc2026eec70f1(&rel_A_info_1_952fc2026eec70f1){
}

void Stratum_3_59fd9dde26237834::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(@info.clause[A(x) :- 
   B(x,y),
   !C(y).]
in file example.dl [10:1-10:24])_");
[&](){
CREATE_OP_CONTEXT(rel_A_info_1_952fc2026eec70f1_op_ctxt,rel_A_info_1_952fc2026eec70f1->createContext());
Tuple<RamDomain,5> tuple{{ramBitCast(RamSigned(1)),ramBitCast(RamSigned(0)),ramBitCast(RamSigned(1)),ramBitCast(RamSigned(2)),ramBitCast(RamSigned(3))}};
rel_A_info_1_952fc2026eec70f1->insert(tuple,READ_OP_CONTEXT(rel_A_info_1_952fc2026eec70f1_op_ctxt));
}
();}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_A_4f7df7f5f9efad6a {
public:
 Stratum_A_4f7df7f5f9efad6a(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_011_iii__0_2_1__111::Type& rel_A_600668de4345e18e,t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type& rel_B_3a4882db196b99ec,t_btree_011_iii__0_2_1__100__111::Type& rel_C_ac888c4d34093402);
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
t_btree_011_iii__0_2_1__111::Type* rel_A_600668de4345e18e;
t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type* rel_B_3a4882db196b99ec;
t_btree_011_iii__0_2_1__100__111::Type* rel_C_ac888c4d34093402;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_A_4f7df7f5f9efad6a::Stratum_A_4f7df7f5f9efad6a(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_011_iii__0_2_1__111::Type& rel_A_600668de4345e18e,t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type& rel_B_3a4882db196b99ec,t_btree_011_iii__0_2_1__100__111::Type& rel_C_ac888c4d34093402):
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
rel_A_600668de4345e18e(&rel_A_600668de4345e18e),
rel_B_3a4882db196b99ec(&rel_B_3a4882db196b99ec),
rel_C_ac888c4d34093402(&rel_C_ac888c4d34093402){
}

void Stratum_A_4f7df7f5f9efad6a::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(A(x) :- 
   B(x,y),
   !C(y).
in file example.dl [10:1-10:24])_");
if(!(rel_B_3a4882db196b99ec->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_A_600668de4345e18e_op_ctxt,rel_A_600668de4345e18e->createContext());
CREATE_OP_CONTEXT(rel_B_3a4882db196b99ec_op_ctxt,rel_B_3a4882db196b99ec->createContext());
CREATE_OP_CONTEXT(rel_C_ac888c4d34093402_op_ctxt,rel_C_ac888c4d34093402->createContext());
for(const auto& env0 : *rel_B_3a4882db196b99ec) {
if( !(!rel_C_ac888c4d34093402->lowerUpperRange_100(Tuple<RamDomain,3>{{ramBitCast(env0[1]), ramBitCast<RamDomain>(MIN_RAM_SIGNED), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,3>{{ramBitCast(env0[1]), ramBitCast<RamDomain>(MAX_RAM_SIGNED), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_C_ac888c4d34093402_op_ctxt)).empty())) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(RamSigned(1)),ramBitCast((ramBitCast<RamSigned>(env0[3]) + ramBitCast<RamSigned>(RamSigned(1))))}};
rel_A_600668de4345e18e->insert(tuple,READ_OP_CONTEXT(rel_A_600668de4345e18e_op_ctxt));
}
}
}
();}
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","2"},{"name","A"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_A_600668de4345e18e);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_A_1_negation_subproof_77cad0df0eb16ed9 {
public:
 Stratum_A_1_negation_subproof_77cad0df0eb16ed9(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type& rel_B_3a4882db196b99ec,t_btree_011_iii__0_2_1__100__111::Type& rel_C_ac888c4d34093402);
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
t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type* rel_B_3a4882db196b99ec;
t_btree_011_iii__0_2_1__100__111::Type* rel_C_ac888c4d34093402;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_A_1_negation_subproof_77cad0df0eb16ed9::Stratum_A_1_negation_subproof_77cad0df0eb16ed9(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type& rel_B_3a4882db196b99ec,t_btree_011_iii__0_2_1__100__111::Type& rel_C_ac888c4d34093402):
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
rel_B_3a4882db196b99ec(&rel_B_3a4882db196b99ec),
rel_C_ac888c4d34093402(&rel_C_ac888c4d34093402){
}

void Stratum_A_1_negation_subproof_77cad0df0eb16ed9::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
std::mutex lock;
[&](){
CREATE_OP_CONTEXT(rel_B_3a4882db196b99ec_op_ctxt,rel_B_3a4882db196b99ec->createContext());
if(!rel_B_3a4882db196b99ec->lowerUpperRange_1100(Tuple<RamDomain,4>{{ramBitCast((args)[0]), ramBitCast((args)[1]), ramBitCast<RamDomain>(MIN_RAM_SIGNED), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,4>{{ramBitCast((args)[0]), ramBitCast((args)[1]), ramBitCast<RamDomain>(MAX_RAM_SIGNED), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_B_3a4882db196b99ec_op_ctxt)).empty()) {
std::lock_guard<std::mutex> guard(lock);
ret.push_back(RamSigned(1));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_B_3a4882db196b99ec_op_ctxt,rel_B_3a4882db196b99ec->createContext());
if(!(!rel_B_3a4882db196b99ec->lowerUpperRange_1100(Tuple<RamDomain,4>{{ramBitCast((args)[0]), ramBitCast((args)[1]), ramBitCast<RamDomain>(MIN_RAM_SIGNED), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,4>{{ramBitCast((args)[0]), ramBitCast((args)[1]), ramBitCast<RamDomain>(MAX_RAM_SIGNED), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_B_3a4882db196b99ec_op_ctxt)).empty())) {
std::lock_guard<std::mutex> guard(lock);
ret.push_back(RamSigned(0));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_C_ac888c4d34093402_op_ctxt,rel_C_ac888c4d34093402->createContext());
if(!rel_C_ac888c4d34093402->lowerUpperRange_100(Tuple<RamDomain,3>{{ramBitCast((args)[1]), ramBitCast<RamDomain>(MIN_RAM_SIGNED), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,3>{{ramBitCast((args)[1]), ramBitCast<RamDomain>(MAX_RAM_SIGNED), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_C_ac888c4d34093402_op_ctxt)).empty()) {
std::lock_guard<std::mutex> guard(lock);
ret.push_back(RamSigned(0));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_C_ac888c4d34093402_op_ctxt,rel_C_ac888c4d34093402->createContext());
if(!(!rel_C_ac888c4d34093402->lowerUpperRange_100(Tuple<RamDomain,3>{{ramBitCast((args)[1]), ramBitCast<RamDomain>(MIN_RAM_SIGNED), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,3>{{ramBitCast((args)[1]), ramBitCast<RamDomain>(MAX_RAM_SIGNED), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_C_ac888c4d34093402_op_ctxt)).empty())) {
std::lock_guard<std::mutex> guard(lock);
ret.push_back(RamSigned(1));
}
}
();}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_A_1_subproof_8ed426fae997606c {
public:
 Stratum_A_1_subproof_8ed426fae997606c(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type& rel_B_3a4882db196b99ec,t_btree_011_iii__0_2_1__100__111::Type& rel_C_ac888c4d34093402);
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
t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type* rel_B_3a4882db196b99ec;
t_btree_011_iii__0_2_1__100__111::Type* rel_C_ac888c4d34093402;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_A_1_subproof_8ed426fae997606c::Stratum_A_1_subproof_8ed426fae997606c(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type& rel_B_3a4882db196b99ec,t_btree_011_iii__0_2_1__100__111::Type& rel_C_ac888c4d34093402):
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
rel_B_3a4882db196b99ec(&rel_B_3a4882db196b99ec),
rel_C_ac888c4d34093402(&rel_C_ac888c4d34093402){
}

void Stratum_A_1_subproof_8ed426fae997606c::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
std::mutex lock;
if(!(rel_B_3a4882db196b99ec->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_B_3a4882db196b99ec_op_ctxt,rel_B_3a4882db196b99ec->createContext());
CREATE_OP_CONTEXT(rel_C_ac888c4d34093402_op_ctxt,rel_C_ac888c4d34093402->createContext());
auto range = rel_B_3a4882db196b99ec->lowerUpperRange_1000(Tuple<RamDomain,4>{{ramBitCast((args)[0]), ramBitCast<RamDomain>(MIN_RAM_SIGNED), ramBitCast<RamDomain>(MIN_RAM_SIGNED), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,4>{{ramBitCast((args)[0]), ramBitCast<RamDomain>(MAX_RAM_SIGNED), ramBitCast<RamDomain>(MAX_RAM_SIGNED), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_B_3a4882db196b99ec_op_ctxt));
for(const auto& env0 : range) {
if( (ramBitCast<RamSigned>(env0[3]) <= ramBitCast<RamSigned>((args)[1])) && (ramBitCast<RamDomain>(env0[3]) != ramBitCast<RamDomain>((args)[1])) && !(!rel_C_ac888c4d34093402->lowerUpperRange_100(Tuple<RamDomain,3>{{ramBitCast(env0[1]), ramBitCast<RamDomain>(MIN_RAM_SIGNED), ramBitCast<RamDomain>(MIN_RAM_SIGNED)}},Tuple<RamDomain,3>{{ramBitCast(env0[1]), ramBitCast<RamDomain>(MAX_RAM_SIGNED), ramBitCast<RamDomain>(MAX_RAM_SIGNED)}},READ_OP_CONTEXT(rel_C_ac888c4d34093402_op_ctxt)).empty())) {
std::lock_guard<std::mutex> guard(lock);
ret.push_back(env0[0]);
ret.push_back(env0[1]);
ret.push_back(env0[2]);
ret.push_back(env0[3]);
ret.push_back(env0[1]);
ret.push_back(0);
ret.push_back(0);
ret.push_back(env0[0]);
ret.push_back((args)[0]);
ret.push_back(env0[3]);
ret.push_back((args)[1]);
}
}
}
();}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_B_1f366d600ff54502 {
public:
 Stratum_B_1f366d600ff54502(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type& rel_B_3a4882db196b99ec);
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
t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type* rel_B_3a4882db196b99ec;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_B_1f366d600ff54502::Stratum_B_1f366d600ff54502(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type& rel_B_3a4882db196b99ec):
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
rel_B_3a4882db196b99ec(&rel_B_3a4882db196b99ec){
}

void Stratum_B_1f366d600ff54502::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x\ty"},{"auxArity","2"},{"fact-dir","./facts"},{"name","B"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"x\", \"y\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
if (!inputDirectory.empty()) {directiveMap["fact-dir"] = inputDirectory;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_B_3a4882db196b99ec);
} catch (std::exception& e) {std::cerr << "Error loading B data: " << e.what() << '\n';
exit(1);
}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_C_c9c181582fee6bd1 {
public:
 Stratum_C_c9c181582fee6bd1(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_011_iii__0_2_1__100__111::Type& rel_C_ac888c4d34093402);
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
t_btree_011_iii__0_2_1__100__111::Type* rel_C_ac888c4d34093402;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_C_c9c181582fee6bd1::Stratum_C_c9c181582fee6bd1(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_011_iii__0_2_1__100__111::Type& rel_C_ac888c4d34093402):
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
rel_C_ac888c4d34093402(&rel_C_ac888c4d34093402){
}

void Stratum_C_c9c181582fee6bd1::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","2"},{"fact-dir","./facts"},{"name","C"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!inputDirectory.empty()) {directiveMap["fact-dir"] = inputDirectory;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_C_ac888c4d34093402);
} catch (std::exception& e) {std::cerr << "Error loading C data: " << e.what() << '\n';
exit(1);
}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Sf_example_np: public SouffleProgram {
public:
 Sf_example_np();
 ~Sf_example_np();
void run();
void runAll(std::string inputDirectoryArg = "",std::string outputDirectoryArg = "",bool performIOArg = true,bool pruneImdtRelsArg = true);
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
SymbolTableImpl symTable;
SpecializedRecordTable<0> recordTable;
ConcurrentCache<std::string,std::regex> regexCache;
Own<t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type> rel_B_3a4882db196b99ec;
souffle::RelationWrapper<t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type> wrapper_rel_B_3a4882db196b99ec;
Own<t_btree_011_iii__0_2_1__100__111::Type> rel_C_ac888c4d34093402;
souffle::RelationWrapper<t_btree_011_iii__0_2_1__100__111::Type> wrapper_rel_C_ac888c4d34093402;
Own<t_btree_011_iii__0_2_1__111::Type> rel_A_600668de4345e18e;
souffle::RelationWrapper<t_btree_011_iii__0_2_1__111::Type> wrapper_rel_A_600668de4345e18e;
Own<t_info<5>> rel_A_info_1_952fc2026eec70f1;
souffle::RelationWrapper<t_info<5>> wrapper_rel_A_info_1_952fc2026eec70f1;
Stratum_3_59fd9dde26237834 stratum_3_6898cfe83383b158;
Stratum_A_4f7df7f5f9efad6a stratum_A_32d0975c99a022af;
Stratum_A_1_negation_subproof_77cad0df0eb16ed9 stratum_A_1_negation_subproof_14546dccd3aadf11;
Stratum_A_1_subproof_8ed426fae997606c stratum_A_1_subproof_3e7c75f6473a44d2;
Stratum_B_1f366d600ff54502 stratum_B_52e2453c9dd5383a;
Stratum_C_c9c181582fee6bd1 stratum_C_10b322c3b2732bab;
std::string inputDirectory;
std::string outputDirectory;
SignalHandler* signalHandler{SignalHandler::instance()};
std::atomic<RamDomain> ctr{};
std::atomic<std::size_t> iter{};
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Sf_example_np::Sf_example_np():
symTable({
	R"_(x)_",
	R"_(B,x,y)_",
	R"_(!C)_",
	R"_(A(x) :- 
   B(x,y),
   !C(y).)_",
}),
recordTable(),
regexCache(),
rel_B_3a4882db196b99ec(mk<t_btree_011_iiii__0_1_3_2__1000__1111__1100::Type>()),
wrapper_rel_B_3a4882db196b99ec(0, *rel_B_3a4882db196b99ec, *this, "B", std::array<const char *,4>{{"i:number","i:number","i:number","i:number"}}, std::array<const char *,4>{{"x","y","@rule_number","@level_number"}}, 2),
rel_C_ac888c4d34093402(mk<t_btree_011_iii__0_2_1__100__111::Type>()),
wrapper_rel_C_ac888c4d34093402(1, *rel_C_ac888c4d34093402, *this, "C", std::array<const char *,3>{{"i:number","i:number","i:number"}}, std::array<const char *,3>{{"x","@rule_number","@level_number"}}, 2),
rel_A_600668de4345e18e(mk<t_btree_011_iii__0_2_1__111::Type>()),
wrapper_rel_A_600668de4345e18e(2, *rel_A_600668de4345e18e, *this, "A", std::array<const char *,3>{{"i:number","i:number","i:number"}}, std::array<const char *,3>{{"x","@rule_number","@level_number"}}, 2),
rel_A_info_1_952fc2026eec70f1(mk<t_info<5>>()),
wrapper_rel_A_info_1_952fc2026eec70f1(3, *rel_A_info_1_952fc2026eec70f1, *this, "A.@info.1", std::array<const char *,5>{{"i:number","s:symbol","s:symbol","s:symbol","s:symbol"}}, std::array<const char *,5>{{"clause_num","head_vars","rel_0","rel_1","clause_repr"}}, 0),
stratum_3_6898cfe83383b158(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_A_info_1_952fc2026eec70f1),
stratum_A_32d0975c99a022af(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_A_600668de4345e18e,*rel_B_3a4882db196b99ec,*rel_C_ac888c4d34093402),
stratum_A_1_negation_subproof_14546dccd3aadf11(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_B_3a4882db196b99ec,*rel_C_ac888c4d34093402),
stratum_A_1_subproof_3e7c75f6473a44d2(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_B_3a4882db196b99ec,*rel_C_ac888c4d34093402),
stratum_B_52e2453c9dd5383a(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_B_3a4882db196b99ec),
stratum_C_10b322c3b2732bab(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_C_ac888c4d34093402){
addRelation("B", wrapper_rel_B_3a4882db196b99ec, true, false);
addRelation("C", wrapper_rel_C_ac888c4d34093402, true, false);
addRelation("A", wrapper_rel_A_600668de4345e18e, false, true);
addRelation("A.@info.1", wrapper_rel_A_info_1_952fc2026eec70f1, false, false);
}

 Sf_example_np::~Sf_example_np(){
}

void Sf_example_np::runFunction(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){

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
 std::vector<RamDomain> args, ret;
stratum_B_52e2453c9dd5383a.run(args, ret);
}
{
 std::vector<RamDomain> args, ret;
stratum_C_10b322c3b2732bab.run(args, ret);
}
{
 std::vector<RamDomain> args, ret;
stratum_A_32d0975c99a022af.run(args, ret);
}
{
 std::vector<RamDomain> args, ret;
stratum_3_6898cfe83383b158.run(args, ret);
}

// -- relation hint statistics --
signalHandler->reset();
}

void Sf_example_np::run(){
runFunction("", "", false, false);
}

void Sf_example_np::runAll(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){
runFunction(inputDirectoryArg, outputDirectoryArg, performIOArg, pruneImdtRelsArg);
}

void Sf_example_np::printAll([[maybe_unused]] std::string outputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","2"},{"name","A"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_A_600668de4345e18e);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

void Sf_example_np::loadAll([[maybe_unused]] std::string inputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x\ty"},{"auxArity","2"},{"fact-dir","./facts"},{"name","B"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"x\", \"y\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_B_3a4882db196b99ec);
} catch (std::exception& e) {std::cerr << "Error loading B data: " << e.what() << '\n';
exit(1);
}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","2"},{"fact-dir","./facts"},{"name","C"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_C_ac888c4d34093402);
} catch (std::exception& e) {std::cerr << "Error loading C data: " << e.what() << '\n';
exit(1);
}
}

void Sf_example_np::dumpInputs(){
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "B";
rwOperation["types"] = "{\"relation\": {\"arity\": 4, \"auxArity\": 0, \"types\": [\"i:number\", \"i:number\", \"i:number\", \"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_B_3a4882db196b99ec);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "C";
rwOperation["types"] = "{\"relation\": {\"arity\": 3, \"auxArity\": 0, \"types\": [\"i:number\", \"i:number\", \"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_C_ac888c4d34093402);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

void Sf_example_np::dumpOutputs(){
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "A";
rwOperation["types"] = "{\"relation\": {\"arity\": 3, \"auxArity\": 0, \"types\": [\"i:number\", \"i:number\", \"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_A_600668de4345e18e);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

SymbolTable& Sf_example_np::getSymbolTable(){
return symTable;
}

RecordTable& Sf_example_np::getRecordTable(){
return recordTable;
}

void Sf_example_np::setNumThreads(std::size_t numThreadsValue){
SouffleProgram::setNumThreads(numThreadsValue);
symTable.setNumLanes(getNumThreads());
recordTable.setNumLanes(getNumThreads());
regexCache.setNumLanes(getNumThreads());
}

void Sf_example_np::executeSubroutine(std::string name,const std::vector<RamDomain>& args,std::vector<RamDomain>& ret){
if (name == "3") {
stratum_3_6898cfe83383b158.run(args, ret);
return;}
if (name == "A") {
stratum_A_32d0975c99a022af.run(args, ret);
return;}
if (name == "A_1_negation_subproof") {
stratum_A_1_negation_subproof_14546dccd3aadf11.run(args, ret);
return;}
if (name == "A_1_subproof") {
stratum_A_1_subproof_3e7c75f6473a44d2.run(args, ret);
return;}
if (name == "B") {
stratum_B_52e2453c9dd5383a.run(args, ret);
return;}
if (name == "C") {
stratum_C_10b322c3b2732bab.run(args, ret);
return;}
fatal(("unknown subroutine " + name).c_str());
}

} // namespace  souffle
namespace souffle {
SouffleProgram *newInstance_example_np(){return new  souffle::Sf_example_np;}
SymbolTable *getST_example_np(SouffleProgram *p){return &reinterpret_cast<souffle::Sf_example_np*>(p)->getSymbolTable();}
} // namespace souffle

#ifndef __EMBEDDED_SOUFFLE__
#include "souffle/CompiledOptions.h"
int main(int argc, char** argv)
{
try{
souffle::CmdOptions opt(R"(example.dl)",
R"()",
R"()",
false,
R"()",
1);
if (!opt.parse(argc,argv)) return 1;
souffle::Sf_example_np obj;
#if defined(_OPENMP) 
obj.setNumThreads(opt.getNumJobs());

#endif
obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());
return 0;
} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}
}
#endif

namespace  souffle {
using namespace souffle;
class factory_Sf_example_np: souffle::ProgramFactory {
public:
souffle::SouffleProgram* newInstance();
 factory_Sf_example_np();
private:
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
souffle::SouffleProgram* factory_Sf_example_np::newInstance(){
return new  souffle::Sf_example_np();
}

 factory_Sf_example_np::factory_Sf_example_np():
souffle::ProgramFactory("example_np"){
}

} // namespace  souffle
namespace souffle {

#ifdef __EMBEDDED_SOUFFLE__
extern "C" {
souffle::factory_Sf_example_np __factory_Sf_example_np_instance;
}
#endif
} // namespace souffle

