# Optional CMake hooks for mod-twow-collections.
#
# Most modules do not need custom CMake here. Source files under src/ are discovered automatically,
# conf/*.conf.dist files are copied automatically, and Addmod_twow_collectionsScripts() is invoked by the
# generated module loader.
#
# This file is included twice:
#   TORTOISE_MODULE_CMAKE_PHASE=DISCOVERY
#   TORTOISE_MODULE_CMAKE_PHASE=POST_TARGETS
#
# Use TW_* helpers for compatibility with this core's module system. Do not use AzerothCore AC_* names.

if(TORTOISE_MODULE_CMAKE_PHASE STREQUAL "DISCOVERY")
  # Source files under src/ are discovered automatically by ConfigureModules.cmake
endif()

# NOTE: Compilation is handled exclusively by the operator/user.
# Any optional test targets or standalone builds should be OFF by default.
