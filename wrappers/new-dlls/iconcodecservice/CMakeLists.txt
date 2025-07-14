
add_definitions(-D__WINESRC__)
include_directories(${REACTOS_SOURCE_DIR}/include/reactos/wine)
spec2def(IconCodecService.dll IconCodecService.spec)

set(baseaddress_IconCodecService     0x64370000)

list(APPEND SOURCE
    IconCodecService.c
    version.rc
    ${CMAKE_CURRENT_BINARY_DIR}/IconCodecService_stubs.c
    ${CMAKE_CURRENT_BINARY_DIR}/IconCodecService.def)

add_library(IconCodecService SHARED ${SOURCE})
set_module_type(IconCodecService win32dll)
target_link_libraries(IconCodecService wine)
add_importlibs(IconCodecService advapi32 msvcrt kernel32 ntdll)
add_cd_file(TARGET IconCodecService DESTINATION reactos/system32 FOR all)
