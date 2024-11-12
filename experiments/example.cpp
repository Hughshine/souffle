#define SOUFFLE_GENERATOR_VERSION "2.4.1-54-g8b2657a77"
#include "souffle/CompiledSouffle.h"
#include "souffle/SignalHandler.h"
#include "souffle/SouffleInterface.h"
#include "souffle/datastructure/BTree.h"
#include "souffle/io/IOSystem.h"
#include "souffle/utility/MiscUtil.h"
#include <any>
namespace functors {
extern "C" {
}
} //namespace functors
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
class Stratum_A_4f7df7f5f9efad6a {
public:
 Stratum_A_4f7df7f5f9efad6a(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_A_600668de4345e18e,t_btree_000_ii__0_1__11::Type& rel_B_3a4882db196b99ec,t_btree_000_i__0__1::Type& rel_C_ac888c4d34093402);
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
t_btree_000_i__0__1::Type* rel_A_600668de4345e18e;
t_btree_000_ii__0_1__11::Type* rel_B_3a4882db196b99ec;
t_btree_000_i__0__1::Type* rel_C_ac888c4d34093402;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_A_4f7df7f5f9efad6a::Stratum_A_4f7df7f5f9efad6a(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_A_600668de4345e18e,t_btree_000_ii__0_1__11::Type& rel_B_3a4882db196b99ec,t_btree_000_i__0__1::Type& rel_C_ac888c4d34093402):
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
for(const auto& env0 : *rel_B_3a4882db196b99ec) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_A_600668de4345e18e->insert(tuple,READ_OP_CONTEXT(rel_A_600668de4345e18e_op_ctxt));
}
}
();}
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"name","A"},{"operation","output"},{"output-dir","/home/hugh/datalog/souffle/experiments/output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_A_600668de4345e18e);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
if (pruneImdtRels) rel_B_3a4882db196b99ec->purge();
if (pruneImdtRels) rel_C_ac888c4d34093402->purge();
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_B_1f366d600ff54502 {
public:
 Stratum_B_1f366d600ff54502(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_ii__0_1__11::Type& rel_B_3a4882db196b99ec);
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
t_btree_000_ii__0_1__11::Type* rel_B_3a4882db196b99ec;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_B_1f366d600ff54502::Stratum_B_1f366d600ff54502(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_ii__0_1__11::Type& rel_B_3a4882db196b99ec):
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
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x\ty"},{"auxArity","0"},{"fact-dir","/home/hugh/datalog/souffle/experiments/facts"},{"name","B"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"x\", \"y\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
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
 Stratum_C_c9c181582fee6bd1(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_C_ac888c4d34093402);
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
t_btree_000_i__0__1::Type* rel_C_ac888c4d34093402;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_C_c9c181582fee6bd1::Stratum_C_c9c181582fee6bd1(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_i__0__1::Type& rel_C_ac888c4d34093402):
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
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"fact-dir","/home/hugh/datalog/souffle/experiments/facts"},{"name","C"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
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
class Sf_example: public SouffleProgram {
public:
 Sf_example();
 ~Sf_example();
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
Own<t_btree_000_ii__0_1__11::Type> rel_B_3a4882db196b99ec;
souffle::RelationWrapper<t_btree_000_ii__0_1__11::Type> wrapper_rel_B_3a4882db196b99ec;
Own<t_btree_000_i__0__1::Type> rel_C_ac888c4d34093402;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_C_ac888c4d34093402;
Own<t_btree_000_i__0__1::Type> rel_A_600668de4345e18e;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_A_600668de4345e18e;
Stratum_A_4f7df7f5f9efad6a stratum_A_32d0975c99a022af;
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
 Sf_example::Sf_example():
symTable(),
recordTable(),
regexCache(),
rel_B_3a4882db196b99ec(mk<t_btree_000_ii__0_1__11::Type>()),
wrapper_rel_B_3a4882db196b99ec(0, *rel_B_3a4882db196b99ec, *this, "B", std::array<const char *,2>{{"i:number","i:number"}}, std::array<const char *,2>{{"x","y"}}, 0),
rel_C_ac888c4d34093402(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_C_ac888c4d34093402(1, *rel_C_ac888c4d34093402, *this, "C", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
rel_A_600668de4345e18e(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_A_600668de4345e18e(2, *rel_A_600668de4345e18e, *this, "A", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"x"}}, 0),
stratum_A_32d0975c99a022af(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_A_600668de4345e18e,*rel_B_3a4882db196b99ec,*rel_C_ac888c4d34093402),
stratum_B_52e2453c9dd5383a(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_B_3a4882db196b99ec),
stratum_C_10b322c3b2732bab(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_C_ac888c4d34093402){
addRelation("B", wrapper_rel_B_3a4882db196b99ec, true, false);
addRelation("C", wrapper_rel_C_ac888c4d34093402, true, false);
addRelation("A", wrapper_rel_A_600668de4345e18e, false, true);
}

 Sf_example::~Sf_example(){
}

void Sf_example::runFunction(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){

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

// -- relation hint statistics --
signalHandler->reset();
}

void Sf_example::run(){
runFunction("", "", false, false);
}

void Sf_example::runAll(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){
runFunction(inputDirectoryArg, outputDirectoryArg, performIOArg, pruneImdtRelsArg);
}

void Sf_example::printAll([[maybe_unused]] std::string outputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"name","A"},{"operation","output"},{"output-dir","/home/hugh/datalog/souffle/experiments/output"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_A_600668de4345e18e);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

void Sf_example::loadAll([[maybe_unused]] std::string inputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x\ty"},{"auxArity","0"},{"fact-dir","/home/hugh/datalog/souffle/experiments/facts"},{"name","B"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 2, \"params\": [\"x\", \"y\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 2, \"types\": [\"i:number\", \"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_B_3a4882db196b99ec);
} catch (std::exception& e) {std::cerr << "Error loading B data: " << e.what() << '\n';
exit(1);
}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x"},{"auxArity","0"},{"fact-dir","/home/hugh/datalog/souffle/experiments/facts"},{"name","C"},{"operation","input"},{"params","{\"records\": {}, \"relation\": {\"arity\": 1, \"params\": [\"x\"]}}"},{"types","{\"ADTs\": {}, \"records\": {}, \"relation\": {\"arity\": 1, \"types\": [\"i:number\"]}}"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_C_ac888c4d34093402);
} catch (std::exception& e) {std::cerr << "Error loading C data: " << e.what() << '\n';
exit(1);
}
}

void Sf_example::dumpInputs(){
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "B";
rwOperation["types"] = "{\"relation\": {\"arity\": 2, \"auxArity\": 0, \"types\": [\"i:number\", \"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_B_3a4882db196b99ec);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "C";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_C_ac888c4d34093402);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

void Sf_example::dumpOutputs(){
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "A";
rwOperation["types"] = "{\"relation\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_A_600668de4345e18e);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

SymbolTable& Sf_example::getSymbolTable(){
return symTable;
}

RecordTable& Sf_example::getRecordTable(){
return recordTable;
}

void Sf_example::setNumThreads(std::size_t numThreadsValue){
SouffleProgram::setNumThreads(numThreadsValue);
symTable.setNumLanes(getNumThreads());
recordTable.setNumLanes(getNumThreads());
regexCache.setNumLanes(getNumThreads());
}

void Sf_example::executeSubroutine(std::string name,const std::vector<RamDomain>& args,std::vector<RamDomain>& ret){
if (name == "A") {
stratum_A_32d0975c99a022af.run(args, ret);
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
SouffleProgram *newInstance_example(){return new  souffle::Sf_example;}
SymbolTable *getST_example(SouffleProgram *p){return &reinterpret_cast<souffle::Sf_example*>(p)->getSymbolTable();}
} // namespace souffle

#ifndef __EMBEDDED_SOUFFLE__
#include "souffle/CompiledOptions.h"
int main(int argc, char** argv)
{
try{
souffle::CmdOptions opt(R"(/home/hugh/datalog/souffle/experiments/example.dl)",
R"()",
R"()",
false,
R"()",
1);
if (!opt.parse(argc,argv)) return 1;
souffle::Sf_example obj;
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
class factory_Sf_example: souffle::ProgramFactory {
public:
souffle::SouffleProgram* newInstance();
 factory_Sf_example();
private:
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
souffle::SouffleProgram* factory_Sf_example::newInstance(){
return new  souffle::Sf_example();
}

 factory_Sf_example::factory_Sf_example():
souffle::ProgramFactory("example"){
}

} // namespace  souffle
namespace souffle {

#ifdef __EMBEDDED_SOUFFLE__
extern "C" {
souffle::factory_Sf_example __factory_Sf_example_instance;
}
#endif
} // namespace souffle

