#include "emap/preprocessingconfiguration.h"

#include "infra/exception.h"
#include "infra/string.h"

namespace emap {

using namespace inf;

PreprocessingConfiguration::PreprocessingConfiguration(
    date::year year,
    const fs::path& spatialPatternsPath,
    const fs::path& countriesVector,
    const fs::path& outputPath)
: _year(year)
, _spatialPatternsPath(spatialPatternsPath)
, _countriesPath(countriesVector)
, _outputPath(outputPath)
{
}

date::year PreprocessingConfiguration::year() const noexcept
{
    return _year;
}

const fs::path& PreprocessingConfiguration::spatial_patterns_path() const noexcept
{
    return _spatialPatternsPath;
}

const fs::path& PreprocessingConfiguration::countries_vector_path() const noexcept
{
    return _countriesPath;
}

const fs::path& PreprocessingConfiguration::output_path() const noexcept
{
    return _outputPath;
}

}
