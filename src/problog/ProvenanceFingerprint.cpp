#include "souffle/problog/ProvenanceFingerprint.h"

#include "souffle/problog/CanonicalRuleSchema.h"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <map>
#include <sstream>
#include <tuple>
#include <utility>

namespace souffle::problog {
namespace {

std::string sized(const std::string &value) {
  return std::to_string(value.size()) + ":" + value;
}

std::string hex64(std::uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

std::string tupleEqualityPattern(const UntypedTuple &tuple) {
  std::vector<std::size_t> firstClasses;
  std::vector<RamDomain> values;
  std::ostringstream out;
  for (const auto value : tuple.fields) {
    auto it = std::find(values.begin(), values.end(), value);
    std::size_t color = 0;
    if (it == values.end()) {
      color = values.size();
      values.push_back(value);
    } else {
      color = static_cast<std::size_t>(std::distance(values.begin(), it));
    }
    firstClasses.push_back(color);
  }
  for (std::size_t i = 0; i < firstClasses.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << firstClasses[i];
  }
  return out.str();
}

std::string bodyRole(const Atom &atom) {
  std::ostringstream out;
  out << (atom.isNegatedAtom() ? "neg" : "pos") << ':'
      << sized(atom.getRelation()) << ':' << atom.getFields().size();
  return out.str();
}

ProvenanceFingerprint128 hash128(const std::string &value) {
  constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
  constexpr std::uint64_t fnvPrime = 1099511628211ULL;
  constexpr std::uint64_t secondOffset = 7809847782465536322ULL;
  constexpr std::uint64_t secondPrime = 0x9e3779b185ebca87ULL;
  ProvenanceFingerprint128 result{secondOffset, fnvOffset};
  for (const unsigned char byte : value) {
    result.low ^= byte;
    result.low *= fnvPrime;
    result.high ^= static_cast<std::uint64_t>(byte) + 0x100ULL;
    result.high *= secondPrime;
    result.high ^= result.high >> 29U;
  }
  return result;
}

struct RuleShape {
  const Rule *rule = nullptr;
  CanonicalRuleSchema schema;
};

struct BodyReference {
  std::string role;
  std::size_t tuple = 0;
};

struct ApplicationDescriptor {
  const RuleShape *shape = nullptr;
  std::string unknownRuleSignature;
  std::vector<BodyReference> body;
  bool complete = true;
};

struct TupleRecord {
  UntypedTuple tuple;
  std::string baseSignature;
  std::vector<ApplicationDescriptor> applications;
  std::size_t bodyReferences = 0;
  bool localComplete = true;
  bool complete = true;
  bool materialized = false;
  bool opaqueLeaf = false;
};

std::vector<std::size_t>
assignColors(const std::vector<std::string> &signatures,
             std::size_t &classCount) {
  std::vector<std::string> ordered = signatures;
  std::sort(ordered.begin(), ordered.end());
  ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());

  std::map<std::string, std::size_t> colorsBySignature;
  for (std::size_t i = 0; i < ordered.size(); ++i) {
    colorsBySignature.emplace(ordered[i], i);
  }

  std::vector<std::size_t> colors;
  colors.reserve(signatures.size());
  for (const auto &signature : signatures) {
    colors.push_back(colorsBySignature.at(signature));
  }
  classCount = ordered.size();
  return colors;
}

std::string applicationSignature(const ApplicationDescriptor &application,
                                 const std::vector<std::size_t> &colors) {
  std::vector<std::string> body;
  body.reserve(application.body.size());
  for (const auto &reference : application.body) {
    body.push_back(reference.role + "@" +
                   std::to_string(colors.at(reference.tuple)));
  }
  std::sort(body.begin(), body.end());

  std::ostringstream out;
  out << "app{";
  if (application.shape != nullptr) {
    out << "schema=" << hex64(application.shape->schema.fingerprint);
  } else {
    out << "unknown=" << sized(application.unknownRuleSignature);
  }
  out << ";complete=" << (application.complete ? 1 : 0) << ";body=";
  for (const auto &item : body) {
    out << sized(item);
  }
  out << '}';
  return out.str();
}

} // namespace

bool ProvenanceFingerprint128::operator==(
    const ProvenanceFingerprint128 &other) const {
  return std::tie(high, low) == std::tie(other.high, other.low);
}

bool ProvenanceFingerprint128::operator!=(
    const ProvenanceFingerprint128 &other) const {
  return !(*this == other);
}

bool ProvenanceFingerprint128::operator<(
    const ProvenanceFingerprint128 &other) const {
  return std::tie(high, low) < std::tie(other.high, other.low);
}

std::string ProvenanceFingerprint128::toHex() const {
  return hex64(high) + hex64(low);
}

ProvenanceFingerprintResult fingerprintMaterializedProvenance(
    const MaterializedDerivationMap &derivations,
    const RuleManager &ruleManager,
    const std::unordered_map<UntypedTuple, double> &probabilisticFacts,
    const std::unordered_set<UntypedTuple> &inputFacts,
    const std::unordered_set<std::string> &opaqueLeafRelations,
    const ProvenanceFingerprintOptions &options) {
  ProvenanceFingerprintResult result;

  std::vector<UntypedTuple> materializedTuples;
  materializedTuples.reserve(derivations.size());
  for (const auto &[tuple, applications] : derivations) {
    (void)applications;
    materializedTuples.push_back(tuple);
  }
  std::sort(materializedTuples.begin(), materializedTuples.end());

  std::map<UntypedTuple, std::size_t> tupleIds;
  std::vector<TupleRecord> records;
  auto getOrCreateTuple = [&](const UntypedTuple &tuple) {
    const auto existing = tupleIds.find(tuple);
    if (existing != tupleIds.end()) {
      return existing->second;
    }
    const auto id = records.size();
    tupleIds.emplace(tuple, id);
    TupleRecord record;
    record.tuple = tuple;
    records.push_back(std::move(record));
    return id;
  };

  for (const auto &tuple : materializedTuples) {
    const auto id = getOrCreateTuple(tuple);
    records[id].materialized = true;
  }

  std::map<std::size_t, RuleShape> ruleShapes;
  auto getRuleShape = [&](const Rule &rule) -> const RuleShape * {
    const auto existing = ruleShapes.find(rule.getRuleId());
    if (existing != ruleShapes.end()) {
      return &existing->second;
    }
    CanonicalRuleSchemaOptions schemaOptions;
    schemaOptions.maxAlphaPermutations = options.maxRuleAlphaPermutations;
    RuleShape shape;
    shape.rule = &rule;
    shape.schema = canonicalizeRuleSchema(rule, schemaOptions);
    return &ruleShapes.emplace(rule.getRuleId(), std::move(shape))
                .first->second;
  };

  for (std::size_t tupleId = 0; tupleId < records.size(); ++tupleId) {
    const UntypedTuple currentTuple = records[tupleId].tuple;
    records[tupleId].opaqueLeaf =
        opaqueLeafRelations.count(currentTuple.relation_name) > 0;
    std::ostringstream base;
    base << "tuple{" << sized(currentTuple.relation_name)
         << ";arity=" << currentTuple.fields.size()
         << ";equality=" << tupleEqualityPattern(currentTuple) << ";prob_fact="
         << (probabilisticFacts.count(currentTuple) > 0 ? 1 : 0)
         << ";input=" << (inputFacts.count(currentTuple) > 0 ? 1 : 0)
         << ";opaque=" << (records[tupleId].opaqueLeaf ? 1 : 0) << '}';
    records[tupleId].baseSignature = base.str();
    if (records[tupleId].opaqueLeaf) {
      continue;
    }

    const auto applicationsIt = derivations.find(currentTuple);
    if (applicationsIt == derivations.end() ||
        applicationsIt->second == nullptr) {
      continue;
    }
    std::vector<ApplicationDescriptor> applications;
    applications.reserve(applicationsIt->second->size());
    std::size_t bodyReferenceCount = 0;
    bool localComplete = true;
    for (const auto &application : *applicationsIt->second) {
      ApplicationDescriptor descriptor;
      const Rule *rule =
          ruleManager.getRule(static_cast<std::size_t>(application.ruleId));
      if (rule == nullptr) {
        descriptor.complete = false;
        descriptor.unknownRuleSignature =
            "rule=" + std::to_string(application.ruleId) +
            ";arity=" + std::to_string(application.varValuesPure.size());
      } else {
        descriptor.shape = getRuleShape(*rule);
        descriptor.complete = descriptor.shape->schema.exact;
        const auto variables = rule->getVars();
        if (variables.size() != application.varValuesPure.size()) {
          descriptor.complete = false;
        } else {
          descriptor.body.reserve(rule->getBodyAtoms().size());
          for (const auto &atom : rule->getBodyAtoms()) {
            UntypedTuple bodyTuple{
                atom.getRelation(),
                atom.instantiatedFields(variables, application.varValuesPure)};
            BodyReference reference;
            reference.role = bodyRole(atom);
            reference.tuple = getOrCreateTuple(bodyTuple);
            descriptor.body.push_back(std::move(reference));
          }
        }
        if (!rule->getAggregates().empty()) {
          descriptor.complete = false;
        }
      }
      bodyReferenceCount += descriptor.body.size();
      localComplete = localComplete && descriptor.complete;
      applications.push_back(std::move(descriptor));
      ++result.ruleApplications;
    }
    records[tupleId].bodyReferences = bodyReferenceCount;
    records[tupleId].localComplete = localComplete;
    records[tupleId].applications = std::move(applications);
    result.bodyReferences += bodyReferenceCount;
  }

  std::vector<std::vector<std::size_t>> reverseUses(records.size());
  std::deque<std::size_t> incomplete;
  for (std::size_t tupleId = 0; tupleId < records.size(); ++tupleId) {
    auto &record = records[tupleId];
    record.complete = record.localComplete;
    if (!record.complete) {
      incomplete.push_back(tupleId);
    }
    for (const auto &application : record.applications) {
      for (const auto &reference : application.body) {
        reverseUses.at(reference.tuple).push_back(tupleId);
      }
    }
  }
  while (!incomplete.empty()) {
    const auto body = incomplete.front();
    incomplete.pop_front();
    for (const auto user : reverseUses[body]) {
      if (records[user].complete) {
        records[user].complete = false;
        incomplete.push_back(user);
      }
    }
  }

  std::vector<std::string> signatures;
  signatures.reserve(records.size());
  for (const auto &record : records) {
    signatures.push_back(record.baseSignature +
                         (record.complete ? ";complete=1" : ";complete=0"));
  }

  std::size_t classCount = 0;
  auto colors = assignColors(signatures, classCount);
  result.converged = records.empty();

  for (std::size_t round = 0;
       round < options.maxRefinementRounds && !records.empty(); ++round) {
    std::vector<std::string> refined;
    refined.reserve(records.size());
    for (std::size_t tupleId = 0; tupleId < records.size(); ++tupleId) {
      const auto &record = records[tupleId];
      std::vector<std::string> applications;
      applications.reserve(record.applications.size());
      for (const auto &application : record.applications) {
        applications.push_back(applicationSignature(application, colors));
      }
      std::sort(applications.begin(), applications.end());

      std::ostringstream signature;
      signature << record.baseSignature
                << ";complete=" << (record.complete ? 1 : 0)
                << ";previous=" << colors[tupleId] << ";applications=";
      for (const auto &application : applications) {
        signature << sized(application);
      }
      refined.push_back(signature.str());
    }

    std::size_t refinedClassCount = 0;
    auto refinedColors = assignColors(refined, refinedClassCount);
    signatures = std::move(refined);
    colors = std::move(refinedColors);
    ++result.refinementRounds;
    if (refinedClassCount == classCount) {
      classCount = refinedClassCount;
      result.converged = true;
      break;
    }
    classCount = refinedClassCount;
  }

  result.materializedTuples = materializedTuples.size();
  result.referencedLeafTuples = records.size() - result.materializedTuples;
  result.colorClasses = classCount;
  for (std::size_t tupleId = 0; tupleId < records.size(); ++tupleId) {
    const auto &record = records[tupleId];
    TupleProvenanceFingerprint tupleFingerprint;
    tupleFingerprint.fingerprint = hash128(signatures.at(tupleId));
    tupleFingerprint.color = colors.at(tupleId);
    tupleFingerprint.incomingApplications = record.applications.size();
    tupleFingerprint.bodyReferences = record.bodyReferences;
    tupleFingerprint.complete = record.complete;
    tupleFingerprint.materialized = record.materialized;
    tupleFingerprint.opaqueLeaf = record.opaqueLeaf;
    result.complete = result.complete && record.complete;
    result.buckets[tupleFingerprint.fingerprint].push_back(record.tuple);
    result.tuples.emplace(record.tuple, std::move(tupleFingerprint));
  }

  for (auto &[fingerprint, tuples] : result.buckets) {
    (void)fingerprint;
    std::sort(tuples.begin(), tuples.end());
    result.maxBucketSize = std::max(result.maxBucketSize, tuples.size());
    if (tuples.size() > 1) {
      ++result.candidateBuckets;
    }
  }
  return result;
}

} // namespace souffle::problog
