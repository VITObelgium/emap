Release 3.2.x
-------------
- Bugfix: Intersections of countries and subgrids sometimes took an additional row and column causing a small fraction of the emissions to end up outside of the grid
- Calculated PMCoarse value will match the PM10 value if no PM2.5 data is available

Release 3.2.1
-------------
- Bugfix: Also perform PMCoarse calculation on the input files with "_extra" suffix.
- Added: Support configuration of the boundaries vector filenames with the `spatial_boundaries_filename` and `spatial_boundaries_eez_filename` configuration keys

Release 3.2.0
-------------
- Added `combine_identical_point_sources` option to enable/disable the combining of point sources with the same location and properties
- Update of the `Chimere rio1` grid
- Improved performance of csv parsing

Release 3.1.1
-------------
- Relaxed CAMS spatial patter filename regex
- Bugfix: Use the same spatial pattern on higher resolutions in case of uniform spread fallback on coursest resolution

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
