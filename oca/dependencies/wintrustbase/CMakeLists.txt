
add_definitions(-D__WINESRC__)
include_directories(BEFORE ${REACTOS_SOURCE_DIR}/sdk/include/reactos/wine)
spec2def(wintrustbase.dll wintrustbase.spec ADD_IMPORTLIB)

list(APPEND SOURCE
    asn.c
    crypt.c
    register.c
    softpub.c
    wintrust_main.c
    precomp.h
    ${CMAKE_CURRENT_BINARY_DIR}/wintrustbase_stubs.c)

add_library(wintrustbase MODULE
    ${SOURCE}
    version.rc
    ${CMAKE_CURRENT_BINARY_DIR}/wintrustbase.def)

set_module_type(wintrustbase win32dll)
target_link_libraries(wintrustbase wine ${PSEH_LIB})
# FIXME: imagehlp should be delay-imported. See CORE-6504
#add_delay_importlibs(wintrustbase cryptui imagehlp)
add_delay_importlibs(wintrustbase cryptui)
add_importlibs(wintrustbase imagehlp crypt32 user32 advapi32 msvcrt kernel32 ntdll)
add_pch(wintrustbase precomp.h SOURCE)
add_cd_file(TARGET wintrustbase DESTINATION reactos/system32 FOR all)
