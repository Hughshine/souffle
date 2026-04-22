#define SOUFFLE_GENERATOR_VERSION "689d5ceb6"
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
#include "souffle/problog/debug/Debugger.h"
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
namespace  souffle {
using namespace souffle;
class Stratum_a_f5cc2531020019db {
public:
 Stratum_a_f5cc2531020019db(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_delta_a_cb7db3f8f52e4070,t_btree_000_i__0__1::Type& rel_delta_b_936be2cc269dd2ee,t_btree_000_i__0__1::Type& rel_delta_c_e10d7ef6690eede3,t_btree_000_i__0__1::Type& rel_new_a_37dc61ccaf89f094,t_btree_000_i__0__1::Type& rel_new_b_235be8f0cfe8fb69,t_btree_000_i__0__1::Type& rel_new_c_6216fae7a16c22cf,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_i__0__1::Type& rel_s_59046bdf5c263ae2);
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
t_btree_000_i__0__1::Type* rel_delta_a_cb7db3f8f52e4070;
t_btree_000_i__0__1::Type* rel_delta_b_936be2cc269dd2ee;
t_btree_000_i__0__1::Type* rel_delta_c_e10d7ef6690eede3;
t_btree_000_i__0__1::Type* rel_new_a_37dc61ccaf89f094;
t_btree_000_i__0__1::Type* rel_new_b_235be8f0cfe8fb69;
t_btree_000_i__0__1::Type* rel_new_c_6216fae7a16c22cf;
t_btree_100_i__0__1::Type* rel_a_454ddee488c1ed6b;
t_btree_100_i__0__1::Type* rel_b_96694f8e93f5c77d;
t_btree_100_i__0__1::Type* rel_c_981811ba2479fc8d;
t_btree_100_i__0__1::Type* rel_s_59046bdf5c263ae2;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_a_f5cc2531020019db::Stratum_a_f5cc2531020019db(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_delta_a_cb7db3f8f52e4070,t_btree_000_i__0__1::Type& rel_delta_b_936be2cc269dd2ee,t_btree_000_i__0__1::Type& rel_delta_c_e10d7ef6690eede3,t_btree_000_i__0__1::Type& rel_new_a_37dc61ccaf89f094,t_btree_000_i__0__1::Type& rel_new_b_235be8f0cfe8fb69,t_btree_000_i__0__1::Type& rel_new_c_6216fae7a16c22cf,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_i__0__1::Type& rel_s_59046bdf5c263ae2):
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
rel_delta_a_cb7db3f8f52e4070(&rel_delta_a_cb7db3f8f52e4070),
rel_delta_b_936be2cc269dd2ee(&rel_delta_b_936be2cc269dd2ee),
rel_delta_c_e10d7ef6690eede3(&rel_delta_c_e10d7ef6690eede3),
rel_new_a_37dc61ccaf89f094(&rel_new_a_37dc61ccaf89f094),
rel_new_b_235be8f0cfe8fb69(&rel_new_b_235be8f0cfe8fb69),
rel_new_c_6216fae7a16c22cf(&rel_new_c_6216fae7a16c22cf),
rel_a_454ddee488c1ed6b(&rel_a_454ddee488c1ed6b),
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d),
rel_c_981811ba2479fc8d(&rel_c_981811ba2479fc8d),
rel_s_59046bdf5c263ae2(&rel_s_59046bdf5c263ae2){
}

void Stratum_a_f5cc2531020019db::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(a(X) :- 
   s(X).
in file test.dl [15:6-15:19])_");
if(!(rel_s_59046bdf5c263ae2->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
CREATE_OP_CONTEXT(rel_s_59046bdf5c263ae2_op_ctxt,rel_s_59046bdf5c263ae2->createContext());
for(const auto& env0 : *rel_s_59046bdf5c263ae2) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_a_454ddee488c1ed6b->insert(tuple,READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("a",tuple);
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
[&](){
CREATE_OP_CONTEXT(rel_delta_a_cb7db3f8f52e4070_op_ctxt,rel_delta_a_cb7db3f8f52e4070->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
for(const auto& env0 : *rel_a_454ddee488c1ed6b) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_delta_a_cb7db3f8f52e4070->insert(tuple,READ_OP_CONTEXT(rel_delta_a_cb7db3f8f52e4070_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_delta_b_936be2cc269dd2ee_op_ctxt,rel_delta_b_936be2cc269dd2ee->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
for(const auto& env0 : *rel_b_96694f8e93f5c77d) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_delta_b_936be2cc269dd2ee->insert(tuple,READ_OP_CONTEXT(rel_delta_b_936be2cc269dd2ee_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_delta_c_e10d7ef6690eede3_op_ctxt,rel_delta_c_e10d7ef6690eede3->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
for(const auto& env0 : *rel_c_981811ba2479fc8d) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_delta_c_e10d7ef6690eede3->insert(tuple,READ_OP_CONTEXT(rel_delta_c_e10d7ef6690eede3_op_ctxt));
}
}
();auto loop_counter = RamUnsigned(1);
iter = 0;
for(;;) {
signalHandler->setMsg(R"_(a(X) :- 
   c(X).
in file test.dl [19:6-19:19])_");
if(!(rel_delta_c_e10d7ef6690eede3->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_c_e10d7ef6690eede3_op_ctxt,rel_delta_c_e10d7ef6690eede3->createContext());
CREATE_OP_CONTEXT(rel_new_a_37dc61ccaf89f094_op_ctxt,rel_new_a_37dc61ccaf89f094->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
if(!(rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))) {
for(const auto& env0 : *rel_delta_c_e10d7ef6690eede3) {
if( !((rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("a", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{2,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
if (!rel_a_454ddee488c1ed6b->contains(tuple)) {
rel_new_a_37dc61ccaf89f094->insert(tuple,READ_OP_CONTEXT(rel_new_a_37dc61ccaf89f094_op_ctxt));
}
auto untypedTuple = UntypedTuple::fromTypedTuple("a",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{2, varValues};
ruleSet->insert(ruleApplication);
}
}
}
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   a(X).
in file test.dl [17:6-17:19])_");
if(!(rel_delta_a_cb7db3f8f52e4070->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_a_cb7db3f8f52e4070_op_ctxt,rel_delta_a_cb7db3f8f52e4070->createContext());
CREATE_OP_CONTEXT(rel_new_b_235be8f0cfe8fb69_op_ctxt,rel_new_b_235be8f0cfe8fb69->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_a_cb7db3f8f52e4070) {
if( !((rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{3,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
if (!rel_b_96694f8e93f5c77d->contains(tuple)) {
rel_new_b_235be8f0cfe8fb69->insert(tuple,READ_OP_CONTEXT(rel_new_b_235be8f0cfe8fb69_op_ctxt));
}
auto untypedTuple = UntypedTuple::fromTypedTuple("b",tuple);
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
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   c(X).
in file test.dl [21:6-21:19])_");
if(!(rel_delta_c_e10d7ef6690eede3->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_c_e10d7ef6690eede3_op_ctxt,rel_delta_c_e10d7ef6690eede3->createContext());
CREATE_OP_CONTEXT(rel_new_b_235be8f0cfe8fb69_op_ctxt,rel_new_b_235be8f0cfe8fb69->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_c_e10d7ef6690eede3) {
if( !((rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{4,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
if (!rel_b_96694f8e93f5c77d->contains(tuple)) {
rel_new_b_235be8f0cfe8fb69->insert(tuple,READ_OP_CONTEXT(rel_new_b_235be8f0cfe8fb69_op_ctxt));
}
auto untypedTuple = UntypedTuple::fromTypedTuple("b",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{4, varValues};
ruleSet->insert(ruleApplication);
}
}
}
}
();}
signalHandler->setMsg(R"_(c(X) :- 
   b(X).
in file test.dl [18:6-18:19])_");
if(!(rel_delta_b_936be2cc269dd2ee->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_b_936be2cc269dd2ee_op_ctxt,rel_delta_b_936be2cc269dd2ee->createContext());
CREATE_OP_CONTEXT(rel_new_c_6216fae7a16c22cf_op_ctxt,rel_new_c_6216fae7a16c22cf->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
if(!(rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_b_936be2cc269dd2ee) {
if( !((rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("c", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{5,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
if (!rel_c_981811ba2479fc8d->contains(tuple)) {
rel_new_c_6216fae7a16c22cf->insert(tuple,READ_OP_CONTEXT(rel_new_c_6216fae7a16c22cf_op_ctxt));
}
auto untypedTuple = UntypedTuple::fromTypedTuple("c",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{5, varValues};
ruleSet->insert(ruleApplication);
}
}
}
}
();}
if(rel_new_a_37dc61ccaf89f094->empty() && rel_new_b_235be8f0cfe8fb69->empty() && rel_new_c_6216fae7a16c22cf->empty()) break;
[&](){
CREATE_OP_CONTEXT(rel_new_a_37dc61ccaf89f094_op_ctxt,rel_new_a_37dc61ccaf89f094->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
for(const auto& env0 : *rel_new_a_37dc61ccaf89f094) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_a_454ddee488c1ed6b->insert(tuple,READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt));
}
}
();std::swap(rel_delta_a_cb7db3f8f52e4070, rel_new_a_37dc61ccaf89f094);
rel_new_a_37dc61ccaf89f094->purge();
[&](){
CREATE_OP_CONTEXT(rel_new_b_235be8f0cfe8fb69_op_ctxt,rel_new_b_235be8f0cfe8fb69->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
for(const auto& env0 : *rel_new_b_235be8f0cfe8fb69) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_b_96694f8e93f5c77d->insert(tuple,READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt));
}
}
();std::swap(rel_delta_b_936be2cc269dd2ee, rel_new_b_235be8f0cfe8fb69);
rel_new_b_235be8f0cfe8fb69->purge();
[&](){
CREATE_OP_CONTEXT(rel_new_c_6216fae7a16c22cf_op_ctxt,rel_new_c_6216fae7a16c22cf->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
for(const auto& env0 : *rel_new_c_6216fae7a16c22cf) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_c_981811ba2479fc8d->insert(tuple,READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt));
}
}
();std::swap(rel_delta_c_e10d7ef6690eede3, rel_new_c_6216fae7a16c22cf);
rel_new_c_6216fae7a16c22cf->purge();
loop_counter = (ramBitCast<RamUnsigned>(loop_counter) + ramBitCast<RamUnsigned>(RamUnsigned(1)));
iter++;
}
iter = 0;
rel_delta_a_cb7db3f8f52e4070->purge();
rel_new_a_37dc61ccaf89f094->purge();
rel_delta_b_936be2cc269dd2ee->purge();
rel_new_b_235be8f0cfe8fb69->purge();
rel_delta_c_e10d7ef6690eede3->purge();
rel_new_c_6216fae7a16c22cf->purge();
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","a"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
{
FunctionTimer timer("writing relation rel_a_454ddee488c1ed6b");
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_a_454ddee488c1ed6b);
}
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","b"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
{
FunctionTimer timer("writing relation rel_b_96694f8e93f5c77d");
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_b_96694f8e93f5c77d);
}
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","c"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
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
class Stratum_a_inc_935a746942ed3383 {
public:
 Stratum_a_inc_935a746942ed3383(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_a_394b3623655cbdd0,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_b_ded826b2e75f35bb,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_c_2409cf566a420e77,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_a_bbea135139bb89ae,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_b_420980c115f01b29,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_c_0d24c4484987ca3f,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_a_58354ad400e6cd67,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_b_1b345b26cae73383,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_s_45ff968c958aa951,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_a_88895cba4fbff038,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_c_70adcfa1afd70315,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0,t_btree_000_i__0__1::Type& rel_delta_tuple_delete_a_249185740a160373,t_btree_000_i__0__1::Type& rel_delta_tuple_delete_b_43dfed04dd4422a4,t_btree_000_i__0__1::Type& rel_delta_tuple_delete_c_99c900b677e7ae7a,t_btree_000_i__0__1::Type& rel_delta_tuple_insert_a_419a5d6ce8e56c01,t_btree_000_i__0__1::Type& rel_delta_tuple_insert_b_a88502da93017be6,t_btree_000_i__0__1::Type& rel_delta_tuple_insert_c_02751d71f015bc27,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_rederive_b_29aee4d2a2657817,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_a_45448aaf3278243c,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_b_2df625d9f0a591a4,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_c_b4a21678aced1fb3,t_btree_000_i__0__1::Type& rel_inc_new_derv_rederive_a_cf31a1985b633fe9,t_btree_000_i__0__1::Type& rel_inc_new_derv_rederive_b_85af71cbeb32637e,t_btree_000_i__0__1::Type& rel_inc_new_derv_rederive_c_c3f4d0e8ef218854,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_a_3943091945425515,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_b_f9480aa9bb17885d,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_c_1ebbcf7f486522d8,t_btree_000_i__0__1::Type& rel_new_derv_delete_a_f6265c4636f36293,t_btree_000_i__0__1::Type& rel_new_derv_delete_b_4199ee6bbba1324f,t_btree_000_i__0__1::Type& rel_new_derv_delete_c_eb3a420fe21b36f5,t_btree_000_i__0__1::Type& rel_new_derv_insert_a_cb7a1502e8af1907,t_btree_000_i__0__1::Type& rel_new_derv_insert_b_8572f4c197f8bd21,t_btree_000_i__0__1::Type& rel_new_derv_insert_c_92944d39136b7981,t_btree_000_i__0__1::Type& rel_old_a_bd7865de58a1cd60,t_btree_000_i__0__1::Type& rel_old_b_38eb3a9d03faff34,t_btree_000_i__0__1::Type& rel_old_c_3e6edc8484191be0,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_i__0__1::Type& rel_s_59046bdf5c263ae2);
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
t_btree_000_i__0__1::Type* rel_inc_delta_derv_delete_a_394b3623655cbdd0;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_delete_b_ded826b2e75f35bb;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_delete_c_2409cf566a420e77;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_insert_a_bbea135139bb89ae;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_insert_b_420980c115f01b29;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_insert_c_0d24c4484987ca3f;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_a_58354ad400e6cd67;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_b_1b345b26cae73383;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_c_43fa1164ffebfc11;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_s_45ff968c958aa951;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_a_88895cba4fbff038;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_c_70adcfa1afd70315;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0;
t_btree_000_i__0__1::Type* rel_delta_tuple_delete_a_249185740a160373;
t_btree_000_i__0__1::Type* rel_delta_tuple_delete_b_43dfed04dd4422a4;
t_btree_000_i__0__1::Type* rel_delta_tuple_delete_c_99c900b677e7ae7a;
t_btree_000_i__0__1::Type* rel_delta_tuple_insert_a_419a5d6ce8e56c01;
t_btree_000_i__0__1::Type* rel_delta_tuple_insert_b_a88502da93017be6;
t_btree_000_i__0__1::Type* rel_delta_tuple_insert_c_02751d71f015bc27;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_rederive_b_29aee4d2a2657817;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263;
t_btree_000_i__0__1::Type* rel_inc_derv_overdelete_a_45448aaf3278243c;
t_btree_000_i__0__1::Type* rel_inc_derv_overdelete_b_2df625d9f0a591a4;
t_btree_000_i__0__1::Type* rel_inc_derv_overdelete_c_b4a21678aced1fb3;
t_btree_000_i__0__1::Type* rel_inc_new_derv_rederive_a_cf31a1985b633fe9;
t_btree_000_i__0__1::Type* rel_inc_new_derv_rederive_b_85af71cbeb32637e;
t_btree_000_i__0__1::Type* rel_inc_new_derv_rederive_c_c3f4d0e8ef218854;
t_btree_100_i__0__1::Type* rel_inc_tuple_overdelete_a_3943091945425515;
t_btree_100_i__0__1::Type* rel_inc_tuple_overdelete_b_f9480aa9bb17885d;
t_btree_100_i__0__1::Type* rel_inc_tuple_overdelete_c_1ebbcf7f486522d8;
t_btree_000_i__0__1::Type* rel_new_derv_delete_a_f6265c4636f36293;
t_btree_000_i__0__1::Type* rel_new_derv_delete_b_4199ee6bbba1324f;
t_btree_000_i__0__1::Type* rel_new_derv_delete_c_eb3a420fe21b36f5;
t_btree_000_i__0__1::Type* rel_new_derv_insert_a_cb7a1502e8af1907;
t_btree_000_i__0__1::Type* rel_new_derv_insert_b_8572f4c197f8bd21;
t_btree_000_i__0__1::Type* rel_new_derv_insert_c_92944d39136b7981;
t_btree_000_i__0__1::Type* rel_old_a_bd7865de58a1cd60;
t_btree_000_i__0__1::Type* rel_old_b_38eb3a9d03faff34;
t_btree_000_i__0__1::Type* rel_old_c_3e6edc8484191be0;
t_btree_100_i__0__1::Type* rel_a_454ddee488c1ed6b;
t_btree_100_i__0__1::Type* rel_b_96694f8e93f5c77d;
t_btree_100_i__0__1::Type* rel_c_981811ba2479fc8d;
t_btree_100_i__0__1::Type* rel_s_59046bdf5c263ae2;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_a_inc_935a746942ed3383::Stratum_a_inc_935a746942ed3383(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_a_394b3623655cbdd0,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_b_ded826b2e75f35bb,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_c_2409cf566a420e77,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_a_bbea135139bb89ae,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_b_420980c115f01b29,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_c_0d24c4484987ca3f,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_a_58354ad400e6cd67,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_b_1b345b26cae73383,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_s_45ff968c958aa951,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_a_88895cba4fbff038,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_c_70adcfa1afd70315,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0,t_btree_000_i__0__1::Type& rel_delta_tuple_delete_a_249185740a160373,t_btree_000_i__0__1::Type& rel_delta_tuple_delete_b_43dfed04dd4422a4,t_btree_000_i__0__1::Type& rel_delta_tuple_delete_c_99c900b677e7ae7a,t_btree_000_i__0__1::Type& rel_delta_tuple_insert_a_419a5d6ce8e56c01,t_btree_000_i__0__1::Type& rel_delta_tuple_insert_b_a88502da93017be6,t_btree_000_i__0__1::Type& rel_delta_tuple_insert_c_02751d71f015bc27,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_rederive_b_29aee4d2a2657817,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_a_45448aaf3278243c,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_b_2df625d9f0a591a4,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_c_b4a21678aced1fb3,t_btree_000_i__0__1::Type& rel_inc_new_derv_rederive_a_cf31a1985b633fe9,t_btree_000_i__0__1::Type& rel_inc_new_derv_rederive_b_85af71cbeb32637e,t_btree_000_i__0__1::Type& rel_inc_new_derv_rederive_c_c3f4d0e8ef218854,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_a_3943091945425515,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_b_f9480aa9bb17885d,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_c_1ebbcf7f486522d8,t_btree_000_i__0__1::Type& rel_new_derv_delete_a_f6265c4636f36293,t_btree_000_i__0__1::Type& rel_new_derv_delete_b_4199ee6bbba1324f,t_btree_000_i__0__1::Type& rel_new_derv_delete_c_eb3a420fe21b36f5,t_btree_000_i__0__1::Type& rel_new_derv_insert_a_cb7a1502e8af1907,t_btree_000_i__0__1::Type& rel_new_derv_insert_b_8572f4c197f8bd21,t_btree_000_i__0__1::Type& rel_new_derv_insert_c_92944d39136b7981,t_btree_000_i__0__1::Type& rel_old_a_bd7865de58a1cd60,t_btree_000_i__0__1::Type& rel_old_b_38eb3a9d03faff34,t_btree_000_i__0__1::Type& rel_old_c_3e6edc8484191be0,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_i__0__1::Type& rel_s_59046bdf5c263ae2):
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
rel_inc_delta_derv_delete_a_394b3623655cbdd0(&rel_inc_delta_derv_delete_a_394b3623655cbdd0),
rel_inc_delta_derv_delete_b_ded826b2e75f35bb(&rel_inc_delta_derv_delete_b_ded826b2e75f35bb),
rel_inc_delta_derv_delete_c_2409cf566a420e77(&rel_inc_delta_derv_delete_c_2409cf566a420e77),
rel_inc_delta_derv_insert_a_bbea135139bb89ae(&rel_inc_delta_derv_insert_a_bbea135139bb89ae),
rel_inc_delta_derv_insert_b_420980c115f01b29(&rel_inc_delta_derv_insert_b_420980c115f01b29),
rel_inc_delta_derv_insert_c_0d24c4484987ca3f(&rel_inc_delta_derv_insert_c_0d24c4484987ca3f),
rel_inc_delta_tuple_delete_a_58354ad400e6cd67(&rel_inc_delta_tuple_delete_a_58354ad400e6cd67),
rel_inc_delta_tuple_delete_b_1b345b26cae73383(&rel_inc_delta_tuple_delete_b_1b345b26cae73383),
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11(&rel_inc_delta_tuple_delete_c_43fa1164ffebfc11),
rel_inc_delta_tuple_delete_s_45ff968c958aa951(&rel_inc_delta_tuple_delete_s_45ff968c958aa951),
rel_inc_delta_tuple_insert_a_88895cba4fbff038(&rel_inc_delta_tuple_insert_a_88895cba4fbff038),
rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46(&rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46),
rel_inc_delta_tuple_insert_c_70adcfa1afd70315(&rel_inc_delta_tuple_insert_c_70adcfa1afd70315),
rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0(&rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0),
rel_delta_tuple_delete_a_249185740a160373(&rel_delta_tuple_delete_a_249185740a160373),
rel_delta_tuple_delete_b_43dfed04dd4422a4(&rel_delta_tuple_delete_b_43dfed04dd4422a4),
rel_delta_tuple_delete_c_99c900b677e7ae7a(&rel_delta_tuple_delete_c_99c900b677e7ae7a),
rel_delta_tuple_insert_a_419a5d6ce8e56c01(&rel_delta_tuple_insert_a_419a5d6ce8e56c01),
rel_delta_tuple_insert_b_a88502da93017be6(&rel_delta_tuple_insert_b_a88502da93017be6),
rel_delta_tuple_insert_c_02751d71f015bc27(&rel_delta_tuple_insert_c_02751d71f015bc27),
rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7(&rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7),
rel_inc_delta_tuple_rederive_b_29aee4d2a2657817(&rel_inc_delta_tuple_rederive_b_29aee4d2a2657817),
rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263(&rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263),
rel_inc_derv_overdelete_a_45448aaf3278243c(&rel_inc_derv_overdelete_a_45448aaf3278243c),
rel_inc_derv_overdelete_b_2df625d9f0a591a4(&rel_inc_derv_overdelete_b_2df625d9f0a591a4),
rel_inc_derv_overdelete_c_b4a21678aced1fb3(&rel_inc_derv_overdelete_c_b4a21678aced1fb3),
rel_inc_new_derv_rederive_a_cf31a1985b633fe9(&rel_inc_new_derv_rederive_a_cf31a1985b633fe9),
rel_inc_new_derv_rederive_b_85af71cbeb32637e(&rel_inc_new_derv_rederive_b_85af71cbeb32637e),
rel_inc_new_derv_rederive_c_c3f4d0e8ef218854(&rel_inc_new_derv_rederive_c_c3f4d0e8ef218854),
rel_inc_tuple_overdelete_a_3943091945425515(&rel_inc_tuple_overdelete_a_3943091945425515),
rel_inc_tuple_overdelete_b_f9480aa9bb17885d(&rel_inc_tuple_overdelete_b_f9480aa9bb17885d),
rel_inc_tuple_overdelete_c_1ebbcf7f486522d8(&rel_inc_tuple_overdelete_c_1ebbcf7f486522d8),
rel_new_derv_delete_a_f6265c4636f36293(&rel_new_derv_delete_a_f6265c4636f36293),
rel_new_derv_delete_b_4199ee6bbba1324f(&rel_new_derv_delete_b_4199ee6bbba1324f),
rel_new_derv_delete_c_eb3a420fe21b36f5(&rel_new_derv_delete_c_eb3a420fe21b36f5),
rel_new_derv_insert_a_cb7a1502e8af1907(&rel_new_derv_insert_a_cb7a1502e8af1907),
rel_new_derv_insert_b_8572f4c197f8bd21(&rel_new_derv_insert_b_8572f4c197f8bd21),
rel_new_derv_insert_c_92944d39136b7981(&rel_new_derv_insert_c_92944d39136b7981),
rel_old_a_bd7865de58a1cd60(&rel_old_a_bd7865de58a1cd60),
rel_old_b_38eb3a9d03faff34(&rel_old_b_38eb3a9d03faff34),
rel_old_c_3e6edc8484191be0(&rel_old_c_3e6edc8484191be0),
rel_a_454ddee488c1ed6b(&rel_a_454ddee488c1ed6b),
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d),
rel_c_981811ba2479fc8d(&rel_c_981811ba2479fc8d),
rel_s_59046bdf5c263ae2(&rel_s_59046bdf5c263ae2){
}

void Stratum_a_inc_935a746942ed3383::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
[&](){
CREATE_OP_CONTEXT(rel_old_a_bd7865de58a1cd60_op_ctxt,rel_old_a_bd7865de58a1cd60->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
for(const auto& env0 : *rel_old_a_bd7865de58a1cd60) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_a_454ddee488c1ed6b->insert(tuple,READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_old_b_38eb3a9d03faff34_op_ctxt,rel_old_b_38eb3a9d03faff34->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
for(const auto& env0 : *rel_old_b_38eb3a9d03faff34) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_b_96694f8e93f5c77d->insert(tuple,READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_old_c_3e6edc8484191be0_op_ctxt,rel_old_c_3e6edc8484191be0->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
for(const auto& env0 : *rel_old_c_3e6edc8484191be0) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_c_981811ba2479fc8d->insert(tuple,READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt));
}
}
();signalHandler->setMsg(R"_(a(X) :- 
   s(X).
in file test.dl [15:6-15:19])_");
if(!(rel_inc_delta_tuple_delete_s_45ff968c958aa951->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_a_394b3623655cbdd0_op_ctxt,rel_inc_delta_derv_delete_a_394b3623655cbdd0->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_s_45ff968c958aa951_op_ctxt,rel_inc_delta_tuple_delete_s_45ff968c958aa951->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_delete_s_45ff968c958aa951) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_delete_a_394b3623655cbdd0->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_delete_a_394b3623655cbdd0_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("a",tuple);
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
for(const auto& tupleDeltaDervDelete: *rel_inc_delta_derv_delete_a_394b3623655cbdd0) {
auto untypedDeltaDervTupleDelete = UntypedTuple::fromTypedTuple("a",tupleDeltaDervDelete);
auto*& untypedDeltaDervTupleDeleteRuleSet = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedDeltaDervTupleDelete];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleDelete];

for(const auto& deletedRuleAppl: *untypedDeltaDervTupleDeleteRuleSet) {
untypedDeltaDervTupleRuleSet->erase(deletedRuleAppl);

}
if (untypedDeltaDervTupleRuleSet->empty()) {
delete untypedDeltaDervTupleRuleSet;

DerivationManager::untypedTuple2RuleApplications.erase(untypedDeltaDervTupleDelete);

if(!isInputFact(untypedDeltaDervTupleDelete))
rel_inc_delta_tuple_delete_a_58354ad400e6cd67->insert(tupleDeltaDervDelete);
 
}
}
DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
for(const auto& deletedTuple: *rel_inc_delta_tuple_delete_a_58354ad400e6cd67) {

rel_a_454ddee488c1ed6b->erase(deletedTuple);

}

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

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt,rel_inc_delta_tuple_delete_a_58354ad400e6cd67->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_delete_a_249185740a160373_op_ctxt,rel_delta_tuple_delete_a_249185740a160373->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_delete_a_58354ad400e6cd67) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_delta_tuple_delete_a_249185740a160373->insert(tuple,READ_OP_CONTEXT(rel_delta_tuple_delete_a_249185740a160373_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt,rel_inc_delta_tuple_delete_a_58354ad400e6cd67->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_a_3943091945425515_op_ctxt,rel_inc_tuple_overdelete_a_3943091945425515->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_delete_a_58354ad400e6cd67) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_a_3943091945425515->insert(tuple,READ_OP_CONTEXT(rel_inc_tuple_overdelete_a_3943091945425515_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt,rel_inc_delta_tuple_delete_b_1b345b26cae73383->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_delete_b_43dfed04dd4422a4_op_ctxt,rel_delta_tuple_delete_b_43dfed04dd4422a4->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_delete_b_1b345b26cae73383) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_delta_tuple_delete_b_43dfed04dd4422a4->insert(tuple,READ_OP_CONTEXT(rel_delta_tuple_delete_b_43dfed04dd4422a4_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt,rel_inc_delta_tuple_delete_b_1b345b26cae73383->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_b_f9480aa9bb17885d_op_ctxt,rel_inc_tuple_overdelete_b_f9480aa9bb17885d->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_delete_b_1b345b26cae73383) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_b_f9480aa9bb17885d->insert(tuple,READ_OP_CONTEXT(rel_inc_tuple_overdelete_b_f9480aa9bb17885d_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt,rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_delete_c_99c900b677e7ae7a_op_ctxt,rel_delta_tuple_delete_c_99c900b677e7ae7a->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_delete_c_43fa1164ffebfc11) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_delta_tuple_delete_c_99c900b677e7ae7a->insert(tuple,READ_OP_CONTEXT(rel_delta_tuple_delete_c_99c900b677e7ae7a_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt,rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_c_1ebbcf7f486522d8_op_ctxt,rel_inc_tuple_overdelete_c_1ebbcf7f486522d8->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_delete_c_43fa1164ffebfc11) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_c_1ebbcf7f486522d8->insert(tuple,READ_OP_CONTEXT(rel_inc_tuple_overdelete_c_1ebbcf7f486522d8_op_ctxt));
}
}
();signalHandler->setMsg(R"_(a(X) :- 
   c(X).
in file test.dl [19:6-19:19])_");
if(!(rel_delta_tuple_delete_c_99c900b677e7ae7a->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_c_99c900b677e7ae7a_op_ctxt,rel_delta_tuple_delete_c_99c900b677e7ae7a->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_a_f6265c4636f36293_op_ctxt,rel_new_derv_delete_a_f6265c4636f36293->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
if(!(rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_delete_c_99c900b677e7ae7a) {
if( (rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("a", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{2,{ramBitCast(env0[0])}})))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_delete_a_f6265c4636f36293->insert(tuple,READ_OP_CONTEXT(rel_new_derv_delete_a_f6265c4636f36293_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("a",tuple);
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
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   a(X).
in file test.dl [17:6-17:19])_");
if(!(rel_delta_tuple_delete_a_249185740a160373->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_a_249185740a160373_op_ctxt,rel_delta_tuple_delete_a_249185740a160373->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_b_4199ee6bbba1324f_op_ctxt,rel_new_derv_delete_b_4199ee6bbba1324f->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_delete_a_249185740a160373) {
if( (rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{3,{ramBitCast(env0[0])}})))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_delete_b_4199ee6bbba1324f->insert(tuple,READ_OP_CONTEXT(rel_new_derv_delete_b_4199ee6bbba1324f_op_ctxt));
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
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   c(X).
in file test.dl [21:6-21:19])_");
if(!(rel_delta_tuple_delete_c_99c900b677e7ae7a->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_c_99c900b677e7ae7a_op_ctxt,rel_delta_tuple_delete_c_99c900b677e7ae7a->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_b_4199ee6bbba1324f_op_ctxt,rel_new_derv_delete_b_4199ee6bbba1324f->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_delete_c_99c900b677e7ae7a) {
if( (rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{4,{ramBitCast(env0[0])}})))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_delete_b_4199ee6bbba1324f->insert(tuple,READ_OP_CONTEXT(rel_new_derv_delete_b_4199ee6bbba1324f_op_ctxt));
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
RuleApplication ruleApplication{4, varValues};
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
}
();}
signalHandler->setMsg(R"_(c(X) :- 
   b(X).
in file test.dl [18:6-18:19])_");
if(!(rel_delta_tuple_delete_b_43dfed04dd4422a4->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_b_43dfed04dd4422a4_op_ctxt,rel_delta_tuple_delete_b_43dfed04dd4422a4->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_c_eb3a420fe21b36f5_op_ctxt,rel_new_derv_delete_c_eb3a420fe21b36f5->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
if(!(rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_delete_b_43dfed04dd4422a4) {
if( (rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("c", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{5,{ramBitCast(env0[0])}})))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_delete_c_eb3a420fe21b36f5->insert(tuple,READ_OP_CONTEXT(rel_new_derv_delete_c_eb3a420fe21b36f5_op_ctxt));
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
RuleApplication ruleApplication{5, varValues};
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
}
();}
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_a_394b3623655cbdd0_op_ctxt,rel_inc_delta_derv_delete_a_394b3623655cbdd0->createContext());
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_a_45448aaf3278243c_op_ctxt,rel_inc_derv_overdelete_a_45448aaf3278243c->createContext());
for(const auto& env0 : *rel_inc_delta_derv_delete_a_394b3623655cbdd0) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_derv_overdelete_a_45448aaf3278243c->insert(tuple,READ_OP_CONTEXT(rel_inc_derv_overdelete_a_45448aaf3278243c_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_b_ded826b2e75f35bb_op_ctxt,rel_inc_delta_derv_delete_b_ded826b2e75f35bb->createContext());
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_b_2df625d9f0a591a4_op_ctxt,rel_inc_derv_overdelete_b_2df625d9f0a591a4->createContext());
for(const auto& env0 : *rel_inc_delta_derv_delete_b_ded826b2e75f35bb) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_derv_overdelete_b_2df625d9f0a591a4->insert(tuple,READ_OP_CONTEXT(rel_inc_derv_overdelete_b_2df625d9f0a591a4_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_c_2409cf566a420e77_op_ctxt,rel_inc_delta_derv_delete_c_2409cf566a420e77->createContext());
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_c_b4a21678aced1fb3_op_ctxt,rel_inc_derv_overdelete_c_b4a21678aced1fb3->createContext());
for(const auto& env0 : *rel_inc_delta_derv_delete_c_2409cf566a420e77) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_derv_overdelete_c_b4a21678aced1fb3->insert(tuple,READ_OP_CONTEXT(rel_inc_derv_overdelete_c_b4a21678aced1fb3_op_ctxt));
}
}
();rel_delta_tuple_delete_a_249185740a160373->purge();
for(const auto& tupleDeltaDervDelete: *rel_new_derv_delete_a_f6265c4636f36293) {
auto untypedDeltaDervTupleDelete = UntypedTuple::fromTypedTuple("a",tupleDeltaDervDelete);
auto*& untypedDeltaDervTupleDeleteRuleSet = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedDeltaDervTupleDelete];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleDelete];

for(const auto& deletedRuleAppl: *untypedDeltaDervTupleDeleteRuleSet) {
untypedDeltaDervTupleRuleSet->erase(deletedRuleAppl);

}
if (untypedDeltaDervTupleRuleSet->empty()) {
delete untypedDeltaDervTupleRuleSet;

DerivationManager::untypedTuple2RuleApplications.erase(untypedDeltaDervTupleDelete);

if(!isInputFact(untypedDeltaDervTupleDelete))
rel_delta_tuple_delete_a_249185740a160373->insert(tupleDeltaDervDelete);
 
}
}
DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
for(const auto& deletedTuple: *rel_delta_tuple_delete_a_249185740a160373) {

rel_a_454ddee488c1ed6b->erase(deletedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt,rel_inc_delta_tuple_delete_a_58354ad400e6cd67->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_delete_a_249185740a160373_op_ctxt,rel_delta_tuple_delete_a_249185740a160373->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_a_249185740a160373) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_a_58354ad400e6cd67->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_a_45448aaf3278243c_op_ctxt,rel_inc_derv_overdelete_a_45448aaf3278243c->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_a_f6265c4636f36293_op_ctxt,rel_new_derv_delete_a_f6265c4636f36293->createContext());
for(const auto& env0 : *rel_new_derv_delete_a_f6265c4636f36293) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_derv_overdelete_a_45448aaf3278243c->insert(tuple,READ_OP_CONTEXT(rel_inc_derv_overdelete_a_45448aaf3278243c_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_a_249185740a160373_op_ctxt,rel_delta_tuple_delete_a_249185740a160373->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_a_3943091945425515_op_ctxt,rel_inc_tuple_overdelete_a_3943091945425515->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_a_249185740a160373) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_a_3943091945425515->insert(tuple,READ_OP_CONTEXT(rel_inc_tuple_overdelete_a_3943091945425515_op_ctxt));
}
}
();rel_new_derv_delete_a_f6265c4636f36293->purge();
rel_delta_tuple_delete_b_43dfed04dd4422a4->purge();
for(const auto& tupleDeltaDervDelete: *rel_new_derv_delete_b_4199ee6bbba1324f) {
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
rel_delta_tuple_delete_b_43dfed04dd4422a4->insert(tupleDeltaDervDelete);
 
}
}
DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
for(const auto& deletedTuple: *rel_delta_tuple_delete_b_43dfed04dd4422a4) {

rel_b_96694f8e93f5c77d->erase(deletedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt,rel_inc_delta_tuple_delete_b_1b345b26cae73383->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_delete_b_43dfed04dd4422a4_op_ctxt,rel_delta_tuple_delete_b_43dfed04dd4422a4->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_b_43dfed04dd4422a4) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_b_1b345b26cae73383->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_b_2df625d9f0a591a4_op_ctxt,rel_inc_derv_overdelete_b_2df625d9f0a591a4->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_b_4199ee6bbba1324f_op_ctxt,rel_new_derv_delete_b_4199ee6bbba1324f->createContext());
for(const auto& env0 : *rel_new_derv_delete_b_4199ee6bbba1324f) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_derv_overdelete_b_2df625d9f0a591a4->insert(tuple,READ_OP_CONTEXT(rel_inc_derv_overdelete_b_2df625d9f0a591a4_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_b_43dfed04dd4422a4_op_ctxt,rel_delta_tuple_delete_b_43dfed04dd4422a4->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_b_f9480aa9bb17885d_op_ctxt,rel_inc_tuple_overdelete_b_f9480aa9bb17885d->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_b_43dfed04dd4422a4) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_b_f9480aa9bb17885d->insert(tuple,READ_OP_CONTEXT(rel_inc_tuple_overdelete_b_f9480aa9bb17885d_op_ctxt));
}
}
();rel_new_derv_delete_b_4199ee6bbba1324f->purge();
rel_delta_tuple_delete_c_99c900b677e7ae7a->purge();
for(const auto& tupleDeltaDervDelete: *rel_new_derv_delete_c_eb3a420fe21b36f5) {
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
rel_delta_tuple_delete_c_99c900b677e7ae7a->insert(tupleDeltaDervDelete);
 
}
}
DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
for(const auto& deletedTuple: *rel_delta_tuple_delete_c_99c900b677e7ae7a) {

rel_c_981811ba2479fc8d->erase(deletedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt,rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_delete_c_99c900b677e7ae7a_op_ctxt,rel_delta_tuple_delete_c_99c900b677e7ae7a->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_c_99c900b677e7ae7a) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_c_b4a21678aced1fb3_op_ctxt,rel_inc_derv_overdelete_c_b4a21678aced1fb3->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_c_eb3a420fe21b36f5_op_ctxt,rel_new_derv_delete_c_eb3a420fe21b36f5->createContext());
for(const auto& env0 : *rel_new_derv_delete_c_eb3a420fe21b36f5) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_derv_overdelete_c_b4a21678aced1fb3->insert(tuple,READ_OP_CONTEXT(rel_inc_derv_overdelete_c_b4a21678aced1fb3_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_c_99c900b677e7ae7a_op_ctxt,rel_delta_tuple_delete_c_99c900b677e7ae7a->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_c_1ebbcf7f486522d8_op_ctxt,rel_inc_tuple_overdelete_c_1ebbcf7f486522d8->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_c_99c900b677e7ae7a) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_c_1ebbcf7f486522d8->insert(tuple,READ_OP_CONTEXT(rel_inc_tuple_overdelete_c_1ebbcf7f486522d8_op_ctxt));
}
}
();rel_new_derv_delete_c_eb3a420fe21b36f5->purge();
auto loop_counter1 = RamUnsigned(1);
iter = 0;
for(;;) {
signalHandler->setMsg(R"_(a(X) :- 
   c(X).
in file test.dl [19:6-19:19])_");
if(!(rel_delta_tuple_delete_c_99c900b677e7ae7a->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_c_99c900b677e7ae7a_op_ctxt,rel_delta_tuple_delete_c_99c900b677e7ae7a->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_a_f6265c4636f36293_op_ctxt,rel_new_derv_delete_a_f6265c4636f36293->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
if(!(rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_delete_c_99c900b677e7ae7a) {
if( (rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("a", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{2,{ramBitCast(env0[0])}})))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_delete_a_f6265c4636f36293->insert(tuple,READ_OP_CONTEXT(rel_new_derv_delete_a_f6265c4636f36293_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("a",tuple);
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
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   a(X).
in file test.dl [17:6-17:19])_");
if(!(rel_delta_tuple_delete_a_249185740a160373->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_a_249185740a160373_op_ctxt,rel_delta_tuple_delete_a_249185740a160373->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_b_4199ee6bbba1324f_op_ctxt,rel_new_derv_delete_b_4199ee6bbba1324f->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_delete_a_249185740a160373) {
if( (rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{3,{ramBitCast(env0[0])}})))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_delete_b_4199ee6bbba1324f->insert(tuple,READ_OP_CONTEXT(rel_new_derv_delete_b_4199ee6bbba1324f_op_ctxt));
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
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   c(X).
in file test.dl [21:6-21:19])_");
if(!(rel_delta_tuple_delete_c_99c900b677e7ae7a->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_c_99c900b677e7ae7a_op_ctxt,rel_delta_tuple_delete_c_99c900b677e7ae7a->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_b_4199ee6bbba1324f_op_ctxt,rel_new_derv_delete_b_4199ee6bbba1324f->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_delete_c_99c900b677e7ae7a) {
if( (rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{4,{ramBitCast(env0[0])}})))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_delete_b_4199ee6bbba1324f->insert(tuple,READ_OP_CONTEXT(rel_new_derv_delete_b_4199ee6bbba1324f_op_ctxt));
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
RuleApplication ruleApplication{4, varValues};
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
}
();}
signalHandler->setMsg(R"_(c(X) :- 
   b(X).
in file test.dl [18:6-18:19])_");
if(!(rel_delta_tuple_delete_b_43dfed04dd4422a4->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_b_43dfed04dd4422a4_op_ctxt,rel_delta_tuple_delete_b_43dfed04dd4422a4->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_c_eb3a420fe21b36f5_op_ctxt,rel_new_derv_delete_c_eb3a420fe21b36f5->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
if(!(rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_delete_b_43dfed04dd4422a4) {
if( (rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("c", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{5,{ramBitCast(env0[0])}})))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_delete_c_eb3a420fe21b36f5->insert(tuple,READ_OP_CONTEXT(rel_new_derv_delete_c_eb3a420fe21b36f5_op_ctxt));
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
RuleApplication ruleApplication{5, varValues};
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
}
();}
if(rel_new_derv_delete_a_f6265c4636f36293->empty() && rel_new_derv_delete_b_4199ee6bbba1324f->empty() && rel_new_derv_delete_c_eb3a420fe21b36f5->empty()) break;
rel_delta_tuple_delete_a_249185740a160373->purge();
for(const auto& tupleDeltaDervDelete: *rel_new_derv_delete_a_f6265c4636f36293) {
auto untypedDeltaDervTupleDelete = UntypedTuple::fromTypedTuple("a",tupleDeltaDervDelete);
auto*& untypedDeltaDervTupleDeleteRuleSet = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedDeltaDervTupleDelete];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleDelete];

for(const auto& deletedRuleAppl: *untypedDeltaDervTupleDeleteRuleSet) {
untypedDeltaDervTupleRuleSet->erase(deletedRuleAppl);

}
if (untypedDeltaDervTupleRuleSet->empty()) {
delete untypedDeltaDervTupleRuleSet;

DerivationManager::untypedTuple2RuleApplications.erase(untypedDeltaDervTupleDelete);

if(!isInputFact(untypedDeltaDervTupleDelete))
rel_delta_tuple_delete_a_249185740a160373->insert(tupleDeltaDervDelete);
 
}
}
DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
for(const auto& deletedTuple: *rel_delta_tuple_delete_a_249185740a160373) {

rel_a_454ddee488c1ed6b->erase(deletedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt,rel_inc_delta_tuple_delete_a_58354ad400e6cd67->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_delete_a_249185740a160373_op_ctxt,rel_delta_tuple_delete_a_249185740a160373->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_a_249185740a160373) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_a_58354ad400e6cd67->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_a_45448aaf3278243c_op_ctxt,rel_inc_derv_overdelete_a_45448aaf3278243c->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_a_f6265c4636f36293_op_ctxt,rel_new_derv_delete_a_f6265c4636f36293->createContext());
for(const auto& env0 : *rel_new_derv_delete_a_f6265c4636f36293) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_derv_overdelete_a_45448aaf3278243c->insert(tuple,READ_OP_CONTEXT(rel_inc_derv_overdelete_a_45448aaf3278243c_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_a_249185740a160373_op_ctxt,rel_delta_tuple_delete_a_249185740a160373->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_a_3943091945425515_op_ctxt,rel_inc_tuple_overdelete_a_3943091945425515->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_a_249185740a160373) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_a_3943091945425515->insert(tuple,READ_OP_CONTEXT(rel_inc_tuple_overdelete_a_3943091945425515_op_ctxt));
}
}
();rel_new_derv_delete_a_f6265c4636f36293->purge();
rel_delta_tuple_delete_b_43dfed04dd4422a4->purge();
for(const auto& tupleDeltaDervDelete: *rel_new_derv_delete_b_4199ee6bbba1324f) {
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
rel_delta_tuple_delete_b_43dfed04dd4422a4->insert(tupleDeltaDervDelete);
 
}
}
DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
for(const auto& deletedTuple: *rel_delta_tuple_delete_b_43dfed04dd4422a4) {

rel_b_96694f8e93f5c77d->erase(deletedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt,rel_inc_delta_tuple_delete_b_1b345b26cae73383->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_delete_b_43dfed04dd4422a4_op_ctxt,rel_delta_tuple_delete_b_43dfed04dd4422a4->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_b_43dfed04dd4422a4) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_b_1b345b26cae73383->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_b_2df625d9f0a591a4_op_ctxt,rel_inc_derv_overdelete_b_2df625d9f0a591a4->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_b_4199ee6bbba1324f_op_ctxt,rel_new_derv_delete_b_4199ee6bbba1324f->createContext());
for(const auto& env0 : *rel_new_derv_delete_b_4199ee6bbba1324f) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_derv_overdelete_b_2df625d9f0a591a4->insert(tuple,READ_OP_CONTEXT(rel_inc_derv_overdelete_b_2df625d9f0a591a4_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_b_43dfed04dd4422a4_op_ctxt,rel_delta_tuple_delete_b_43dfed04dd4422a4->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_b_f9480aa9bb17885d_op_ctxt,rel_inc_tuple_overdelete_b_f9480aa9bb17885d->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_b_43dfed04dd4422a4) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_b_f9480aa9bb17885d->insert(tuple,READ_OP_CONTEXT(rel_inc_tuple_overdelete_b_f9480aa9bb17885d_op_ctxt));
}
}
();rel_new_derv_delete_b_4199ee6bbba1324f->purge();
rel_delta_tuple_delete_c_99c900b677e7ae7a->purge();
for(const auto& tupleDeltaDervDelete: *rel_new_derv_delete_c_eb3a420fe21b36f5) {
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
rel_delta_tuple_delete_c_99c900b677e7ae7a->insert(tupleDeltaDervDelete);
 
}
}
DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
for(const auto& deletedTuple: *rel_delta_tuple_delete_c_99c900b677e7ae7a) {

rel_c_981811ba2479fc8d->erase(deletedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt,rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_delete_c_99c900b677e7ae7a_op_ctxt,rel_delta_tuple_delete_c_99c900b677e7ae7a->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_c_99c900b677e7ae7a) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_c_b4a21678aced1fb3_op_ctxt,rel_inc_derv_overdelete_c_b4a21678aced1fb3->createContext());
CREATE_OP_CONTEXT(rel_new_derv_delete_c_eb3a420fe21b36f5_op_ctxt,rel_new_derv_delete_c_eb3a420fe21b36f5->createContext());
for(const auto& env0 : *rel_new_derv_delete_c_eb3a420fe21b36f5) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_derv_overdelete_c_b4a21678aced1fb3->insert(tuple,READ_OP_CONTEXT(rel_inc_derv_overdelete_c_b4a21678aced1fb3_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_delete_c_99c900b677e7ae7a_op_ctxt,rel_delta_tuple_delete_c_99c900b677e7ae7a->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_c_1ebbcf7f486522d8_op_ctxt,rel_inc_tuple_overdelete_c_1ebbcf7f486522d8->createContext());
for(const auto& env0 : *rel_delta_tuple_delete_c_99c900b677e7ae7a) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_c_1ebbcf7f486522d8->insert(tuple,READ_OP_CONTEXT(rel_inc_tuple_overdelete_c_1ebbcf7f486522d8_op_ctxt));
}
}
();rel_new_derv_delete_c_eb3a420fe21b36f5->purge();
loop_counter1 = (ramBitCast<RamUnsigned>(loop_counter1) + ramBitCast<RamUnsigned>(RamUnsigned(1)));
iter++;
}
iter = 0;
rel_delta_tuple_delete_a_249185740a160373->purge();
rel_new_derv_delete_a_f6265c4636f36293->purge();
rel_delta_tuple_delete_b_43dfed04dd4422a4->purge();
rel_new_derv_delete_b_4199ee6bbba1324f->purge();
rel_delta_tuple_delete_c_99c900b677e7ae7a->purge();
rel_new_derv_delete_c_eb3a420fe21b36f5->purge();
auto loop_counter_rederive = RamUnsigned(1);
iter = 0;
for(;;) {
signalHandler->setMsg(R"_(a(X) :- 
   c(X).
in file test.dl [19:6-19:19])_");
if(!(rel_inc_derv_overdelete_a_45448aaf3278243c->empty()) && !(rel_c_981811ba2479fc8d->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_a_45448aaf3278243c_op_ctxt,rel_inc_derv_overdelete_a_45448aaf3278243c->createContext());
CREATE_OP_CONTEXT(rel_inc_new_derv_rederive_a_cf31a1985b633fe9_op_ctxt,rel_inc_new_derv_rederive_a_cf31a1985b633fe9->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
if(!(rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))) {
for(const auto& env0 : *rel_inc_derv_overdelete_a_45448aaf3278243c) {
if( !((rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("a", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{2,{ramBitCast(env0[0])}})))) && rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_new_derv_rederive_a_cf31a1985b633fe9->insert(tuple,READ_OP_CONTEXT(rel_inc_new_derv_rederive_a_cf31a1985b633fe9_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("a",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaDeleteRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{2, varValues};
ruleSet->erase(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   a(X).
in file test.dl [17:6-17:19])_");
if(!(rel_inc_derv_overdelete_b_2df625d9f0a591a4->empty()) && !(rel_a_454ddee488c1ed6b->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_b_2df625d9f0a591a4_op_ctxt,rel_inc_derv_overdelete_b_2df625d9f0a591a4->createContext());
CREATE_OP_CONTEXT(rel_inc_new_derv_rederive_b_85af71cbeb32637e_op_ctxt,rel_inc_new_derv_rederive_b_85af71cbeb32637e->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_inc_derv_overdelete_b_2df625d9f0a591a4) {
if( !((rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{3,{ramBitCast(env0[0])}})))) && rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_new_derv_rederive_b_85af71cbeb32637e->insert(tuple,READ_OP_CONTEXT(rel_inc_new_derv_rederive_b_85af71cbeb32637e_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("b",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaDeleteRuleApplications[untypedTuple];
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
ruleSet->erase(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   c(X).
in file test.dl [21:6-21:19])_");
if(!(rel_inc_derv_overdelete_b_2df625d9f0a591a4->empty()) && !(rel_c_981811ba2479fc8d->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_b_2df625d9f0a591a4_op_ctxt,rel_inc_derv_overdelete_b_2df625d9f0a591a4->createContext());
CREATE_OP_CONTEXT(rel_inc_new_derv_rederive_b_85af71cbeb32637e_op_ctxt,rel_inc_new_derv_rederive_b_85af71cbeb32637e->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_inc_derv_overdelete_b_2df625d9f0a591a4) {
if( !((rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{4,{ramBitCast(env0[0])}})))) && rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_new_derv_rederive_b_85af71cbeb32637e->insert(tuple,READ_OP_CONTEXT(rel_inc_new_derv_rederive_b_85af71cbeb32637e_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("b",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaDeleteRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{4, varValues};
ruleSet->erase(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
signalHandler->setMsg(R"_(c(X) :- 
   b(X).
in file test.dl [18:6-18:19])_");
if(!(rel_inc_derv_overdelete_c_b4a21678aced1fb3->empty()) && !(rel_b_96694f8e93f5c77d->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_derv_overdelete_c_b4a21678aced1fb3_op_ctxt,rel_inc_derv_overdelete_c_b4a21678aced1fb3->createContext());
CREATE_OP_CONTEXT(rel_inc_new_derv_rederive_c_c3f4d0e8ef218854_op_ctxt,rel_inc_new_derv_rederive_c_c3f4d0e8ef218854->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
if(!(rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))) {
for(const auto& env0 : *rel_inc_derv_overdelete_c_b4a21678aced1fb3) {
if( !((rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("c", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{5,{ramBitCast(env0[0])}})))) && rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_new_derv_rederive_c_c3f4d0e8ef218854->insert(tuple,READ_OP_CONTEXT(rel_inc_new_derv_rederive_c_c3f4d0e8ef218854_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("c",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2DeltaDeleteRuleApplications[untypedTuple];
auto*& ruleSet2 = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
if (ruleSet2 == nullptr) {
ruleSet2 = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
varValues.emplace_back(ramBitCast(env0[0]));
RuleApplication ruleApplication{5, varValues};
ruleSet->erase(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
if(rel_inc_new_derv_rederive_a_cf31a1985b633fe9->empty() && rel_inc_new_derv_rederive_b_85af71cbeb32637e->empty() && rel_inc_new_derv_rederive_c_c3f4d0e8ef218854->empty()) break;
rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7->purge();
for(const auto& tupleDeltaDervInsert: *rel_inc_new_derv_rederive_a_cf31a1985b633fe9) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("a",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& insertedTuple: *rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7) {

rel_a_454ddee488c1ed6b->insert(insertedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7_op_ctxt,rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_a_3943091945425515->erase(tuple);
}
}
();rel_inc_new_derv_rederive_a_cf31a1985b633fe9->purge();
rel_inc_delta_tuple_rederive_b_29aee4d2a2657817->purge();
for(const auto& tupleDeltaDervInsert: *rel_inc_new_derv_rederive_b_85af71cbeb32637e) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("b",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_inc_delta_tuple_rederive_b_29aee4d2a2657817->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& insertedTuple: *rel_inc_delta_tuple_rederive_b_29aee4d2a2657817) {

rel_b_96694f8e93f5c77d->insert(insertedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_rederive_b_29aee4d2a2657817_op_ctxt,rel_inc_delta_tuple_rederive_b_29aee4d2a2657817->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_rederive_b_29aee4d2a2657817) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_b_f9480aa9bb17885d->erase(tuple);
}
}
();rel_inc_new_derv_rederive_b_85af71cbeb32637e->purge();
rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263->purge();
for(const auto& tupleDeltaDervInsert: *rel_inc_new_derv_rederive_c_c3f4d0e8ef218854) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("c",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& insertedTuple: *rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263) {

rel_c_981811ba2479fc8d->insert(insertedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263_op_ctxt,rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_tuple_overdelete_c_1ebbcf7f486522d8->erase(tuple);
}
}
();rel_inc_new_derv_rederive_c_c3f4d0e8ef218854->purge();
loop_counter_rederive = (ramBitCast<RamUnsigned>(loop_counter_rederive) + ramBitCast<RamUnsigned>(RamUnsigned(1)));
iter++;
}
iter = 0;
rel_inc_delta_tuple_delete_a_58354ad400e6cd67->purge();
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt,rel_inc_delta_tuple_delete_a_58354ad400e6cd67->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_a_3943091945425515_op_ctxt,rel_inc_tuple_overdelete_a_3943091945425515->createContext());
for(const auto& env0 : *rel_inc_tuple_overdelete_a_3943091945425515) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_a_58354ad400e6cd67->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt));
}
}
();rel_inc_new_derv_rederive_a_cf31a1985b633fe9->purge();
rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7->purge();
rel_inc_tuple_overdelete_a_3943091945425515->purge();
rel_inc_derv_overdelete_a_45448aaf3278243c->purge();
rel_inc_delta_tuple_delete_b_1b345b26cae73383->purge();
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt,rel_inc_delta_tuple_delete_b_1b345b26cae73383->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_b_f9480aa9bb17885d_op_ctxt,rel_inc_tuple_overdelete_b_f9480aa9bb17885d->createContext());
for(const auto& env0 : *rel_inc_tuple_overdelete_b_f9480aa9bb17885d) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_b_1b345b26cae73383->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt));
}
}
();rel_inc_new_derv_rederive_b_85af71cbeb32637e->purge();
rel_inc_delta_tuple_rederive_b_29aee4d2a2657817->purge();
rel_inc_tuple_overdelete_b_f9480aa9bb17885d->purge();
rel_inc_derv_overdelete_b_2df625d9f0a591a4->purge();
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->purge();
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt,rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->createContext());
CREATE_OP_CONTEXT(rel_inc_tuple_overdelete_c_1ebbcf7f486522d8_op_ctxt,rel_inc_tuple_overdelete_c_1ebbcf7f486522d8->createContext());
for(const auto& env0 : *rel_inc_tuple_overdelete_c_1ebbcf7f486522d8) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt));
}
}
();rel_inc_new_derv_rederive_c_c3f4d0e8ef218854->purge();
rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263->purge();
rel_inc_tuple_overdelete_c_1ebbcf7f486522d8->purge();
rel_inc_derv_overdelete_c_b4a21678aced1fb3->purge();
signalHandler->setMsg(R"_(a(X) :- 
   s(X).
in file test.dl [15:6-15:19])_");
if(!(rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_insert_a_bbea135139bb89ae_op_ctxt,rel_inc_delta_derv_insert_a_bbea135139bb89ae->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0_op_ctxt,rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_derv_insert_a_bbea135139bb89ae->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_derv_insert_a_bbea135139bb89ae_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("a",tuple);
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
for(const auto& tupleDeltaDervInsert: *rel_inc_delta_derv_insert_a_bbea135139bb89ae) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("a",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_inc_delta_tuple_insert_a_88895cba4fbff038->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& insertedTuple: *rel_inc_delta_tuple_insert_a_88895cba4fbff038) {

rel_a_454ddee488c1ed6b->insert(insertedTuple);

}

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
for(const auto& insertedTuple: *rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46) {

rel_b_96694f8e93f5c77d->insert(insertedTuple);

}

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
for(const auto& insertedTuple: *rel_inc_delta_tuple_insert_c_70adcfa1afd70315) {

rel_c_981811ba2479fc8d->insert(insertedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_a_88895cba4fbff038_op_ctxt,rel_inc_delta_tuple_insert_a_88895cba4fbff038->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_insert_a_419a5d6ce8e56c01_op_ctxt,rel_delta_tuple_insert_a_419a5d6ce8e56c01->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_insert_a_88895cba4fbff038) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_delta_tuple_insert_a_419a5d6ce8e56c01->insert(tuple,READ_OP_CONTEXT(rel_delta_tuple_insert_a_419a5d6ce8e56c01_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46_op_ctxt,rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_insert_b_a88502da93017be6_op_ctxt,rel_delta_tuple_insert_b_a88502da93017be6->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_delta_tuple_insert_b_a88502da93017be6->insert(tuple,READ_OP_CONTEXT(rel_delta_tuple_insert_b_a88502da93017be6_op_ctxt));
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_c_70adcfa1afd70315_op_ctxt,rel_inc_delta_tuple_insert_c_70adcfa1afd70315->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_insert_c_02751d71f015bc27_op_ctxt,rel_delta_tuple_insert_c_02751d71f015bc27->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_insert_c_70adcfa1afd70315) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_delta_tuple_insert_c_02751d71f015bc27->insert(tuple,READ_OP_CONTEXT(rel_delta_tuple_insert_c_02751d71f015bc27_op_ctxt));
}
}
();signalHandler->setMsg(R"_(a(X) :- 
   c(X).
in file test.dl [19:6-19:19])_");
if(!(rel_delta_tuple_insert_c_02751d71f015bc27->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_insert_c_02751d71f015bc27_op_ctxt,rel_delta_tuple_insert_c_02751d71f015bc27->createContext());
CREATE_OP_CONTEXT(rel_new_derv_insert_a_cb7a1502e8af1907_op_ctxt,rel_new_derv_insert_a_cb7a1502e8af1907->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
if(!(rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_insert_c_02751d71f015bc27) {
if( !((rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("a", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{2,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_insert_a_cb7a1502e8af1907->insert(tuple,READ_OP_CONTEXT(rel_new_derv_insert_a_cb7a1502e8af1907_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("a",tuple);
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
RuleApplication ruleApplication{2, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   a(X).
in file test.dl [17:6-17:19])_");
if(!(rel_delta_tuple_insert_a_419a5d6ce8e56c01->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_insert_a_419a5d6ce8e56c01_op_ctxt,rel_delta_tuple_insert_a_419a5d6ce8e56c01->createContext());
CREATE_OP_CONTEXT(rel_new_derv_insert_b_8572f4c197f8bd21_op_ctxt,rel_new_derv_insert_b_8572f4c197f8bd21->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_insert_a_419a5d6ce8e56c01) {
if( !((rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{3,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_insert_b_8572f4c197f8bd21->insert(tuple,READ_OP_CONTEXT(rel_new_derv_insert_b_8572f4c197f8bd21_op_ctxt));
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
RuleApplication ruleApplication{3, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   c(X).
in file test.dl [21:6-21:19])_");
if(!(rel_delta_tuple_insert_c_02751d71f015bc27->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_insert_c_02751d71f015bc27_op_ctxt,rel_delta_tuple_insert_c_02751d71f015bc27->createContext());
CREATE_OP_CONTEXT(rel_new_derv_insert_b_8572f4c197f8bd21_op_ctxt,rel_new_derv_insert_b_8572f4c197f8bd21->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_insert_c_02751d71f015bc27) {
if( !((rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{4,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_insert_b_8572f4c197f8bd21->insert(tuple,READ_OP_CONTEXT(rel_new_derv_insert_b_8572f4c197f8bd21_op_ctxt));
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
RuleApplication ruleApplication{4, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
signalHandler->setMsg(R"_(c(X) :- 
   b(X).
in file test.dl [18:6-18:19])_");
if(!(rel_delta_tuple_insert_b_a88502da93017be6->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_insert_b_a88502da93017be6_op_ctxt,rel_delta_tuple_insert_b_a88502da93017be6->createContext());
CREATE_OP_CONTEXT(rel_new_derv_insert_c_92944d39136b7981_op_ctxt,rel_new_derv_insert_c_92944d39136b7981->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
if(!(rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_insert_b_a88502da93017be6) {
if( !((rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("c", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{5,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_insert_c_92944d39136b7981->insert(tuple,READ_OP_CONTEXT(rel_new_derv_insert_c_92944d39136b7981_op_ctxt));
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
RuleApplication ruleApplication{5, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
rel_delta_tuple_insert_a_419a5d6ce8e56c01->purge();
for(const auto& tupleDeltaDervInsert: *rel_new_derv_insert_a_cb7a1502e8af1907) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("a",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_delta_tuple_insert_a_419a5d6ce8e56c01->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& insertedTuple: *rel_delta_tuple_insert_a_419a5d6ce8e56c01) {

rel_a_454ddee488c1ed6b->insert(insertedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_a_88895cba4fbff038_op_ctxt,rel_inc_delta_tuple_insert_a_88895cba4fbff038->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_insert_a_419a5d6ce8e56c01_op_ctxt,rel_delta_tuple_insert_a_419a5d6ce8e56c01->createContext());
for(const auto& env0 : *rel_delta_tuple_insert_a_419a5d6ce8e56c01) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_insert_a_88895cba4fbff038->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_insert_a_88895cba4fbff038_op_ctxt));
}
}
();rel_new_derv_insert_a_cb7a1502e8af1907->purge();
rel_delta_tuple_insert_b_a88502da93017be6->purge();
for(const auto& tupleDeltaDervInsert: *rel_new_derv_insert_b_8572f4c197f8bd21) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("b",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_delta_tuple_insert_b_a88502da93017be6->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& insertedTuple: *rel_delta_tuple_insert_b_a88502da93017be6) {

rel_b_96694f8e93f5c77d->insert(insertedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46_op_ctxt,rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_insert_b_a88502da93017be6_op_ctxt,rel_delta_tuple_insert_b_a88502da93017be6->createContext());
for(const auto& env0 : *rel_delta_tuple_insert_b_a88502da93017be6) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46_op_ctxt));
}
}
();rel_new_derv_insert_b_8572f4c197f8bd21->purge();
rel_delta_tuple_insert_c_02751d71f015bc27->purge();
for(const auto& tupleDeltaDervInsert: *rel_new_derv_insert_c_92944d39136b7981) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("c",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_delta_tuple_insert_c_02751d71f015bc27->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& insertedTuple: *rel_delta_tuple_insert_c_02751d71f015bc27) {

rel_c_981811ba2479fc8d->insert(insertedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_c_70adcfa1afd70315_op_ctxt,rel_inc_delta_tuple_insert_c_70adcfa1afd70315->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_insert_c_02751d71f015bc27_op_ctxt,rel_delta_tuple_insert_c_02751d71f015bc27->createContext());
for(const auto& env0 : *rel_delta_tuple_insert_c_02751d71f015bc27) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_insert_c_70adcfa1afd70315->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_insert_c_70adcfa1afd70315_op_ctxt));
}
}
();rel_new_derv_insert_c_92944d39136b7981->purge();
auto loop_counter2 = RamUnsigned(1);
iter = 0;
for(;;) {
signalHandler->setMsg(R"_(a(X) :- 
   c(X).
in file test.dl [19:6-19:19])_");
if(!(rel_delta_tuple_insert_c_02751d71f015bc27->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_insert_c_02751d71f015bc27_op_ctxt,rel_delta_tuple_insert_c_02751d71f015bc27->createContext());
CREATE_OP_CONTEXT(rel_new_derv_insert_a_cb7a1502e8af1907_op_ctxt,rel_new_derv_insert_a_cb7a1502e8af1907->createContext());
CREATE_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt,rel_a_454ddee488c1ed6b->createContext());
if(!(rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_insert_c_02751d71f015bc27) {
if( !((rel_a_454ddee488c1ed6b->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_a_454ddee488c1ed6b_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("a", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{2,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_insert_a_cb7a1502e8af1907->insert(tuple,READ_OP_CONTEXT(rel_new_derv_insert_a_cb7a1502e8af1907_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("a",tuple);
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
RuleApplication ruleApplication{2, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   a(X).
in file test.dl [17:6-17:19])_");
if(!(rel_delta_tuple_insert_a_419a5d6ce8e56c01->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_insert_a_419a5d6ce8e56c01_op_ctxt,rel_delta_tuple_insert_a_419a5d6ce8e56c01->createContext());
CREATE_OP_CONTEXT(rel_new_derv_insert_b_8572f4c197f8bd21_op_ctxt,rel_new_derv_insert_b_8572f4c197f8bd21->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_insert_a_419a5d6ce8e56c01) {
if( !((rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{3,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_insert_b_8572f4c197f8bd21->insert(tuple,READ_OP_CONTEXT(rel_new_derv_insert_b_8572f4c197f8bd21_op_ctxt));
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
RuleApplication ruleApplication{3, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
signalHandler->setMsg(R"_(b(X) :- 
   c(X).
in file test.dl [21:6-21:19])_");
if(!(rel_delta_tuple_insert_c_02751d71f015bc27->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_insert_c_02751d71f015bc27_op_ctxt,rel_delta_tuple_insert_c_02751d71f015bc27->createContext());
CREATE_OP_CONTEXT(rel_new_derv_insert_b_8572f4c197f8bd21_op_ctxt,rel_new_derv_insert_b_8572f4c197f8bd21->createContext());
CREATE_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt,rel_b_96694f8e93f5c77d->createContext());
if(!(rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_insert_c_02751d71f015bc27) {
if( !((rel_b_96694f8e93f5c77d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_b_96694f8e93f5c77d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("b", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{4,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_insert_b_8572f4c197f8bd21->insert(tuple,READ_OP_CONTEXT(rel_new_derv_insert_b_8572f4c197f8bd21_op_ctxt));
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
RuleApplication ruleApplication{4, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
signalHandler->setMsg(R"_(c(X) :- 
   b(X).
in file test.dl [18:6-18:19])_");
if(!(rel_delta_tuple_insert_b_a88502da93017be6->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_delta_tuple_insert_b_a88502da93017be6_op_ctxt,rel_delta_tuple_insert_b_a88502da93017be6->createContext());
CREATE_OP_CONTEXT(rel_new_derv_insert_c_92944d39136b7981_op_ctxt,rel_new_derv_insert_c_92944d39136b7981->createContext());
CREATE_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt,rel_c_981811ba2479fc8d->createContext());
if(!(rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))) {
for(const auto& env0 : *rel_delta_tuple_insert_b_a88502da93017be6) {
if( !((rel_c_981811ba2479fc8d->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_c_981811ba2479fc8d_op_ctxt)))&& ((DerivationManager::ruleAppExistsInCompleteSet(UntypedTuple::fromTypedTuple("c", Tuple<RamDomain,1>{{ramBitCast(env0[0])}}),RuleApplication{5,{ramBitCast(env0[0])}}))))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_new_derv_insert_c_92944d39136b7981->insert(tuple,READ_OP_CONTEXT(rel_new_derv_insert_c_92944d39136b7981_op_ctxt));
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
RuleApplication ruleApplication{5, varValues};
ruleSet->insert(ruleApplication);
ruleSet2->insert(ruleApplication);
}
}
}
}
();}
if(rel_new_derv_insert_a_cb7a1502e8af1907->empty() && rel_new_derv_insert_b_8572f4c197f8bd21->empty() && rel_new_derv_insert_c_92944d39136b7981->empty()) break;
rel_delta_tuple_insert_a_419a5d6ce8e56c01->purge();
for(const auto& tupleDeltaDervInsert: *rel_new_derv_insert_a_cb7a1502e8af1907) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("a",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_delta_tuple_insert_a_419a5d6ce8e56c01->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& insertedTuple: *rel_delta_tuple_insert_a_419a5d6ce8e56c01) {

rel_a_454ddee488c1ed6b->insert(insertedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_a_88895cba4fbff038_op_ctxt,rel_inc_delta_tuple_insert_a_88895cba4fbff038->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_insert_a_419a5d6ce8e56c01_op_ctxt,rel_delta_tuple_insert_a_419a5d6ce8e56c01->createContext());
for(const auto& env0 : *rel_delta_tuple_insert_a_419a5d6ce8e56c01) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_insert_a_88895cba4fbff038->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_insert_a_88895cba4fbff038_op_ctxt));
}
}
();rel_new_derv_insert_a_cb7a1502e8af1907->purge();
rel_delta_tuple_insert_b_a88502da93017be6->purge();
for(const auto& tupleDeltaDervInsert: *rel_new_derv_insert_b_8572f4c197f8bd21) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("b",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_delta_tuple_insert_b_a88502da93017be6->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& insertedTuple: *rel_delta_tuple_insert_b_a88502da93017be6) {

rel_b_96694f8e93f5c77d->insert(insertedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46_op_ctxt,rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_insert_b_a88502da93017be6_op_ctxt,rel_delta_tuple_insert_b_a88502da93017be6->createContext());
for(const auto& env0 : *rel_delta_tuple_insert_b_a88502da93017be6) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46_op_ctxt));
}
}
();rel_new_derv_insert_b_8572f4c197f8bd21->purge();
rel_delta_tuple_insert_c_02751d71f015bc27->purge();
for(const auto& tupleDeltaDervInsert: *rel_new_derv_insert_c_92944d39136b7981) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("c",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_delta_tuple_insert_c_02751d71f015bc27->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& insertedTuple: *rel_delta_tuple_insert_c_02751d71f015bc27) {

rel_c_981811ba2479fc8d->insert(insertedTuple);

}

[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_c_70adcfa1afd70315_op_ctxt,rel_inc_delta_tuple_insert_c_70adcfa1afd70315->createContext());
CREATE_OP_CONTEXT(rel_delta_tuple_insert_c_02751d71f015bc27_op_ctxt,rel_delta_tuple_insert_c_02751d71f015bc27->createContext());
for(const auto& env0 : *rel_delta_tuple_insert_c_02751d71f015bc27) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_insert_c_70adcfa1afd70315->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_insert_c_70adcfa1afd70315_op_ctxt));
}
}
();rel_new_derv_insert_c_92944d39136b7981->purge();
loop_counter2 = (ramBitCast<RamUnsigned>(loop_counter2) + ramBitCast<RamUnsigned>(RamUnsigned(1)));
iter++;
}
iter = 0;
rel_delta_tuple_insert_a_419a5d6ce8e56c01->purge();
rel_new_derv_insert_a_cb7a1502e8af1907->purge();
rel_delta_tuple_insert_b_a88502da93017be6->purge();
rel_new_derv_insert_b_8572f4c197f8bd21->purge();
rel_delta_tuple_insert_c_02751d71f015bc27->purge();
rel_new_derv_insert_c_92944d39136b7981->purge();
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","a"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
{
FunctionTimer timer("writing relation rel_a_454ddee488c1ed6b");
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_a_454ddee488c1ed6b);
}
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","b"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
{
FunctionTimer timer("writing relation rel_b_96694f8e93f5c77d");
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_b_96694f8e93f5c77d);
}
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","c"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
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
class Stratum_inc_table_update_8edbd431ea6611a3 {
public:
 Stratum_inc_table_update_8edbd431ea6611a3(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_a_394b3623655cbdd0,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_b_ded826b2e75f35bb,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_c_2409cf566a420e77,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_s_7d6bf28557a39eb2,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_a_58354ad400e6cd67,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_b_1b345b26cae73383,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_s_45ff968c958aa951,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_a_45448aaf3278243c,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_b_2df625d9f0a591a4,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_c_b4a21678aced1fb3,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_s_5c0aaf8f13aa36d3,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_a_3943091945425515,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_b_f9480aa9bb17885d,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_c_1ebbcf7f486522d8,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_s_7f73c56f85808368,t_btree_000_i__0__1::Type& rel_old_a_bd7865de58a1cd60,t_btree_000_i__0__1::Type& rel_old_b_38eb3a9d03faff34,t_btree_000_i__0__1::Type& rel_old_c_3e6edc8484191be0,t_btree_000_i__0__1::Type& rel_old_s_b5d15abca01ce135,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_i__0__1::Type& rel_s_59046bdf5c263ae2);
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
t_btree_000_i__0__1::Type* rel_inc_delta_derv_delete_a_394b3623655cbdd0;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_delete_b_ded826b2e75f35bb;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_delete_c_2409cf566a420e77;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_delete_s_7d6bf28557a39eb2;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_a_58354ad400e6cd67;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_b_1b345b26cae73383;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_c_43fa1164ffebfc11;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_s_45ff968c958aa951;
t_btree_000_i__0__1::Type* rel_inc_derv_overdelete_a_45448aaf3278243c;
t_btree_000_i__0__1::Type* rel_inc_derv_overdelete_b_2df625d9f0a591a4;
t_btree_000_i__0__1::Type* rel_inc_derv_overdelete_c_b4a21678aced1fb3;
t_btree_000_i__0__1::Type* rel_inc_derv_overdelete_s_5c0aaf8f13aa36d3;
t_btree_100_i__0__1::Type* rel_inc_tuple_overdelete_a_3943091945425515;
t_btree_100_i__0__1::Type* rel_inc_tuple_overdelete_b_f9480aa9bb17885d;
t_btree_100_i__0__1::Type* rel_inc_tuple_overdelete_c_1ebbcf7f486522d8;
t_btree_100_i__0__1::Type* rel_inc_tuple_overdelete_s_7f73c56f85808368;
t_btree_000_i__0__1::Type* rel_old_a_bd7865de58a1cd60;
t_btree_000_i__0__1::Type* rel_old_b_38eb3a9d03faff34;
t_btree_000_i__0__1::Type* rel_old_c_3e6edc8484191be0;
t_btree_000_i__0__1::Type* rel_old_s_b5d15abca01ce135;
t_btree_100_i__0__1::Type* rel_a_454ddee488c1ed6b;
t_btree_100_i__0__1::Type* rel_b_96694f8e93f5c77d;
t_btree_100_i__0__1::Type* rel_c_981811ba2479fc8d;
t_btree_100_i__0__1::Type* rel_s_59046bdf5c263ae2;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_inc_table_update_8edbd431ea6611a3::Stratum_inc_table_update_8edbd431ea6611a3(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_a_394b3623655cbdd0,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_b_ded826b2e75f35bb,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_c_2409cf566a420e77,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_s_7d6bf28557a39eb2,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_a_58354ad400e6cd67,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_b_1b345b26cae73383,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_s_45ff968c958aa951,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_a_45448aaf3278243c,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_b_2df625d9f0a591a4,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_c_b4a21678aced1fb3,t_btree_000_i__0__1::Type& rel_inc_derv_overdelete_s_5c0aaf8f13aa36d3,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_a_3943091945425515,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_b_f9480aa9bb17885d,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_c_1ebbcf7f486522d8,t_btree_100_i__0__1::Type& rel_inc_tuple_overdelete_s_7f73c56f85808368,t_btree_000_i__0__1::Type& rel_old_a_bd7865de58a1cd60,t_btree_000_i__0__1::Type& rel_old_b_38eb3a9d03faff34,t_btree_000_i__0__1::Type& rel_old_c_3e6edc8484191be0,t_btree_000_i__0__1::Type& rel_old_s_b5d15abca01ce135,t_btree_100_i__0__1::Type& rel_a_454ddee488c1ed6b,t_btree_100_i__0__1::Type& rel_b_96694f8e93f5c77d,t_btree_100_i__0__1::Type& rel_c_981811ba2479fc8d,t_btree_100_i__0__1::Type& rel_s_59046bdf5c263ae2):
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
rel_inc_delta_derv_delete_a_394b3623655cbdd0(&rel_inc_delta_derv_delete_a_394b3623655cbdd0),
rel_inc_delta_derv_delete_b_ded826b2e75f35bb(&rel_inc_delta_derv_delete_b_ded826b2e75f35bb),
rel_inc_delta_derv_delete_c_2409cf566a420e77(&rel_inc_delta_derv_delete_c_2409cf566a420e77),
rel_inc_delta_derv_delete_s_7d6bf28557a39eb2(&rel_inc_delta_derv_delete_s_7d6bf28557a39eb2),
rel_inc_delta_tuple_delete_a_58354ad400e6cd67(&rel_inc_delta_tuple_delete_a_58354ad400e6cd67),
rel_inc_delta_tuple_delete_b_1b345b26cae73383(&rel_inc_delta_tuple_delete_b_1b345b26cae73383),
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11(&rel_inc_delta_tuple_delete_c_43fa1164ffebfc11),
rel_inc_delta_tuple_delete_s_45ff968c958aa951(&rel_inc_delta_tuple_delete_s_45ff968c958aa951),
rel_inc_derv_overdelete_a_45448aaf3278243c(&rel_inc_derv_overdelete_a_45448aaf3278243c),
rel_inc_derv_overdelete_b_2df625d9f0a591a4(&rel_inc_derv_overdelete_b_2df625d9f0a591a4),
rel_inc_derv_overdelete_c_b4a21678aced1fb3(&rel_inc_derv_overdelete_c_b4a21678aced1fb3),
rel_inc_derv_overdelete_s_5c0aaf8f13aa36d3(&rel_inc_derv_overdelete_s_5c0aaf8f13aa36d3),
rel_inc_tuple_overdelete_a_3943091945425515(&rel_inc_tuple_overdelete_a_3943091945425515),
rel_inc_tuple_overdelete_b_f9480aa9bb17885d(&rel_inc_tuple_overdelete_b_f9480aa9bb17885d),
rel_inc_tuple_overdelete_c_1ebbcf7f486522d8(&rel_inc_tuple_overdelete_c_1ebbcf7f486522d8),
rel_inc_tuple_overdelete_s_7f73c56f85808368(&rel_inc_tuple_overdelete_s_7f73c56f85808368),
rel_old_a_bd7865de58a1cd60(&rel_old_a_bd7865de58a1cd60),
rel_old_b_38eb3a9d03faff34(&rel_old_b_38eb3a9d03faff34),
rel_old_c_3e6edc8484191be0(&rel_old_c_3e6edc8484191be0),
rel_old_s_b5d15abca01ce135(&rel_old_s_b5d15abca01ce135),
rel_a_454ddee488c1ed6b(&rel_a_454ddee488c1ed6b),
rel_b_96694f8e93f5c77d(&rel_b_96694f8e93f5c77d),
rel_c_981811ba2479fc8d(&rel_c_981811ba2479fc8d),
rel_s_59046bdf5c263ae2(&rel_s_59046bdf5c263ae2){
}

void Stratum_inc_table_update_8edbd431ea6611a3::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
rel_old_s_b5d15abca01ce135->purge();
[&](){
CREATE_OP_CONTEXT(rel_old_s_b5d15abca01ce135_op_ctxt,rel_old_s_b5d15abca01ce135->createContext());
CREATE_OP_CONTEXT(rel_s_59046bdf5c263ae2_op_ctxt,rel_s_59046bdf5c263ae2->createContext());
for(const auto& env0 : *rel_s_59046bdf5c263ae2) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_old_s_b5d15abca01ce135->insert(tuple,READ_OP_CONTEXT(rel_old_s_b5d15abca01ce135_op_ctxt));
}
}
();rel_inc_tuple_overdelete_s_7f73c56f85808368->purge();
rel_inc_derv_overdelete_s_5c0aaf8f13aa36d3->purge();
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_s_7d6bf28557a39eb2_op_ctxt,rel_inc_delta_derv_delete_s_7d6bf28557a39eb2->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_s_45ff968c958aa951_op_ctxt,rel_inc_delta_tuple_delete_s_45ff968c958aa951->createContext());
for(const auto& env0 : *rel_inc_delta_derv_delete_s_7d6bf28557a39eb2) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_s_45ff968c958aa951->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_s_45ff968c958aa951_op_ctxt));
}
}
();rel_old_a_bd7865de58a1cd60->purge();
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
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_a_394b3623655cbdd0_op_ctxt,rel_inc_delta_derv_delete_a_394b3623655cbdd0->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt,rel_inc_delta_tuple_delete_a_58354ad400e6cd67->createContext());
for(const auto& env0 : *rel_inc_delta_derv_delete_a_394b3623655cbdd0) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_a_58354ad400e6cd67->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_a_58354ad400e6cd67_op_ctxt));
}
}
();rel_old_b_38eb3a9d03faff34->purge();
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
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_b_ded826b2e75f35bb_op_ctxt,rel_inc_delta_derv_delete_b_ded826b2e75f35bb->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt,rel_inc_delta_tuple_delete_b_1b345b26cae73383->createContext());
for(const auto& env0 : *rel_inc_delta_derv_delete_b_ded826b2e75f35bb) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_b_1b345b26cae73383->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_b_1b345b26cae73383_op_ctxt));
}
}
();rel_old_c_3e6edc8484191be0->purge();
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
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_derv_delete_c_2409cf566a420e77_op_ctxt,rel_inc_delta_derv_delete_c_2409cf566a420e77->createContext());
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt,rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->createContext());
for(const auto& env0 : *rel_inc_delta_derv_delete_c_2409cf566a420e77) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11->insert(tuple,READ_OP_CONTEXT(rel_inc_delta_tuple_delete_c_43fa1164ffebfc11_op_ctxt));
}
}
();}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_s_b2449a0dcafe0672 {
public:
 Stratum_s_b2449a0dcafe0672(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_i__0__1::Type& rel_s_59046bdf5c263ae2);
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
t_btree_100_i__0__1::Type* rel_s_59046bdf5c263ae2;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_s_b2449a0dcafe0672::Stratum_s_b2449a0dcafe0672(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_100_i__0__1::Type& rel_s_59046bdf5c263ae2):
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
rel_s_59046bdf5c263ae2(&rel_s_59046bdf5c263ae2){
}

void Stratum_s_b2449a0dcafe0672::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"cache","true"},{"fact-dir","./data"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","s"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!inputDirectory.empty()) {directiveMap["fact-dir"] = inputDirectory;}
{
FunctionTimer timer("reading relation rel_s_59046bdf5c263ae2");
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_s_59046bdf5c263ae2);
for (auto& tuple: *rel_s_59046bdf5c263ae2) {
auto untypedTuple = UntypedTuple::fromTypedTuple("s",tuple);
inputFactSet.insert(untypedTuple);
initialInputRelations["s"].insert(untypedTuple);
}
}
} catch (std::exception& e) {std::cerr << "Error loading s data: " << e.what() << '\n';
exit(1);
}
}
signalHandler->setMsg(R"_(s(1).
in file test.dl [13:1-13:11])_");
[&](){
CREATE_OP_CONTEXT(rel_s_59046bdf5c263ae2_op_ctxt,rel_s_59046bdf5c263ae2->createContext());
Tuple<RamDomain,1> tuple{{ramBitCast(RamSigned(1))}};
rel_s_59046bdf5c263ae2->insert(tuple,READ_OP_CONTEXT(rel_s_59046bdf5c263ae2_op_ctxt));
auto untypedTuple = UntypedTuple::fromTypedTuple("s",tuple);
auto*& ruleSet = DerivationManager::untypedTuple2RuleApplications[untypedTuple];
if (ruleSet == nullptr) {
ruleSet = new std::unordered_set<RuleApplication>();
}
std::vector<souffle::RamDomain> varValues{};
RuleApplication ruleApplication{6, varValues};
ruleSet->insert(ruleApplication);
}
();}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_s_inc_1c92ce0ad6d324da {
public:
 Stratum_s_inc_1c92ce0ad6d324da(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_s_7d6bf28557a39eb2,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_s_c3f23dace39b5066,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_s_45ff968c958aa951,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0,t_btree_000_i__0__1::Type& rel_old_s_b5d15abca01ce135,t_btree_100_i__0__1::Type& rel_s_59046bdf5c263ae2);
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
t_btree_000_i__0__1::Type* rel_inc_delta_derv_delete_s_7d6bf28557a39eb2;
t_btree_000_i__0__1::Type* rel_inc_delta_derv_insert_s_c3f23dace39b5066;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_delete_s_45ff968c958aa951;
t_btree_000_i__0__1::Type* rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0;
t_btree_000_i__0__1::Type* rel_old_s_b5d15abca01ce135;
t_btree_100_i__0__1::Type* rel_s_59046bdf5c263ae2;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_s_inc_1c92ce0ad6d324da::Stratum_s_inc_1c92ce0ad6d324da(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_inc_delta_derv_delete_s_7d6bf28557a39eb2,t_btree_000_i__0__1::Type& rel_inc_delta_derv_insert_s_c3f23dace39b5066,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_delete_s_45ff968c958aa951,t_btree_000_i__0__1::Type& rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0,t_btree_000_i__0__1::Type& rel_old_s_b5d15abca01ce135,t_btree_100_i__0__1::Type& rel_s_59046bdf5c263ae2):
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
rel_inc_delta_derv_delete_s_7d6bf28557a39eb2(&rel_inc_delta_derv_delete_s_7d6bf28557a39eb2),
rel_inc_delta_derv_insert_s_c3f23dace39b5066(&rel_inc_delta_derv_insert_s_c3f23dace39b5066),
rel_inc_delta_tuple_delete_s_45ff968c958aa951(&rel_inc_delta_tuple_delete_s_45ff968c958aa951),
rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0(&rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0),
rel_old_s_b5d15abca01ce135(&rel_old_s_b5d15abca01ce135),
rel_s_59046bdf5c263ae2(&rel_s_59046bdf5c263ae2){
}

void Stratum_s_inc_1c92ce0ad6d324da::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
rel_s_59046bdf5c263ae2->purge();
[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_delete_s_45ff968c958aa951_op_ctxt,rel_inc_delta_tuple_delete_s_45ff968c958aa951->createContext());
CREATE_OP_CONTEXT(rel_old_s_b5d15abca01ce135_op_ctxt,rel_old_s_b5d15abca01ce135->createContext());
CREATE_OP_CONTEXT(rel_s_59046bdf5c263ae2_op_ctxt,rel_s_59046bdf5c263ae2->createContext());
for(const auto& env0 : *rel_old_s_b5d15abca01ce135) {
if( !(rel_inc_delta_tuple_delete_s_45ff968c958aa951->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_inc_delta_tuple_delete_s_45ff968c958aa951_op_ctxt)))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_s_59046bdf5c263ae2->insert(tuple,READ_OP_CONTEXT(rel_s_59046bdf5c263ae2_op_ctxt));
}
}
}
();[&](){
CREATE_OP_CONTEXT(rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0_op_ctxt,rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0->createContext());
CREATE_OP_CONTEXT(rel_s_59046bdf5c263ae2_op_ctxt,rel_s_59046bdf5c263ae2->createContext());
for(const auto& env0 : *rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_s_59046bdf5c263ae2->insert(tuple,READ_OP_CONTEXT(rel_s_59046bdf5c263ae2_op_ctxt));
}
}
();signalHandler->setMsg(R"_(s(1).
in file test.dl [13:1-13:11])_");
[&](){
CREATE_OP_CONTEXT(rel_s_59046bdf5c263ae2_op_ctxt,rel_s_59046bdf5c263ae2->createContext());
Tuple<RamDomain,1> tuple{{ramBitCast(RamSigned(1))}};
rel_s_59046bdf5c263ae2->insert(tuple,READ_OP_CONTEXT(rel_s_59046bdf5c263ae2_op_ctxt));
}
();for(const auto& tupleDeltaDervInsert: *rel_inc_delta_derv_insert_s_c3f23dace39b5066) {
auto untypedDeltaDervTupleInsert = UntypedTuple::fromTypedTuple("s",tupleDeltaDervInsert);
auto*& untypedDeltaDervTupleInsertRuleSet = DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications[untypedDeltaDervTupleInsert];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleInsert];

if (untypedDeltaDervTupleRuleSet == nullptr) {
untypedDeltaDervTupleRuleSet = untypedDeltaDervTupleInsertRuleSet;

if(!isInputFact(untypedDeltaDervTupleInsert))
rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0->insert(tupleDeltaDervInsert);
 
} else {
untypedDeltaDervTupleRuleSet->insert(untypedDeltaDervTupleInsertRuleSet->begin(), untypedDeltaDervTupleInsertRuleSet->end());

}
}
DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
for(const auto& tupleDeltaDervDelete: *rel_inc_delta_derv_delete_s_7d6bf28557a39eb2) {
auto untypedDeltaDervTupleDelete = UntypedTuple::fromTypedTuple("s",tupleDeltaDervDelete);
auto*& untypedDeltaDervTupleDeleteRuleSet = DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications[untypedDeltaDervTupleDelete];

auto*& untypedDeltaDervTupleRuleSet = DerivationManager::untypedTuple2RuleApplications[untypedDeltaDervTupleDelete];

for(const auto& deletedRuleAppl: *untypedDeltaDervTupleDeleteRuleSet) {
untypedDeltaDervTupleRuleSet->erase(deletedRuleAppl);

}
if (untypedDeltaDervTupleRuleSet->empty()) {
delete untypedDeltaDervTupleRuleSet;

DerivationManager::untypedTuple2RuleApplications.erase(untypedDeltaDervTupleDelete);

if(!isInputFact(untypedDeltaDervTupleDelete))
rel_inc_delta_tuple_delete_s_45ff968c958aa951->insert(tupleDeltaDervDelete);
 
}
}
DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
for(const auto& deletedTuple: *rel_inc_delta_tuple_delete_s_45ff968c958aa951) {

rel_s_59046bdf5c263ae2->erase(deletedTuple);

}

for(const auto& insertedTuple: *rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0) {

rel_s_59046bdf5c263ae2->insert(insertedTuple);

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
Own<t_btree_100_i__0__1::Type> rel_s_59046bdf5c263ae2;
souffle::RelationWrapper<t_btree_100_i__0__1::Type> wrapper_rel_s_59046bdf5c263ae2;
Own<t_btree_000_i__0__1::Type> rel_old_s_b5d15abca01ce135;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_derv_insert_s_c3f23dace39b5066;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_derv_insert_s_c3f23dace39b5066;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_derv_delete_s_7d6bf28557a39eb2;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_derv_delete_s_7d6bf28557a39eb2;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_delete_s_45ff968c958aa951;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_inc_delta_tuple_delete_s_45ff968c958aa951;
Own<t_btree_000_i__0__1::Type> rel_tmp_s_7c108f37b26319d0;
Own<t_btree_000_i__0__1::Type> rel_tmp2_s_e5f1466eabd0bbb3;
Own<t_btree_000_i__0__1::Type> rel_tmp3_s_4881ed89cf256c76;
Own<t_btree_000_i__0__1::Type> rel_tmp4_s_8cdafdca384014cb;
Own<t_btree_100_i__0__1::Type> rel_inc_tuple_overdelete_s_7f73c56f85808368;
Own<t_btree_000_i__0__1::Type> rel_inc_derv_overdelete_s_5c0aaf8f13aa36d3;
Own<t_btree_000_i__0__1::Type> rel_inc_new_derv_rederive_s_c78971a151cce535;
Own<t_btree_000_i__0__1::Type> rel_inc_delta_tuple_rederive_s_2f014c8c7f6d3d46;
Own<t_btree_000_i__0__1::Type> rel_delta_tuple_delete_s_56272c71eb18f1f7;
Own<t_btree_000_i__0__1::Type> rel_delta_tuple_insert_s_6df563ba8628b874;
Own<t_btree_000_i__0__1::Type> rel_new_derv_delete_s_8a6f90dc9d93c124;
Own<t_btree_000_i__0__1::Type> rel_new_derv_insert_s_f2f59d6c997958fb;
Own<t_btree_100_i__0__1::Type> rel_a_454ddee488c1ed6b;
souffle::RelationWrapper<t_btree_100_i__0__1::Type> wrapper_rel_a_454ddee488c1ed6b;
Own<t_btree_000_i__0__1::Type> rel_new_a_37dc61ccaf89f094;
Own<t_btree_000_i__0__1::Type> rel_delta_a_cb7db3f8f52e4070;
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
Own<t_btree_100_i__0__1::Type> rel_b_96694f8e93f5c77d;
souffle::RelationWrapper<t_btree_100_i__0__1::Type> wrapper_rel_b_96694f8e93f5c77d;
Own<t_btree_000_i__0__1::Type> rel_new_b_235be8f0cfe8fb69;
Own<t_btree_000_i__0__1::Type> rel_delta_b_936be2cc269dd2ee;
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
Own<t_btree_000_i__0__1::Type> rel_new_c_6216fae7a16c22cf;
Own<t_btree_000_i__0__1::Type> rel_delta_c_e10d7ef6690eede3;
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
Stratum_a_f5cc2531020019db stratum_a_f1f7b26a4b108ce3;
Stratum_a_inc_935a746942ed3383 stratum_a_inc_a6264e86fb843b31;
Stratum_inc_table_update_8edbd431ea6611a3 stratum_inc_table_update_da18684048fa1d45;
Stratum_s_b2449a0dcafe0672 stratum_s_595201a1c8044e6c;
Stratum_s_inc_1c92ce0ad6d324da stratum_s_inc_f37416214dcf74e9;
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
rel_s_59046bdf5c263ae2(mk<t_btree_100_i__0__1::Type>()),
wrapper_rel_s_59046bdf5c263ae2(0, *rel_s_59046bdf5c263ae2, *this, "s", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_old_s_b5d15abca01ce135(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_derv_insert_s_c3f23dace39b5066(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_insert_s_c3f23dace39b5066(1, *rel_inc_delta_derv_insert_s_c3f23dace39b5066, *this, "$inc_delta_derv_insert_s", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_derv_delete_s_7d6bf28557a39eb2(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_delete_s_7d6bf28557a39eb2(2, *rel_inc_delta_derv_delete_s_7d6bf28557a39eb2, *this, "$inc_delta_derv_delete_s", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0(3, *rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0, *this, "$inc_delta_tuple_insert_s", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_tuple_delete_s_45ff968c958aa951(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_delete_s_45ff968c958aa951(4, *rel_inc_delta_tuple_delete_s_45ff968c958aa951, *this, "$inc_delta_tuple_delete_s", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_tmp_s_7c108f37b26319d0(mk<t_btree_000_i__0__1::Type>()),
rel_tmp2_s_e5f1466eabd0bbb3(mk<t_btree_000_i__0__1::Type>()),
rel_tmp3_s_4881ed89cf256c76(mk<t_btree_000_i__0__1::Type>()),
rel_tmp4_s_8cdafdca384014cb(mk<t_btree_000_i__0__1::Type>()),
rel_inc_tuple_overdelete_s_7f73c56f85808368(mk<t_btree_100_i__0__1::Type>()),
rel_inc_derv_overdelete_s_5c0aaf8f13aa36d3(mk<t_btree_000_i__0__1::Type>()),
rel_inc_new_derv_rederive_s_c78971a151cce535(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_tuple_rederive_s_2f014c8c7f6d3d46(mk<t_btree_000_i__0__1::Type>()),
rel_delta_tuple_delete_s_56272c71eb18f1f7(mk<t_btree_000_i__0__1::Type>()),
rel_delta_tuple_insert_s_6df563ba8628b874(mk<t_btree_000_i__0__1::Type>()),
rel_new_derv_delete_s_8a6f90dc9d93c124(mk<t_btree_000_i__0__1::Type>()),
rel_new_derv_insert_s_f2f59d6c997958fb(mk<t_btree_000_i__0__1::Type>()),
rel_a_454ddee488c1ed6b(mk<t_btree_100_i__0__1::Type>()),
wrapper_rel_a_454ddee488c1ed6b(5, *rel_a_454ddee488c1ed6b, *this, "a", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_new_a_37dc61ccaf89f094(mk<t_btree_000_i__0__1::Type>()),
rel_delta_a_cb7db3f8f52e4070(mk<t_btree_000_i__0__1::Type>()),
rel_old_a_bd7865de58a1cd60(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_derv_insert_a_bbea135139bb89ae(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_insert_a_bbea135139bb89ae(6, *rel_inc_delta_derv_insert_a_bbea135139bb89ae, *this, "$inc_delta_derv_insert_a", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_derv_delete_a_394b3623655cbdd0(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_delete_a_394b3623655cbdd0(7, *rel_inc_delta_derv_delete_a_394b3623655cbdd0, *this, "$inc_delta_derv_delete_a", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_tuple_insert_a_88895cba4fbff038(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_insert_a_88895cba4fbff038(8, *rel_inc_delta_tuple_insert_a_88895cba4fbff038, *this, "$inc_delta_tuple_insert_a", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_tuple_delete_a_58354ad400e6cd67(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_delete_a_58354ad400e6cd67(9, *rel_inc_delta_tuple_delete_a_58354ad400e6cd67, *this, "$inc_delta_tuple_delete_a", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
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
rel_b_96694f8e93f5c77d(mk<t_btree_100_i__0__1::Type>()),
wrapper_rel_b_96694f8e93f5c77d(10, *rel_b_96694f8e93f5c77d, *this, "b", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_new_b_235be8f0cfe8fb69(mk<t_btree_000_i__0__1::Type>()),
rel_delta_b_936be2cc269dd2ee(mk<t_btree_000_i__0__1::Type>()),
rel_old_b_38eb3a9d03faff34(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_derv_insert_b_420980c115f01b29(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_insert_b_420980c115f01b29(11, *rel_inc_delta_derv_insert_b_420980c115f01b29, *this, "$inc_delta_derv_insert_b", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_derv_delete_b_ded826b2e75f35bb(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_delete_b_ded826b2e75f35bb(12, *rel_inc_delta_derv_delete_b_ded826b2e75f35bb, *this, "$inc_delta_derv_delete_b", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46(13, *rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46, *this, "$inc_delta_tuple_insert_b", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_tuple_delete_b_1b345b26cae73383(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_delete_b_1b345b26cae73383(14, *rel_inc_delta_tuple_delete_b_1b345b26cae73383, *this, "$inc_delta_tuple_delete_b", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
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
wrapper_rel_c_981811ba2479fc8d(15, *rel_c_981811ba2479fc8d, *this, "c", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_new_c_6216fae7a16c22cf(mk<t_btree_000_i__0__1::Type>()),
rel_delta_c_e10d7ef6690eede3(mk<t_btree_000_i__0__1::Type>()),
rel_old_c_3e6edc8484191be0(mk<t_btree_000_i__0__1::Type>()),
rel_inc_delta_derv_insert_c_0d24c4484987ca3f(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_insert_c_0d24c4484987ca3f(16, *rel_inc_delta_derv_insert_c_0d24c4484987ca3f, *this, "$inc_delta_derv_insert_c", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_derv_delete_c_2409cf566a420e77(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_derv_delete_c_2409cf566a420e77(17, *rel_inc_delta_derv_delete_c_2409cf566a420e77, *this, "$inc_delta_derv_delete_c", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_tuple_insert_c_70adcfa1afd70315(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_insert_c_70adcfa1afd70315(18, *rel_inc_delta_tuple_insert_c_70adcfa1afd70315, *this, "$inc_delta_tuple_insert_c", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_inc_delta_tuple_delete_c_43fa1164ffebfc11(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_inc_delta_tuple_delete_c_43fa1164ffebfc11(19, *rel_inc_delta_tuple_delete_c_43fa1164ffebfc11, *this, "$inc_delta_tuple_delete_c", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
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
stratum_a_f1f7b26a4b108ce3(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_delta_a_cb7db3f8f52e4070,*rel_delta_b_936be2cc269dd2ee,*rel_delta_c_e10d7ef6690eede3,*rel_new_a_37dc61ccaf89f094,*rel_new_b_235be8f0cfe8fb69,*rel_new_c_6216fae7a16c22cf,*rel_a_454ddee488c1ed6b,*rel_b_96694f8e93f5c77d,*rel_c_981811ba2479fc8d,*rel_s_59046bdf5c263ae2),
stratum_a_inc_a6264e86fb843b31(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_inc_delta_derv_delete_a_394b3623655cbdd0,*rel_inc_delta_derv_delete_b_ded826b2e75f35bb,*rel_inc_delta_derv_delete_c_2409cf566a420e77,*rel_inc_delta_derv_insert_a_bbea135139bb89ae,*rel_inc_delta_derv_insert_b_420980c115f01b29,*rel_inc_delta_derv_insert_c_0d24c4484987ca3f,*rel_inc_delta_tuple_delete_a_58354ad400e6cd67,*rel_inc_delta_tuple_delete_b_1b345b26cae73383,*rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,*rel_inc_delta_tuple_delete_s_45ff968c958aa951,*rel_inc_delta_tuple_insert_a_88895cba4fbff038,*rel_inc_delta_tuple_insert_b_b7b9ad5b7a6f1c46,*rel_inc_delta_tuple_insert_c_70adcfa1afd70315,*rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0,*rel_delta_tuple_delete_a_249185740a160373,*rel_delta_tuple_delete_b_43dfed04dd4422a4,*rel_delta_tuple_delete_c_99c900b677e7ae7a,*rel_delta_tuple_insert_a_419a5d6ce8e56c01,*rel_delta_tuple_insert_b_a88502da93017be6,*rel_delta_tuple_insert_c_02751d71f015bc27,*rel_inc_delta_tuple_rederive_a_727cfac7d4aba9f7,*rel_inc_delta_tuple_rederive_b_29aee4d2a2657817,*rel_inc_delta_tuple_rederive_c_1a0da8ba58cab263,*rel_inc_derv_overdelete_a_45448aaf3278243c,*rel_inc_derv_overdelete_b_2df625d9f0a591a4,*rel_inc_derv_overdelete_c_b4a21678aced1fb3,*rel_inc_new_derv_rederive_a_cf31a1985b633fe9,*rel_inc_new_derv_rederive_b_85af71cbeb32637e,*rel_inc_new_derv_rederive_c_c3f4d0e8ef218854,*rel_inc_tuple_overdelete_a_3943091945425515,*rel_inc_tuple_overdelete_b_f9480aa9bb17885d,*rel_inc_tuple_overdelete_c_1ebbcf7f486522d8,*rel_new_derv_delete_a_f6265c4636f36293,*rel_new_derv_delete_b_4199ee6bbba1324f,*rel_new_derv_delete_c_eb3a420fe21b36f5,*rel_new_derv_insert_a_cb7a1502e8af1907,*rel_new_derv_insert_b_8572f4c197f8bd21,*rel_new_derv_insert_c_92944d39136b7981,*rel_old_a_bd7865de58a1cd60,*rel_old_b_38eb3a9d03faff34,*rel_old_c_3e6edc8484191be0,*rel_a_454ddee488c1ed6b,*rel_b_96694f8e93f5c77d,*rel_c_981811ba2479fc8d,*rel_s_59046bdf5c263ae2),
stratum_inc_table_update_da18684048fa1d45(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_inc_delta_derv_delete_a_394b3623655cbdd0,*rel_inc_delta_derv_delete_b_ded826b2e75f35bb,*rel_inc_delta_derv_delete_c_2409cf566a420e77,*rel_inc_delta_derv_delete_s_7d6bf28557a39eb2,*rel_inc_delta_tuple_delete_a_58354ad400e6cd67,*rel_inc_delta_tuple_delete_b_1b345b26cae73383,*rel_inc_delta_tuple_delete_c_43fa1164ffebfc11,*rel_inc_delta_tuple_delete_s_45ff968c958aa951,*rel_inc_derv_overdelete_a_45448aaf3278243c,*rel_inc_derv_overdelete_b_2df625d9f0a591a4,*rel_inc_derv_overdelete_c_b4a21678aced1fb3,*rel_inc_derv_overdelete_s_5c0aaf8f13aa36d3,*rel_inc_tuple_overdelete_a_3943091945425515,*rel_inc_tuple_overdelete_b_f9480aa9bb17885d,*rel_inc_tuple_overdelete_c_1ebbcf7f486522d8,*rel_inc_tuple_overdelete_s_7f73c56f85808368,*rel_old_a_bd7865de58a1cd60,*rel_old_b_38eb3a9d03faff34,*rel_old_c_3e6edc8484191be0,*rel_old_s_b5d15abca01ce135,*rel_a_454ddee488c1ed6b,*rel_b_96694f8e93f5c77d,*rel_c_981811ba2479fc8d,*rel_s_59046bdf5c263ae2),
stratum_s_595201a1c8044e6c(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_s_59046bdf5c263ae2),
stratum_s_inc_f37416214dcf74e9(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_inc_delta_derv_delete_s_7d6bf28557a39eb2,*rel_inc_delta_derv_insert_s_c3f23dace39b5066,*rel_inc_delta_tuple_delete_s_45ff968c958aa951,*rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0,*rel_old_s_b5d15abca01ce135,*rel_s_59046bdf5c263ae2){
addRelation("s", wrapper_rel_s_59046bdf5c263ae2, true, false);
addRelation("$inc_delta_derv_insert_s", wrapper_rel_inc_delta_derv_insert_s_c3f23dace39b5066, false, false);
addRelation("$inc_delta_derv_delete_s", wrapper_rel_inc_delta_derv_delete_s_7d6bf28557a39eb2, false, false);
addRelation("$inc_delta_tuple_insert_s", wrapper_rel_inc_delta_tuple_insert_s_0c7fb58186d29ad0, false, false);
addRelation("$inc_delta_tuple_delete_s", wrapper_rel_inc_delta_tuple_delete_s_45ff968c958aa951, false, false);
addRelation("a", wrapper_rel_a_454ddee488c1ed6b, false, true);
addRelation("$inc_delta_derv_insert_a", wrapper_rel_inc_delta_derv_insert_a_bbea135139bb89ae, false, false);
addRelation("$inc_delta_derv_delete_a", wrapper_rel_inc_delta_derv_delete_a_394b3623655cbdd0, false, false);
addRelation("$inc_delta_tuple_insert_a", wrapper_rel_inc_delta_tuple_insert_a_88895cba4fbff038, false, false);
addRelation("$inc_delta_tuple_delete_a", wrapper_rel_inc_delta_tuple_delete_a_58354ad400e6cd67, false, false);
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
FunctionTimer timer("stratum_s");
 std::vector<RamDomain> args, ret;
stratum_s_595201a1c8044e6c.run(args, ret);
}
{
FunctionTimer timer("stratum_a");
 std::vector<RamDomain> args, ret;
stratum_a_f1f7b26a4b108ce3.run(args, ret);
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
FunctionTimer timer("stratum_s_inc");
 std::vector<RamDomain> args, ret;
stratum_s_inc_f37416214dcf74e9.run(args, ret);
}
{
FunctionTimer timer("stratum_a_inc");
 std::vector<RamDomain> args, ret;
stratum_a_inc_a6264e86fb843b31.run(args, ret);
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
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","a"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_a_454ddee488c1ed6b);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","b"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_b_96694f8e93f5c77d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","c"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_c_981811ba2479fc8d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","a"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_a_454ddee488c1ed6b);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","b"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_b_96694f8e93f5c77d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","c"},{"operation","output"},{"output-dir","./output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_c_981811ba2479fc8d);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

void Sf_compute::loadAll([[maybe_unused]] std::string inputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"cache","true"},{"fact-dir","./data"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","s"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_s_59046bdf5c263ae2);
} catch (std::exception& e) {std::cerr << "Error loading s data: " << e.what() << '\n';
exit(1);
}
}

void Sf_compute::loadAllExcept([[maybe_unused]] std::string inputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"cache","true"},{"fact-dir","./data"},{"inc-delete","false"},{"inc-insert","false"},{"incDelta","false"},{"name","s"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAllExcept(*rel_s_59046bdf5c263ae2, *rel_inc_delta_tuple_delete_s_45ff968c958aa951);
} catch (std::exception& e) {std::cerr << "Error loading with filters data: " << e.what() << '\n';
exit(1);
}
}

void Sf_compute::dumpInputs(){
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "s";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_s_59046bdf5c263ae2);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

void Sf_compute::dumpOutputs(){
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "a";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_a_454ddee488c1ed6b);
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
rwOperation["name"] = "a";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_a_454ddee488c1ed6b);
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
if (name == "inc_table_update") {
stratum_inc_table_update_da18684048fa1d45.run(args, ret);
return;}
if (name == "s") {
stratum_s_595201a1c8044e6c.run(args, ret);
return;}
if (name == "s_inc") {
stratum_s_inc_f37416214dcf74e9.run(args, ret);
return;}
fatal(("unknown subroutine " + name).c_str());
}

} // namespace  souffle
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
R"(./data)",
R"(./output)",
false,
R"()",
1, "log.txt", false,"inc");
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
debugger.startTurn();
debugger.startStage(StageKind::SEMINAIVE_FULL);
obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());
debugger.endStage();
try {
debugger.startStage(StageKind::IO_LOAD_FULL);
{
FunctionTimer timer("Reading fact probability from " + opt.getInputFileDir());
{
std::string rel = "s";
std::cout << "reading: " << opt.getInputFileDir() << "/" << rel << ".facts and " << opt.getInputFileDir() << "/" << rel << ".prob" << std::endl;
std::ifstream factFile(opt.getInputFileDir() + "/" + rel + ".facts");std::ifstream probFile(opt.getInputFileDir() + "/" + rel + ".prob");std::string factLine, probLine;while (std::getline(factFile, factLine) && std::getline(probFile, probLine)) {std::istringstream fs(factLine);std::istringstream ps(probLine);double prob; ps >> prob;
assert (prob >= 0 && prob <= 1);
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
const Atom rule1_head = Atom("a", std::vector<SymbolicField>{SymbolicField::makeVariable("X")});
const Atom atom_1_1 = Atom{"s", {SymbolicField::makeVariable("X"), }};
const Rule rule1 = Rule(1,rule1_head, {atom_1_1}, {"X"}, 0.200000, 0, 1);
const Atom rule2_head = Atom("a", std::vector<SymbolicField>{SymbolicField::makeVariable("X")});
const Atom atom_2_1 = Atom{"c", {SymbolicField::makeVariable("X"), }};
const Rule rule2 = Rule(2,rule2_head, {atom_2_1}, {"X"}, 0.500000, 1, 1);
const Atom rule3_head = Atom("b", std::vector<SymbolicField>{SymbolicField::makeVariable("X")});
const Atom atom_3_1 = Atom{"a", {SymbolicField::makeVariable("X"), }};
const Rule rule3 = Rule(3,rule3_head, {atom_3_1}, {"X"}, 0.300000, 1, 1);
const Atom rule4_head = Atom("b", std::vector<SymbolicField>{SymbolicField::makeVariable("X")});
const Atom atom_4_1 = Atom{"c", {SymbolicField::makeVariable("X"), }};
const Rule rule4 = Rule(4,rule4_head, {atom_4_1}, {"X"}, 0.600000, 1, 1);
const Atom rule5_head = Atom("c", std::vector<SymbolicField>{SymbolicField::makeVariable("X")});
const Atom atom_5_1 = Atom{"b", {SymbolicField::makeVariable("X"), }};
const Rule rule5 = Rule(5,rule5_head, {atom_5_1}, {"X"}, 0.400000, 1, 1);
const Atom rule6_head = Atom("s", std::vector<SymbolicField>{SymbolicField{1}});
const Rule rule6 = Rule(6,rule6_head, {}, {}, 0.500000, 0, 0);
ruleManager = RuleManager({rule1, rule2, rule3, rule4, rule5, rule6});
debugger.endStage();
std::cout << std::fixed << std::setprecision(10);
debugger.startStage(StageKind::CREATE_GRAPH_FULL);
auto graph = WorkingDerivationGraph::createFrom(DerivationManager::untypedTuple2RuleApplications, ruleManager, fact_prob);
debugger.endStage();
// graph->dumpStatistics(std::cout);
graph->dumpDot("before_prune.dot");

debugger.startStage(StageKind::PRUNING_FULL);
auto view = graph->prune(obj.getOutputRelations());

debugger.endStage();
view.dumpDot("after_prune.dot");

view.dumpJson("derivation.json");
if (obj.getKnowledge() == souffle::Knowledge::BDD) {
std::map<NodePtr, BddNodeRef> nodeFormulas;std::map<EdgePtr, BddNodeRef> edgeFormulas;WeightedBDDManager bddManager;
if (!opt.isDerivationOnly()) {
{

debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
buildFormulasCyclewise(view, bddManager, nodeFormulas, edgeFormulas);
debugger.endStage();
}
{
debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);
for (const auto& [node, bdd] : nodeFormulas) {
//    std::cout << "Node" << node->getId() ;
//    std::cout << "Node" << node->getId() << " " << node->getTuple().toString() << ": ";
//    std::cout << bddManager.toString(bdd) << "\t";
    auto prob = bddManager.computeWeightedModelCount(bdd);
    probResult[node] = prob;
//    std::cout << "Probability: " << prob << std::endl;
}
debugger.endStage();
debugger.startStage(StageKind::IO_DUMP_FULL);
dumpProbabilities(probResult, opt.getOutputFileDir());
debugger.endStage();
debugger.endTurn();
dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");
}
}

IncrementalCLI cli(&obj, graph, &ruleManager, &bddManager, &nodeFormulas, &edgeFormulas);
cli.setCmdOptions(opt);
cli.run();
}

else if (obj.getKnowledge() == souffle::Knowledge::SDD) {
std::map<NodePtr, SddNodeRef> nodeFormulas;std::map<EdgePtr, SddNodeRef> edgeFormulas;SddFormulaManager sddManager(view.getNodes().size() + view.getEdges().size());
if (!opt.isDerivationOnly()) {
{

buildFormulasCyclewise(view, sddManager, nodeFormulas, edgeFormulas);
}
{
debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);
for (const auto& [node, sdd] : nodeFormulas) {
//    std::cout << "Node" << node->getId() ;
//    std::cout << "Node" << node->getId() << " " << node->getTuple().toString() << ": ";
//    std::cout << sddManager.toString(sdd) << "\t";
    auto prob = sddManager.computeWeightedModelCount(sdd);
    probResult[node] = prob;
//    std::cout << "Probability: " << prob << std::endl;
}
debugger.endStage();
debugger.startStage(StageKind::IO_DUMP_FULL);
dumpProbabilities(probResult, opt.getOutputFileDir());
debugger.endStage();
debugger.endTurn();
}

}

IncrementalCLI cli(&obj, graph, &ruleManager, &sddManager, &nodeFormulas, &edgeFormulas);
cli.setCmdOptions(opt);
cli.run();
}
Debugger& debugger = Debugger::getInstance();
std::string reportFile = generateFilename(opt.getLogFileName(), ".json");
std::ofstream ofs = std::ofstream(reportFile);
debugger.printReportJson(ofs);
// debugger.printReport(std::cout);
} catch (std::exception& e) {std::cerr << "Problog calc failed" << e.what() << std::endl;}
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

