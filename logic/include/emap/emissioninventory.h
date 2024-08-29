#pragma once

#include "emap/emissions.h"
#include "infra/math.h"

namespace emap {

class RunSummary;
class ScalingFactors;
class RunConfiguration;

class EmissionInventoryEntry
{
public:
    EmissionInventoryEntry() noexcept = default;
    EmissionInventoryEntry(EmissionIdentifier id, double diffuseEmissions) noexcept
    : _id(id)
    , _diffuseEmission(diffuseEmissions)
    {
    }

    EmissionInventoryEntry(EmissionIdentifier id, double diffuseEmissions, std::vector<EmissionEntry> pointEmissionEntries) noexcept
    : _id(id)
    , _diffuseEmission(diffuseEmissions)
    , _pointEmissionEntries(std::move(pointEmissionEntries))
    {
    }

    const EmissionIdentifier& id() const noexcept
    {
        return _id;
    }

    double diffuse_emissions() const noexcept
    {
        return _diffuseEmission;
    }

    void set_diffuse_emissions(double value) noexcept
    {
        _diffuseEmission = value;
    }

    double point_emission_sum() const noexcept
    {
        return std::accumulate(_pointEmissionEntries.cbegin(), _pointEmissionEntries.cend(), 0.0, [](double total, const auto& current) {
            return total + current.value().amount().value_or(0.0);
        });
    }

    std::span<const EmissionEntry> point_emissions() const noexcept
    {
        return _pointEmissionEntries;
    }

    std::vector<EmissionEntry> scaled_point_emissions() const noexcept
    {
        std::vector<EmissionEntry> result;
        result.reserve(_pointEmissionEntries.size());
        std::transform(_pointEmissionEntries.begin(), _pointEmissionEntries.end(), std::back_inserter(result), [this](const EmissionEntry& entry) {
            auto scaledEntry = entry;
            scaledEntry.set_value(entry.value() * _pointAutoScaling * _pointUserScaling);
            return scaledEntry;
        });
        return result;
    }

    double scaled_total_emissions_sum() const noexcept
    {
        return scaled_point_emissions_sum() + scaled_diffuse_emissions_sum();
    }

    double scaled_diffuse_emissions_sum() const noexcept
    {
        return _diffuseEmission * _diffuseUserScaling * _diffuseAutoScaling;
    }

    double scaled_point_emissions_sum() const noexcept
    {
        return point_emission_sum() * _pointAutoScaling * _pointUserScaling;
    }

    void set_point_auto_scaling(double factor) noexcept
    {
        _pointAutoScaling = factor;
    }

    void set_point_user_scaling(double factor) noexcept
    {
        _pointUserScaling = factor;
    }

    void set_diffuse_auto_scaling(double factor) noexcept
    {
        _diffuseAutoScaling = factor;
    }

    void set_diffuse_user_scaling(double factor) noexcept
    {
        _diffuseUserScaling = factor;
    }

    double point_auto_scaling_factor() const noexcept
    {
        return _pointAutoScaling;
    }

    double point_user_scaling_factor() const noexcept
    {
        return _pointUserScaling;
    }

    double diffuse_auto_scaling_factor() const noexcept
    {
        return _diffuseAutoScaling;
    }

    double diffuse_user_scaling_factor() const noexcept
    {
        return _diffuseUserScaling;
    }

private:
    EmissionIdentifier _id;
    double _diffuseEmission = 0.0;
    std::vector<EmissionEntry> _pointEmissionEntries;
    double _pointAutoScaling   = 1.0; // Automatic correction of the point sources when they exceed the total emission
    double _pointUserScaling   = 1.0; // User defined scaling of the point sources
    double _diffuseAutoScaling = 1.0;
    double _diffuseUserScaling = 1.0;
};

template <typename TEmission>
class EmissionCollection
{
public:
    using value_type     = TEmission;
    using size_type      = std::size_t;
    using pointer        = TEmission*;
    using const_pointer  = const TEmission*;
    using iterator       = typename std::vector<TEmission>::iterator;
    using const_iterator = typename std::vector<TEmission>::const_iterator;

    EmissionCollection(date::year year)
    : _year(year)
    {
    }

    EmissionCollection(date::year year, std::vector<TEmission> emissions)
    : _year(year)
    {
        set_emissions(std::move(emissions));
    }

    date::year year() const noexcept
    {
        return _year;
    }

    bool validate_uniqueness() const noexcept
    {
        std::unordered_set<EmissionIdentifier> set;
        for (auto& em : _emissions) {
            if (set.count(em.id()) > 0) {
                return false;
            }

            set.insert(em.id());
        }

        return true;
    }

    void add_emission(TEmission&& info)
    {
        // Make sure the emissions remain sorted
        auto emissionIter = find_sorted(info.id());
        _emissions.insert(emissionIter, std::move(info));
    }

    void add_emissions(std::span<const TEmission> emissions)
    {
        // Make sure the emissions remain sorted

        inf::append_to_container(_emissions, emissions);
        sort_emissions();
    }

    void set_emissions(std::vector<TEmission> emissions)
    {
        // Sort the emissions after assignment
        _emissions = std::move(emissions);
        sort_emissions();
    }

    void update_emission(TEmission&& info)
    {
        auto emissionIter = find_sorted(info.id());
        if (emissionIter != _emissions.end() && emissionIter->id() == info.id()) {
            *emissionIter = info;
        } else {
            throw inf::RuntimeError("Update of non existing emission");
        }
    }

    void update_or_add_emission(TEmission&& info)
    {
        auto emissionIter = find_sorted(info.id());
        if (emissionIter != _emissions.end() && emissionIter->id() == info.id()) {
            // update the existing emission
            *emissionIter = std::forward<TEmission&&>(info);
        } else {
            _emissions.insert(emissionIter, std::forward<TEmission&&>(info));
        }
    }

    const TEmission& emission_with_id(const EmissionIdentifier& id) const
    {
        auto emissionIter = find_sorted(id);
        if (emissionIter != _emissions.end() && emissionIter->id() == id) {
            return *emissionIter;
        }

        throw inf::RuntimeError("No emission found with id: {}", id);
    }

    const TEmission& emission_with_id_at_coordinate(const EmissionIdentifier& id, Coordinate coord) const
    {
        auto iter = std::find_if(_emissions.begin(), _emissions.end(), [&id, &coord](const TEmission& em) {
            if (em.id() == id) {
                if (auto coordOpt = em.coordinate(); coordOpt.has_value()) {
                    return inf::math::approx_equal(coord.x, coordOpt->x, 1e-4) &&
                           inf::math::approx_equal(coord.y, coordOpt->y, 1e-4);
                }
            }

            return false;
        });

        if (iter == _emissions.end()) {
            throw inf::RuntimeError("No emission found with id: {} at coordinate {}", id, coord);
        }

        return *iter;
    }

    std::optional<TEmission> try_emission_with_id(const EmissionIdentifier& id) const noexcept
    {
        auto emissionIter = find_sorted(id);
        if (emissionIter != _emissions.end() && emissionIter->id() == id) {
            return *emissionIter;
        }

        return {};
    }

    std::vector<TEmission> emissions_with_id(const EmissionIdentifier& id) const
    {
        std::vector<TEmission> result;
        std::copy_if(_emissions.begin(), _emissions.end(), std::back_inserter(result), [&id](const TEmission& em) {
            return em.id() == id;
        });

        return result;
    }

    std::vector<TEmission> emissions_with_id_at_coordinate(const EmissionIdentifier& id, Coordinate coord) const
    {
        std::vector<TEmission> result;
        std::copy_if(_emissions.begin(), _emissions.end(), std::back_inserter(result), [&id, &coord](const TEmission& em) {
            if (em.id() == id) {
                if (auto coordOpt = em.coordinate(); coordOpt.has_value()) {
                    return inf::math::approx_equal(coord.x, coordOpt->x, 1e-4) &&
                           inf::math::approx_equal(coord.y, coordOpt->y, 1e-4);
                }
            }

            return false;
        });

        return result;
    }

    size_t empty() const noexcept
    {
        return _emissions.empty();
    }

    auto data() const
    {
        return _emissions.data();
    }

    size_t size() const noexcept
    {
        return _emissions.size();
    }

    auto begin() const noexcept
    {
        return _emissions.begin();
    }

    auto end() const noexcept
    {
        return _emissions.end();
    }

    auto begin() noexcept
    {
        return _emissions.begin();
    }

    auto end() noexcept
    {
        return _emissions.end();
    }

private:
    void sort_emissions()
    {
        std::sort(_emissions.begin(), _emissions.end(), [](const TEmission& lhs, const TEmission& rhs) {
            return lhs.id() < rhs.id();
        });
    }

    auto find_sorted(const EmissionIdentifier& id)
    {
        return std::lower_bound(_emissions.begin(), _emissions.end(), id, [](const TEmission& lhs, const EmissionIdentifier& id) {
            return lhs.id() < id;
        });
    }

    auto find_sorted(const EmissionIdentifier& id) const
    {
        return std::lower_bound(_emissions.begin(), _emissions.end(), id, [](const TEmission& lhs, const EmissionIdentifier& id) {
            return lhs.id() < id;
        });
    }

    date::year _year;
    std::vector<TEmission> _emissions;
};

using SingleEmissions   = EmissionCollection<EmissionEntry>;
using EmissionInventory = EmissionCollection<EmissionInventoryEntry>;

template <typename T>
void merge_emissions(EmissionCollection<T>& output, EmissionCollection<T>&& toMerge)
{
    if (output.empty()) {
        std::swap(output, toMerge);
    } else {
        for (auto& emission : toMerge) {
            output.update_or_add_emission(std::move(emission));
        }
    }
}

template <typename T>
void merge_unique_emissions(EmissionCollection<T>& output, EmissionCollection<T>&& toMerge)
{
    if (output.empty()) {
        std::swap(output, toMerge);
    } else {
        output.add_emissions(std::span(toMerge.begin(), toMerge.end()));
    }
}

EmissionInventory create_emission_inventory(SingleEmissions totalEmissionsNfr,
                                            SingleEmissions totalEmissionsGnfr,
                                            const std::optional<SingleEmissions>& extraEmissions,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& scalings,
                                            const RunConfiguration& cfg,
                                            RunSummary& runSummary);

EmissionInventory create_emission_inventory(SingleEmissions totalEmissionsNfr,
                                            SingleEmissions totalEmissionsNfrOlder,
                                            SingleEmissions totalEmissionsGnfr,
                                            const std::optional<SingleEmissions>& extraEmissions,
                                            const SingleEmissions& pointSourceEmissions,
                                            const ScalingFactors& scalings,
                                            const RunConfiguration& cfg,
                                            RunSummary& runSummary);

SingleEmissions read_nfr_emissions(date::year year, const RunConfiguration& cfg, RunSummary& runSummary);
SingleEmissions read_country_point_sources(const RunConfiguration& cfg, const Country& country, RunSummary& runSummary);

EmissionInventory make_emission_inventory(const RunConfiguration& cfg, RunSummary& summary);

}
