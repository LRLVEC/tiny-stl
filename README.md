Usage:
1. add ```set(TINY_STL_BUILD_TESTS OFF CACHE BOOL " " FORCE)``` and ```mark_as_advanced(TINY_STL_BUILD_TESTS)``` to your CMakeLists.txt to disable building tests.
2. add ```include_directories(".../include")``` or use target_include_directories to your CMakeLists.txt, "..." is the relative position to your project.