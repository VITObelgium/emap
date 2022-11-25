[![Build](https://github.com/VITObelgium/emap/workflows/Vcpkg%20build/badge.svg?branch=develop)](https://github.com/VITObelgium/emap/actions?query=workflow%3AVcpkg%20build)

The emission mapper (E-MAP) is an emission preprocessor for different Air Quality Models (OPS, BelEUROS, Chimere and AURORA). 

## Building
### Linux
Run these commands in the project root. Requires: a C++17 compliant compiler
Build all the required dependencies:<br/>
`./bootstrap.py --triplet=x64-linux`<br/>
Build emap:<br/>
`./build.py --triplet=x64-linux --dist`

The binary can now be found in `project_root/build/emap-release-x64-linux-dist/`

## Running
The emap model is a command line tool that supports the following arguments
```
emapcli [-?|-h|--help] [-l|--log] [--log-level <number>] [--no-progress] [--concurrency <number>] [-d|--debug] -c|--config <path>

OPTIONS, ARGUMENTS:
  -?, -h, --help
  -l, --log               Print logging on the console
  --log-level <number>    Log level when logging is enabled [1 (debug) - 5 (critical)] (default=2)
  --no-progress           Suppress progress info on the console
  --concurrency <number>  Number of cores to use
  -d, --debug             Dumps internal grid usages
  -c, --config <path>     The e-map run configuration
```

The model run details are configured in the specfied config file
### Config file format
The config file is expected to be in the toml format (https://github.com/toml-lang/toml)

Example
```toml
[model]
grid = "vlops1km"
datapath = "./_input"
year = 2020
report_year = 2022
spatial_pattern_exceptions = "./_input/03_spatial_disaggregation/exceptions.xlsx"
included_pollutants = ["PM2.5", "PM10"]


[output]
path = "./_output_gnfr"
filename_suffix = "_test"
sector_level = "GNFR"
create_country_rasters = false
create_grid_rasters = true

[options]
validation = true
```

### Model section
This section configures the model run
- `grid` the output grid of the model
  
  possible values:
    - "vlops1km"
    - "vlops250m"
    - "chimere_05deg"
    - "chimere_01deg"
    - "chimere_005deg_large"
    - "chimere_005deg_small"
    - "chimere_0025deg"
    - "chimere_emep_01deg"
    - "chimere_cams_01-005deg"
    - "chimere_rio1"
    - "chimere_rio4"
    - "chimere_rio32"

- `datapath` the directory path to the model input data
- `year` the year to run the model for
- `report_year` the report year of the emission data of the model run
- `spatial_pattern_exceptions` path to an xlsx file in which exceptions for spatial patterns are configured. These exceptions overrule the standard rules for spatial patterns.
- `included_pollutants` List of pollutants to include in the model run, this setting is optional, if it is not present all the configured pollutants will be included in the run.

### Output section
This section configures the output of the model
- `path` directory path where the created output files will be stored
- `filename_suffix` suffix to add to filename of the created files in the output directory
- `sector_level` the level in which the output will be aggregated
  
  possible values:
    - "NFR"
    - "GNFR"
    - "SNAP"
- `create_country_rasters` set this option to true to generate geotiffs containing the emissions for the individual countries for each pollutant and each sector
- `create_grid_rasters` set this option to true to generate geotiffs for the configured grid for each pollutant and each sector

### Options section
Additional options
- `validation` when this option is true an additional verification step is done when the model has completed that will compare the input emissions against the output emissions after they have been spread over the grid. The run summary will contain an additional tab with the details.
