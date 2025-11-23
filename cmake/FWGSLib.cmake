# FWGSLib.cmake - Compatibility module for mainui_cpp
# This is a simplified version for building with MinGW-w64

# Function to fix MSVC settings (no-op for MinGW)
function(fwgs_fix_default_msvc_settings)
    # Nothing to do for MinGW
endfunction()

# Function to set default properties for a target
function(fwgs_set_default_properties target)
    # Set some basic properties if needed
    if(WIN32)
        set_target_properties(${target} PROPERTIES
            PREFIX ""
        )
    endif()
endfunction()

# Function to install target (simplified version)
function(fwgs_install target)
    # Install the target to the appropriate location
    # Check if GAME_DIR is defined, otherwise use default
    if(DEFINED GAME_DIR)
        install(TARGETS ${target}
            LIBRARY DESTINATION ${GAME_DIR}
            RUNTIME DESTINATION ${GAME_DIR}
            ARCHIVE DESTINATION ${GAME_DIR}
        )
    else()
        # Default installation if GAME_DIR is not set
        install(TARGETS ${target}
            LIBRARY DESTINATION lib
            RUNTIME DESTINATION bin
            ARCHIVE DESTINATION lib
        )
    endif()
endfunction()

