
add_definitions(-D__WINESRC__)
include_directories(${REACTOS_SOURCE_DIR}/include/reactos/wine)
include_directories(${REACTOS_SOURCE_DIR}/sdk/include/reactos/libs/libpng)
spec2def(IconCodecService.dll IconCodecService.spec)

set(baseaddress_IconCodecService     0x64370000)

list(APPEND SOURCE
    icon.c
    version.rc
    ${CMAKE_CURRENT_BINARY_DIR}/IconCodecService_stubs.c
    ${CMAKE_CURRENT_BINARY_DIR}/IconCodecService.def)

add_library(IconCodecService SHARED ${SOURCE})
set_module_type(IconCodecService win32dll)
target_link_libraries(IconCodecService wine)
add_delay_importlibs(IconCodecService libpng user32)
add_importlibs(IconCodecService gdi32 msvcrt kernel32 ntdll)
add_cd_file(TARGET IconCodecService DESTINATION reactos/system32 FOR all)
