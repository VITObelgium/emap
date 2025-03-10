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
