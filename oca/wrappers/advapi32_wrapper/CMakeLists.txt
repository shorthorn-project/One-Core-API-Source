
add_definitions(-D_ADVAPI32_)

remove_definitions(-D_WIN32_WINNT=0x502)
add_definitions(-D_WIN32_WINNT=0x600)
include_directories(${REACTOS_SOURCE_DIR}/sdk/include/reactos/wine)
include_directories(${REACTOS_SOURCE_DIR}/sdk/lib/rtl)
include_directories(${REACTOS_SOURCE_DIR}/dll/wrappers/ntdllnew)
include_directories(${REACTOS_SOURCE_DIR}/dll/win32/kernel32/include)
include_directories(${REACTOS_SOURCE_DIR}/sdk/lib/evtlib)

spec2def(advapi32_wrapper.dll advapi32_wrapper.spec ADD_IMPORTLIB)

set(baseaddress_advapi32_wrapper     0x6c520000)

list(APPEND SOURCE
    cred.c
    efs.c
    eventlog.c
    forwards.c
	hooks.c
	main.c
	perf.c
	procedures.c
	process.c
	reg.c
	scm.c
	security.c
	sha1.c
	shutdown.c
	unimplemented.c
	workarounds.c
    version.rc
    ${CMAKE_CURRENT_BINARY_DIR}/advapi32_wrapper_stubs.c
    ${CMAKE_CURRENT_BINARY_DIR}/advapi32_wrapper.def)

add_library(advapi32_wrapper SHARED ${SOURCE})
set_module_type(advapi32_wrapper win32dll ENTRYPOINT DllMain)

target_link_libraries(advapi32_wrapper wine)

add_importlibs(advapi32_wrapper advapibase ntext kernel32 wevtapibase)

add_cd_file(TARGET advapi32_wrapper DESTINATION reactos/system32 FOR all)
