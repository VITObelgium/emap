#pragma once

#include "infra/exception.h"
#include "infra/string.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace emap {

class InputConversions
{
public:
    void add_conversion(std::string_view key, std::string_view value)
    {
        auto iter = std::find_if(_conversions.begin(), _conversions.end(), [key](const std::pair<std::string, std::vector<std::string>>& conv) {
            return conv.first == key;
        });

        if (iter == _conversions.end()) {
            _conversions.emplace_back(key, std::vector<std::string>({std::string(value)}));
        } else {
            iter->second.emplace_back(value);
        }
    }

    std::string_view lookup(std::string_view str) const noexcept
    {
        for (const auto& [key, conversions] : _conversions) {
            auto iter = std::find_if(conversions.begin(), conversions.end(), [str](const std::string& conv) {
                return inf::str::iequals(str, conv);
            });

            if (iter != conversions.end()) {
                return key;
            }
        }

        return {};
    }

private:
    std::vector<std::pair<std::string, std::vector<std::string>>> _conversions;
};
}
