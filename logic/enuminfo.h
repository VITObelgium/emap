#pragma once

#include <algorithm>
#include <cassert>
#include <string_view>
#include <vector>

namespace emap {

template <typename TEnum>
struct EnumInfo
{
    constexpr bool is_serialized_name(std::string_view name) const noexcept
    {
        return serializedName == name;
    }

    TEnum id;
    std::string_view serializedName;
    std::string_view description;
};

template <typename TEnum>
struct MultiEnumInfo
{
    std::string_view serialized_name() const noexcept
    {
        assert(!serializedNames.empty());
        return serializedNames.front();
    }

    bool is_serialized_name(std::string_view name) const noexcept
    {
        return std::any_of(serializedNames.begin(), serializedNames.end(), [=](std::string_view serialized) {
            return serialized == name;
        });
    }

    TEnum id;
    std::vector<std::string_view> serializedNames; // Multiple possible serialized names, with the first entry being the preferred one
    std::string_view description;
};

}
