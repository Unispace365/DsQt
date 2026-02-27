# ----------------------------------------------------------------------
# Qt discovery helper (Qt6)
#
# Supports:
#   - QT_BASE cache var (base folder with 6.x.y subdirs OR a direct kit prefix)
#   - env QT6_DIR / QT_DIR pointing either to:
#       * base folder (e.g. C:/Qt)
#       * version folder (e.g. C:/Qt/6.6.2)
#       * kit prefix (e.g. C:/Qt/6.6.2/msvc2022_64)
#
# Cache vars:
#   - QT_KIT  (kit folder name, e.g. msvc2022_64 / mingw_64)
#   - QT_BASE (optional override)
# ----------------------------------------------------------------------

set(QT_KIT "msvc2022_64" CACHE STRING "Qt kit folder name (e.g. msvc2022_64, mingw_64, clang_64)")

# Resolve a starting path (cache QT_BASE wins; else env QT6_DIR; else env QT_DIR)
if(NOT DEFINED QT_BASE OR QT_BASE STREQUAL "")
  if(DEFINED ENV{QT6_DIR} AND NOT "$ENV{QT6_DIR}" STREQUAL "")
    set(_qt_start "$ENV{QT6_DIR}")
  elseif(DEFINED ENV{QT_DIR} AND NOT "$ENV{QT_DIR}" STREQUAL "")
    set(_qt_start "$ENV{QT_DIR}")
  else()
    set(_qt_start "")
  endif()
  set(QT_BASE "${_qt_start}" CACHE PATH "Qt base/version/prefix folder (see comments)" FORCE)
endif()

if(QT_BASE STREQUAL "")
  message(FATAL_ERROR
    "No Qt path provided.\n"
    "Set QT_BASE, or set env QT6_DIR / QT_DIR.\n"
    "Examples:\n"
    "  set QT6_DIR=C:/Qt\n"
    "  set QT6_DIR=C:/Qt/6.6.2\n"
    "  set QT6_DIR=C:/Qt/6.6.2/msvc2022_64\n"
    "Or configure with -DQT_BASE=...")
endif()

# Helper: append prefix if it contains Qt6Config.cmake
function(_qt_try_add_prefix out_list prefix)
  if(prefix AND EXISTS "${prefix}/lib/cmake/Qt6/Qt6Config.cmake")
    list(APPEND ${out_list} "${prefix}")
    set(${out_list} "${${out_list}}" PARENT_SCOPE)
  endif()
endfunction()

set(_qt_prefixes "")

# 1) If QT_BASE already IS a kit prefix, accept it.
_qt_try_add_prefix(_qt_prefixes "${QT_BASE}")

# 2) If QT_BASE looks like a version dir, try QT_BASE/QT_KIT.
_qt_try_add_prefix(_qt_prefixes "${QT_BASE}/${QT_KIT}")

# 3) Otherwise treat QT_BASE as a base dir containing 6.x.y subdirs.
if(_qt_prefixes STREQUAL "")
  file(GLOB _qt_version_dirs LIST_DIRECTORIES true "${QT_BASE}/6.*.*")
  list(SORT _qt_version_dirs COMPARE NATURAL ORDER DESCENDING)

  foreach(_verdir IN LISTS _qt_version_dirs)
    _qt_try_add_prefix(_qt_prefixes "${_verdir}/${QT_KIT}")
  endforeach()
endif()

# De-dup while preserving order (first = most preferred)
if(NOT _qt_prefixes STREQUAL "")
  list(REMOVE_DUPLICATES _qt_prefixes)
  list(PREPEND CMAKE_PREFIX_PATH ${_qt_prefixes})

  # Optional: make it easy to see what got picked
  message(STATUS "Qt6 prefixes added to CMAKE_PREFIX_PATH:")
  foreach(p IN LISTS _qt_prefixes)
    message(STATUS "  ${p}")
  endforeach()
else()
  message(FATAL_ERROR
    "No usable Qt6 found from QT_BASE='${QT_BASE}' with QT_KIT='${QT_KIT}'.\n"
    "Expected Qt6Config.cmake at one of:\n"
    "  ${QT_BASE}/lib/cmake/Qt6/Qt6Config.cmake\n"
    "  ${QT_BASE}/${QT_KIT}/lib/cmake/Qt6/Qt6Config.cmake\n"
    "  ${QT_BASE}/6.*.*/${QT_KIT}/lib/cmake/Qt6/Qt6Config.cmake")
endif()