# ==============================================================================
#
#  This file is part of the JUCE library.
#  Copyright (c) 2022 - Raw Material Software Limited
#
#  JUCE is an open source library subject to commercial or open-source
#  licensing.
#
#  By using JUCE, you agree to the terms of both the JUCE 7 End-User License
#  Agreement and JUCE Privacy Policy.
#
#  End User License Agreement: www.juce.com/juce-7-licence
#  Privacy Policy: www.juce.com/juce-privacy-policy
#
#  Or: You may also use this code under the terms of the GPL v3 (see
#  www.gnu.org/licenses).
#
#  JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
#  EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
#  DISCLAIMED.
#
# ==============================================================================

# The juceaide program generates intermediate build files including BinaryData sources, icons, and
# plists. To ensure that we always build it for the host system, and not for, say, a device or
# simulator if we're targeting iOS or Android, we reinvoke cmake here and build juceaide during the
# configuration stage of the outer project.

if(JUCE_BUILD_HELPER_TOOLS)
    # Build the tool for the current system
    juce_add_console_app(juceaide)
    target_compile_features(juceaide PRIVATE cxx_std_17)

    target_sources(juceaide PRIVATE Main.cpp)

    target_compile_definitions(juceaide PRIVATE
        JUCE_DISABLE_JUCE_VERSION_PRINTING=1
        JUCE_USE_CURL=0)

    target_link_libraries(juceaide PRIVATE
        juce::juce_build_tools
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

    set_target_properties(juceaide PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    export(TARGETS juceaide
           NAMESPACE juce_tools::
           FILE "${JUCE_BINARY_DIR}/JUCEToolsExport.cmake")
else()
    # If we're building using the NDK, the gradle wrapper will try to inject its own compiler using
    # environment variables, which is unfortunate because we really don't want to cross-compile
    # juceaide.
    # Similarly, when cross-compiling from Linux->Windows (e.g. using
    # Fedora's mingw64-cmake command), the environment might be configured
    # for cross-compiling, and we'll need to attempt to put it back to the
    # host settings in order to build an executable that can run on the host
    # machine.
    if(CMAKE_CROSSCOMPILING)
        unset(ENV{ADDR2LINE})
        unset(ENV{AR})
        unset(ENV{ASM})
        unset(ENV{AS})
        unset(ENV{CC})
        unset(ENV{CPP})
        unset(ENV{CXXFILT})
        unset(ENV{CXX})
        unset(ENV{DLLTOOL})
        unset(ENV{DLLWRAP})
        unset(ENV{ELFEDIT})
        unset(ENV{GCC})
        unset(ENV{GCOV_DUMP})
        unset(ENV{GCOV_TOOL})
        unset(ENV{GCOV})
        unset(ENV{GPROF})
        unset(ENV{GXX})
        unset(ENV{LDFLAGS})
        unset(ENV{LD_BFD})
        unset(ENV{LD})
        unset(ENV{LTO_DUMP})
        unset(ENV{NM})
        unset(ENV{OBJCOPY})
        unset(ENV{OBJDUMP})
        unset(ENV{PKG_CONFIG_LIBDIR})
        unset(ENV{PKG_CONFIG})
        unset(ENV{RANLIB})
        unset(ENV{RC})
        unset(ENV{READELF})
        unset(ENV{SIZE})
        unset(ENV{STRINGS})
        unset(ENV{STRIP})
        unset(ENV{WIDL})
        unset(ENV{WINDMC})
        unset(ENV{WINDRES})

        if(DEFINED ENV{PATH_ORIG})
            set(ENV{PATH} "$ENV{PATH_ORIG}")
        endif()
    else()
        # When building with clang-cl in Clion on Windows for an x64 target, the ABI detection phase
        # of the inner build can fail unless we pass through these flags too
        set(extra_configure_flags
            "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS} ${temp_language_version}"
            "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
            "-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
            "-DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}")
    endif()

    message(STATUS "Configuring juceaide")

    # Looks like we're bootstrapping, reinvoke CMake
    execute_process(COMMAND "${CMAKE_COMMAND}"
            "."
            "-B${JUCE_BINARY_DIR}/tools"
            "-G${CMAKE_GENERATOR}"
            "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCMAKE_BUILD_TYPE=Debug"
            "-DJUCE_BUILD_HELPER_TOOLS=ON"
            "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
            ${extra_configure_flags}
        WORKING_DIRECTORY "${JUCE_SOURCE_DIR}"
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_output
        RESULT_VARIABLE result_variable)

    if(result_variable)
        message(FATAL_ERROR "Failed to configure juceaide\n${command_output}")
    endif()

    message(STATUS "Building juceaide")

    execute_process(COMMAND "${CMAKE_COMMAND}"
            --build "${JUCE_BINARY_DIR}/tools"
            --config Debug
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_output
        RESULT_VARIABLE result_variable)

    if(result_variable)
        message(FATAL_ERROR "Failed to build juceaide\n${command_output}")
    endif()

    message(STATUS "Exporting juceaide")

    # This will be generated by the recursive invocation of CMake (above)
    include("${JUCE_BINARY_DIR}/tools/JUCEToolsExport.cmake")

    add_executable(juceaide IMPORTED GLOBAL)
    get_target_property(imported_location juce_tools::juceaide IMPORTED_LOCATION_DEBUG)
    set_target_properties(juceaide PROPERTIES IMPORTED_LOCATION "${imported_location}")

    add_executable(juce::juceaide ALIAS juceaide)

    set(JUCE_TOOL_INSTALL_DIR "bin/JUCE-${JUCE_VERSION}" CACHE STRING
        "The location, relative to the install prefix, where juceaide will be installed")

    install(PROGRAMS "${imported_location}" DESTINATION "${JUCE_TOOL_INSTALL_DIR}")

    get_filename_component(binary_name "${imported_location}" NAME)
    set(JUCE_JUCEAIDE_NAME "${binary_name}" CACHE INTERNAL "The name of the juceaide program")
endif()
