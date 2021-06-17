#include "emap/inputparsers.h"
#include "infra/csvreader.h"

namespace emep {

std::vector<std::string> parse_emissions(const fs::path& /*emissionsCsv*/)
{
    // csv columns: type;scenario;year;reporting;country;nfr_sector;pollutant;emission;unit
    //inf::CsvReader csv(emissionsCsv);

    // for (auto& line : csv) {
    // }

    return {};
}

}