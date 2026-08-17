
add_definitions(-D__WINESRC__)
include_directories(BEFORE ${REACTOS_SOURCE_DIR}/sdk/include/reactos/wine)
spec2def(version_wrapper.dll version_wrapper.spec ADD_IMPORTLIB)

list(APPEND SOURCE
    main.c
    ${CMAKE_CURRENT_BINARY_DIR}/version_wrapper.def)

add_library(version_wrapper MODULE ${SOURCE} version.rc)
set_module_type(version_wrapper win32dll)
target_link_libraries(version_wrapper wine)
add_importlibs(version_wrapper versionbase  msvcrt kernel32 ntdll)
add_cd_file(TARGET version_wrapper DESTINATION reactos/system32 FOR all)
