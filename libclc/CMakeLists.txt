cmake_minimum_required( VERSION 3.9.2 )

add_custom_target(libspirv-builtins COMMENT "Build libspirv builtins")
add_custom_target(libclc-builtins COMMENT "Build libclc builtins")

# Add path for custom modules
set(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
  ${CMAKE_MODULE_PATH})

# If we are not building as a part of LLVM, build libclc as an
# standalone project, using LLVM/Clang as external tools.
if( CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR )
  project( libclc VERSION 0.2.0 LANGUAGES CXX C )

  set( LIBCLC_STANDALONE_BUILD 1 )

  include(HandleOutOfTreeLLVM)
else()
  include(HandleInLLVMTree)
endif()

include( GNUInstallDirs )
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  amdgcn-amdhsa/lib/SOURCES;
  amdgcn/lib/SOURCES;
  amdgcn-mesa3d/lib/SOURCES;
  amdgpu/lib/SOURCES;
  generic/lib/SOURCES;
  ptx/lib/SOURCES;
  ptx-nvidiacl/lib/SOURCES;
  r600/lib/SOURCES
)

include( AddLibclc )

# List of all targets
set( LIBCLC_TARGETS_ALL
  amdgcn--
  amdgcn--amdhsa
  r600--
  nvptx--
  nvptx64--
  nvptx--nvidiacl
  nvptx64--nvidiacl
)

set( LIBCLC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR} )

set( LIBCLC_TARGETS_TO_BUILD "all"
    CACHE STRING "Semicolon-separated list of targets to build, or 'all'." )

option( ENABLE_RUNTIME_SUBNORMAL "Enable runtime linking of subnormal support."
OFF )

# mesa3d environment is only available since LLVM 4.0
if( ${LLVM_VERSION} VERSION_GREATER "3.9.0" )
	set( LIBCLC_TARGETS_ALL ${LIBCLC_TARGETS_ALL} amdgcn-mesa-mesa3d )
endif()

if( LIBCLC_TARGETS_TO_BUILD STREQUAL "all" )
	set( LIBCLC_TARGETS_TO_BUILD ${LIBCLC_TARGETS_ALL} )
endif()

list( SORT LIBCLC_TARGETS_TO_BUILD )

# These were not properly reported in early LLVM and we don't need them
set( LLVM_CXX_FLAGS ${LLVM_CXX_FLAGS} -fno-rtti -fno-exceptions )

# Print LLVM variables
message( "LLVM libdir: ${LLVM_LIBRARY_DIR}" )
message( "LLVM bindir: ${LLVM_TOOLS_BINARY_DIR}" )
message( "LLVM cxx flags: ${LLVM_CXX_FLAGS}" )
message( "" )

# Print toolchain
message( "clang: ${LLVM_CLANG}" )
message( "llvm-as: ${LLVM_AS}" )
message( "llvm-link: ${LLVM_LINK}" )
message( "opt: ${LLVM_OPT}" )
message( "" )
if( NOT LLVM_CLANG OR NOT LLVM_OPT OR NOT LLVM_AS OR NOT LLVM_LINK )
	message( FATAL_ERROR "toolchain incomplete!" )
endif()

set( CMAKE_CLC_COMPILER ${LLVM_CLANG} )
set( CMAKE_CLC_ARCHIVE ${LLVM_LINK} )
set( CMAKE_LLAsm_PREPROCESSOR ${LLVM_CLANG} )
set( CMAKE_LLAsm_COMPILER ${LLVM_AS} )
set( CMAKE_LLAsm_ARCHIVE ${LLVM_LINK} )
enable_language( CLC LLAsm )

# Configure prepare_builtins
add_subdirectory(utils)

# Setup arch devices
set( r600--_devices cedar cypress barts cayman )
set( amdgcn--_devices tahiti )
set( amdgcn-mesa-mesa3d_devices ${amdgcn--_devices} )
set( amdgcn--amdhsa_devices none )
set( nvptx--_devices none )
set( nvptx64--_devices none )
set( nvptx--nvidiacl_devices none )
set( nvptx64--nvidiacl_devices none )

# Setup aliases
set( cedar_aliases palm sumo sumo2 redwood juniper )
set( cypress_aliases hemlock )
set( barts_aliases turks caicos )
set( cayman_aliases aruba )
set( tahiti_aliases pitcairn verde oland hainan bonaire kabini kaveri hawaii
	mullins tonga iceland carrizo fiji stoney polaris10 polaris11 )

# Support for gfx9 was added in LLVM 5.0 (r295554)
if( ${LLVM_VERSION} VERSION_GREATER "4.99.99" )
	set( tahiti_aliases ${tahiti_aliases} gfx900 gfx902 )
endif()

# Support for Vega12 and Vega20 was added in LLVM 7 (r331215)
if( ${LLVM_VERSION} VERSION_GREATER "6.99.99" )
	set( tahiti_aliases ${tahiti_aliases} gfx904 gfx906 )
endif()

# pkg-config file
configure_file( libclc.pc.in libclc.pc @ONLY )
install( FILES ${CMAKE_CURRENT_BINARY_DIR}/libclc.pc DESTINATION ${CMAKE_INSTALL_DATADIR}/pkgconfig )
install( DIRECTORY generic/include/clc DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} )

if( ENABLE_RUNTIME_SUBNORMAL )
	add_library( subnormal_use_default STATIC
		generic/lib/subnormal_use_default.ll )
	add_library( subnormal_disable STATIC
		generic/lib/subnormal_disable.ll )
	install( TARGETS subnormal_use_default subnormal_disable ARCHIVE
		DESTINATION ${CMAKE_INSTALL_DATADIR}/clc )
endif()

find_program( PYTHON python )
file( TO_CMAKE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/generic/lib/gen_convert.py clc_script_loc )
file( TO_CMAKE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/generic/libspirv/gen_convert.py spirv_script_loc )

add_custom_command(
	OUTPUT convert-spirv.cl
	COMMAND ${PYTHON} ${spirv_script_loc} > convert-spirv.cl
	DEPENDS ${spirv_script_loc} )
add_custom_target( "generate_convert_spirv.cl" DEPENDS convert-spirv.cl )

add_custom_command(
	OUTPUT convert-clc.cl
	COMMAND ${PYTHON} ${clc_script_loc} > convert-clc.cl
	DEPENDS ${clc_script_loc} )
add_custom_target( "generate_convert_clc.cl" DEPENDS convert-clc.cl )

enable_testing()

if (LIBCLC_STANDALONE_BUILD)
  set(LIBCLC_LIBRARY_OUTPUT_INTDIR ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib${LLVM_LIBDIR_SUFFIX})
else(LIBCLC_STANDALONE_BUILD)
  set(LIBCLC_LIBRARY_OUTPUT_INTDIR ${LLVM_LIBRARY_OUTPUT_INTDIR})
endif(LIBCLC_STANDALONE_BUILD)
file( TO_CMAKE_PATH ${LIBCLC_LIBRARY_OUTPUT_INTDIR}/clc LIBCLC_LIBRARY_OUTPUT_INTDIR )

foreach( t ${LIBCLC_TARGETS_TO_BUILD} )
	message( "BUILDING ${t}" )
	string( REPLACE "-" ";" TRIPLE  ${t} )
	list( GET TRIPLE 0 ARCH )
	list( GET TRIPLE 1 VENDOR )
	list( GET TRIPLE 2 OS )

	set( dirs generic )
	if( ${ARCH} STREQUAL r600 OR ${ARCH} STREQUAL amdgcn )
		set( dirs ${dirs} amdgpu )
	endif()

	#nvptx is special
	if( ${ARCH} STREQUAL nvptx OR ${ARCH} STREQUAL nvptx64 )
		set( DARCH ptx )
	else()
		set( DARCH ${ARCH} )
	endif()

	set( lib_files )
	libclc_configure_lib_source(lib_files
		LIB_DIR lib
		DIRS ${dirs} ${DARCH} ${DARCH}-${OS} ${DARCH}-${VENDOR}-${OS}
		DEPS convert-clc.cl )
	set( libspirv_files )
	libclc_configure_lib_source(libspirv_files
		LIB_DIR libspirv
		DIRS ${dirs} ${DARCH} ${DARCH}-${OS} ${DARCH}-${VENDOR}-${OS}
		DEPS convert-spirv.cl )

	foreach( d ${${t}_devices} )
		# Some targets don't have a specific GPU to target
		if( ${d} STREQUAL "none" )
			set( mcpu )
			set( arch_suffix "${t}" )
		else()
			set( mcpu "-mcpu=${d}" )
			set( arch_suffix "${d}-${t}" )
		endif()
		message( "    DEVICE: ${d} ( ${${d}_aliases} )" )

		add_libclc_builtin_set(libspirv-${arch_suffix}
			TRIPLE ${t}
			TARGET_ENV libspirv
			COMPILE_OPT ${mcpu}
			FILES ${libspirv_files}
			ALIASES ${${d}_aliases}
			GENERATE_TARGET "generate_convert_clc.cl"
			PARENT_TARGET libspirv-builtins)

		add_libclc_builtin_set(clc-${arch_suffix}
			TRIPLE ${t}
			TARGET_ENV clc
			COMPILE_OPT ${mcpu}
			FILES ${lib_files}
			LIB_DEP libspirv-${arch_suffix}
			ALIASES ${${d}_aliases}
			GENERATE_TARGET "generate_convert_spirv.cl"
			PARENT_TARGET libclc-builtins)
	endforeach( d )
endforeach( t )

install(DIRECTORY ${LIBCLC_LIBRARY_OUTPUT_INTDIR}
				DESTINATION lib${LLVM_LIBDIR_SUFFIX}
				COMPONENT libspirv-builtins
				FILES_MATCHING PATTERN "libspirv-*")

install(DIRECTORY ${LIBCLC_LIBRARY_OUTPUT_INTDIR}
				DESTINATION lib${LLVM_LIBDIR_SUFFIX}
				COMPONENT clc-builtins
				FILES_MATCHING PATTERN "clc-*")
