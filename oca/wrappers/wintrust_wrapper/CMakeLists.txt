
add_definitions(-D__WINESRC__)
include_directories(BEFORE ${REACTOS_SOURCE_DIR}/sdk/include/reactos/wine)
spec2def(wintrust_wrapper.dll wintrust_wrapper.spec ADD_IMPORTLIB)

list(APPEND SOURCE
    forwards.c
    main.c
    precomp.h
    ${CMAKE_CURRENT_BINARY_DIR}/wintrust_wrapper_stubs.c)

add_library(wintrust_wrapper MODULE
    ${SOURCE}
    version.rc
    ${CMAKE_CURRENT_BINARY_DIR}/wintrust_wrapper.def)

set_module_type(wintrust_wrapper win32dll ENTRYPOINT 0)
target_link_libraries(wintrust_wrapper wine ${PSEH_LIB})
add_importlibs(wintrust_wrapper wintrustbase kernel32 ntdll)
add_pch(wintrust_wrapper precomp.h SOURCE)
add_cd_file(TARGET wintrust_wrapper DESTINATION reactos/system32 FOR all)
