
add_definitions(-D__WINESRC__)
include_directories(BEFORE ${REACTOS_SOURCE_DIR}/sdk/include/reactos/wine)
spec2def(versionbase.dll versionbase.spec ADD_IMPORTLIB)

list(APPEND SOURCE
    versionbase.c
    ${CMAKE_CURRENT_BINARY_DIR}/versionbase.def)

add_library(versionbase MODULE ${SOURCE} versionbase.rc)
set_module_type(versionbase win32dll)
target_link_libraries(versionbase wine)
add_importlibs(versionbase msvcrt kernel32 ntdll)
add_cd_file(TARGET versionbase DESTINATION reactos/system32 FOR all)
