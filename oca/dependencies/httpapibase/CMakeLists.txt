
spec2def(httpapibase.dll httpapibase.spec ADD_IMPORTLIB)

list(APPEND SOURCE
    ${CMAKE_CURRENT_BINARY_DIR}/httpapibase_stubs.c
    ${CMAKE_CURRENT_BINARY_DIR}/httpapibase.def)

add_library(httpapibase MODULE ${SOURCE})
set_module_type(httpapibase win32dll)
add_importlibs(httpapibase msvcrt kernel32 ntdll)
add_cd_file(TARGET httpapibase DESTINATION reactos/system32 FOR all)
