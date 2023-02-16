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

std::optional<double> ScalingFactors::scaling_for_id(const EmissionIdentifier& id, EmissionSourceType type, date::year year) const
{
    std::optional<double> exactResult;    // Result that matched the exact year -> highest priority
    std::optional<double> rangeResult;    // Result that matched a year range -> only used when no exact result is present
    std::optional<double> wildCardResult; // Result the * wildcard -> only used when nothing else is present

    auto findScalingFactor = [&](const EmissionIdentifier searchId) {
        for (auto& sf : _scalingFactors) {
            if (!sf.id_matches(searchId) || !sf.type_matches(type)) {
                continue;
            }

            switch (sf.year_match(year)) {
            case ScalingFactor::YearMatch::Exact:
                if (exactResult.has_value()) {
                    throw RuntimeError("Ambiguous scaling factor specified for {}. Multiple exact years specified with the same value", id);
                }
                exactResult = sf.factor();
                break;
            case ScalingFactor::YearMatch::Range:
                if (rangeResult.has_value()) {
                    throw RuntimeError("Ambiguous scaling factor specified for {}. Overlapping year ranges", id);
                }
                rangeResult = sf.factor();
                break;
            case ScalingFactor::YearMatch::WildCard:
                if (wildCardResult.has_value()) {
                    throw RuntimeError("Ambiguous scaling factor specified for {}. Multiple * wildcards specified", id);
                }
                wildCardResult = sf.factor();
                break;
            default:
                break;
            }
        }
    };

    findScalingFactor(id);

    if (id.sector.type() == EmissionSector::Type::Nfr && !(exactResult.has_value() || rangeResult.has_value() || wildCardResult.has_value())) {
        // No match found for the NFR sector, check if we have a GNFR match
        findScalingFactor(id.with_sector(EmissionSector(id.sector.gnfr_sector())));
    }

    if (exactResult.has_value()) {
        return exactResult;
    }

    if (rangeResult.has_value()) {
        return rangeResult;
    }

    return wildCardResult;
}

}