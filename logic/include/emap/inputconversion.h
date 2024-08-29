#pragma once

#include "infra/exception.h"
#include "infra/string.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace emap {

class InputConversions
{
public:
    void add_conversion(std::string_view key, std::string_view value, std::optional<int32_t> priority)
    {
        auto iter = std::find_if(_conversions.begin(), _conversions.end(), [key](const std::pair<std::string, std::vector<LookupEntry>>& conv) {
            return conv.first == key;
        });

        if (iter == _conversions.end()) {
            _conversions.emplace_back(key, std::vector<LookupEntry>({LookupEntry(priority.value_or(1), std::string(value))}));
        } else {
            iter->second.emplace_back(priority.value_or(1), value);
        }
    }

    std::string_view lookup(std::string_view str) const noexcept
    {
        for (const auto& [key, conversions] : _conversions) {
            auto iter = std::find_if(conversions.begin(), conversions.end(), [str](const LookupEntry& entry) {
                return inf::str::iequals(str, entry.value);
            });

            if (iter != conversions.end()) {
                return key;
            }
        }

        return {};
    }

    std::pair<std::string_view, int32_t> lookup_with_priority(std::string_view str) const noexcept
    {
        for (const auto& [key, conversions] : _conversions) {
            auto iter = std::find_if(conversions.begin(), conversions.end(), [str](const LookupEntry& entry) {
                return inf::str::iequals(str, entry.value);
            });

            if (iter != conversions.end()) {
                return {key, iter->priority};
            }
        }

        return {};
    }

private:
    struct LookupEntry
    {
        LookupEntry() noexcept = default;
        LookupEntry(int32_t prio, std::string_view val)
        : priority(prio)
        , value(val)
        {
        }

        int32_t priority = 1;
        std::string value;
    };

    std::vector<std::pair<std::string, std::vector<LookupEntry>>> _conversions;
};
}
