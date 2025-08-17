
add_definitions(-D__WINESRC__)
remove_definitions(-D_WIN32_WINNT=0x502)
add_definitions(-D_WIN32_WINNT=0x602)	
include_directories(${REACTOS_SOURCE_DIR}/sdk/include/host)
include_directories(${REACTOS_SOURCE_DIR}/sdk/include/reactos/wine)
include_directories(${REACTOS_SOURCE_DIR}/sdk/include/reactos/win32k)
include_directories(${REACTOS_SOURCE_DIR}/sdk/include/reactos/subsys/csr)
include_directories(${REACTOS_SOURCE_DIR}/win32ss/user/winsrv/usersrv)
include_directories(${REACTOS_SOURCE_DIR}/win32ss/include)
include_directories(${REACTOS_SOURCE_DIR}/sdk/include/reactos/libs/libpng)

spec2def(user32_wrapper.dll user32_wrapper.spec)

if(ARCH STREQUAL "amd64")
	set(baseaddress_user32_wrapper     0x67C30000)
elseif(ARCH STREQUAL "i386")
	set(baseaddress_user32_wrapper     0x67a20000)
endif()

list(APPEND SOURCE
	clipboard.c
	cursor.c
	desktop.c
	display.c
	dpi.c
	dwm.c
	exticon.c
	hooks.c
	icon.c
	input.c
	magnification
    main.c
	message.c
	misc.c
	notification.c
	process.c
	shutdown.c
	sysparams.c
	timer.c
	touch.c
	window.c
    version.rc
    ${CMAKE_CURRENT_BINARY_DIR}/user32_wrapper_stubs.c
    ${CMAKE_CURRENT_BINARY_DIR}/user32_wrapper.def)

add_library(user32_wrapper SHARED ${SOURCE})
set_module_type(user32_wrapper win32dll ENTRYPOINT DllMain)

target_link_libraries(user32_wrapper wine uuid)

add_delay_importlibs(user32_wrapper libpng)

add_importlibs(user32_wrapper userbase kernel32 gdi32 ntdll advapi32 winsrv)

add_cd_file(TARGET user32_wrapper DESTINATION reactos/system32 FOR all)
