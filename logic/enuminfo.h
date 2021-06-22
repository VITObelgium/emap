#pragma once

namespace emap {

template <typename TEnum>
struct EnumInfo
{
    TEnum id;
    std::string_view serializedName;
    std::string_view description;
};

}
