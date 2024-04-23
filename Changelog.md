Release 3.1.1
-------------
- Relaxed CAMS spatial patter filename regex

Release 3.1.0
-------------
- Added `separate_point_sources` output config option to configure wheter point sources should be output separately for chimere grids
- Added `scenario` model config option that causes to first search for input files with the `_scenario` suffix, for point sources the scnario name is check as infix: emap_{scenario}_{pollutant}_{year}_*.csv
- Support scaling of the emissions through the scaling input file
- Support automatic scaling of the point sources when they exceed the reported total emissions
- Addition scaling information in the run summary

Release 3.0.0
-------------
- Complete rewrite of E-MAP as a command line tool
