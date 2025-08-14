/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    icon.c

Abstract:

        This file implements the NT icon png convert routines.

Author:

    Skulltrail 30-July-2025

Revision History:

--*/

#include <png.h>
#include <stdlib.h>
#include <windows.h>
#include <wingdi.h>
#include <ldrfuncs.h>

#include "wine/exception.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(user32);

/* We only use Wide string functions */
#undef MAKEINTRESOURCE
#define MAKEINTRESOURCE MAKEINTRESOURCEW

static const WCHAR DISPLAYW[] = L"DISPLAY";

#define RIFF_FOURCC( c0, c1, c2, c3 ) \
        ( (DWORD)(BYTE)(c0) | ( (DWORD)(BYTE)(c1) << 8 ) | \
        ( (DWORD)(BYTE)(c2) << 16 ) | ( (DWORD)(BYTE)(c3) << 24 ) )
#define PNG_SIGN RIFF_FOURCC(0x89,'P','N','G')

#define NB_USER_HANDLES  ((LAST_USER_HANDLE - FIRST_USER_HANDLE + 1) >> 1)
#define USER_HANDLE_TO_INDEX(hwnd) ((LOWORD(hwnd) - FIRST_USER_HANDLE) >> 1)

#define PNG_CHECK_SIG_SIZE 8 /* Check signature size */

typedef struct {
    png_bytep buffer;
    png_size_t size;
} PNG_READER;

static void PNGAPI ReadPngData(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead)
{
    PNG_READER *reader = (PNG_READER *)png_get_io_ptr(png_ptr);
    if (!reader || reader->size < byteCountToRead) {
        png_error(png_ptr, "Read Error");
        return;
    }

    memcpy(outBytes, reader->buffer, byteCountToRead);
    reader->buffer += byteCountToRead;
    reader->size -= byteCountToRead;
}

typedef LPBITMAPINFOHEADER(*PCONVERT_TO_DIB_PROC)(LPBITMAPINFOHEADER lpPngData, DWORD dwSize);

BOOL WINAPI PrivateRegisterICSProc(PCONVERT_TO_DIB_PROC AddrOfFn);

LPBITMAPINFOHEADER __stdcall ConvertToDIBProc(LPBITMAPINFOHEADER lpPngData, DWORD dwSize)
{
    png_structp png_ptr;
    png_infop info_ptr;
    PNG_READER reader;
    png_bytep *row_pointers;
    png_bytep png_buffer;
    png_size_t png_size;

    INT width;
    INT height;
    INT i;
    WORD bitCount;

    DWORD imageSize;
    DWORD headerSize;
    HGLOBAL hMem;
    LPBYTE lpOutputBits;
    LPBITMAPINFOHEADER lpOutputHeader;

    if (!lpPngData || dwSize < 8) return NULL;

    png_buffer = (png_bytep)lpPngData;
    png_size = (png_size_t)dwSize;

    if (png_sig_cmp(png_buffer, 0, 8) != 0) return NULL;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return NULL;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    reader.buffer = png_buffer;
    reader.size = png_size;

    png_set_read_fn(png_ptr, (png_voidp)&reader, ReadPngData);
    png_read_info(png_ptr, info_ptr);

    width = (INT)png_get_image_width(png_ptr, info_ptr);
    height = (INT)png_get_image_height(png_ptr, info_ptr);
    bitCount = 32; /* Sempre 32 bits */

    if (png_get_bit_depth(png_ptr, info_ptr) == 16) {
        png_set_strip_16(png_ptr);
    }

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY &&
        png_get_bit_depth(png_ptr, info_ptr) < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB ||
        png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY) {
        png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
    }

    png_set_bgr(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    row_pointers = (png_bytep *)HeapAlloc(GetProcessHeap(), 0, sizeof(png_bytep) * height);
    if (!row_pointers) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    imageSize = width * height * 4;
    headerSize = sizeof(BITMAPINFOHEADER);
    hMem = GlobalAlloc(GMEM_MOVEABLE, headerSize + imageSize);
    if (!hMem) {
        HeapFree(GetProcessHeap(), 0, row_pointers);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    lpOutputHeader = (LPBITMAPINFOHEADER)GlobalLock(hMem);
    if (!lpOutputHeader) {
        HeapFree(GetProcessHeap(), 0, row_pointers);
        GlobalFree(hMem);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    lpOutputBits = (LPBYTE)(lpOutputHeader + 1);

    for (i = 0; i < height; i++) {
        row_pointers[height - 1 - i] = lpOutputBits + (i * width * 4);
    }

    png_read_image(png_ptr, row_pointers);

    lpOutputHeader->biSize = sizeof(BITMAPINFOHEADER);
    lpOutputHeader->biWidth = width;
    lpOutputHeader->biHeight = height;
    lpOutputHeader->biPlanes = 1;
    lpOutputHeader->biBitCount = bitCount;
    lpOutputHeader->biCompression = BI_RGB;
    lpOutputHeader->biSizeImage = imageSize;
    lpOutputHeader->biXPelsPerMeter = 0;
    lpOutputHeader->biYPelsPerMeter = 0;
    lpOutputHeader->biClrUsed = 0;
    lpOutputHeader->biClrImportant = 0;

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    HeapFree(GetProcessHeap(), 0, row_pointers);

    GlobalUnlock(hMem);
    return (LPBITMAPINFOHEADER)GlobalLock(hMem);
}

HICON __stdcall ConvertToIconProc(LPBITMAPINFOHEADER lpPngData, DWORD dwSize)
{
    png_structp png_ptr;
    png_infop info_ptr;
    PNG_READER reader;
    png_bytep *row_pointers;
    png_bytep png_buffer;
    png_size_t png_size;

    INT width;
    INT height;
    INT i;
    WORD bitCount;

    DWORD imageSize;
    DWORD headerSize;
    HGLOBAL hMem;
    LPBYTE lpOutputBits;
    LPBITMAPINFOHEADER lpOutputHeader;

    HBITMAP hColor;
    HBITMAP hMask;
    ICONINFO iconInfo;
    HICON hIcon;
    BITMAPINFO bmi;

    OutputDebugStringA("ConvertPngIconToHICON: start\n");

    if (!lpPngData || dwSize < 8) {
        OutputDebugStringA("Invalid PNG input: NULL or too small.\n");
        return NULL;
    }

    png_buffer = (png_bytep)lpPngData;
    png_size = (png_size_t)dwSize;

    if (png_sig_cmp(png_buffer, 0, 8) != 0) {
        OutputDebugStringA("PNG signature mismatch. Not a valid PNG.\n");
        return NULL;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        OutputDebugStringA("Failed to create PNG read struct.\n");
        return NULL;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        OutputDebugStringA("Failed to create PNG info struct.\n");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        OutputDebugStringA("Error during PNG decoding (setjmp triggered).\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    reader.buffer = png_buffer;
    reader.size = png_size;
    png_set_read_fn(png_ptr, (png_voidp)&reader, ReadPngData);
    png_read_info(png_ptr, info_ptr);

    width = (INT)png_get_image_width(png_ptr, info_ptr);
    height = (INT)png_get_image_height(png_ptr, info_ptr);
    bitCount = 32;

    if (width <= 0 || height <= 0) {
        OutputDebugStringA("Invalid PNG dimensions.\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    if (png_get_bit_depth(png_ptr, info_ptr) == 16)
        png_set_strip_16(png_ptr);

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY &&
        png_get_bit_depth(png_ptr, info_ptr) < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB ||
        png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY)
        png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);

    png_set_bgr(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    row_pointers = (png_bytep *)HeapAlloc(GetProcessHeap(), 0, sizeof(png_bytep) * height);
    if (!row_pointers) {
        OutputDebugStringA("HeapAlloc for row_pointers failed.\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    imageSize = width * height * 4;
    headerSize = sizeof(BITMAPINFOHEADER);
    hMem = GlobalAlloc(GMEM_MOVEABLE, headerSize + imageSize);
    if (!hMem) {
        OutputDebugStringA("GlobalAlloc failed.\n");
        HeapFree(GetProcessHeap(), 0, row_pointers);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    lpOutputHeader = (LPBITMAPINFOHEADER)GlobalLock(hMem);
    if (!lpOutputHeader) {
        OutputDebugStringA("GlobalLock failed.\n");
        HeapFree(GetProcessHeap(), 0, row_pointers);
        GlobalFree(hMem);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    lpOutputBits = (LPBYTE)(lpOutputHeader + 1);
    for (i = 0; i < height; i++) {
        row_pointers[height - 1 - i] = lpOutputBits + (i * width * 4);
    }

    png_read_image(png_ptr, row_pointers);

    /* Preenche BITMAPINFOHEADER */
    lpOutputHeader->biSize = sizeof(BITMAPINFOHEADER);
    lpOutputHeader->biWidth = width;
    lpOutputHeader->biHeight = height;
    lpOutputHeader->biPlanes = 1;
    lpOutputHeader->biBitCount = bitCount;
    lpOutputHeader->biCompression = BI_RGB;
    lpOutputHeader->biSizeImage = imageSize;
    lpOutputHeader->biXPelsPerMeter = 0;
    lpOutputHeader->biYPelsPerMeter = 0;
    lpOutputHeader->biClrUsed = 0;
    lpOutputHeader->biClrImportant = 0;

    /* Prepara BITMAPINFO para CreateDIBitmap */
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader = *lpOutputHeader;

    hColor = CreateDIBitmap(GetDC(NULL), &bmi.bmiHeader, CBM_INIT, lpOutputBits, &bmi, DIB_RGB_COLORS);
    if (!hColor) {
        OutputDebugStringA("CreateDIBitmap (color) failed.\n");
        GlobalUnlock(hMem);
        GlobalFree(hMem);
        HeapFree(GetProcessHeap(), 0, row_pointers);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    hMask = CreateBitmap(width, height, 1, 1, NULL); /* Máscara vazia (tudo visível) */
    if (!hMask) {
        OutputDebugStringA("CreateBitmap (mask) failed.\n");
        DeleteObject(hColor);
        GlobalUnlock(hMem);
        GlobalFree(hMem);
        HeapFree(GetProcessHeap(), 0, row_pointers);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    iconInfo.fIcon = TRUE;
    iconInfo.xHotspot = 0;
    iconInfo.yHotspot = 0;
    iconInfo.hbmMask = hMask;
    iconInfo.hbmColor = hColor;

    hIcon = CreateIconIndirect(&iconInfo);

    if (!hIcon) {
        OutputDebugStringA("CreateIconIndirect failed.\n");
    } else {
        OutputDebugStringA("ConvertPngIconToHICON: success.\n");
    }

    DeleteObject(hMask);
    DeleteObject(hColor);
    GlobalUnlock(hMem);
    GlobalFree(hMem);
    HeapFree(GetProcessHeap(), 0, row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return hIcon;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("(0x%p, %d, %p)\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            PrivateRegisterICSProc((PCONVERT_TO_DIB_PROC)ConvertToIconProc);
            break;
    }

    return TRUE;
}