cmake_minimum_required(VERSION 3.15)
include(CMakeDependentOption)

project(gdal
    VERSION 3.3.1
    LANGUAGES C CXX
)

enable_testing()

option(GDAL_BUILD_TOOLS "Build the various gdal tools" ON)
option(HAVE_IEEEFP "The floating point format supported by the machine" ON)
option(GDAL_OPENCL_SUPPORT "Enable opencl support" OFF)
option(GDAL_PNG_SUPPORT "Enable png support" OFF)
option(GDAL_JPEG_SUPPORT "Enable jpeg support" OFF)
option(GDAL_ZLIB_SUPPORT "Enable zlib support" OFF)
option(GDAL_GEOS_SUPPORT "Enable geos support" OFF)
option(GDAL_ICONV_SUPPORT "Enable iconv support" OFF)
option(GDAL_LIBXML2_SUPPORT "Enable libxml2 support" OFF)
option(GDAL_CURL_SUPPORT "Enable curl support" OFF)

option(GDAL_FRMTS_AAIGRID "Arc ascii driver support" ON)
option(GDAL_FRMTS_BMP "Bitmap driver support" OFF)
option(GDAL_FRMTS_GIF "Gif driver support" OFF)
option(GDAL_FRMTS_GRIB "Grib driver support" OFF)
option(GDAL_FRMTS_HDF5 "Hdf5 driver support" OFF)
option(GDAL_FRMTS_MBTILES "Mbtiles driver support (sqlite required)" OFF)
option(GDAL_FRMTS_NETCDF "Netcdf driver support" OFF)
option(GDAL_FRMTS_PCRASTER "PCRaster driver support" OFF)
option(GDAL_FRMTS_RAW "Raw driver support" OFF)

option(GDAL_OGR_FRMTS_GEOPKG "Geopackage driver support (sqlite required)" OFF)
option(GDAL_OGR_FRMTS_MVT "Mapbox vector tiles driver support (sqlite required)" OFF)
option(GDAL_OGR_FRMTS_SHAPE "Esri shapefile driver support" OFF)
option(GDAL_OGR_FRMTS_SQLITE "Sqlite driver support (requires spatialite)" OFF)
option(GDAL_OGR_FRMTS_XLSX "Xlsx driver support (expat required)" OFF)
option(GDAL_OGR_FRMTS_CSV "Csv driver support" OFF)

option(GDAL_INSTALL_DATA_FILES "Install the data files" ON)

if (NOT BUILD_SHARED_LIBS)
    set(CPL_DISABLE_DLL ON)
endif ()

set (GDAL_FRMTS_GTIFF ON CACHE BOOL "" FORCE)

if (GDAL_FRMTS_MBTILES)
    set(GDAL_OGR_FRMTS_MVT ON CACHE BOOL "" FORCE)
endif ()

if (GDAL_OGR_FRMTS_MVT OR GDAL_OGR_FRMTS_GEOPKG)
    set (GDAL_OGR_FRMTS_SQLITE ON CACHE BOOL "" FORCE)
endif ()

include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(CheckLibraryExists)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckVariableExists)
include(CheckTypeSize)
include(TestBigEndian)
include(CMakePackageConfigHelpers)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(GDAL_COMPILATION)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options (-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options (-fcolor-diagnostics)
endif ()

if (MINGW)
    add_compile_definitions(SUPPORTS_WMAIN)
    add_link_options(-municode)
endif ()

set(GDAL_THREAD_SUPPORT OFF)
if (NOT GDAL_DISABLE_THREADS)
    find_package(Threads)
    if (CMAKE_USE_PTHREADS_INIT)
        set(CPL_MULTIPROC_PTHREAD ON)
        check_symbol_exists(PTHREAD_MUTEX_RECURSIVE "pthread.h" HAVE_PTHREAD_MUTEX_RECURSIVE)
        check_symbol_exists(PTHREAD_MUTEX_ADAPTIVE_NP "pthread.h" HAVE_PTHREAD_MUTEX_ADAPTIVE_NP)
        check_symbol_exists(PTHREAD_SPINLOCK "pthread.h" HAVE_PTHREAD_SPINLOCK)
    endif ()
    if (TARGET Threads::Threads)
        set (GDAL_THREAD_SUPPORT ON)
    endif ()
endif ()

if (GDAL_OPENCL_SUPPORT)
    find_package(OpenCL REQUIRED)
    add_compile_definitions(HAVE_OPENCL)
endif ()

if (GDAL_JPEG_SUPPORT)
    find_package(JPEG REQUIRED)
endif ()

if (GDAL_PNG_SUPPORT)
    find_package(PNG REQUIRED)
endif ()

if (GDAL_ZLIB_SUPPORT)
    find_package(ZLIB REQUIRED)
    add_compile_definitions(HAVE_LIBZ)
endif ()

if (GDAL_ICONV_SUPPORT)
    find_package(Iconv REQUIRED)
endif ()

if (GDAL_LIBXML2_SUPPORT)
    find_package(LibXml2 REQUIRED)
    add_compile_definitions(HAVE_LIBXML2)
endif ()

if (GDAL_GEOS_SUPPORT)
    find_package(GEOS CONFIG REQUIRED)
    add_compile_definitions(HAVE_GEOS)
endif ()

if (GDAL_CURL_SUPPORT)
    find_package(CURL REQUIRED)
    add_compile_definitions(HAVE_CURL)
endif ()

if (GDAL_OGR_FRMTS_SQLITE)
    find_package(libspatialite REQUIRED)
    add_compile_definitions(HAVE_SQLITE)
endif ()

if (GDAL_FRMTS_GTIFF)
    find_package(TIFF REQUIRED)
    add_compile_definitions(BIGTIFF_SUPPORT)
endif ()

if (GDAL_OGR_FRMTS_XLSX)
    find_package(EXPAT REQUIRED)
    add_compile_definitions(HAVE_EXPAT)
endif ()

find_package(PROJ CONFIG REQUIRED)

if (NOT MSVC)
    check_include_file(stdlib.h STDC_HEADERS)
    check_include_file(assert.h HAVE_ASSERT_H)
    check_function_exists(atoll HAVE_ATOLL)
    check_function_exists(strtoull HAVE_STRTOULL)
    check_function_exists(getrlimit HAVE_GETRLIMIT)
    check_variable_exists(RLIMIT_AS HAVE_RLIMIT_AS)
    check_include_file(csf.h HAVE_CSF_H)
    check_include_file(dbmalloc.h HAVE_DBMALLOC_H)
    check_function_exists(strtof HAVE_DECL_STRTOF)
    check_include_file(direct.h HAVE_DIRECT_H)
    check_include_file(dlfcn.h HAVE_DLFCN_H)
    check_function_exists(vprintf HAVE_VPRINTF)
    if (NOT HAVE_VPRINTF)
        check_function_exists(_doprnt HAVE_DOPRNT)
    endif ()
    check_include_file(errno.h HAVE_ERRNO_H)
    check_include_file(fcntl.h HAVE_FCNTL_H)
    check_include_file(float.h HAVE_FLOAT_H)
    check_function_exists(getcwd HAVE_GETCWD)
    check_include_file(inttypes.h HAVE_INTTYPES_H)
    check_type_size(int INT)
    check_type_size(long LONG)
    check_type_size("long long" LONG_LONG)
    check_type_size("unsigned long" UNSIGNED_LONG)
    check_type_size(int8 INT8)
    check_type_size(int16 INT16)
    check_type_size(int32 INT32)
    check_type_size(__uint128_t UINT128_T)
    check_type_size(uintptr_t UINTPTR_T)
    check_library_exists(dl dlopen "" HAVE_LIBDL)
    check_library_exists(m sqrt "" HAVE_LIBM)
    check_library_exists(rt clock "" HAVE_LIBRT)
    check_include_file(limits.h HAVE_LIMITS_H)
    check_include_file(locale.h HAVE_LOCALE_H)
    check_include_file(memory.h HAVE_MEMORY_H)
    check_function_exists(snprintf HAVE_SNPRINTF)
    check_function_exists(vsnprintf HAVE_VSNPRINTF)
    check_function_exists(vprintf HAVE_VPRINTF)
    check_include_file(stdlib.h HAVE_STDLIB_H)
    check_include_file(strings.h HAVE_STRINGS_H)
    check_include_file(string.h HAVE_STRING_H)
    check_function_exists(strtof HAVE_STRTOF)
    check_include_file(sys/stat.h HAVE_SYS_STAT_H)
    check_include_file(unistd.h HAVE_UNISTD_H)
    check_include_file(values.h HAVE_VALUES_H)
    check_function_exists(readlink HAVE_READLINK)
    check_function_exists(posix_spawnp HAVE_POSIX_SPAWNP)
    check_function_exists(posix_memalign HAVE_POSIX_MEMALIGN)
    check_function_exists(vfork HAVE_VFORK)
    check_function_exists(mmap HAVE_MMAP)
    check_function_exists(sigaction HAVE_MMAP)
    check_function_exists(statvfs HAVE_STATVFS)
    check_function_exists(statvfs64 HAVE_STATVFS64)
    check_function_exists(fseek64 UNIX_STDIO_64)
    check_function_exists(lstat HAVE_LSTAT)
    check_function_exists(fopen64 HAVE_FOPEN64)

    check_c_source_compiles(
        "#ifdef __MINGW32__
         #ifndef __MSVCRT_VERSION__
         #define __MSVCRT_VERSION__ 0x0601
         #endif
         #endif
        
         #include <sys/types.h>
         #include <sys/stat.h>
         int main() { struct __stat64 buf; _stat64( \"\", &buf ); return 0; }"
         HAVE_STAT64
    )

    if (NOT APPLE AND HAVE_STAT64)
        set (VSI_STAT64 "_stat64")
        set (VSI_STAT64_T "__stat64")
    else ()
        set (VSI_STAT64 "stat")
        set (VSI_STAT64_T "stat")
    endif ()

    check_c_source_compiles(
        "int main(int argc, char** argv) {
            int i; __sync_add_and_fetch(&i, 1);
            __sync_sub_and_fetch(&i, 1);
            __sync_bool_compare_and_swap(&i, 0, 1);
            return 0;
        }"
        HAVE_GCC_ATOMIC_BUILTINS
    )

    check_c_source_compiles("
        #include <x86intrin.h>
        int main(int argc, char** argv) {
            (void)__builtin_bswap32(0);
            (void)__builtin_bswap64(0);
            return 0;
        }"
        HAVE_GCC_BSWAP
    )

    check_cxx_source_compiles("
        #include <cmath>
        int main(int argc, char** argv) {
            (void)std::isnan(1.0);
            return 0;
        }"
        HAVE_STD_IS_NAN
    )

    check_cxx_source_compiles("
        #define _XOPEN_SOURCE 700
        #include <locale.h>
        int main(int argc, char** argv) {
            locale_t alocale = newlocale (LC_NUMERIC_MASK, \"C\", 0);
            locale_t oldlocale = uselocale(alocale);
            uselocale(oldlocale);
            freelocale(alocale);
            return 0;
        }"
        HAVE_USE_LOCALE_H
    )

    # 
    # Test if sprintf can be overloaded with _XOPEN_SOURCE being defined or other,
    # includes being included before, which mingw64 4.8 doesn't like (#6390)
    check_cxx_source_compiles("
        #define _XOPEN_SOURCE
        #include <vector>
        #include <stdio.h>
        extern \"C\"
        {
            int sprintf(char *str, const char* fmt, ...);
        }"
        DEPRECATE_SPRINTF
    )

    if (NOT DEPRECATE_SPRINTF)
        add_compile_definitions(DONT_DEPRECATE_SPRINTF)
    endif ()

    test_big_endian(WORDS_BIGENDIAN)
    if (WORDS_BIGENDIAN)
        set(HOST_FILLORDER "FILLORDER_MSB2LSB")
    else ()
        set(HOST_FILLORDER "FILLORDER_LSB2MSB")
    endif ()

    if ("${CMAKE_CXX_VISIBILITY_PRESET}" STREQUAL "hidden")
        set(USE_GCC_VISIBILITY_FLAG ON)
    endif ()
endif ()

if (TARGET JPEG::JPEG)
    set(HAVE_JPEGLIB_H ON)
endif ()

if (TARGET PNG::PNG)
    set(HAVE_PNG_H ON)
endif ()

if (TARGET Iconv::Iconv)
    set(HAVE_ICONV ON)
endif ()

set(SSE_CODE_SNIPPET "
    #ifdef __SSE__
    #include <xmmintrin.h>
        void foo() {
            float fEpsilon = 0.0000000000001f;
            __m128 xmm_small = _mm_load1_ps(&fEpsilon);
        }

        int main() { return 0; }
    #else
        some_error
    #endif"
)

set(SSEFLAGS)
check_c_source_compiles("${SSE_CODE_SNIPPET}" HAVE_SSE_AT_COMPILE_TIME)
if (NOT HAVE_SSE_AT_COMPILE_TIME)
    if (MSVC)
        set(HAVE_SSE_AT_COMPILE_TIME ON)
        if (NOT CMAKE_CL_64)
            # sse2 is on by default on x64 so only enable on 32bit
            set(SSEFLAGS "/arch:SSE2")
        endif ()
    else ()
        set (CMAKE_REQUIRED_FLAGS "-msse")
        check_c_source_compiles("${SSE_CODE_SNIPPET}" HAVE_SSE_AT_COMPILE_TIME_WITH_FLAG)
        unset(CMAKE_REQUIRED_FLAGS)
        if (HAVE_SSE_AT_COMPILE_TIME_WITH_FLAG)
            set(HAVE_SSE_AT_COMPILE_TIME ON)
            set(SSEFLAGS "-msse")
        endif ()
    endif ()
endif ()

set(SSSE3_CODE_SNIPPET "
    #ifdef __SSSE3__
    #include <tmmintrin.h>
        void foo() {
            __m128i xmm_i = _mm_set1_epi16(0);
            xmm_i = _mm_shuffle_epi8(xmm_i, xmm_i);
        }

        int main() { return 0; }
    #else
        some_error
    #endif"
)

set(SSSE3FLAGS)
check_c_source_compiles("${SSSE3_CODE_SNIPPET}" HAVE_SSSE3_AT_COMPILE_TIME)
if (NOT HAVE_SSSE3_AT_COMPILE_TIME)
    if (NOT MSVC)
        set (CMAKE_REQUIRED_FLAGS "-mssse3")
        check_c_source_compiles("${SSSE3_CODE_SNIPPET}" HAVE_SSSE3_AT_COMPILE_TIME_WITH_FLAG)
        unset(CMAKE_REQUIRED_FLAGS)
        if (HAVE_SSSE3_AT_COMPILE_TIME_WITH_FLAG)
            set(HAVE_SSSE3_AT_COMPILE_TIME ON)
            set(SSSE3FLAGS "-mssse3")
        endif ()
    endif ()
endif ()

if (HAVE_SSE_AT_COMPILE_TIME)
    add_compile_definitions(HAVE_SSE_AT_COMPILE_TIME)
    if (SSEFLAGS AND NOT SSSE3FLAGS)
        add_compile_options(${SSEFLAGS})
    endif ()
endif ()

if (HAVE_SSSE3_AT_COMPILE_TIME)
    add_compile_definitions(HAVE_SSSE3_AT_COMPILE_TIME)
    if (SSSE3FLAGS)
        add_compile_options(${SSSE3FLAGS})
    endif ()
endif ()

check_c_source_compiles("
    #include <iconv.h>
    int main(int argc, char** argv) {
        iconv_t cd = 0;
        return iconv (cd, (const char **) 0, 0, 0, 0);
    }"
    ICONV_REQUIRE_CONST
)

check_cxx_source_compiles("
    #include <iconv.h>
    int main(int argc, char** argv) {
        iconv_t cd = 0;
        return iconv (cd, (const char **) 0, 0, 0, 0);
    }"
    ICONV_CPP_REQUIRE_CONST
)

if (ICONV_REQUIRE_CONST)
    set (ICONV_CONST "const")
else ()
    set (ICONV_CONST "")
endif ()

if (ICONV_CPP_REQUIRE_CONST)
    set (ICONV_CPP_CONST "const")
else ()
    set (ICONV_CPP_CONST "")
endif ()

if (MSVC)
    add_compile_definitions(
        NOMINMAX
        _CRT_NONSTDC_NO_DEPRECATE
    )
endif ()

############################################################################################
# port
############################################################################################

add_library(port OBJECT
    port/cpl_aws_win32.cpp
    port/cpl_conv.cpp
    port/cpl_error.cpp
    port/cpl_string.cpp
    port/cplstring.cpp
    port/cplstringlist.cpp
    port/cpl_strtod.cpp
    port/cpl_vsisimple.cpp
    port/cplgetsymbol.cpp
    port/cpl_path.cpp
    port/cpl_csv.cpp
    port/cpl_findfile.cpp
    port/cpl_vsil_win32.cpp
    port/cpl_vsil_unix_stdio_64.cpp
    port/cpl_minixml.cpp
    port/cpl_multiproc.cpp
    port/cpl_list.cpp
    port/cpl_getexecpath.cpp
    port/cpl_vsil.cpp
    port/cpl_vsi_mem.cpp
    port/cpl_http.cpp
    port/cpl_hash_set.cpp
    port/cplkeywordparser.cpp
    port/cpl_recode.cpp
    port/cpl_recode_iconv.cpp
    port/cpl_recode_stub.cpp
    port/cpl_quad_tree.cpp
    port/cpl_vsil_gzip.cpp
    port/cpl_minizip_ioapi.cpp
    port/cpl_minizip_unzip.cpp
    port/cpl_minizip_zip.cpp
    port/cpl_vsil_subfile.cpp
    port/cpl_atomic_ops.cpp
    port/cpl_time.cpp
    port/cpl_vsil_stdout.cpp
    port/cpl_vsil_sparsefile.cpp
    port/cpl_vsil_abstract_archive.cpp
    port/cpl_vsil_tar.cpp
    port/cpl_vsil_curl.cpp
    port/cpl_vsil_s3.cpp
    port/cpl_vsil_gs.cpp
    port/cpl_vsil_az.cpp
    port/cpl_vsil_oss.cpp
    port/cpl_vsil_plugin.cpp
    port/cpl_vsil_swift.cpp
    port/cpl_vsil_webhdfs.cpp
    port/cpl_vsil_hdfs.cpp
    port/cpl_vsil_curl_streaming.cpp
    port/cpl_vsil_stdin.cpp
    port/cpl_vsil_buffered_reader.cpp
    port/cpl_vsil_cache.cpp
    port/cpl_vsil_adls.cpp
    port/cpl_vsil_uploadonclose.cpp
    port/cpl_base64.cpp
    port/cpl_xml_validate.cpp
    port/cpl_spawn.cpp
    port/cpl_google_oauth2.cpp
    port/cpl_progress.cpp
    port/cpl_virtualmem.cpp
    port/cpl_worker_thread_pool.cpp
    port/cpl_vsil_crypt.cpp
    port/cpl_sha1.cpp
    port/cpl_sha256.cpp
    port/cpl_aws.cpp
    port/cpl_vsi_error.cpp
    port/cpl_cpu_features.cpp
    port/cpl_google_cloud.cpp
    port/cpl_azure.cpp
    port/cpl_alibaba_oss.cpp
    port/cpl_json_streaming_parser.cpp
    port/cpl_json_streaming_writer.cpp
    port/cpl_json.cpp
    port/cpl_md5.cpp
    port/cpl_swift.cpp
    port/cpl_vax.cpp
)

set (PORT_PUBLIC_HEADERS
    ${CMAKE_BINARY_DIR}/cpl_config.h
    #cpl_config_extras.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_atomic_ops.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_auto_close.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_conv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_csv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_error.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_hash_set.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_http.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_json.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_list.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_minixml.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_multiproc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_odbc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_port.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_progress.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_quad_tree.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_spawn.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_string.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_time.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_virtualmem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_vsi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_vsi_error.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_vsi_virtual.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/cplkeywordparser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/port/gdal_csv.h
)

if (TARGET ZLIB::ZLIB)
    target_compile_definitions(port PUBLIC HAVE_ZLIB)
    target_sources(port PRIVATE
        port/cpl_vsil_gzip.cpp
        port/cpl_minizip_ioapi.cpp
        port/cpl_minizip_unzip.cpp
        port/cpl_minizip_zip.cpp
    )
    list(APPEND PORT_PUBLIC_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_minizip_ioapi.h
        ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_minizip_unzip.h
        ${CMAKE_CURRENT_SOURCE_DIR}/port/cpl_minizip_zip.h
    )
endif ()

target_include_directories(port
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/port>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/geojson/libjson>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(port PRIVATE
    $<TARGET_NAME_IF_EXISTS:ZLIB::ZLIB>
    $<TARGET_NAME_IF_EXISTS:JPEG::JPEG>
    $<TARGET_NAME_IF_EXISTS:PNG::PNG>
    $<TARGET_NAME_IF_EXISTS:Threads::Threads>
    $<TARGET_NAME_IF_EXISTS:LibXml2::LibXml2>
    $<TARGET_NAME_IF_EXISTS:Iconv::Iconv>
    $<TARGET_NAME_IF_EXISTS:CURL::libcurl>
    ${CMAKE_DL_LIBS}
)

set_target_properties(port PROPERTIES PUBLIC_HEADER "${PORT_PUBLIC_HEADERS}")

############################################################################################
# gcore
############################################################################################

add_library(gcore OBJECT
    gcore/gdalopeninfo.cpp
    gcore/gdaldrivermanager.cpp
    gcore/gdaldriver.cpp
    gcore/gdaldataset.cpp
    gcore/gdalrasterband.cpp
    gcore/gdal_thread_pool.cpp
    gcore/gdal_misc.cpp
    gcore/nasakeywordhandler.cpp
    gcore/tilematrixset.cpp

    gcore/rasterio.cpp
    gcore/rasterio_ssse3.cpp
    gcore/gdalrasterblock.cpp
    gcore/gdal_rat.cpp
    gcore/gdalcolortable.cpp
    gcore/overview.cpp
    gcore/gdaldefaultoverviews.cpp
    gcore/gdalmajorobject.cpp
    gcore/gdalpamdataset.cpp
    gcore/gdalpamrasterband.cpp
    gcore/gdaljp2metadata.cpp
    gcore/gdaljp2box.cpp
    gcore/gdalmultidomainmetadata.cpp
    gcore/gdalpamproxydb.cpp
    gcore/gdalallvalidmaskband.cpp
    gcore/gdalnodatamaskband.cpp
    gcore/gdalproxydataset.cpp
    gcore/gdalproxypool.cpp
    gcore/gdalnodatavaluesmaskband.cpp
    gcore/gdaldefaultasync.cpp
    gcore/gdaldllmain.cpp
    gcore/gdalexif.cpp
    gcore/gdalgeorefpamdataset.cpp
    gcore/gdaljp2abstractdataset.cpp
    gcore/gdalvirtualmem.cpp
    gcore/gdaloverviewdataset.cpp
    gcore/gdalrescaledalphaband.cpp
    gcore/gdaljp2structure.cpp
    gcore/gdal_mdreader.cpp
    gcore/gdaljp2metadatagenerator.cpp
    gcore/gdalabstractbandblockcache.cpp
    gcore/rawdataset.cpp
    gcore/gdalarraybandblockcache.cpp
    gcore/gdalhashsetbandblockcache.cpp
    gcore/gdalmultidim.cpp
    gcore/gdalpython.cpp
    gcore/gdalpythondriverloader.cpp

    gcore/mdreader/reader_digital_globe.cpp
    gcore/mdreader/reader_geo_eye.cpp
    gcore/mdreader/reader_orb_view.cpp
    gcore/mdreader/reader_pleiades.cpp
    gcore/mdreader/reader_rdk1.cpp
    gcore/mdreader/reader_landsat.cpp
    gcore/mdreader/reader_spot.cpp
    gcore/mdreader/reader_rapid_eye.cpp
    gcore/mdreader/reader_alos.cpp
    gcore/mdreader/reader_eros.cpp
    gcore/mdreader/reader_kompsat.cpp
)

set(GCORE_PUBLIC_HEADERS
    ${CMAKE_SOURCE_DIR}/gcore/gdal.h
    ${CMAKE_SOURCE_DIR}/gcore/gdal_frmts.h
    ${CMAKE_SOURCE_DIR}/gcore/gdal_mdreader.h
    ${CMAKE_SOURCE_DIR}/gcore/gdal_pam.h
    ${CMAKE_SOURCE_DIR}/gcore/gdal_priv.h
    ${CMAKE_SOURCE_DIR}/gcore/gdal_proxy.h
    ${CMAKE_SOURCE_DIR}/gcore/gdal_rat.h
	${CMAKE_SOURCE_DIR}/gcore/gdalgeorefpamdataset.h
    ${CMAKE_SOURCE_DIR}/gcore/gdaljp2abstractdataset.h
    ${CMAKE_SOURCE_DIR}/gcore/gdaljp2metadata.h
    ${CMAKE_SOURCE_DIR}/gcore/rawdataset.h
    
    # gnm.h
    # gnm_api.h
    # gnmgraph.h
)

target_include_directories(gcore
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/gcore>
PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts/gtiff>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts/mem>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts/vrt>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/generic>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/gnm/>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/gnm/gnm_frmts/>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/geojson>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/alg>
)

target_link_libraries(gcore PUBLIC
    port
    $<TARGET_NAME_IF_EXISTS:GEOS::geos_c>
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:psapi>
)

set_target_properties(gcore PROPERTIES PUBLIC_HEADER "${GCORE_PUBLIC_HEADERS}")
############################################################################################
# frmts
############################################################################################
add_library(frmts OBJECT
    frmts/gdalallregister.cpp
    frmts/derived/deriveddataset.cpp
    frmts/derived/derivedlist.c
    
    frmts/hfa/hfaopen.cpp
    frmts/hfa/hfaentry.cpp
    frmts/hfa/hfadictionary.cpp
    frmts/hfa/hfafield.cpp
    frmts/hfa/hfatype.cpp
    frmts/hfa/hfaband.cpp
    frmts/hfa/hfacompress.cpp
    frmts/hfa/hfadataset.cpp
    frmts/hfa/hfa_overviews.cpp

    frmts/vrt/vrtdataset.cpp
    frmts/vrt/vrtrasterband.cpp
    frmts/vrt/vrtdriver.cpp
    frmts/vrt/vrtsources.cpp
    frmts/vrt/vrtfilters.cpp
    frmts/vrt/vrtsourcedrasterband.cpp
    frmts/vrt/vrtrawrasterband.cpp
    frmts/vrt/vrtwarped.cpp
    frmts/vrt/vrtderivedrasterband.cpp
    frmts/vrt/vrtpansharpened.cpp
    frmts/vrt/pixelfunctions.cpp
    frmts/vrt/vrtmultidim.cpp

    frmts/mem/memdataset.cpp
)

set (FRMTS_PUBLIC_HEADERS
    ${CMAKE_SOURCE_DIR}/frmts/vrt/gdal_vrt.h
    ${CMAKE_SOURCE_DIR}/frmts/vrt/vrtdataset.h
    ${CMAKE_SOURCE_DIR}/frmts/mem/memdataset.h
)

target_compile_definitions(frmts PRIVATE FRMT_mem  FRMT_vrt FRMT_hfa)

if (TARGET PNG::PNG)
    target_sources(frmts PRIVATE frmts/png/pngdataset.cpp)
    target_link_libraries(frmts PRIVATE PNG::PNG)
    target_compile_definitions(frmts PRIVATE FRMT_png)
endif ()

if (GDAL_FRMTS_AAIGRID)
    target_sources(frmts PRIVATE frmts/aaigrid/aaigriddataset.cpp)
    target_compile_definitions(frmts PRIVATE FRMT_aaigrid)
endif ()

if (GDAL_FRMTS_BMP)
    target_sources(frmts PRIVATE frmts/bmp/bmpdataset.cpp)
    target_compile_definitions(frmts PRIVATE FRMT_bmp)
endif ()

if (GDAL_FRMTS_GIF)
    find_package(GIF REQUIRED)
    target_sources(frmts PRIVATE 
        frmts/gif/biggifdataset.cpp
        frmts/gif/gifabstractdataset.cpp
        frmts/gif/gifdataset.cpp
    )
    target_link_libraries(frmts PRIVATE GIF::GIF)
    target_compile_definitions(frmts PRIVATE FRMT_gif)
endif ()

if (GDAL_FRMTS_GRIB)
    target_sources(frmts PRIVATE 
        frmts/grib/gribdataset.cpp
        frmts/grib/gribcreatecopy.cpp

        frmts/grib/degrib/degrib/degrib1.cpp
        frmts/grib/degrib/degrib/degrib2.cpp
        frmts/grib/degrib/degrib/grib1tab.cpp
        frmts/grib/degrib/degrib/inventory.cpp
        frmts/grib/degrib/degrib/metaname.cpp
        frmts/grib/degrib/degrib/metaparse.cpp
        frmts/grib/degrib/degrib/metaprint.cpp
        frmts/grib/degrib/degrib/tendian.cpp
        frmts/grib/degrib/degrib/clock.c
        frmts/grib/degrib/degrib/engribapi.c
        frmts/grib/degrib/degrib/grib2api.c
        frmts/grib/degrib/degrib/hazard.c
        frmts/grib/degrib/degrib/myassert.c
        frmts/grib/degrib/degrib/myerror.c
        frmts/grib/degrib/degrib/myutil.c
        frmts/grib/degrib/degrib/scan.c
        frmts/grib/degrib/degrib/weather.c

        frmts/grib/degrib/g2clib/cmplxpack.c
        frmts/grib/degrib/g2clib/compack.c
        frmts/grib/degrib/g2clib/comunpack.c
        frmts/grib/degrib/g2clib/dec_png.c
        frmts/grib/degrib/g2clib/drstemplates.c
        frmts/grib/degrib/g2clib/enc_jpeg2000.c
        frmts/grib/degrib/g2clib/enc_png.c
        frmts/grib/degrib/g2clib/g2_addfield.c
        frmts/grib/degrib/g2clib/g2_addgrid.c
        frmts/grib/degrib/g2clib/g2_addlocal.c
        frmts/grib/degrib/g2clib/g2_create.c
        frmts/grib/degrib/g2clib/g2_free.c
        frmts/grib/degrib/g2clib/g2_getfld.c
        frmts/grib/degrib/g2clib/g2_gribend.c
        frmts/grib/degrib/g2clib/g2_info.c
        frmts/grib/degrib/g2clib/g2_miss.c
        frmts/grib/degrib/g2clib/g2_unpack1.c
        frmts/grib/degrib/g2clib/g2_unpack2.c
        frmts/grib/degrib/g2clib/g2_unpack3.c
        frmts/grib/degrib/g2clib/g2_unpack4.c
        frmts/grib/degrib/g2clib/g2_unpack5.c
        frmts/grib/degrib/g2clib/g2_unpack6.c
        frmts/grib/degrib/g2clib/g2_unpack7.c
        frmts/grib/degrib/g2clib/gbits.c
        frmts/grib/degrib/g2clib/getdim.c
        frmts/grib/degrib/g2clib/getpoly.c
        frmts/grib/degrib/g2clib/gridtemplates.c
        frmts/grib/degrib/g2clib/int_power.c
        frmts/grib/degrib/g2clib/jpcpack.c
        frmts/grib/degrib/g2clib/jpcunpack.c
        frmts/grib/degrib/g2clib/misspack.c
        frmts/grib/degrib/g2clib/mkieee.c
        frmts/grib/degrib/g2clib/pack_gp.c
        frmts/grib/degrib/g2clib/pdstemplates.c
        frmts/grib/degrib/g2clib/pngpack.c
        frmts/grib/degrib/g2clib/pngunpack.c
        frmts/grib/degrib/g2clib/rdieee.c
        frmts/grib/degrib/g2clib/reduce.c
        frmts/grib/degrib/g2clib/seekgb.c
        frmts/grib/degrib/g2clib/simpack.c
        frmts/grib/degrib/g2clib/simunpack.c
        frmts/grib/degrib/g2clib/specpack.c
        frmts/grib/degrib/g2clib/specunpack.c

        frmts/grib/degrib/g2clib/dec_jpeg2000.cpp
    )

    target_include_directories(frmts PRIVATE frmts/grib/degrib/degrib frmts/grib/degrib/g2clib)
    target_compile_definitions(frmts PRIVATE FRMT_grib)
endif ()

if (GDAL_FRMTS_GTIFF)
    target_sources(frmts PRIVATE 
        frmts/gtiff/geotiff.cpp
        frmts/gtiff/gt_wkt_srs.cpp
        frmts/gtiff/gt_overview.cpp
        frmts/gtiff/tifvsi.cpp
        frmts/gtiff/tif_float.c
        frmts/gtiff/gt_citation.cpp
        frmts/gtiff/gt_jpeg_copy.cpp
        frmts/gtiff/cogdriver.cpp

        frmts/gtiff/libgeotiff/xtiff.c
        frmts/gtiff/libgeotiff/geo_free.c
        frmts/gtiff/libgeotiff/geo_get.c
        frmts/gtiff/libgeotiff/geo_names.c
        frmts/gtiff/libgeotiff/geo_new.c
        frmts/gtiff/libgeotiff/geo_print.c
        frmts/gtiff/libgeotiff/geo_set.c
        frmts/gtiff/libgeotiff/geo_tiffp.c
        frmts/gtiff/libgeotiff/geo_write.c
        frmts/gtiff/libgeotiff/geo_normalize.c
        frmts/gtiff/libgeotiff/geotiff_proj4.c
        frmts/gtiff/libgeotiff/geo_extra.c
        frmts/gtiff/libgeotiff/geo_trans.c
        frmts/gtiff/libgeotiff/geo_simpletags.c
    )
    target_include_directories(frmts PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts/gtiff>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts/gtiff/libgeotiff>
    )
    target_compile_definitions(frmts PRIVATE FRMT_gtiff DONT_DEPRECATE_SPRINTF INTERNAL_LIBGEOTIFF RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS)
    target_link_libraries(frmts PRIVATE TIFF::TIFF)
endif ()

if (GDAL_FRMTS_HDF5)
    find_package(HDF5 CONFIG REQUIRED)
    target_sources(frmts PRIVATE 
        frmts/hdf5/hdf5dataset.cpp
        frmts/hdf5/hdf5imagedataset.cpp
        frmts/hdf5/bagdataset.cpp
        frmts/hdf5/gh5_convenience.cpp
        frmts/hdf5/iso19115_srs.cpp
        frmts/hdf5/hdf5multidim.cpp
    )
    target_link_libraries(frmts PRIVATE hdf5-static)
    target_compile_definitions(frmts PRIVATE FRMT_hdf5)
endif ()

if (GDAL_FRMTS_MBTILES)
    target_sources(frmts PRIVATE 
        frmts/mbtiles/mbtilesdataset.cpp
    )
    target_include_directories(frmts PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/geojson>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/mvt>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/gpkg>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/sqlite>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/geojson/libjson>
    )
    target_link_libraries(frmts PRIVATE $<TARGET_NAME_IF_EXISTS:GEOS::geos_c> sqlite3)
    target_compile_definitions(frmts PRIVATE FRMT_mbtiles)
endif ()

if (GDAL_FRMTS_NETCDF)
    find_package(netCDF CONFIG REQUIRED)
    target_sources(frmts PRIVATE 
        frmts/netcdf/netcdfdataset.cpp
        frmts/netcdf/netcdflayer.cpp
        frmts/netcdf/netcdfwriterconfig.cpp
        frmts/netcdf/gmtdataset.cpp
        frmts/netcdf/netcdfsg.cpp
        frmts/netcdf/netcdflayersg.cpp
        frmts/netcdf/netcdfsgwriterutil.cpp
        frmts/netcdf/netcdfmultidim.cpp
        frmts/netcdf/netcdfvirtual.cpp
        frmts/netcdf/netcdf_sentinel3_sral_mwr.cpp
    )
    target_link_libraries(frmts PRIVATE netCDF::netcdf)
    target_compile_definitions(frmts PRIVATE FRMT_netcdf NETCDF_HAS_NC4 $<$<TARGET_EXISTS:hdf5-static>:HAVE_HDF5>)
endif ()

if (GDAL_FRMTS_PCRASTER)
    target_sources(frmts PRIVATE 
        frmts/pcraster/pcrasterdataset.cpp
        frmts/pcraster/pcrastermisc.cpp
        frmts/pcraster/pcrasterrasterband.cpp
        frmts/pcraster/pcrasterutil.cpp
        # internal libcsf
        frmts/pcraster/libcsf/_getcell.c
        frmts/pcraster/libcsf/_getrow.c
        frmts/pcraster/libcsf/_gsomece.c
        frmts/pcraster/libcsf/_putcell.c
        frmts/pcraster/libcsf/_rputrow.c
        frmts/pcraster/libcsf/angle.c
        frmts/pcraster/libcsf/attravai.c
        frmts/pcraster/libcsf/attrsize.c
        frmts/pcraster/libcsf/cellsize.c
        frmts/pcraster/libcsf/create2.c
        frmts/pcraster/libcsf/csfglob.c
        frmts/pcraster/libcsf/csfsup.c
        frmts/pcraster/libcsf/delattr.c
        frmts/pcraster/libcsf/dumconv.c
        frmts/pcraster/libcsf/endian.c
        frmts/pcraster/libcsf/file.c
        frmts/pcraster/libcsf/filename.c
        frmts/pcraster/libcsf/gattrblk.c
        frmts/pcraster/libcsf/gattridx.c
        frmts/pcraster/libcsf/gcellrep.c
        frmts/pcraster/libcsf/gdattype.c
        frmts/pcraster/libcsf/getattr.c
        frmts/pcraster/libcsf/getx0.c
        frmts/pcraster/libcsf/gety0.c
        frmts/pcraster/libcsf/ggisfid.c
        frmts/pcraster/libcsf/gmaxval.c
        frmts/pcraster/libcsf/gminval.c
        frmts/pcraster/libcsf/gnrcols.c
        frmts/pcraster/libcsf/gnrrows.c
        frmts/pcraster/libcsf/gproj.c
        frmts/pcraster/libcsf/gputproj.c
        frmts/pcraster/libcsf/gvalscal.c
        frmts/pcraster/libcsf/gvartype.c
        frmts/pcraster/libcsf/gversion.c
        frmts/pcraster/libcsf/ismv.c
        frmts/pcraster/libcsf/kernlcsf.c
        frmts/pcraster/libcsf/legend.c
        frmts/pcraster/libcsf/mclose.c
        frmts/pcraster/libcsf/mopen.c
        frmts/pcraster/libcsf/moreattr.c
        frmts/pcraster/libcsf/mperror.c
        frmts/pcraster/libcsf/pgisfid.c
        frmts/pcraster/libcsf/pmaxval.c
        frmts/pcraster/libcsf/pminval.c
        frmts/pcraster/libcsf/putallmv.c
        frmts/pcraster/libcsf/putattr.c
        frmts/pcraster/libcsf/putsomec.c
        frmts/pcraster/libcsf/putx0.c
        frmts/pcraster/libcsf/puty0.c
        frmts/pcraster/libcsf/pvalscal.c
        frmts/pcraster/libcsf/rattrblk.c
        frmts/pcraster/libcsf/rcomp.c
        frmts/pcraster/libcsf/rcoords.c
        frmts/pcraster/libcsf/rdup2.c
        frmts/pcraster/libcsf/reseterr.c
        frmts/pcraster/libcsf/rextend.c
        frmts/pcraster/libcsf/rmalloc.c
        frmts/pcraster/libcsf/rrowcol.c
        frmts/pcraster/libcsf/ruseas.c
        frmts/pcraster/libcsf/setangle.c
        frmts/pcraster/libcsf/setmv.c
        frmts/pcraster/libcsf/setvtmv.c
        frmts/pcraster/libcsf/strconst.c
        frmts/pcraster/libcsf/strpad.c
        frmts/pcraster/libcsf/swapio.c
        frmts/pcraster/libcsf/trackmm.c
        frmts/pcraster/libcsf/vs2.c
        frmts/pcraster/libcsf/vsdef.c
        frmts/pcraster/libcsf/vsis.c
        frmts/pcraster/libcsf/vsvers.c
        frmts/pcraster/libcsf/wattrblk.c
    )
    target_compile_definitions(frmts PRIVATE FRMT_pcraster USE_IN_GDAL)
    target_include_directories(frmts PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts/pcraster/libcsf>
    )
endif ()

if (GDAL_FRMTS_RAW)
    target_sources(frmts PRIVATE
        frmts/raw/ehdrdataset.cpp
        frmts/raw/pauxdataset.cpp
        frmts/raw/doq1dataset.cpp
        frmts/raw/doq2dataset.cpp
        frmts/raw/mffdataset.cpp
        frmts/raw/hkvdataset.cpp
        frmts/raw/pnmdataset.cpp
        frmts/raw/fujibasdataset.cpp
        frmts/raw/envidataset.cpp
        frmts/raw/gscdataset.cpp
        frmts/raw/fastdataset.cpp
        frmts/raw/atlsci_spheroid.cpp
        frmts/raw/btdataset.cpp
        frmts/raw/landataset.cpp
        frmts/raw/cpgdataset.cpp
        frmts/raw/idadataset.cpp
        frmts/raw/ndfdataset.cpp
        frmts/raw/dipxdataset.cpp
        frmts/raw/genbindataset.cpp
        frmts/raw/lcpdataset.cpp
        frmts/raw/eirdataset.cpp
        frmts/raw/gtxdataset.cpp
        frmts/raw/loslasdataset.cpp
        frmts/raw/ntv2dataset.cpp
        frmts/raw/ace2dataset.cpp
        frmts/raw/snodasdataset.cpp
        frmts/raw/ctable2dataset.cpp
        frmts/raw/krodataset.cpp
        frmts/raw/roipacdataset.cpp
        frmts/raw/iscedataset.cpp
        frmts/raw/rrasterdataset.cpp
        frmts/raw/byndataset.cpp
        frmts/raw/ntv1dataset.cpp
    )
    target_compile_definitions(frmts PRIVATE FRMT_raw)
endif ()

target_include_directories(frmts PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/alg>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts/raw>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/apps>
)

target_link_libraries(frmts PRIVATE gcore)
set_target_properties(frmts PROPERTIES PUBLIC_HEADER "${FRMTS_PUBLIC_HEADERS}")

message(STATUS "Supported raster formats:")
message(STATUS "Arc ascii driver support:         ${GDAL_FRMTS_AAIGRID}")
message(STATUS "Bitmap driver support:            ${GDAL_FRMTS_BMP}")
message(STATUS "Gif driver support:               ${GDAL_FRMTS_GIF}")
message(STATUS "Grib driver support:              ${GDAL_FRMTS_GRIB}")
message(STATUS "Geotiff driver support:           ${GDAL_FRMTS_GTIFF}")
message(STATUS "Hdf5 driver support:              ${GDAL_FRMTS_HDF5}")
message(STATUS "Mbtiles driver support:           ${GDAL_FRMTS_MBTILES}")
message(STATUS "Netcdf driver support:            ${GDAL_FRMTS_NETCDF}")
message(STATUS "PCRaster driver support:          ${GDAL_FRMTS_PCRASTER}")
message(STATUS "Raw driver support:               ${GDAL_FRMTS_RAW}")

############################################################################################
# ogr
############################################################################################

add_library(ogr OBJECT
    ogr/ogrgeometryfactory.cpp
    ogr/ogrpoint.cpp
    ogr/ogrcurve.cpp
    ogr/ogrsurface.cpp
    ogr/ogr_api.cpp
    ogr/ogrlinestring.cpp
    ogr/ogrtriangle.cpp
    ogr/ogrpolygon.cpp
    ogr/ogrlinearring.cpp
    ogr/ogrutils.cpp
    ogr/ogrgeometry.cpp
    ogr/ogrgeometrycollection.cpp
    ogr/ogrmultipolygon.cpp
    ogr/ogrmultipoint.cpp
    ogr/ogrmultilinestring.cpp
    ogr/ogrcircularstring.cpp
    ogr/ogrcompoundcurve.cpp
    ogr/ogrcurvepolygon.cpp
    ogr/ogrtriangulatedsurface.cpp
    ogr/ogrcurvecollection.cpp
    ogr/ogrmultisurface.cpp
    ogr/ogrmulticurve.cpp
    ogr/ogrpolyhedralsurface.cpp
    ogr/ogrfeature.cpp
    ogr/ogrfeaturedefn.cpp
    ogr/ogrfielddefn.cpp
    ogr/ogr_srsnode.cpp
    ogr/ogrspatialreference.cpp
    ogr/ogr_fromepsg.cpp
    ogr/ogrct.cpp
    ogr/ogrfeaturestyle.cpp
    ogr/ogr_srs_esri.cpp
    ogr/ogrfeaturequery.cpp
    ogr/ogr_srs_xml.cpp
    ogr/ograssemblepolygon.cpp
    ogr/gml2ogrgeometry.cpp
    ogr/ogr2gmlgeometry.cpp
    ogr/ogr_srs_pci.cpp
    ogr/ogr_srs_usgs.cpp
    ogr/ogr_srs_dict.cpp
    ogr/ogr_srs_panorama.cpp
    ogr/ogr_srs_ozi.cpp
    ogr/ogr_srs_erm.cpp
    ogr/ogr_expat.cpp
    ogr/swq.cpp
    ogr/swq_parser.cpp
    ogr/swq_select.cpp
    ogr/swq_op_registrar.cpp
    ogr/swq_op_general.cpp
    ogr/swq_expr_node.cpp
    ogr/ogrpgeogeometry.cpp
    ogr/ogrgeomediageometry.cpp
    ogr/ogr_geocoding.cpp
    ogr/ogrgeomfielddefn.cpp
    ogr/ograpispy.cpp
    ogr/ogr_xerces.cpp
    ogr/ogr_geo_utils.cpp
    ogr/ogr_proj_p.cpp
    
    ogr/ogrsf_frmts/generic/ogrsfdriverregistrar.cpp
    ogr/ogrsf_frmts/generic/ogrlayer.cpp
    ogr/ogrsf_frmts/generic/ogrdatasource.cpp
    ogr/ogrsf_frmts/generic/ogrsfdriver.cpp
    ogr/ogrsf_frmts/generic/ogrregisterall.cpp
    ogr/ogrsf_frmts/generic/ogr_gensql.cpp
    ogr/ogrsf_frmts/generic/ogr_attrind.cpp
    ogr/ogrsf_frmts/generic/ogr_miattrind.cpp
    ogr/ogrsf_frmts/generic/ogrlayerdecorator.cpp
    ogr/ogrsf_frmts/generic/ogrwarpedlayer.cpp
    ogr/ogrsf_frmts/generic/ogrunionlayer.cpp
    ogr/ogrsf_frmts/generic/ogrlayerpool.cpp
    ogr/ogrsf_frmts/generic/ogrmutexedlayer.cpp
    ogr/ogrsf_frmts/generic/ogrmutexeddatasource.cpp
    ogr/ogrsf_frmts/generic/ogremulatedtransaction.cpp
    ogr/ogrsf_frmts/generic/ogreditablelayer.cpp

    ogr/ogrsf_frmts/mem/ogrmemdatasource.cpp
    ogr/ogrsf_frmts/mem/ogrmemdriver.cpp
    ogr/ogrsf_frmts/mem/ogrmemlayer.cpp

    ogr/ogrsf_frmts/geojson/ogrgeojsondriver.cpp
    ogr/ogrsf_frmts/geojson/ogrgeojsondatasource.cpp
    ogr/ogrsf_frmts/geojson/ogrgeojsonlayer.cpp
    ogr/ogrsf_frmts/geojson/ogrgeojsonwritelayer.cpp
    ogr/ogrsf_frmts/geojson/ogrgeojsonutils.cpp
    ogr/ogrsf_frmts/geojson/ogrgeojsonreader.cpp
    ogr/ogrsf_frmts/geojson/ogrgeojsonwriter.cpp
    ogr/ogrsf_frmts/geojson/ogresrijsondriver.cpp
    ogr/ogrsf_frmts/geojson/ogresrijsonreader.cpp
    ogr/ogrsf_frmts/geojson/ogrtopojsondriver.cpp
    ogr/ogrsf_frmts/geojson/ogrtopojsonreader.cpp
    ogr/ogrsf_frmts/geojson/ogrgeojsonseqdriver.cpp
    ogr/ogrsf_frmts/geojson/libjson/arraylist.c
    ogr/ogrsf_frmts/geojson/libjson/debug.c
    ogr/ogrsf_frmts/geojson/libjson/json_object.c
    ogr/ogrsf_frmts/geojson/libjson/json_tokener.c
    ogr/ogrsf_frmts/geojson/libjson/json_util.c
    ogr/ogrsf_frmts/geojson/libjson/linkhash.c
    ogr/ogrsf_frmts/geojson/libjson/printbuf.c
    ogr/ogrsf_frmts/geojson/libjson/json_object_iterator.c
    ogr/ogrsf_frmts/geojson/libjson/json_c_version.c
    
    ogr/ogrsf_frmts/kml/ogr2kmlgeometry.cpp
    ogr/ogrsf_frmts/kml/ogrkmldriver.cpp
    ogr/ogrsf_frmts/kml/ogrkmldatasource.cpp
    ogr/ogrsf_frmts/kml/ogrkmllayer.cpp
    $<$<TARGET_EXISTS:EXPAT::EXPAT>:ogr/ogrsf_frmts/kml/kml.cpp>
    $<$<TARGET_EXISTS:EXPAT::EXPAT>:ogr/ogrsf_frmts/kml/kmlnode.cpp>
    $<$<TARGET_EXISTS:EXPAT::EXPAT>:ogr/ogrsf_frmts/kml/kmlvector.cpp>

    ogr/ogrsf_frmts/mitab/mitab_rawbinblock.cpp
    ogr/ogrsf_frmts/mitab/mitab_mapheaderblock.cpp
    ogr/ogrsf_frmts/mitab/mitab_mapindexblock.cpp
    ogr/ogrsf_frmts/mitab/mitab_mapobjectblock.cpp
    ogr/ogrsf_frmts/mitab/mitab_mapcoordblock.cpp
    ogr/ogrsf_frmts/mitab/mitab_feature.cpp
    ogr/ogrsf_frmts/mitab/mitab_feature_mif.cpp
    ogr/ogrsf_frmts/mitab/mitab_mapfile.cpp
    ogr/ogrsf_frmts/mitab/mitab_idfile.cpp
    ogr/ogrsf_frmts/mitab/mitab_datfile.cpp
    ogr/ogrsf_frmts/mitab/mitab_tabfile.cpp
    ogr/ogrsf_frmts/mitab/mitab_miffile.cpp
    ogr/ogrsf_frmts/mitab/mitab_utils.cpp
    ogr/ogrsf_frmts/mitab/mitab_imapinfofile.cpp
    ogr/ogrsf_frmts/mitab/mitab_middatafile.cpp
    ogr/ogrsf_frmts/mitab/mitab_bounds.cpp
    ogr/ogrsf_frmts/mitab/mitab_maptoolblock.cpp
    ogr/ogrsf_frmts/mitab/mitab_tooldef.cpp
    ogr/ogrsf_frmts/mitab/mitab_coordsys.cpp
    ogr/ogrsf_frmts/mitab/mitab_spatialref.cpp
    ogr/ogrsf_frmts/mitab/mitab_ogr_driver.cpp
    ogr/ogrsf_frmts/mitab/mitab_indfile.cpp
    ogr/ogrsf_frmts/mitab/mitab_tabview.cpp
    ogr/ogrsf_frmts/mitab/mitab_ogr_datasource.cpp
    ogr/ogrsf_frmts/mitab/mitab_geometry.cpp
    ogr/ogrsf_frmts/mitab/mitab_tabseamless.cpp

    ogr/ogrsf_frmts/vrt/ogrvrtdatasource.cpp
    ogr/ogrsf_frmts/vrt/ogrvrtdriver.cpp
    ogr/ogrsf_frmts/vrt/ogrvrtlayer.cpp
)

set(OGR_PUBLIC_HEADERS
    ${CMAKE_SOURCE_DIR}/ogr/ogr_api.h
    ${CMAKE_SOURCE_DIR}/ogr/ogr_core.h
    ${CMAKE_SOURCE_DIR}/ogr/ogr_feature.h
    ${CMAKE_SOURCE_DIR}/ogr/ogr_featurestyle.h
    ${CMAKE_SOURCE_DIR}/ogr/ogr_geocoding.h
    ${CMAKE_SOURCE_DIR}/ogr/ogr_geometry.h
    ${CMAKE_SOURCE_DIR}/ogr/ogr_p.h
    ${CMAKE_SOURCE_DIR}/ogr/ogr_spatialref.h
    ${CMAKE_SOURCE_DIR}/ogr/ogr_srs_api.h
    ${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/ogrsf_frmts.h
)

if (GDAL_OGR_FRMTS_CSV)
    target_sources(ogr PRIVATE
        ogr/ogrsf_frmts/csv/ogrcsvdatasource.cpp
        ogr/ogrsf_frmts/csv/ogrcsvdriver.cpp
        ogr/ogrsf_frmts/csv/ogrcsvlayer.cpp
    )
    target_compile_definitions(ogr PRIVATE
        CSV_ENABLED
    )
endif ()

if (GDAL_OGR_FRMTS_SHAPE)
    target_sources(ogr PRIVATE
        ogr/ogrsf_frmts/shape/dbfopen.c
        ogr/ogrsf_frmts/shape/dbfopen_wrapper.c
        ogr/ogrsf_frmts/shape/ogrshapedatasource.cpp
        ogr/ogrsf_frmts/shape/ogrshapedriver.cpp
        ogr/ogrsf_frmts/shape/ogrshapelayer.cpp
        ogr/ogrsf_frmts/shape/sbnsearch.c
        ogr/ogrsf_frmts/shape/sbnsearch_wrapper.c
        ogr/ogrsf_frmts/shape/shape2ogr.cpp
        ogr/ogrsf_frmts/shape/shp_vsi.c
        ogr/ogrsf_frmts/shape/shpopen.c
        ogr/ogrsf_frmts/shape/shpopen_wrapper.c
        ogr/ogrsf_frmts/shape/shptree.c
        ogr/ogrsf_frmts/shape/shptree_wrapper.c
    )
    target_compile_definitions(ogr PRIVATE
        SHAPE_ENABLED
        RENAME_INTERNAL_SHAPELIB_SYMBOLS
        SHPAPI_CALL=
        SAOffset=vsi_l_offset
        USE_CPL
    )
endif ()

if (GDAL_OGR_FRMTS_MVT)
    target_sources(ogr PRIVATE
        ogr/ogrsf_frmts/mvt/ogrmvtdataset.cpp
        ogr/ogrsf_frmts/mvt/mvtutils.cpp
        ogr/ogrsf_frmts/mvt/mvt_tile.cpp
    )
    target_compile_definitions(ogr PRIVATE MVT_ENABLED)
    target_link_libraries(ogr PRIVATE sqlite3)
    target_include_directories(ogr PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/osm>
    )
endif ()

if (GDAL_OGR_FRMTS_GEOPKG OR GDAL_OGR_FRMTS_SQLITE)
    target_sources(ogr PRIVATE
        ogr/ogrsf_frmts/gpkg/gdalgeopackagerasterband.cpp
        ogr/ogrsf_frmts/gpkg/ogrgeopackagedatasource.cpp
        ogr/ogrsf_frmts/gpkg/ogrgeopackagedriver.cpp
        ogr/ogrsf_frmts/gpkg/ogrgeopackagelayer.cpp
        ogr/ogrsf_frmts/gpkg/ogrgeopackageselectlayer.cpp
        ogr/ogrsf_frmts/gpkg/ogrgeopackagetablelayer.cpp
        ogr/ogrsf_frmts/gpkg/ogrgeopackageutility.cpp
    )
    target_compile_definitions(ogr PRIVATE GPKG_ENABLED)
    target_include_directories(ogr PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/sqlite>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts/mem>
    )

    target_sources(ogr PRIVATE
        ogr/ogrsf_frmts/sqlite/ogrsqliteapiroutines.c
        ogr/ogrsf_frmts/sqlite/ogrsqlitedatasource.cpp
        ogr/ogrsf_frmts/sqlite/ogrsqlitedriver.cpp
        ogr/ogrsf_frmts/sqlite/ogrsqliteexecutesql.cpp
        ogr/ogrsf_frmts/sqlite/ogrsqlitelayer.cpp
        ogr/ogrsf_frmts/sqlite/ogrsqliteselectlayer.cpp
        ogr/ogrsf_frmts/sqlite/ogrsqlitesinglefeaturelayer.cpp
        ogr/ogrsf_frmts/sqlite/ogrsqlitetablelayer.cpp
        ogr/ogrsf_frmts/sqlite/ogrsqliteutility.cpp
        ogr/ogrsf_frmts/sqlite/ogrsqlitevfs.cpp
        ogr/ogrsf_frmts/sqlite/ogrsqliteviewlayer.cpp
        ogr/ogrsf_frmts/sqlite/ogrsqlitevirtualogr.cpp
        ogr/ogrsf_frmts/sqlite/rasterlite2.cpp
    )
    target_compile_definitions(ogr PRIVATE
        SQLITE_ENABLED
        SQLITE_HAS_COLUMN_METADATA
        HAVE_SPATIALITE
        SPATIALITE_412_OR_LATER
    )
    target_link_libraries(ogr PRIVATE libspatialite::spatialite)
endif ()

if (GDAL_OGR_FRMTS_XLSX)
    target_sources(ogr PRIVATE
        ogr/ogrsf_frmts/xlsx/ogrxlsxdatasource.cpp
        ogr/ogrsf_frmts/xlsx/ogrxlsxdriver.cpp
    )
    target_compile_definitions(ogr PRIVATE XLSX_ENABLED)
    if (NOT BUILD_SHARED_LIBS)
        target_compile_definitions(ogr PUBLIC XML_STATIC)
    endif ()
    target_link_libraries(ogr PRIVATE EXPAT::EXPAT)
endif ()

target_compile_definitions(ogr PRIVATE
    OGR
    MITAB_USE_OFTDATETIME
    KML_ENABLED
    MEM_ENABLED
    GEOJSON_ENABLED
    VRT_ENABLED
)

target_include_directories(ogr
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts>
PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/alg>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/apps>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/generic>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/mem>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/geojson>
)

target_link_libraries(ogr PRIVATE
    gcore
    PROJ::proj
    $<TARGET_NAME_IF_EXISTS:GEOS::geos>
    $<TARGET_NAME_IF_EXISTS:ZLIB::ZLIB>
)
set_target_properties(ogr PROPERTIES PUBLIC_HEADER "${OGR_PUBLIC_HEADERS}")

message(STATUS "Supported vector formats:")
message(STATUS "Csv driver support:               ${GDAL_OGR_FRMTS_CSV}")
message(STATUS "Esri shapefile driver support:    ${GDAL_OGR_FRMTS_SHAPE}")
message(STATUS "Geojson driver support:           ON")
message(STATUS "Geo package driver support:       ${GDAL_OGR_FRMTS_GEOPKG}")
message(STATUS "KML driver support:               ON")
message(STATUS "Memory driver support:            ON")
message(STATUS "Sqlite driver support:            ${GDAL_OGR_FRMTS_SQLITE}")
message(STATUS "Xlsx driver support:              ${GDAL_OGR_FRMTS_XLSX}")

############################################################################################
# alg
############################################################################################
add_library(alg OBJECT
    alg/gdaldither.cpp
    alg/gdalmediancut.cpp
    alg/gdal_crs.cpp
    alg/gdaltransformer.cpp
    alg/gdalsimplewarp.cpp
    alg/gdalwarper.cpp
    alg/gdalwarpkernel.cpp
    alg/gdalwarpkernel_opencl.cpp
    alg/gdalwarpoperation.cpp
    alg/gdalchecksum.cpp
    alg/gdal_rpc.cpp
    alg/gdal_tps.cpp
    alg/thinplatespline.cpp
    alg/llrasterize.cpp
    alg/gdalrasterize.cpp
    alg/gdalgeoloc.cpp
    alg/gdalgrid.cpp
    alg/gdalgridsse.cpp
    alg/gdalgridavx.cpp
    alg/gdalcutline.cpp
    alg/gdalproximity.cpp
    alg/rasterfill.cpp
    alg/gdalrasterpolygonenumerator.cpp
    alg/gdalsievefilter.cpp
    alg/polygonize.cpp
    alg/contour.cpp
    alg/gdaltransformgeolocs.cpp
    alg/gdallinearsystem.cpp
    alg/gdal_octave.cpp
    alg/gdal_simplesurf.cpp
    alg/gdalmatching.cpp
    alg/delaunay.c
    alg/gdalpansharpen.cpp
    alg/gdalapplyverticalshiftgrid.cpp
)

set (ALG_PUBLIC_HEADERS
    ${CMAKE_SOURCE_DIR}/alg/gdal_simplesurf.h
    ${CMAKE_SOURCE_DIR}/alg/gdal_alg.h
    ${CMAKE_SOURCE_DIR}/alg/gdal_alg_priv.h
    ${CMAKE_SOURCE_DIR}/alg/gdalgrid.h
    ${CMAKE_SOURCE_DIR}/alg/gdalgrid_priv.h
    ${CMAKE_SOURCE_DIR}/alg/gdalpansharpen.h
    ${CMAKE_SOURCE_DIR}/alg/gdalwarper.h
)
set_target_properties(alg PROPERTIES PUBLIC_HEADER "${ALG_PUBLIC_HEADERS}")

target_include_directories(alg
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/alg>
PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/alg/marching_squares>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/gcore>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/apps>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts/vrt>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts>
)

target_link_libraries(alg PRIVATE
    port
    PROJ::proj
    $<TARGET_NAME_IF_EXISTS:GEOS::geos_c>
    $<TARGET_NAME_IF_EXISTS:OpenCL::OpenCL>
)

############################################################################################
# apps
############################################################################################
add_library(gdal
    apps/commonutils.cpp
    apps/gdalinfo_lib.cpp
    apps/gdal_translate_lib.cpp
    apps/gdalwarp_lib.cpp
    apps/ogr2ogr_lib.cpp
    apps/gdaldem_lib.cpp
    apps/nearblack_lib.cpp
    apps/gdal_grid_lib.cpp
    apps/gdal_rasterize_lib.cpp
    apps/gdalbuildvrt_lib.cpp
    $<TARGET_OBJECTS:gcore>
    $<TARGET_OBJECTS:alg>
    $<TARGET_OBJECTS:frmts>
    $<TARGET_OBJECTS:ogr>
    $<TARGET_OBJECTS:port>
)

set(GDAL_PUBLIC_HEADERS
    ${CMAKE_BINARY_DIR}/gdal_version.h
    ${CMAKE_SOURCE_DIR}/apps/gdal_utils.h
)
set_target_properties(gdal PROPERTIES
    PUBLIC_HEADER "${GDAL_PUBLIC_HEADERS}"
    EXPORT_NAME "GDAL"
)

if (MINGW)
    target_compile_definitions(gdal PUBLIC SUPPORTS_WMAIN)
endif ()

target_include_directories(gdal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/gcore>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/frmts/vrt>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/geojson>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/ogr/ogrsf_frmts/generic>
)

target_link_libraries(gdal PUBLIC
    gcore
    ogr
    alg
    frmts
    $<TARGET_NAME_IF_EXISTS:TIFF::TIFF>
)

macro(add_gdal_app_bin NAME)
    add_executable(${NAME} apps/${NAME}_bin.cpp)
    target_link_libraries(${NAME} PRIVATE gdal)
    install(TARGETS ${NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endmacro()

macro(add_gdal_app NAME)
    add_executable(${NAME} apps/${NAME}.cpp)
    target_link_libraries(${NAME} PRIVATE gdal)
    install(TARGETS ${NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endmacro()

if (GDAL_BUILD_TOOLS)
    add_gdal_app_bin(gdalinfo BIN)
    add_gdal_app_bin(gdalwarp)
    add_gdal_app_bin(nearblack)
    add_gdal_app_bin(gdal_translate)
    add_gdal_app_bin(gdal_rasterize)
    add_gdal_app_bin(gdal_grid)
    add_gdal_app_bin(gdaldem)
    add_gdal_app_bin(gdalbuildvrt)
    add_gdal_app_bin(ogr2ogr)

    add_gdal_app(gdalserver)
    add_gdal_app(gdaladdo)
    add_gdal_app(gdalmanage)
    add_gdal_app(gdalenhance)
    add_gdal_app(gdaltransform)
    add_gdal_app(gdallocationinfo)

    add_gdal_app(gdal_contour)
    add_gdal_app(gdaltindex)
    add_gdal_app(ogrinfo)
    add_gdal_app(ogrtindex)
    add_gdal_app(ogrlineref)
    add_gdal_app(testepsg)
endif ()

message(STATUS "Opencl support:     ${GDAL_OPENCL_SUPPORT}")
message(STATUS "Png support:        ${GDAL_PNG_SUPPORT}")
message(STATUS "Jpeg support:       ${GDAL_JPEG_SUPPORT}")
message(STATUS "Zlib support:       ${GDAL_ZLIB_SUPPORT}")
message(STATUS "Geos support:       ${GDAL_GEOS_SUPPORT}")
message(STATUS "Iconv support:      ${GDAL_ICONV_SUPPORT}")
message(STATUS "Libxml2 support:    ${GDAL_LIBXML2_SUPPORT}")
message(STATUS "Curl support:       ${GDAL_CURL_SUPPORT}")

set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/gdal)
write_basic_package_version_file("GDALConfigVersion.cmake"
    VERSION ${gdal_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(${CMAKE_SOURCE_DIR}/GDALConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/GDALConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

install(TARGETS gdal gcore alg ogr frmts port EXPORT gdal-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT gdal-targets
    FILE GdalTargets.cmake
    NAMESPACE GDAL::
    DESTINATION ${INSTALL_CONFIGDIR}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/GDALConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/GDALConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

if (GDAL_INSTALL_DATA_FILES)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.TXT
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/gdal
    )
    
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/gdal
    )
endif ()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gcore/gdal_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/gdal_version.h @ONLY)
if (MSVC)
    configure_file(cpl_config.h.vc.cmake.in ${CMAKE_BINARY_DIR}/cpl_config.h)
else ()
    configure_file(cpl_config.h.cmake.in ${CMAKE_BINARY_DIR}/cpl_config.h)
endif ()