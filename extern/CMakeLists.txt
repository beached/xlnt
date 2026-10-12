include(FetchContent)

# libstudxml has no CMake build of its own; this only populates the sources.
# The target is defined in third-party/libstudxml.build/CMakeLists.txt
FetchContent_Declare(libstudxml
  GIT_REPOSITORY https://git.codesynthesis.com/libstudxml/libstudxml.git
  GIT_TAG e729667b0f34a39ce4a61339843af4154648667b
  SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../third-party/libstudxml)
FetchContent_MakeAvailable(libstudxml)
