[![Build](https://github.com/VITObelgium/emap/workflows/Vcpkg%20build/badge.svg?branch=develop)](https://github.com/VITObelgium/emap/actions?query=workflow%3AVcpkg%20build)

The emission mapper (E-MAP) is an emission preprocessor for different Air Quality Models (OPS, BelEUROS, Chimere and AURORA). 

## Building
#### Requirements
- cpp-infra (https://github.com/VITObelgium/cpp-infra)
- Lyra (https://github.com/bfgroup/Lyra)
- Doctest (https://github.com/onqtam/doctest)

The library is built using CMake:
```
cmake -G Ninja "/path/to/emap"
```
