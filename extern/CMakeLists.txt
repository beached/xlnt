include(FetchContent)

# libstudxml has no CMake build of its own; this only populates the sources.
# The target is defined in third-party/libstudxml.build/CMakeLists.txt
FetchContent_Declare(libstudxml
  GIT_REPOSITORY https://git.codesynthesis.com/libstudxml/libstudxml.git
  GIT_TAG e729667b0f34a39ce4a61339843af4154648667b
  SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../third-party/libstudxml)
FetchContent_MakeAvailable(libstudxml)

# UTF-8 conversion (daw::utf8). EXCLUDE_FROM_ALL keeps its install rules out of xlnt's install
FetchContent_Declare(daw_utf_range
  GIT_REPOSITORY https://github.com/beached/utf_range.git
  GIT_TAG b0b9cd05ea00f73d51cda5e3a2fd8fb004f5881d # v2.2.6
  EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(daw_utf_range)
