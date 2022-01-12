#pragma once

namespace emap::test {

namespace sectors {
namespace gnfr {

const GnfrSector PublicPower("A_PublicPower", GnfrId(601), "Public power", EmissionDestination::Land);
const GnfrSector Industry("B_Industry", GnfrId(602), "Industry", EmissionDestination::Land);
const GnfrSector Fugitive("D_Fugitive", GnfrId(604), "Fugitive", EmissionDestination::Land);
const GnfrSector Solvents("E_Solvents", GnfrId(605), "Solvents", EmissionDestination::Land);
const GnfrSector RoadTransport("F_RoadTransport", GnfrId(606), "Road transport", EmissionDestination::Land);
const GnfrSector Shipping("G_Shipping", GnfrId(607), "Shipping", EmissionDestination::Sea);
const GnfrSector Offroad("I_Offroad", GnfrId(609), "Offroad", EmissionDestination::Land);
const GnfrSector Waste("J_Waste", GnfrId(610), "Waste", EmissionDestination::Land);
const GnfrSector AgriOther("L_AgriOther", GnfrId(612), "Agricultural other", EmissionDestination::Land);

}

namespace nfr {
const NfrSector Nfr1A1a("1A1a", NfrId(7001), gnfr::PublicPower, "Public electricity and heat production");
const NfrSector Nfr1A2a("1A2a", NfrId(7004), gnfr::Industry, "Stationary combustion in manufacturing industries and construction: Iron and steel");
const NfrSector Nfr1A2b("1A2b", NfrId(7005), gnfr::Industry, "Stationary combustion in manufacturing industries and construction: Non-ferrous metals");
const NfrSector Nfr1A3bi("1A3bi", NfrId(7014), gnfr::RoadTransport, "Road transport: Passenger cars");
const NfrSector Nfr1A3bii("1A3bii", NfrId(7015), gnfr::RoadTransport, "Road transport: Light duty vehicles");
const NfrSector Nfr1A3biii("1A3biii", NfrId(7016), gnfr::RoadTransport, "Road transport: Heavy duty vehicles and buses");
const NfrSector Nfr1A3c("1A3c", NfrId(7021), gnfr::Offroad, "Railways");
const NfrSector Nfr1A3biv("1A3biv", NfrId(7017), gnfr::RoadTransport, "Road transport: Mopeds & motorcycles");
const NfrSector Nfr1A3bv("1A3bv", NfrId(7018), gnfr::RoadTransport, "Road transport: Gasoline evaporation");
const NfrSector Nfr1A3bvi("1A3bvi", NfrId(7019), gnfr::RoadTransport, "Road transport: Automobile tyre and brake wear");
const NfrSector Nfr1A3bvii("1A3bvii", NfrId(7020), gnfr::RoadTransport, "Road transport: Automobile road abrasion");
const NfrSector Nfr1A3di_ii("1A3di(ii)", NfrId(7022), gnfr::Shipping, "International inland waterways");
const NfrSector Nfr1B2b("1B2b", NfrId(7041), gnfr::Fugitive, "Fugitive emissions from natural gas (exploration, production, processing, transmission, storage, distribution and other)");
const NfrSector Nfr1A4bi("1A4bi", NfrId(7028), gnfr::Offroad, "Residential: Stationary");
const NfrSector Nfr1A5b("1A5b", NfrId(7034), gnfr::Offroad, "Other, Mobile (including military, land based and recreational boats)");
const NfrSector Nfr2C7d("2C7d", NfrId(7068), gnfr::Industry, "Storage, handling and transport of metal products ");
const NfrSector Nfr2D3d("2D3d", NfrId(7072), gnfr::Solvents, "Coating applications");
const NfrSector Nfr3Da1("3Da1", NfrId(7099), gnfr::AgriOther, "Inorganic N-fertilizers (includes also urea application)");
const NfrSector Nfr3Dc("3Dc", NfrId(7106), gnfr::AgriOther, "Farm-level agricultural operations including storage, handling and transport of agricultural products");
const NfrSector Nfr5C1bii("5C1bii", NfrId(7117), gnfr::Waste, "Hazardous waste incineration");
const NfrSector Nfr5C2("5C2", NfrId(7122), gnfr::Waste, "Open burning of waste");
const NfrSector Nfr5D3("5D3", NfrId(7125), gnfr::Waste, "Other wastewater handling");

}
}

namespace pollutants {
const Pollutant PM10("PM10", "Particulate matter (diameter < 10µm)");
const Pollutant PM2_5("PM2.5", "Particulate matter (diameter < 2.5µm)");
const Pollutant PMcoarse("PMcoarse", "Particulate matter(2.5µm < diameter < 10µm)");
const Pollutant NOx("NOx", "Nitrogen oxides (NOx as NO2)");
const Pollutant NMVOC("NMVOC", "Non-methane volatile organic compounds");
const Pollutant CO("CO", "Carbon monoxide");
const Pollutant PCBs("PCBs", "Polychlorinated buphenyls");
const Pollutant NH3("NH3", "Ammonia");
const Pollutant SOx("SOx", "Sulphur oxides (SOx as SO2)");
const Pollutant PCDD_PCDF("PCDD-PCDF", "PCDD/ PCDF (dioxins/ furans)");
const Pollutant Cd("Cd", "Cadmium");
const Pollutant TSP("TSP", "Total Suspended Particles");
const Pollutant Hg("Hg", "Mercury");

}

}
