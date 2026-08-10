
spec2def(powrprof_wrapper.dll powrprof_wrapper.spec)

list(APPEND SOURCE
	forwards.c
	main.c
    version.rc
    ${CMAKE_CURRENT_BINARY_DIR}/powrprof_wrapper_stubs.c
    ${CMAKE_CURRENT_BINARY_DIR}/powrprof_wrapper.def)

add_library(powrprof_wrapper SHARED ${SOURCE})

set(baseaddress_powrprof_wrapper     0x60020000)

set_module_type(powrprof_wrapper win32dll ENTRYPOINT DllMain)
add_importlibs(powrprof_wrapper advapi32 powrprofbase user32 comctl32 msvcrt kernel32 ntdll)
add_dependencies(powrprof_wrapper psdk)
add_cd_file(TARGET powrprof_wrapper DESTINATION reactos/system32 FOR all)
