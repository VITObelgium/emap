Release 3.x.x
-------------
- Added `separate_point_sources` output config option to configure wheter point sources should be output separately for chimere grids
- Added `scenario` model config option that causes to first search for input files with the `_scenario` suffix, for point sources the scnario name is check as infix: emap_{scenario}_{pollutant}_{year}_*.csv
- Support scaling of the emissions throug the scaling input file
- Support automatic scaling of the point sources when they exceed the reported total emissions

Release 3.0.0
-------------
- Complete rewrite of E-MAP as a command line tool
