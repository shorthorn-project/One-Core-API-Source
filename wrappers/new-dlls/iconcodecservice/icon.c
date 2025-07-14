/*++

Copyright (c) 2018 Shorthorn Project

Module Name:

    icon.c

Abstract:

        This file implements the NT icons routines.

Author:

    Skulltrail 18-April-2018

Revision History:

--*/

#include <main.h>
#include <png.h>
#include <stdlib.h>
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

/* libpng helpers */
typedef struct _PNG_READER_STATE
{
    png_bytep buffer;
    size_t bufSize;
    size_t currentPos;
} PNG_READER_STATE;

struct png_wrapper
{
    const char *buffer;
    size_t size, pos;
};

static void PNGAPI user_read_data(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead)
{
    struct mem_io_struct
    {
        BYTE *data;
        DWORD size;
        DWORD offset;
    } *io;

    io = (struct mem_io_struct *)png_get_io_ptr(png_ptr);
    if (io->offset + byteCountToRead <= io->size)
    {
        memcpy(outBytes, io->data + io->offset, byteCountToRead);
        io->offset += (DWORD)byteCountToRead;
    }
}

#define ICO_PNG_SIGNATURE "\x89PNG\r\n\x1a\n"
#define ICO_HEADER_SIZE 6
#define ICO_ENTRY_SIZE 16

// static HICON CreateIconFromPngBits(BYTE *data, DWORD size)
// {
    // png_structp png_ptr;
    // png_infop info_ptr;
    // struct mem_io_struct
    // {
        // BYTE *data;
        // DWORD size;
        // DWORD offset;
    // } png_io;

    // int width, height;
    // png_bytep *row_pointers;
    // int i;
    // BITMAPV5HEADER bi;
    // void *dibBits;
    // HDC hDC;
    // HBITMAP hBitmap;
    // HBITMAP hMask;
    // ICONINFO ii;
    // HICON hIcon;

    // png_ptr = NULL;
    // info_ptr = NULL;
    // row_pointers = NULL;
    // dibBits = NULL;
    // hBitmap = NULL;
    // hMask = NULL;
    // hIcon = NULL;

    // png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    // if (!png_ptr)
    // {
        // DbgPrint("[PNG] Falha ao criar png_struct.\n");
        // return NULL;
    // }

    // info_ptr = png_create_info_struct(png_ptr);
    // if (!info_ptr)
    // {
        // DbgPrint("[PNG] Falha ao criar info_struct.\n");
        // png_destroy_read_struct(&png_ptr, NULL, NULL);
        // return NULL;
    // }

    // png_io.data = data;
    // png_io.size = size;
    // png_io.offset = 0;

    // png_set_read_fn(png_ptr, &png_io, user_read_data);
    // png_read_info(png_ptr, info_ptr);

    // width = (int)png_get_image_width(png_ptr, info_ptr);
    // height = (int)png_get_image_height(png_ptr, info_ptr);

    // png_set_expand(png_ptr);
    // png_set_bgr(png_ptr);
    // png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
    // png_read_update_info(png_ptr, info_ptr);

    // row_pointers = (png_bytep *)HeapAlloc(GetProcessHeap(), 0, sizeof(png_bytep) * height);
    // if (!row_pointers)
    // {
        // DbgPrint("[PNG] Falha ao alocar row_pointers.\n");
        // goto done;
    // }

    // for (i = 0; i < height; i++)
    // {
        // row_pointers[i] = (png_bytep)HeapAlloc(GetProcessHeap(), 0, width * 4);
        // if (!row_pointers[i])
        // {
            // DbgPrint("[PNG] Falha ao alocar linha do PNG.\n");
            // goto done;
        // }
    // }

    // png_read_image(png_ptr, row_pointers);

    // ZeroMemory(&bi, sizeof(bi));
    // bi.bV5Size = sizeof(bi);
    // bi.bV5Width = width;
    // bi.bV5Height = -height;
    // bi.bV5Planes = 1;
    // bi.bV5BitCount = 32;
    // bi.bV5Compression = BI_BITFIELDS;
    // bi.bV5RedMask   = 0x00FF0000;
    // bi.bV5GreenMask = 0x0000FF00;
    // bi.bV5BlueMask  = 0x000000FF;
    // bi.bV5AlphaMask = 0xFF000000;

    // hDC = GetDC(NULL);
    // hBitmap = CreateDIBSection(hDC, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &dibBits, NULL, 0);
    // ReleaseDC(NULL, hDC);

    // if (!hBitmap || !dibBits)
    // {
        // DbgPrint("[PNG] Falha ao criar DIBSection.\n");
        // goto done;
    // }

    // for (i = 0; i < height; i++)
    // {
        // memcpy((BYTE *)dibBits + i * width * 4, row_pointers[i], width * 4);
    // }

    // hMask = CreateBitmap(width, height, 1, 1, NULL);
    // if (!hMask)
    // {
        // DbgPrint("[PNG] Falha ao criar máscara.\n");
        // goto done;
    // }

    // ii.fIcon = TRUE;
    // ii.xHotspot = 0;
    // ii.yHotspot = 0;
    // ii.hbmMask = hMask;
    // ii.hbmColor = hBitmap;

    // hIcon = CreateIconIndirect(&ii);
    // if (!hIcon)
    // {
        // DbgPrint("[PNG] CreateIconIndirect falhou.\n");
    // }

// done:
    // if (row_pointers)
    // {
        // for (i = 0; i < height; i++)
        // {
            // if (row_pointers[i])
                // HeapFree(GetProcessHeap(), 0, row_pointers[i]);
        // }
        // HeapFree(GetProcessHeap(), 0, row_pointers);
    // }

    // png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    // if (hBitmap)
        // DeleteObject(hBitmap);
    // if (hMask)
        // DeleteObject(hMask);

    // return hIcon;
// }

/* Convert PNG raw data to BMP icon data */
static LPBYTE
ConvertPngToBmpIcon(
    _In_ LPBYTE pngBits,
    _In_ DWORD fileSize,
    _Out_ PDWORD pBmpIconSize)
{
    if (!pngBits || fileSize < PNG_CHECK_SIG_SIZE || !png_check_sig(pngBits, PNG_CHECK_SIG_SIZE))
        return NULL;

    TRACE("pngBits %p fileSize %d\n", pngBits, fileSize);

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        ERR("png_create_read_struct error\n");
        return NULL;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        ERR("png_create_info_struct error\n");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    /* Set our own read_function */
    PNG_READER_STATE readerState = { pngBits, fileSize, PNG_CHECK_SIG_SIZE };
    png_set_read_fn(png_ptr, &readerState, ReadMemoryPng);
    png_set_sig_bytes(png_ptr, PNG_CHECK_SIG_SIZE);

    /* Read png info */
    png_read_info(png_ptr, info_ptr);

    /* Add translation of some PNG formats and update info */
    int colorType = png_get_color_type(png_ptr, info_ptr);
    if (colorType == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    else if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);
    png_set_scale_16(png_ptr); /* Convert 16-bit channels to 8-bit */
    png_read_update_info(png_ptr, info_ptr);

    /* Get updated png info */
    png_uint_32 width, height;
    int bitDepth;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bitDepth, &colorType, NULL, NULL, NULL);
    TRACE("width %d, height %d, bitDepth %d, colorType %d\n",
          width, height, bitDepth, colorType);

    int channels = png_get_channels(png_ptr, info_ptr);
    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    int imageSize = height * rowbytes;
    TRACE("rowbytes %d, channels %d, imageSize %d\n", rowbytes, channels, imageSize);

    /* Allocate rows data */
    png_bytepp rows = png_malloc(png_ptr, sizeof(png_bytep) * height);
    if (!rows)
    {
        ERR("png_malloc failed\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    for (int i = 0; i < (int)height; i++)
    {
        rows[i] = png_malloc(png_ptr, rowbytes);
        if (!rows[i])
        {
            ERR("png_malloc failed\n");

            /* Clean up */
            while (--i >= 0)
                png_free(png_ptr, rows[i]);
            png_free(png_ptr, rows);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

            return NULL;
        }
    }

    /* Read png image data */
    png_set_rows(png_ptr, info_ptr, rows);
    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, info_ptr);

    /* After reading the image, you can deal with row pointers */
    LPBYTE imageBytes = HeapAlloc(GetProcessHeap(), 0, imageSize);
    if (imageBytes)
    {
        LPBYTE pb = imageBytes;
        for (int i = height - 1; i >= 0; i--)
        {
            png_bytep row = rows[i];
            for (int j = 0; j < channels * width; j += channels)
            {
                *pb++ = row[j + 2]; /* Red */
                *pb++ = row[j + 1]; /* Green */
                *pb++ = row[j + 0]; /* Blue */
                if (channels == 4)
                    *pb++ = row[j + 3]; /* Alpha */
            }
            pb += (channels * width) % 4;
        }
    }

    /* Clean up */
    for (int i = 0; i < (int)height; i++)
        png_free(png_ptr, rows[i]);
    png_free(png_ptr, rows);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    if (!imageBytes)
    {
        ERR("HeapAlloc failed\n");
        return NULL;
    }

    /* BPP (Bits Per Pixel) */
    WORD bpp = (WORD)(bitDepth * channels);

    /* The byte size of mask bits */
    DWORD maskSize = get_dib_image_size(width, height, 1);

    /* Build BITMAPINFOHEADER */
    BITMAPINFOHEADER info = { sizeof(info) };
    info.biWidth = width;
    info.biHeight = 2 * height;
    info.biPlanes = 1;
    info.biBitCount = bpp;
    info.biCompression = BI_RGB;

    /* Build CURSORICONFILEDIR */
    CURSORICONFILEDIR cifd = { 0, 1, 1 };
    cifd.idEntries[0].bWidth = (BYTE)width;
    cifd.idEntries[0].bHeight = (BYTE)height;
    cifd.idEntries[0].bColorCount = 0; /* No color palette */
    cifd.idEntries[0].wPlanes = 1; /* Must be 1 */
    cifd.idEntries[0].wBitCount = bpp;
    cifd.idEntries[0].dwDIBSize = (DWORD)(sizeof(info) + imageSize + maskSize);
    cifd.idEntries[0].dwDIBOffset = (DWORD)sizeof(cifd);

    /* Allocate BMP icon data */
    *pBmpIconSize = (DWORD)(sizeof(cifd) + sizeof(info) + imageSize + maskSize);
    LPBYTE pbBmpIcon = HeapAlloc(GetProcessHeap(), 0, *pBmpIconSize);
    if (!pbBmpIcon)
    {
        ERR("HeapAlloc failed\n");
        HeapFree(GetProcessHeap(), 0, imageBytes);
        return NULL;
    }

    /* Store data to pbBmpIcon */
    PBYTE pb = pbBmpIcon;
    RtlCopyMemory(pb, &cifd, sizeof(cifd));
    pb += sizeof(cifd);
    RtlCopyMemory(pb, &info, sizeof(info));
    pb += sizeof(info);
    RtlCopyMemory(pb, imageBytes, imageSize);
    pb += imageSize;
    RtlFillMemory(pb, maskSize, 0xFF); /* Mask bits for AND operation */

    HeapFree(GetProcessHeap(), 0, imageBytes);
    return pbBmpIcon;
}