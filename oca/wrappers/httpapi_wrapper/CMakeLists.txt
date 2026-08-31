
spec2def(httpapi_wrapper.dll httpapi_wrapper.spec)

set(baseaddress_httpapi_wrapper     0x760D0000)

list(APPEND SOURCE
    forwards.c
    main.c
    ${CMAKE_CURRENT_BINARY_DIR}/httpapi_wrapper_stubs.c
    ${CMAKE_CURRENT_BINARY_DIR}/httpapi_wrapper.def)

add_library(httpapi_wrapper MODULE ${SOURCE})
set_module_type(httpapi_wrapper win32dll ENTRYPOINT 0)
add_importlibs(httpapi_wrapper httpapibase kernel32 ntdll)
add_cd_file(TARGET httpapi_wrapper DESTINATION reactos/system32 FOR all)
