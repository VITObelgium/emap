#include "emap/scalingfactors.h"

#include "infra/exception.h"

namespace emap {

using namespace inf;

void ScalingFactors::add_scaling_factor(ScalingFactor&& sf)
{
    _scalingFactors.push_back(std::move(sf));
}

size_t ScalingFactors::size() const noexcept
{
    return _scalingFactors.size();
}

struct SFMatch
{
    const ScalingFactor* sf              = nullptr;
    ScalingFactor::MatchResult idMatch   = ScalingFactor::MatchResult::NoMatch;
    ScalingFactor::MatchResult yearMatch = ScalingFactor::MatchResult::NoMatch;
    ScalingFactor::MatchResult typeMatch = ScalingFactor::MatchResult::NoMatch;

    bool matches() const noexcept
    {
        return idMatch != ScalingFactor::MatchResult::NoMatch &&
               typeMatch != ScalingFactor::MatchResult::NoMatch &&
               yearMatch != ScalingFactor::MatchResult::NoMatch;
    }

    bool is_exact() const noexcept
    {
        return idMatch == ScalingFactor::MatchResult::Exact &&
               typeMatch == ScalingFactor::MatchResult::Exact &&
               yearMatch == ScalingFactor::MatchResult::Exact;
    }

    int32_t match_score() const noexcept
    {
        int32_t score = 0;
        switch (idMatch) {
        case ScalingFactor::MatchResult::Exact:
            score += 100000;
            break;
        case ScalingFactor::MatchResult::Range:
            score += 10000;
            break;
        case ScalingFactor::MatchResult::WildCard:
            score += 5000;
            break;
        default:
            break;
        }

        if (typeMatch == ScalingFactor::MatchResult::Exact) {
            score += 1000;
        } else {
            score += 500;
        }

        if (yearMatch == ScalingFactor::MatchResult::Exact) {
            score += 100;
        } else if (yearMatch == ScalingFactor::MatchResult::Range) {
            score += 50;
        } else {
            score += 10;
        }

        return score;
    }

    bool operator==(const SFMatch& other) const noexcept
    {
        return idMatch == other.idMatch &&
               yearMatch == other.yearMatch &&
               typeMatch == other.typeMatch;
    }

    bool operator<(const SFMatch& other) const noexcept
    {
        return match_score() > other.match_score();
    }
};

static std::vector<SFMatch> get_matches(std::span<const ScalingFactor> sfs, const EmissionIdentifier& id, EmissionSourceType type, date::year year)
{
    std::vector<SFMatch> result;

    for (auto& sf : sfs) {
        SFMatch match;
        match.sf        = &sf;
        match.idMatch   = sf.id_match(id);
        match.typeMatch = sf.type_match(type);
        match.yearMatch = sf.year_match(year);

        if (!match.matches()) {
            continue;
        }

        result.push_back(match);
    }

    if (auto iter = std::adjacent_find(result.begin(), result.end()); iter != result.end()) {
        throw RuntimeError("Ambiguous scaling factor specification: Multiple matches for {} in year {}", id, static_cast<int>(year));
    }

    std::sort(result.begin(), result.end());

    return result;
}

std::optional<double> ScalingFactors::scaling_for_id(const EmissionIdentifier& id, EmissionSourceType type, date::year year) const
{
    std::optional<double> exactResult;    // Result that matched the exact year -> highest priority
    std::optional<double> rangeResult;    // Result that matched a year range -> only used when no exact result is present
    std::optional<double> wildCardResult; // Result the * wildcard -> only used when nothing else is present

    if (auto results = get_matches(_scalingFactors, id, type, year); !results.empty()) {
        return results.front().sf->factor();
    } else if (results = get_matches(_scalingFactors, id.with_sector(EmissionSector(id.sector.gnfr_sector())), type, year); !results.empty()) {
        return results.front().sf->factor();
    }

    return wildCardResult;
}
}