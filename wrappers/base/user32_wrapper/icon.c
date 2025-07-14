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

int WINAPI LookupIconIdFromDirectoryExInternal(
  _In_  PBYTE presbits,
  _In_  BOOL fIcon,
  _In_  int cxDesired,
  _In_  int cyDesired,
  _In_  UINT Flags
);

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

static inline void * heap_calloc(SIZE_T count, SIZE_T size)
{
    SIZE_T len = count * size;

    if (size && len / size != count)
        return NULL;
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
}

/* This function is used for reading png data from memory */
static VOID
ReadMemoryPng(
    _Inout_ png_structp png_ptr,
    _Out_ png_bytep data,
    _In_ size_t length)
{
    PNG_READER_STATE *state = png_get_io_ptr(png_ptr);
    if ((state->currentPos + length) > state->bufSize)
    {
        ERR("png read error\n");
        png_error(png_ptr, "read error in ReadMemoryPng");
        return;
    }
    RtlCopyMemory(data, state->buffer + state->currentPos, length);
    state->currentPos += length;
}

/* libpng.dll is delay-loaded. If no libpng.dll exists, we have to avoid exception */
static BOOL
LibPngExists(VOID)
{
    static BOOL bLibPngFound = -1;
    if (bLibPngFound == -1)
    {
        HINSTANCE hLibPng = LoadLibraryExW(L"libpng.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
        bLibPngFound  = !!hLibPng;
        FreeLibrary(hLibPng);
    }
    return bLibPngFound;
}

static int get_dib_image_size(int width, int height, int depth);

/* Convert PNG raw data to BMP icon data */
PBYTE CURSORICON_ConvertPngToBmpIcon(
    PBYTE pngBits,
    DWORD fileSize,
    PDWORD pBmpIconSize)
{
    png_structp png_ptr;
    png_infop info_ptr;
    struct PNG_READER_STATE
    {
        PBYTE data;
        DWORD size;
        DWORD pos;
    } readerState;
    int channels, rowbytes, imageSize;
    png_uint_32 width, height;
    int bitDepth, colorType;
    int i, j;
    PBYTE imageBytes;
    png_bytep row;
    png_bytepp rows;
    WORD bpp;
    DWORD maskSize;
    BITMAPINFOHEADER info;
    struct CURSORICONFILEDIR
    {
        WORD idReserved;
        WORD idType;
        WORD idCount;
        struct {
            BYTE bWidth;
            BYTE bHeight;
            BYTE bColorCount;
            BYTE bReserved;
            WORD wPlanes;
            WORD wBitCount;
            DWORD dwDIBSize;
            DWORD dwDIBOffset;
        } idEntries[1];
    } cifd;
    PBYTE pbBmpIcon, pb;

    if (!pngBits || fileSize < 8 || !png_check_sig(pngBits, 8)) return NULL;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return NULL;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    readerState.data = pngBits;
    readerState.size = fileSize;
    readerState.pos = 8;
    png_set_read_fn(png_ptr, &readerState, ReadMemoryPng);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    colorType = png_get_color_type(png_ptr, info_ptr);
    if (colorType == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    else if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);
    png_set_scale_16(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bitDepth, &colorType, NULL, NULL, NULL);

    channels = png_get_channels(png_ptr, info_ptr);
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    imageSize = height * rowbytes;

    rows = (png_bytepp)png_malloc(png_ptr, sizeof(png_bytep) * height);
    if (!rows) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    for (i = 0; i < (int)height; i++) {
        rows[i] = (png_bytep)png_malloc(png_ptr, rowbytes);
        if (!rows[i]) {
            while (--i >= 0) png_free(png_ptr, rows[i]);
            png_free(png_ptr, rows);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            return NULL;
        }
    }

    png_set_rows(png_ptr, info_ptr, rows);
    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, info_ptr);

    imageBytes = (PBYTE)HeapAlloc(GetProcessHeap(), 0, imageSize);
    if (!imageBytes) {
        for (i = 0; i < (int)height; i++) png_free(png_ptr, rows[i]);
        png_free(png_ptr, rows);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    pb = imageBytes;
    for (i = height - 1; i >= 0; i--) {
        row = rows[i];
        for (j = 0; j < (int)(channels * width); j += channels) {
            *pb++ = row[j + 2];
            *pb++ = row[j + 1];
            *pb++ = row[j + 0];
            if (channels == 4)
                *pb++ = row[j + 3];
        }
        pb += (channels * width) % 4;
    }

    for (i = 0; i < (int)height; i++) png_free(png_ptr, rows[i]);
    png_free(png_ptr, rows);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    bpp = (WORD)(bitDepth * channels);
    maskSize = get_dib_image_size(width, height, 1);

    memset(&info, 0, sizeof(info));
    info.biSize = sizeof(info);
    info.biWidth = width;
    info.biHeight = 2 * height;
    info.biPlanes = 1;
    info.biBitCount = bpp;
    info.biCompression = BI_RGB;

    memset(&cifd, 0, sizeof(cifd));
    cifd.idReserved = 0;
    cifd.idType = 1;
    cifd.idCount = 1;
    cifd.idEntries[0].bWidth = (BYTE)width;
    cifd.idEntries[0].bHeight = (BYTE)height;
    cifd.idEntries[0].wPlanes = 1;
    cifd.idEntries[0].wBitCount = bpp;
    cifd.idEntries[0].dwDIBSize = sizeof(info) + imageSize + maskSize;
    cifd.idEntries[0].dwDIBOffset = sizeof(cifd);

    *pBmpIconSize = sizeof(cifd) + sizeof(info) + imageSize + maskSize;
    pbBmpIcon = (PBYTE)HeapAlloc(GetProcessHeap(), 0, *pBmpIconSize);
    if (!pbBmpIcon) {
        HeapFree(GetProcessHeap(), 0, imageBytes);
        return NULL;
    }

    pb = pbBmpIcon;
    memcpy(pb, &cifd, sizeof(cifd));
    pb += sizeof(cifd);
    memcpy(pb, &info, sizeof(info));
    pb += sizeof(info);
    memcpy(pb, imageBytes, imageSize);
    pb += imageSize;
    memset(pb, 0xFF, maskSize);

    HeapFree(GetProcessHeap(), 0, imageBytes);
    return pbBmpIcon;
}

BOOL 
WINAPI 
PrivateRegisterICSProc(RegisterCallback registrator)
{
  BOOL result; // eax@2

  if ( gpICSProc )
  {
    result = FALSE;
  }
  else
  {
    gpICSProc = registrator;
    result = TRUE;
  }
  return result;
}

/**********************************************************************
 *              GetIconInfoExA (USER32.@)
 */
BOOL WINAPI GetIconInfoExA( HICON icon, ICONINFOEXA *info )
{
    ICONINFOEXW infoW;

    if (info->cbSize != sizeof(*info))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    infoW.cbSize = sizeof(infoW);
    if (!GetIconInfoExW( icon, &infoW )) return FALSE;
    info->fIcon    = infoW.fIcon;
    info->xHotspot = infoW.xHotspot;
    info->yHotspot = infoW.yHotspot;
    info->hbmColor = infoW.hbmColor;
    info->hbmMask  = infoW.hbmMask;
    info->wResID   = infoW.wResID;
    WideCharToMultiByte( CP_ACP, 0, infoW.szModName, -1, info->szModName, MAX_PATH, NULL, NULL );
    WideCharToMultiByte( CP_ACP, 0, infoW.szResName, -1, info->szResName, MAX_PATH, NULL, NULL );
    return TRUE;
}

/**********************************************************************
 *              GetIconInfoExW (USER32.@)
 */
BOOL WINAPI GetIconInfoExW(HICON hIcon, ICONINFOEXW *ret)
{
    ICONINFO info;
    if (!ret || ret->cbSize != sizeof(*ret))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!GetIconInfo(hIcon, &info))
        return FALSE;

    ret->fIcon = info.fIcon;
    ret->xHotspot = info.xHotspot;
    ret->yHotspot = info.yHotspot;
    ret->hbmColor = info.hbmColor;
    ret->hbmMask  = info.hbmMask;

    /* Em modo usuário, não há como descobrir o módulo ou o nome do recurso original */
    ret->wResID = 0;
    ret->szModName[0] = L'\0';
    ret->szResName[0] = L'\0';

    return TRUE;
}

/*******************************************************************
 *		InternalGetWindowIcon (USER32.@)
 */
INT WINAPI InternalGetWindowIcon(HWND hwnd, UINT iconType )
{
    return 0;
}

/************* IMPLEMENTATION HELPERS ******************/
/**********************************************************************
 * User objects management
 */

static HBITMAP create_color_bitmap( int width, int height )
{
    HDC hdc = CreateDCW(DISPLAYW, NULL, NULL, NULL);
    HBITMAP ret = CreateCompatibleBitmap( hdc, width, height );
    DeleteDC( hdc );
    return ret;
}

static int get_display_bpp(void)
{
    HDC hdc = CreateDCW(DISPLAYW, NULL, NULL, NULL);
    int ret = GetDeviceCaps( hdc, BITSPIXEL );
    DeleteDC( hdc );
    return ret;
}

static void free_icon_frame( struct cursoricon_frame *frame )
{
    if (frame->color) DeleteObject( frame->color );
    if (frame->alpha) DeleteObject( frame->alpha );
    if (frame->mask)  DeleteObject( frame->mask );
}


/***********************************************************************
 *             map_fileW
 *
 * Helper function to map a file to memory:
 *  name			-	file name
 *  [RETURN] ptr		-	pointer to mapped file
 *  [RETURN] filesize           -       pointer size of file to be stored if not NULL
 */
static const void *map_fileW( LPCWSTR name, LPDWORD filesize )
{
    HANDLE hFile, hMapping;
    LPVOID ptr = NULL;
	
	DbgPrint("map_fileW called\n");

    hFile = CreateFileW( name, GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0 );
    if (hFile != INVALID_HANDLE_VALUE)
    {
		DbgPrint("map_fileW:: calling CreateFileMappingW\n");
        hMapping = CreateFileMappingW( hFile, NULL, PAGE_READONLY, 0, 0, NULL );
        if (hMapping)
        {
			DbgPrint("map_fileW:: calling MapViewOfFile\n");
            ptr = MapViewOfFile( hMapping, FILE_MAP_READ, 0, 0, 0 );
            CloseHandle( hMapping );
            if (filesize){
                *filesize = GetFileSize( hFile, NULL );
			}
        }
        CloseHandle( hFile );
    }
	DbgPrint("map_fileW:: return file content\n");
    return ptr;
}


/***********************************************************************
 *          get_dib_image_size
 *
 * Return the size of a DIB bitmap in bytes.
 */
static int get_dib_image_size( int width, int height, int depth )
{
    return (((width * depth + 31) / 8) & ~3) * abs( height );
}


/***********************************************************************
 *           bitmap_info_size
 *
 * Return the size of the bitmap info structure including color table.
 */
int bitmap_info_size( const BITMAPINFO * info, WORD coloruse )
{
    unsigned int colors, size, masks = 0;

    if (info->bmiHeader.biSize == sizeof(BITMAPCOREHEADER))
    {
        const BITMAPCOREHEADER *core = (const BITMAPCOREHEADER *)info;
        colors = (core->bcBitCount <= 8) ? 1 << core->bcBitCount : 0;
        return sizeof(BITMAPCOREHEADER) + colors *
             ((coloruse == DIB_RGB_COLORS) ? sizeof(RGBTRIPLE) : sizeof(WORD));
    }
    else  /* assume BITMAPINFOHEADER */
    {
        colors = info->bmiHeader.biClrUsed;
        if (colors > 256) /* buffer overflow otherwise */
                colors = 256;
        if (!colors && (info->bmiHeader.biBitCount <= 8))
            colors = 1 << info->bmiHeader.biBitCount;
        if (info->bmiHeader.biCompression == BI_BITFIELDS) masks = 3;
        size = max( info->bmiHeader.biSize, sizeof(BITMAPINFOHEADER) + masks * sizeof(DWORD) );
        return size + colors * ((coloruse == DIB_RGB_COLORS) ? sizeof(RGBQUAD) : sizeof(WORD));
    }
}


/***********************************************************************
 *          is_dib_monochrome
 *
 * Returns whether a DIB can be converted to a monochrome DDB.
 *
 * A DIB can be converted if its color table contains only black and
 * white. Black must be the first color in the color table.
 *
 * Note : If the first color in the color table is white followed by
 *        black, we can't convert it to a monochrome DDB with
 *        SetDIBits, because black and white would be inverted.
 */
static BOOL is_dib_monochrome( const BITMAPINFO* info )
{
    if (info->bmiHeader.biSize == sizeof(BITMAPCOREHEADER))
    {
        const RGBTRIPLE *rgb = ((const BITMAPCOREINFO*)info)->bmciColors;

        if (((const BITMAPCOREINFO*)info)->bmciHeader.bcBitCount != 1) return FALSE;

        /* Check if the first color is black */
        if ((rgb->rgbtRed == 0) && (rgb->rgbtGreen == 0) && (rgb->rgbtBlue == 0))
        {
            rgb++;

            /* Check if the second color is white */
            return ((rgb->rgbtRed == 0xff) && (rgb->rgbtGreen == 0xff)
                 && (rgb->rgbtBlue == 0xff));
        }
        else return FALSE;
    }
    else  /* assume BITMAPINFOHEADER */
    {
        const RGBQUAD *rgb = info->bmiColors;

        if (info->bmiHeader.biBitCount != 1) return FALSE;

        /* Check if the first color is black */
        if ((rgb->rgbRed == 0) && (rgb->rgbGreen == 0) &&
            (rgb->rgbBlue == 0) && (rgb->rgbReserved == 0))
        {
            rgb++;

            /* Check if the second color is white */
            return ((rgb->rgbRed == 0xff) && (rgb->rgbGreen == 0xff)
                 && (rgb->rgbBlue == 0xff) && (rgb->rgbReserved == 0));
        }
        else return FALSE;
    }
}

/***********************************************************************
 *           DIB_GetBitmapInfo
 *
 * Get the info from a bitmap header.
 * Return 1 for INFOHEADER, 0 for COREHEADER, -1 in case of failure.
 */
static int DIB_GetBitmapInfo( const BITMAPINFOHEADER *header, LONG *width,
                              LONG *height, WORD *bpp, DWORD *compr )
{
    if (header->biSize == sizeof(BITMAPCOREHEADER))
    {
        const BITMAPCOREHEADER *core = (const BITMAPCOREHEADER *)header;
        *width  = core->bcWidth;
        *height = core->bcHeight;
        *bpp    = core->bcBitCount;
        *compr  = 0;
        return 0;
    }
    else if (header->biSize == sizeof(BITMAPINFOHEADER) ||
             header->biSize == sizeof(BITMAPV4HEADER) ||
             header->biSize == sizeof(BITMAPV5HEADER))
    {
        *width  = header->biWidth;
        *height = header->biHeight;
        *bpp    = header->biBitCount;
        *compr  = header->biCompression;
        return 1;
    }
    WARN("unknown/wrong size (%lu) for header\n", header->biSize);
    return -1;
}

// /**********************************************************************
 // *              get_icon_size
 // */
// BOOL get_icon_size( HICON handle, SIZE *size )
// {
    // BOOL ret = NtUserGetIconSize( handle, 0, &size->cx, &size->cy );
    // if (ret) size->cy /= 2;
    // return ret;
// }

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

static unsigned be_uint(unsigned val)
{
    union
    {
        unsigned val;
        unsigned char c[4];
    } u;

    u.val = val;
    return (u.c[0] << 24) | (u.c[1] << 16) | (u.c[2] << 8) | u.c[3];
}

static BOOL get_png_info(const void *png_data, DWORD size, int *width, int *height, int *bpp)
{
    static const char png_sig[8] = { 0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a };
    static const char png_IHDR[8] = { 0,0,0,0x0d,'I','H','D','R' };
    const struct
    {
        char png_sig[8];
        char ihdr_sig[8];
        unsigned width, height;
        char bit_depth, color_type, compression, filter, interlace;
    } *png = png_data;

    if (size < sizeof(*png)) return FALSE;
    if (memcmp(png->png_sig, png_sig, sizeof(png_sig)) != 0) return FALSE;
    if (memcmp(png->ihdr_sig, png_IHDR, sizeof(png_IHDR)) != 0) return FALSE;

    *bpp = (png->color_type == PNG_COLOR_TYPE_RGB_ALPHA) ? 32 : 24;
    *width = be_uint(png->width);
    *height = be_uint(png->height);

    return TRUE;
}

static BITMAPINFO *load_png(const char *png_data, DWORD *size)
{
    struct png_wrapper png;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep *row_pointers = NULL;
    int color_type, bit_depth, bpp, width, height;
    int rowbytes, image_size, mask_size = 0, i;
    BITMAPINFO *info = NULL;
    unsigned char *image_data;
    SIZE_T row_pointers_size;

    if (!get_png_info(png_data, *size, &width, &height, &bpp)) return NULL;

    png.buffer = png_data;
    png.size = *size;
    png.pos = 0;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return NULL;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    png_set_crc_action(png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
    png_set_read_fn(png_ptr, &png, user_read_data);
    png_read_info(png_ptr, info_ptr);

    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE || bit_depth < 8)
        png_set_expand(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    bpp = 0;
    switch (color_type)
    {
    case PNG_COLOR_TYPE_RGB:
        if (bit_depth == 8)
            bpp = 24;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        if (bit_depth == 8)
        {
            png_set_bgr(png_ptr);
            bpp = 32;
        }
        break;
    default:
        break;
    }

    if (!bpp)
    {
        FIXME("unsupported PNG color format %d, %d bpp\n", color_type, bit_depth);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    width  = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);

    rowbytes    = (width * bpp + 7) / 8;
    image_size  = height * rowbytes;
    if (bpp != 32)
        mask_size = (width + 7) / 8 * height;

    info = (BITMAPINFO *)RtlAllocateHeap(GetProcessHeap(), 0, sizeof(BITMAPINFOHEADER) + image_size + mask_size);
    if (!info)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    image_data = (unsigned char *)info + sizeof(BITMAPINFOHEADER);
    memset(image_data + image_size, 0, mask_size);

    row_pointers_size = height * sizeof(png_bytep);
    row_pointers = (png_bytep *)RtlAllocateHeap(GetProcessHeap(), 0, row_pointers_size);
    if (!row_pointers)
    {
        RtlFreeHeap(GetProcessHeap(), 0, info);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    for (i = 0; i < height; i++)
        row_pointers[i] = image_data + (height - i - 1) * rowbytes;

    png_read_image(png_ptr, row_pointers);
    RtlFreeHeap(GetProcessHeap(), 0, row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    info->bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth         = width;
    info->bmiHeader.biHeight        = height * 2;
    info->bmiHeader.biPlanes        = 1;
    info->bmiHeader.biBitCount      = bpp;
    info->bmiHeader.biCompression   = BI_RGB;
    info->bmiHeader.biSizeImage     = image_size;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed       = 0;
    info->bmiHeader.biClrImportant  = 0;

    *size = sizeof(BITMAPINFOHEADER) + image_size + mask_size;
    return info;
}

/*
 *  The following macro functions account for the irregularities of
 *   accessing cursor and icon resources in files and resource entries.
 */
typedef BOOL (*fnGetCIEntry)( LPCVOID dir, DWORD size, int n,
                              int *width, int *height, int *bits );

/**********************************************************************
 *	    CURSORICON_FindBestIcon
 *
 * Find the icon closest to the requested size and bit depth.
 */
static int CURSORICON_FindBestIcon( LPCVOID dir, DWORD size, fnGetCIEntry get_entry,
                                    int width, int height, int depth, UINT loadflags )
{
    int i, cx, cy, bits, bestEntry = -1;
    UINT iTotalDiff, iXDiff=0, iYDiff=0, iColorDiff;
    UINT iTempXDiff, iTempYDiff, iTempColorDiff;

    /* Find Best Fit */
    iTotalDiff = 0xFFFFFFFF;
    iColorDiff = 0xFFFFFFFF;

    if (loadflags & LR_DEFAULTSIZE)
    {
        if (!width) width = GetSystemMetrics( SM_CXICON );
        if (!height) height = GetSystemMetrics( SM_CYICON );
    }
    else if (!width && !height)
    {
        /* use the size of the first entry */
        if (!get_entry( dir, size, 0, &width, &height, &bits )) return -1;
        iTotalDiff = 0;
    }

    for ( i = 0; iTotalDiff && get_entry( dir, size, i, &cx, &cy, &bits ); i++ )
    {
        iTempXDiff = abs(width - cx);
        iTempYDiff = abs(height - cy);

        if(iTotalDiff > (iTempXDiff + iTempYDiff))
        {
            iXDiff = iTempXDiff;
            iYDiff = iTempYDiff;
            iTotalDiff = iXDiff + iYDiff;
        }
    }

    /* Find Best Colors for Best Fit */
    for ( i = 0; get_entry( dir, size, i, &cx, &cy, &bits ); i++ )
    {
        TRACE("entry %d: %d x %d, %d bpp\n", i, cx, cy, bits);

        if(abs(width - cx) == iXDiff && abs(height - cy) == iYDiff)
        {
            iTempColorDiff = abs(depth - bits);
            if(iColorDiff > iTempColorDiff)
            {
                bestEntry = i;
                iColorDiff = iTempColorDiff;
            }
        }
    }

    return bestEntry;
}

static BOOL CURSORICON_GetResIconEntry( LPCVOID dir, DWORD size, int n,
                                        int *width, int *height, int *bits )
{
    const CURSORICONDIR *resdir = dir;
    const ICONRESDIR *icon;

    if ( resdir->idCount <= n )
        return FALSE;
    if ((const char *)&resdir->idEntries[n + 1] - (const char *)dir > size)
        return FALSE;
    icon = &resdir->idEntries[n].ResInfo.icon;
    *width = icon->bWidth;
    *height = icon->bHeight;
    *bits = resdir->idEntries[n].wBitCount;
    if (!*width && !*height) *width = *height = 256;
    return TRUE;
}

/**********************************************************************
 *	    CURSORICON_FindBestCursor
 *
 * Find the cursor closest to the requested size.
 *
 * FIXME: parameter 'color' ignored.
 */
static int CURSORICON_FindBestCursor( LPCVOID dir, DWORD size, fnGetCIEntry get_entry,
                                      int width, int height, int depth, UINT loadflags )
{
    int i, maxwidth, maxheight, maxbits, cx, cy, bits, bestEntry = -1;

    if (loadflags & LR_DEFAULTSIZE)
    {
        if (!width) width = GetSystemMetrics( SM_CXCURSOR );
        if (!height) height = GetSystemMetrics( SM_CYCURSOR );
    }
    else if (!width && !height)
    {
        /* use the first entry */
        if (!get_entry( dir, size, 0, &width, &height, &bits )) return -1;
        return 0;
    }

    /* First find the largest one smaller than or equal to the requested size*/

    maxwidth = maxheight = maxbits = 0;
    for ( i = 0; get_entry( dir, size, i, &cx, &cy, &bits ); i++ )
    {
        if (cx > width || cy > height) continue;
        if (cx < maxwidth || cy < maxheight) continue;
        if (cx == maxwidth && cy == maxheight)
        {
            if (loadflags & LR_MONOCHROME)
            {
                if (maxbits && bits >= maxbits) continue;
            }
            else if (bits <= maxbits) continue;
        }
        bestEntry = i;
        maxwidth  = cx;
        maxheight = cy;
        maxbits = bits;
    }
    if (bestEntry != -1) return bestEntry;

    /* Now find the smallest one larger than the requested size */

    maxwidth = maxheight = 255;
    for ( i = 0; get_entry( dir, size, i, &cx, &cy, &bits ); i++ )
    {
        if (cx > maxwidth || cy > maxheight) continue;
        if (cx == maxwidth && cy == maxheight)
        {
            if (loadflags & LR_MONOCHROME)
            {
                if (maxbits && bits >= maxbits) continue;
            }
            else if (bits <= maxbits) continue;
        }
        bestEntry = i;
        maxwidth  = cx;
        maxheight = cy;
        maxbits = bits;
    }
    if (bestEntry == -1) bestEntry = 0;

    return bestEntry;
}

static BOOL CURSORICON_GetResCursorEntry( LPCVOID dir, DWORD size, int n,
                                          int *width, int *height, int *bits )
{
    const CURSORICONDIR *resdir = dir;
    const CURSORDIR *cursor;

    if ( resdir->idCount <= n )
        return FALSE;
    if ((const char *)&resdir->idEntries[n + 1] - (const char *)dir > size)
        return FALSE;
    cursor = &resdir->idEntries[n].ResInfo.cursor;
    *width = cursor->wWidth;
    *height = cursor->wHeight;
    *bits = resdir->idEntries[n].wBitCount;
    if (*height == *width * 2) *height /= 2;
    return TRUE;
}

static const CURSORICONDIRENTRY *CURSORICON_FindBestIconRes( const CURSORICONDIR * dir, DWORD size,
                                                             int width, int height, int depth,
                                                             UINT loadflags )
{
    int n;

    n = CURSORICON_FindBestIcon( dir, size, CURSORICON_GetResIconEntry,
                                 width, height, depth, loadflags );
    if ( n < 0 )
        return NULL;
    return &dir->idEntries[n];
}

static const CURSORICONDIRENTRY *CURSORICON_FindBestCursorRes( const CURSORICONDIR *dir, DWORD size,
                                                               int width, int height, int depth,
                                                               UINT loadflags )
{
    int n = CURSORICON_FindBestCursor( dir, size, CURSORICON_GetResCursorEntry,
                                       width, height, depth, loadflags );
    if ( n < 0 )
        return NULL;
    return &dir->idEntries[n];
}

static BOOL CURSORICON_GetFileEntry( LPCVOID dir, DWORD size, int n,
                                     int *width, int *height, int *bits )
{
    const CURSORICONFILEDIR *filedir = dir;
    const CURSORICONFILEDIRENTRY *entry;
    const BITMAPINFOHEADER *info;

    if ( filedir->idCount <= n )
	{
		DbgPrint("CURSORICON_GetFileEntry::filedir->idCount <= n\n");
        return FALSE;
	}
    if ((const char *)&filedir->idEntries[n + 1] - (const char *)dir > size){
        DbgPrint("CURSORICON_GetFileEntry::(const char *)&filedir->idEntries[n + 1] - (const char *)dir > size\n");
		return FALSE;
	}
    entry = &filedir->idEntries[n];
    if (entry->dwDIBOffset > size - sizeof(info->biSize)){ 
		DbgPrint("CURSORICON_GetFileEntry::entry->dwDIBOffset > size - sizeof(info->biSize)\n");
		return FALSE;
	}
    info = (const BITMAPINFOHEADER *)((const char *)dir + entry->dwDIBOffset);

    if (info->biSize == PNG_SIGN) 
	{	
		DbgPrint("CURSORICON_GetFileEntry::calling get_png_info\n");
		return get_png_info(info, size, width, height, bits);
	}

    if (info->biSize != sizeof(BITMAPCOREHEADER))
    {
		DbgPrint("CURSORICON_GetFileEntry::info->biSize != sizeof(BITMAPCOREHEADER)\n");
        if ((const char *)(info + 1) - (const char *)dir > size) return FALSE;
        *bits = info->biBitCount;
    }
    else
    {
        const BITMAPCOREHEADER *coreinfo = (const BITMAPCOREHEADER *)((const char *)dir + entry->dwDIBOffset);
        if ((const char *)(coreinfo + 1) - (const char *)dir > size) return FALSE;
        *bits = coreinfo->bcBitCount;
    }
    *width = entry->bWidth;
    *height = entry->bHeight;
    return TRUE;
}

static const CURSORICONFILEDIRENTRY *CURSORICON_FindBestCursorFile( const CURSORICONFILEDIR *dir, DWORD size,
                                                                    int width, int height, int depth,
                                                                    UINT loadflags )
{
    int n = CURSORICON_FindBestCursor( dir, size, CURSORICON_GetFileEntry,
                                       width, height, depth, loadflags );
    if ( n < 0 )
        return NULL;
    return &dir->idEntries[n];
}

static const CURSORICONFILEDIRENTRY *CURSORICON_FindBestIconFile( const CURSORICONFILEDIR *dir, DWORD size,
                                                                  int width, int height, int depth,
                                                                  UINT loadflags )
{
    int n = CURSORICON_FindBestIcon( dir, size, CURSORICON_GetFileEntry,
                                     width, height, depth, loadflags );
    if ( n < 0 )
        return NULL;
    return &dir->idEntries[n];
}

/***********************************************************************
 *          bmi_has_alpha
 */
static BOOL bmi_has_alpha( const BITMAPINFO *info, const void *bits )
{
    int i;
    BOOL has_alpha = FALSE;
    const unsigned char *ptr = bits;

    if (info->bmiHeader.biBitCount != 32) return FALSE;
    for (i = 0; i < info->bmiHeader.biWidth * abs(info->bmiHeader.biHeight); i++, ptr += 4)
        if ((has_alpha = (ptr[3] != 0))) break;
    return has_alpha;
}

/***********************************************************************
 *          create_alpha_bitmap
 *
 * Create the alpha bitmap for a 32-bpp icon that has an alpha channel.
 */
static HBITMAP create_alpha_bitmap( HBITMAP color, const BITMAPINFO *src_info, const void *color_bits )
{
    HBITMAP alpha = 0;
    BITMAPINFO *info = NULL;
    BITMAP bm;
    HDC hdc;
    void *bits;
    unsigned char *ptr;
    int i;

    if (!GetObjectW( color, sizeof(bm), &bm )) return 0;
    if (bm.bmBitsPixel != 32) return 0;

    if (!(hdc = CreateCompatibleDC( 0 ))) return 0;
    if (!(info = HeapAlloc( GetProcessHeap(), 0, FIELD_OFFSET( BITMAPINFO, bmiColors[256] )))) goto done;
    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = bm.bmWidth;
    info->bmiHeader.biHeight = -bm.bmHeight;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biSizeImage = bm.bmWidth * bm.bmHeight * 4;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;
    if (!(alpha = CreateDIBSection( hdc, info, DIB_RGB_COLORS, &bits, NULL, 0 ))) goto done;

    if (src_info)
    {
        SelectObject( hdc, alpha );
        StretchDIBits( hdc, 0, 0, bm.bmWidth, bm.bmHeight,
                       0, 0, src_info->bmiHeader.biWidth, src_info->bmiHeader.biHeight,
                       color_bits, src_info, DIB_RGB_COLORS, SRCCOPY );

    }
    else
    {
        GetDIBits( hdc, color, 0, bm.bmHeight, bits, info, DIB_RGB_COLORS );
        if (!bmi_has_alpha( info, bits ))
        {
            DeleteObject( alpha );
            alpha = 0;
            goto done;
        }
    }

    /* pre-multiply by alpha */
    for (i = 0, ptr = bits; i < bm.bmWidth * bm.bmHeight; i++, ptr += 4)
    {
        unsigned int alpha = ptr[3];
        ptr[0] = (ptr[0] * alpha + 127) / 255;
        ptr[1] = (ptr[1] * alpha + 127) / 255;
        ptr[2] = (ptr[2] * alpha + 127) / 255;
    }

done:
    DeleteDC( hdc );
    HeapFree( GetProcessHeap(), 0, info );
    return alpha;
}

static BOOL create_icon_frame( const BITMAPINFO *bmi, DWORD maxsize, POINT hotspot, BOOL is_icon,
                               INT width, INT height, UINT flags, struct cursoricon_frame *frame )
{
    DWORD size, color_size, mask_size, compr;
    const void *color_bits, *mask_bits;
    void *alpha_mask_bits = NULL;
    LONG bmi_width, bmi_height;
    BITMAPINFO *bmi_copy;
    BOOL do_stretch;
    HDC hdc = 0;
    WORD bpp;
    BOOL ret = FALSE;

    memset( frame, 0, sizeof(*frame) );

    /* Check bitmap header */

    if (bmi->bmiHeader.biSize == PNG_SIGN)
    {
        BITMAPINFO *bmi_png;

        if (!(bmi_png = load_png( (const char *)bmi, &maxsize ))) return FALSE;
        ret = create_icon_frame( bmi_png, maxsize, hotspot, is_icon, width, height, flags, frame );
        HeapFree( GetProcessHeap(), 0, bmi_png );
        return ret;
    }

    if (maxsize < sizeof(BITMAPCOREHEADER))
    {
        WARN( "invalid size %lu\n", maxsize );
        return FALSE;
    }
    if (maxsize < bmi->bmiHeader.biSize)
    {
        WARN( "invalid header size %lu\n", bmi->bmiHeader.biSize );
        return FALSE;
    }
    if ( (bmi->bmiHeader.biSize != sizeof(BITMAPCOREHEADER)) &&
         (bmi->bmiHeader.biSize != sizeof(BITMAPINFOHEADER)  ||
         (bmi->bmiHeader.biCompression != BI_RGB &&
          bmi->bmiHeader.biCompression != BI_BITFIELDS)) )
    {
        WARN( "invalid bitmap header %lu\n", bmi->bmiHeader.biSize );
        return FALSE;
    }

    size = bitmap_info_size( bmi, DIB_RGB_COLORS );
    DIB_GetBitmapInfo(&bmi->bmiHeader, &bmi_width, &bmi_height, &bpp, &compr);
    color_size = get_dib_image_size( bmi_width, bmi_height / 2,
                                     bpp );
    mask_size = get_dib_image_size( bmi_width, bmi_height / 2, 1 );
    if (size > maxsize || color_size > maxsize - size)
    {
        WARN( "truncated file %lu < %lu+%lu+%lu\n", maxsize, size, color_size, mask_size );
        return 0;
    }
    if (mask_size > maxsize - size - color_size) mask_size = 0;  /* no mask */

    if (flags & LR_DEFAULTSIZE)
    {
        if (!width) width = GetSystemMetrics( is_icon ? SM_CXICON : SM_CXCURSOR );
        if (!height) height = GetSystemMetrics( is_icon ? SM_CYICON : SM_CYCURSOR );
    }
    else
    {
        if (!width) width = bmi_width;
        if (!height) height = bmi_height/2;
    }
    do_stretch = (bmi_height/2 != height) || (bmi_width != width);

    /* Scale the hotspot */
    if (is_icon)
    {
        hotspot.x = width / 2;
        hotspot.y = height / 2;
    }
    else if (do_stretch)
    {
        hotspot.x = (hotspot.x * width) / bmi_width;
        hotspot.y = (hotspot.y * height) / (bmi_height / 2);
    }

    if (!(bmi_copy = HeapAlloc( GetProcessHeap(), 0, max( size, FIELD_OFFSET( BITMAPINFO, bmiColors[2] )))))
        return 0;
    if (!(hdc = CreateCompatibleDC( 0 ))) goto done;

    memcpy( bmi_copy, bmi, size );
    if (bmi_copy->bmiHeader.biSize != sizeof(BITMAPCOREHEADER))
        bmi_copy->bmiHeader.biHeight /= 2;
    else
        ((BITMAPCOREINFO *)bmi_copy)->bmciHeader.bcHeight /= 2;
    bmi_height /= 2;

    color_bits = (const char*)bmi + size;
    mask_bits = (const char*)color_bits + color_size;

    if (is_dib_monochrome( bmi ))
    {
        if (!(frame->mask = CreateBitmap( width, height * 2, 1, 1, NULL ))) goto done;

        /* copy color data into second half of mask bitmap */
        SelectObject( hdc, frame->mask );
        StretchDIBits( hdc, 0, height, width, height,
                       0, 0, bmi_width, bmi_height,
                       color_bits, bmi_copy, DIB_RGB_COLORS, SRCCOPY );
    }
    else
    {
        if (!(frame->mask = CreateBitmap( width, height, 1, 1, NULL ))) goto done;
        if (!(frame->color = create_color_bitmap( width, height ))) goto done;
        SelectObject( hdc, frame->color );
        StretchDIBits( hdc, 0, 0, width, height,
                       0, 0, bmi_width, bmi_height,
                       color_bits, bmi_copy, DIB_RGB_COLORS, SRCCOPY );

        if (bmi_has_alpha( bmi_copy, color_bits ))
        {
            frame->alpha = create_alpha_bitmap( frame->color, bmi_copy, color_bits );
            if (!mask_size)  /* generate mask from alpha */
            {
                LONG x, y, dst_stride = ((bmi_width + 31) / 8) & ~3;

                if ((alpha_mask_bits = heap_calloc( bmi_height, dst_stride )))
                {
                    static const unsigned char masks[] = { 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1 };
                    const DWORD *src = color_bits;
                    unsigned char *dst = alpha_mask_bits;

                    for (y = 0; y < bmi_height; y++, src += bmi_width, dst += dst_stride)
                        for (x = 0; x < bmi_width; x++)
                            if (src[x] >> 24 != 0xff) dst[x >> 3] |= masks[x & 7];

                    mask_bits = alpha_mask_bits;
                    mask_size = bmi_height * dst_stride;
                }
            }
        }

        /* convert info to monochrome to copy the mask */
        if (bmi_copy->bmiHeader.biSize != sizeof(BITMAPCOREHEADER))
        {
            RGBQUAD *rgb = bmi_copy->bmiColors;

            bmi_copy->bmiHeader.biBitCount = 1;
            bmi_copy->bmiHeader.biClrUsed = bmi_copy->bmiHeader.biClrImportant = 2;
            rgb[0].rgbBlue = rgb[0].rgbGreen = rgb[0].rgbRed = 0x00;
            rgb[1].rgbBlue = rgb[1].rgbGreen = rgb[1].rgbRed = 0xff;
            rgb[0].rgbReserved = rgb[1].rgbReserved = 0;
        }
        else
        {
            RGBTRIPLE *rgb = (RGBTRIPLE *)(((BITMAPCOREHEADER *)bmi_copy) + 1);

            ((BITMAPCOREINFO *)bmi_copy)->bmciHeader.bcBitCount = 1;
            rgb[0].rgbtBlue = rgb[0].rgbtGreen = rgb[0].rgbtRed = 0x00;
            rgb[1].rgbtBlue = rgb[1].rgbtGreen = rgb[1].rgbtRed = 0xff;
        }
    }

    if (mask_size)
    {
        SelectObject( hdc, frame->mask );
        StretchDIBits( hdc, 0, 0, width, height,
                       0, 0, bmi_width, bmi_height,
                       mask_bits, bmi_copy, DIB_RGB_COLORS, SRCCOPY );
    }

    frame->width   = width;
    frame->height  = height;
    frame->hotspot = hotspot;
    ret = TRUE;

done:
    if (!ret) free_icon_frame( frame );
    DeleteDC( hdc );
    HeapFree( GetProcessHeap(), 0, bmi_copy );
    HeapFree( GetProcessHeap(), 0, alpha_mask_bits );
    return ret;
}

static HICON create_cursoricon_object(const struct cursoricon_desc *desc, BOOL is_icon)
{
    ICONINFO info;
    HICON hicon;
    const struct cursoricon_frame *frame;

    /* Sanidade: exige pelo menos 1 frame */
    if (!desc || desc->num_frames == 0 || !desc->frames)
        return NULL;

    /* Usa o primeiro frame */
    frame = &desc->frames[0];

    info.fIcon    = is_icon;
    info.xHotspot = frame->hotspot.x;
    info.yHotspot = frame->hotspot.y;
    info.hbmMask  = frame->mask;
    info.hbmColor = frame->color;

    /* Fallback: usa alpha como color, se necessário */
    if (!info.hbmColor && frame->alpha)
    {
        info.hbmColor = frame->alpha;
    }

    hicon = CreateIconIndirect(&info);
    return hicon;
}

/***********************************************************************
 *          create_icon_from_bmi
 *
 * Create an icon from its BITMAPINFO.
 */
static HICON create_icon_from_bmi( const BITMAPINFO *bmi, DWORD maxsize, HMODULE module, LPCWSTR resname,
                                   HRSRC rsrc, POINT hotspot, BOOL bIcon, INT width, INT height,
                                   UINT flags )
{
    struct cursoricon_frame frame;
    struct cursoricon_desc desc;
    HICON ret;
	
	desc.flags = flags;
	desc.frames = &frame;

    if (!create_icon_frame( bmi, maxsize, hotspot, bIcon, width, height, flags, &frame )) return 0;

    ret = create_cursoricon_object( &desc, bIcon/*, module, resname, rsrc*/ );
    if (!ret) free_icon_frame( &frame );
    return ret;
}

static HICON CURSORICON_LoadFromFile( LPCWSTR filename,
                             INT width, INT height, INT depth,
                             BOOL fCursor, UINT loadflags)
{
    const CURSORICONFILEDIRENTRY *entry;
    const CURSORICONFILEDIR *dir;
    DWORD filesize = 0;
    HICON hIcon = 0;
    const BYTE *bits;
    POINT hotspot;

    TRACE("CURSORICON_LoadFromFile %s\n", debugstr_w( filename ));

    bits = map_fileW( filename, &filesize );
    if (!bits){
		DbgPrint("CURSORICON_LoadFromFile::map_fileW falhou\n");
		return hIcon;
	}

    // /* Check for .ani. */
    // if (memcmp( bits, "RIFF", 4 ) == 0)
    // {
        // hIcon = CURSORICON_CreateIconFromANI( bits, filesize, width, height, depth, !fCursor, loadflags );
        // goto end;
    // }
	

    dir = (const CURSORICONFILEDIR*) bits;
    if ( filesize < FIELD_OFFSET( CURSORICONFILEDIR, idEntries[dir->idCount] )){
        DbgPrint("CURSORICON_LoadFromFile::filesize < FIELD_OFFSET( CURSORICONFILEDIR, idEntries[dir->idCount] falhando\n");
		goto end;
	}

    if ( fCursor ){
		DbgPrint("CURSORICON_LoadFromFile::!calling CURSORICON_FindBestCursorFile\n");
        entry = CURSORICON_FindBestCursorFile( dir, filesize, width, height, depth, loadflags );
    }else{
		DbgPrint("CURSORICON_LoadFromFile::!calling CURSORICON_FindBestIconFile\n");
        entry = CURSORICON_FindBestIconFile( dir, filesize, width, height, depth, loadflags );
	}

    if ( !entry )
	{
		DbgPrint("CURSORICON_LoadFromFile::!entry\n");
        goto end;
	}

    /* check that we don't run off the end of the file */
    if ( entry->dwDIBOffset > filesize ){
        DbgPrint("CURSORICON_LoadFromFile::entry->dwDIBOffset > filesize\n");
		goto end;
	}
    if ( entry->dwDIBOffset + entry->dwDIBSize > filesize )
	{
		DbgPrint("CURSORICON_LoadFromFile::entry->dwDIBOffset + entry->dwDIBSize > filesize \n");
        goto end;
	}

    hotspot.x = entry->xHotspot;
    hotspot.y = entry->yHotspot;
	DbgPrint("Calling create_icon_from_bmi\n");
    hIcon = create_icon_from_bmi( (const BITMAPINFO *)&bits[entry->dwDIBOffset], filesize - entry->dwDIBOffset,
                                  NULL, NULL, NULL, hotspot, !fCursor, width, height, loadflags );
end:
    TRACE("loaded %s -> %p\n", debugstr_w( filename ), hIcon );
    UnmapViewOfFile( bits );
    return hIcon;
}

/**********************************************************************
 *          CURSORICON_Load
 *
 * Load a cursor or icon from resource or file.
 */
static HICON CURSORICON_Load(HINSTANCE hInstance, LPCWSTR name,
                             INT width, INT height, INT depth,
                             BOOL fCursor, UINT loadflags)
{
    HANDLE handle = 0;
    HICON hIcon = 0;
    HRSRC hRsrc;
    DWORD size;
    const CURSORICONDIR *dir;
    const CURSORICONDIRENTRY *dirEntry;
    const BYTE *bits;
    WORD wResId;
    POINT hotspot;

    TRACE("%p, %s, %dx%d, depth %d, fCursor %d, flags 0x%04x\n",
          hInstance, debugstr_w(name), width, height, depth, fCursor, loadflags);

    if ( loadflags & LR_LOADFROMFILE ) {   /* Load from file */
        DbgPrint("CURSORICON_Load:: calling CURSORICON_LoadFromFile\n");
		return CURSORICON_LoadFromFile( name, width, height, depth, fCursor, loadflags );
	}

    //if (!hInstance) hInstance = user32_module;  /* Load OEM cursor/icon */

    /* don't cache 16-bit instances (FIXME: should never get 16-bit instances in the first place) */
    if ((ULONG_PTR)hInstance >> 16 == 0) loadflags &= ~LR_SHARED;

    /* Get directory resource ID */

    if (!(hRsrc = FindResourceW( hInstance, name,
                                 (LPWSTR)(fCursor ? RT_GROUP_CURSOR : RT_GROUP_ICON) )))
    {
		DbgPrint("CURSORICON_Load::FindResourceW para RT_GROUP_CURSOR : RT_GROUP_ICON falhou\n");
        // /* try animated resource */
        // if (!(hRsrc = FindResourceW( hInstance, name,
                                    // (LPWSTR)(fCursor ? RT_ANICURSOR : RT_ANIICON) ))) return 0;
        // if (!(handle = LoadResource( hInstance, hRsrc ))) return 0;
        // bits = LockResource( handle );
        // return CURSORICON_CreateIconFromANI( bits, SizeofResource( hInstance, handle ),
                                             // width, height, depth, !fCursor, loadflags );
    }

    /* Find the best entry in the directory */

    if (!(handle = LoadResource( hInstance, hRsrc ))) return 0;
    if (!(dir = LockResource( handle ))) return 0;
    size = SizeofResource( hInstance, hRsrc );
    if (fCursor)
        dirEntry = CURSORICON_FindBestCursorRes( dir, size, width, height, depth, loadflags );
    else
        dirEntry = CURSORICON_FindBestIconRes( dir, size, width, height, depth, loadflags );
    if (!dirEntry) return 0;
    wResId = dirEntry->wResId;
    FreeResource( handle );

    /* Load the resource */

    if (!(hRsrc = FindResourceW(hInstance,MAKEINTRESOURCEW(wResId),
                                (LPWSTR)(fCursor ? RT_CURSOR : RT_ICON) ))) return 0;

    // /* If shared icon, check whether it was already loaded */
    // if (loadflags & LR_SHARED)
    // {
        // WCHAR module_buf[MAX_PATH];
        // UNICODE_STRING module_str, res_str;

        // res_str.Length = 0;
        // res_str.Buffer = MAKEINTRESOURCEW(wResId);
        // module_str.Buffer = module_buf;
        // module_str.MaximumLength = sizeof(module_buf);
        // if (!LdrGetDllFullName( hInstance, &module_str ) &&
            // (hIcon = NtUserFindExistingCursorIcon( &module_str, &res_str, hRsrc )))
            // return hIcon;
    // }

    if (!(handle = LoadResource( hInstance, hRsrc ))) return 0;
    size = SizeofResource( hInstance, hRsrc );
    bits = LockResource( handle );

    if (!fCursor)
    {
        hotspot.x = width / 2;
        hotspot.y = height / 2;
    }
    else /* get the hotspot */
    {
        const SHORT *pt = (const SHORT *)bits;
        hotspot.x = pt[0];
        hotspot.y = pt[1];
        bits += 2 * sizeof(SHORT);
        size -= 2 * sizeof(SHORT);
    }
    hIcon = create_icon_from_bmi( (const BITMAPINFO *)bits, size, hInstance, name, hRsrc,
                                  hotspot, !fCursor, width, height, loadflags );
    FreeResource( handle );
    return hIcon;
}

#define ICO_PNG_SIGNATURE "\x89PNG\r\n\x1a\n"
#define ICO_HEADER_SIZE 6
#define ICO_ENTRY_SIZE 16

static HICON CreateIconFromPngBits(BYTE *data, DWORD size)
{
    png_structp png_ptr;
    png_infop info_ptr;
    struct mem_io_struct
    {
        BYTE *data;
        DWORD size;
        DWORD offset;
    } png_io;

    int width, height;
    png_bytep *row_pointers;
    int i;
    BITMAPV5HEADER bi;
    void *dibBits;
    HDC hDC;
    HBITMAP hBitmap;
    HBITMAP hMask;
    ICONINFO ii;
    HICON hIcon;

    png_ptr = NULL;
    info_ptr = NULL;
    row_pointers = NULL;
    dibBits = NULL;
    hBitmap = NULL;
    hMask = NULL;
    hIcon = NULL;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        DbgPrint("[PNG] Falha ao criar png_struct.\n");
        return NULL;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        DbgPrint("[PNG] Falha ao criar info_struct.\n");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    png_io.data = data;
    png_io.size = size;
    png_io.offset = 0;

    png_set_read_fn(png_ptr, &png_io, user_read_data);
    png_read_info(png_ptr, info_ptr);

    width = (int)png_get_image_width(png_ptr, info_ptr);
    height = (int)png_get_image_height(png_ptr, info_ptr);

    png_set_expand(png_ptr);
    png_set_bgr(png_ptr);
    png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
    png_read_update_info(png_ptr, info_ptr);

    row_pointers = (png_bytep *)HeapAlloc(GetProcessHeap(), 0, sizeof(png_bytep) * height);
    if (!row_pointers)
    {
        DbgPrint("[PNG] Falha ao alocar row_pointers.\n");
        goto done;
    }

    for (i = 0; i < height; i++)
    {
        row_pointers[i] = (png_bytep)HeapAlloc(GetProcessHeap(), 0, width * 4);
        if (!row_pointers[i])
        {
            DbgPrint("[PNG] Falha ao alocar linha do PNG.\n");
            goto done;
        }
    }

    png_read_image(png_ptr, row_pointers);

    ZeroMemory(&bi, sizeof(bi));
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = width;
    bi.bV5Height = -height;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask   = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask  = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    hDC = GetDC(NULL);
    hBitmap = CreateDIBSection(hDC, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &dibBits, NULL, 0);
    ReleaseDC(NULL, hDC);

    if (!hBitmap || !dibBits)
    {
        DbgPrint("[PNG] Falha ao criar DIBSection.\n");
        goto done;
    }

    for (i = 0; i < height; i++)
    {
        memcpy((BYTE *)dibBits + i * width * 4, row_pointers[i], width * 4);
    }

    hMask = CreateBitmap(width, height, 1, 1, NULL);
    if (!hMask)
    {
        DbgPrint("[PNG] Falha ao criar máscara.\n");
        goto done;
    }

    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask = hMask;
    ii.hbmColor = hBitmap;

    hIcon = CreateIconIndirect(&ii);
    if (!hIcon)
    {
        DbgPrint("[PNG] CreateIconIndirect falhou.\n");
    }

done:
    if (row_pointers)
    {
        for (i = 0; i < height; i++)
        {
            if (row_pointers[i])
                HeapFree(GetProcessHeap(), 0, row_pointers[i]);
        }
        HeapFree(GetProcessHeap(), 0, row_pointers);
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    if (hBitmap)
        DeleteObject(hBitmap);
    if (hMask)
        DeleteObject(hMask);

    return hIcon;
}

// Função hook simulando LoadImageW com suporte a .ico com PNG embutido
HANDLE LoadImagePngFromFile(
    HINSTANCE hinst,
    LPCWSTR lpszName,
    UINT cxDesired,
    UINT cyDesired,
    UINT fuLoad,
    BOOL bIcon)
{
    HANDLE hFile;
    HANDLE hMapping;
    DWORD fileSize;
    LPVOID fileData;
    HANDLE hIcon;
    BYTE *data;
    DWORD imageOffset;
    DWORD imageSize;
    BYTE *pngData;

    hIcon = NULL;

    // if (!(fuLoad & LR_LOADFROMFILE) || !bIcon)
    // {
        // DbgPrint("[LoadImageW Hook] Chamando LoadImageW original.\n");
        // return LoadImageW(hinst, lpszName, IMAGE_ICON, cxDesired, cyDesired, fuLoad);
    // }

    hFile = CreateFileW(lpszName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DbgPrint("[LoadImageW Hook] Falha ao abrir arquivo.\n");
        return NULL;
    }

    fileSize = GetFileSize(hFile, NULL);
    if (fileSize < ICO_HEADER_SIZE + ICO_ENTRY_SIZE)
    {
        DbgPrint("[LoadImageW Hook] Arquivo muito pequeno para ser um .ico válido.\n");
        CloseHandle(hFile);
        return NULL;
    }

    hMapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping)
    {
        DbgPrint("[LoadImageW Hook] Falha ao criar mapeamento de arquivo.\n");
        CloseHandle(hFile);
        return NULL;
    }

    fileData = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!fileData)
    {
        DbgPrint("[LoadImageW Hook] Falha ao mapear arquivo.\n");
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return NULL;
    }

    data = (BYTE *)fileData;
    imageOffset = *(DWORD *)(data + ICO_HEADER_SIZE + 12);
    if (imageOffset >= fileSize)
    {
        DbgPrint("[LoadImageW Hook] Offset inválido para imagem no arquivo .ico.\n");
        goto cleanup;
    }

    imageSize = fileSize - imageOffset;
    pngData = data + imageOffset;

    if (imageSize < 8 || memcmp(pngData, ICO_PNG_SIGNATURE, 8) != 0)
    {
        DbgPrint("[LoadImageW Hook] Imagem não contém assinatura PNG.\n");
        goto cleanup;
    }

    hIcon = CreateIconFromPngBits(pngData, imageSize);
    if (!hIcon)
    {
        DbgPrint("[LoadImageW Hook] Falha ao criar ícone a partir de PNG.\n");
    }

cleanup:
    UnmapViewOfFile(fileData);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    return hIcon;
}

int WINAPI LookupIconIdFromDirectoryExHook(
  _In_  PBYTE presbits,
  _In_  BOOL fIcon,
  _In_  int cxDesired,
  _In_  int cyDesired,
  _In_  UINT Flags
)
{
    WORD bppDesired;
    CURSORICONDIR* dir = (CURSORICONDIR*)presbits;
    CURSORICONDIRENTRY* entry;
    int i, numMatch = 0, iIndex = -1;
    WORD width, height, BitCount = 0;
    BOOL notPaletted = FALSE;
    ULONG bestScore = 0xFFFFFFFF, score;

    TRACE("%p, %x, %i, %i, %x.\n", presbits, fIcon, cxDesired, cyDesired, Flags);

    if(!(dir && !dir->idReserved && (dir->idType & 3)))
    {
        WARN("Invalid resource.\n");
        return 0;
    }

    if(Flags & LR_MONOCHROME)
        bppDesired = 1;
    else
    {
        HDC icScreen;
        icScreen = CreateICW(DISPLAYW, NULL, NULL, NULL);
        if(!icScreen)
            return FALSE;

        bppDesired = GetDeviceCaps(icScreen, BITSPIXEL);
        DeleteDC(icScreen);
    }

    if(!cxDesired)
        cxDesired = Flags & LR_DEFAULTSIZE ? GetSystemMetrics(fIcon ? SM_CXICON : SM_CXCURSOR) : 256;
    if(!cyDesired)
        cyDesired = Flags & LR_DEFAULTSIZE ? GetSystemMetrics(fIcon ? SM_CYICON : SM_CYCURSOR) : 256;

    /* Find the best match for the desired size */
    for(i = 0; i < dir->idCount; i++)
    {
        entry = &dir->idEntries[i];
        width = fIcon ? entry->ResInfo.icon.bWidth : entry->ResInfo.cursor.wWidth;
        /* Height is twice as big in cursor resources */
        height = fIcon ? entry->ResInfo.icon.bHeight : entry->ResInfo.cursor.wHeight/2;
        /* 0 represents 256 */
        if(!width) width = 256;
        if(!height) height = 256;
        /* Calculate the "score" (lower is better) */
        score = 2*(abs(width - cxDesired) + abs(height - cyDesired));
        if( score > bestScore)
            continue;
        /* Bigger than requested lowers the score */
        if(width > cxDesired)
            score -= width - cxDesired;
        if(height > cyDesired)
            score -= height - cyDesired;
        if(score > bestScore)
            continue;
        if(score == bestScore)
        {
            if(entry->wBitCount > BitCount)
                BitCount = entry->wBitCount;
            numMatch++;
            continue;
        }
        iIndex = i;
        numMatch = 1;
        bestScore = score;
        BitCount = entry->wBitCount;
    }

    if(numMatch == 1)
    {
        /* Only one entry fits the asked dimensions */
        return dir->idEntries[iIndex].wResId;
    }

    /* Avoid paletted icons on non-paletted device */
    if (bppDesired > 8 && BitCount > 8)
        notPaletted = TRUE;

    BitCount = 0;
    iIndex = -1;
    /* Now find the entry with the best depth */
    for(i = 0; i < dir->idCount; i++)
    {
        entry = &dir->idEntries[i];
        width = fIcon ? entry->ResInfo.icon.bWidth : entry->ResInfo.cursor.wWidth;
        height = fIcon ? entry->ResInfo.icon.bHeight : entry->ResInfo.cursor.wHeight/2;
        /* 0 represents 256 */
        if(!width) width = 256;
        if(!height) height = 256;
        /* Check if this is the best match we had */
        score = 2*(abs(width - cxDesired) + abs(height - cyDesired));
        if(width > cxDesired)
            score -= width - cxDesired;
        if(height > cyDesired)
            score -= height - cyDesired;
        if(score != bestScore)
            continue;
        /* Exact match? */
        if(entry->wBitCount == bppDesired)
            return entry->wResId;
        /* We take the highest possible but smaller  than the display depth */
        if((entry->wBitCount > BitCount) && (entry->wBitCount < bppDesired))
        {
            /* Avoid paletted icons on non paletted devices */
            if ((entry->wBitCount <= 8) && notPaletted)
                continue;
            iIndex = i;
            BitCount = entry->wBitCount;
        }
    }

    if(iIndex >= 0)
        return dir->idEntries[iIndex].wResId;

    /* No inferior or equal depth available. Get the smallest bigger one */
    BitCount = 0xFFFF;
    iIndex = -1;
    for(i = 0; i < dir->idCount; i++)
    {
        entry = &dir->idEntries[i];
        width = fIcon ? entry->ResInfo.icon.bWidth : entry->ResInfo.cursor.wWidth;
        height = fIcon ? entry->ResInfo.icon.bHeight : entry->ResInfo.cursor.wHeight/2;
        /* 0 represents 256 */
        if(!width) width = 256;
        if(!height) height = 256;
        /* Check if this is the best match we had */
        score = 2*(abs(width - cxDesired) + abs(height - cyDesired));
        if(width > cxDesired)
            score -= width - cxDesired;
        if(height > cyDesired)
            score -= height - cyDesired;
        if(score != bestScore)
            continue;
        /* Check the bit depth */
        if(entry->wBitCount < BitCount)
        {
            if((entry->wBitCount <= 8) && notPaletted)
                continue;
            iIndex = i;
            BitCount = entry->wBitCount;
        }
    }
    if (iIndex >= 0)
        return dir->idEntries[iIndex].wResId;

    return 0;
}

// HANDLE LoadImagePngFromResource(
    // HINSTANCE hinst,
    // LPCWSTR lpszName,
    // UINT cxDesired,
    // UINT cyDesired,
    // UINT fuLoad,
	// BOOL bIcon,
    // LPWSTR rt)
// {
    // //HRSRC hRes;
    // //HGLOBAL hGlobal;
    // LPBYTE pResData;
    // DWORD resSize;
    // DWORD imageOffset, imageSize;
    // BYTE *pngData;
    // HANDLE hIcon;
	// LPWSTR  lpszGroupType;
	// //WORD wResId;
	// //CURSORICONDIR* dir;
	// //HANDLE handle;
	// NTSTATUS status;
	// LDR_RESOURCE_INFO info;
	// PIMAGE_RESOURCE_DATA_ENTRY entry;
	
	// DbgPrint("LoadImagePngFromResource called\n");
	
	// lpszGroupType = RT_GROUP_CURSOR + (rt - RT_CURSOR);

    // // //hRes = FindResourceW(hinst, lpszName, (LPCWSTR)RT_ICON);
    // // if (hRes = FindResourceW(hinst, lpszName, (LPCWSTR)lpszGroupType)) {
				
		// // DbgPrint("LoadImagePngFromResource has RT_GROUP_CURSOR or RT_CURSOR\n");		

        // // /*
         // // * Load the directory resource.
         // // */
        // // hGlobal = LoadResource(hinst, hRes);

        // // /*
         // // * Now load and lock that resource.
         // // */
        // // if ((pResData = (LPBYTE)LockResource(hGlobal)) == NULL){
            // // DbgPrint("LoadImagePngFromResource pResData is NULL\n");
			// // return 0;
		// // }

        // // /*
         // // * Find the id of the icon that best fits the display characteristics
         // // * of this display.
         // // */
        // // // lpszName = MAKEINTRESOURCE(RtlGetIdFromDirectory(pResData, (UINT)rt,
                // // // pdi, &dwSize));
				
		// // lpszName = MAKEINTRESOURCE(LookupIconIdFromDirectoryEx((PBYTE)pResData, bIcon, cxDesired, cyDesired, fuLoad));
		// // FreeResource(hGlobal);				

        // // //UNLOCKRESOURCE(h, hmod);
    // // }

    // // //hRes = FindResourceW(hinst, lpszName, rt);
    // // hRes = FindResourceW(
        // // hinst,
        // // MAKEINTRESOURCEW(wResId),
        // // bIcon ? RT_ICON : RT_CURSOR);	
   // /* Find resource ID */
    // // hRes = FindResourceW(
        // // hinst,
        // // lpszName,
        // // bIcon ? RT_GROUP_ICON : RT_GROUP_CURSOR);
		
	// // info.Type     = (ULONG_PTR)RT_GROUP_ICON;           // ou MAKEINTRESOURCEW(3)
	// // info.Name     = 1;                             // ID do recurso
	// // info.Language = 0;                             // geralmente 0 ou 1033

	// // status = LdrFindResource_U(hinst, &info, 3, &entry);	

	// // if(!NT_SUCCESS(status)){
		// // DbgPrint("[LoadImageW Hook] Recurso RT_GROUP_ICON ou RT_GROUP_CURSOR nao encontrado.\n");
	// // }
	
	// // pResData = (BYTE *)hinst + entry->OffsetToData;
	// //resSize = entry->Size;
		

    // // /* We let FindResource, LoadResource, etc. call SetLastError */
    // // if(!hRes){
        // // DbgPrint("[LoadImageW Hook] Recurso RT_ICON não encontrado.\n");
	// // }

    // // handle = LoadResource(hinst, hRes);
    // // if(!handle){
        // // DbgPrint("[LoadImageW Hook] LoadResource falhou para RT_GROUP_ICON : RT_GROUP_CURSOR\n");
	// // }

    // // dir = LockResource(handle);
    // // if(!dir){
        // // DbgPrint("[LoadImageW Hook] LockResource falhou para RT_GROUP_ICON : RT_GROUP_CURSOR\n");
	// // }

    // // wResId = LookupIconIdFromDirectoryExHook((PBYTE)dir, bIcon, cxDesired, cyDesired, fuLoad);
    // // FreeResource(handle);
	
	// info.Type     = (ULONG_PTR)RT_ICON;           // ou MAKEINTRESOURCEW(3)
	// info.Name     = 1;                             // ID do recurso
	// info.Language = 0;                             // geralmente 0 ou 1033

	// status = LdrFindResource_U(hinst, &info, 3, &entry);	

	// if(!NT_SUCCESS(status)){
		// DbgPrint("[LoadImageW Hook] Recurso RT_ICON ou RT_CURSOR nao encontrado.\n");
	// }
	
	// // pResData = (BYTE *)hinst + entry->OffsetToData;
	// // resSize = entry->Size;
	
    // status = LdrAccessResource(hinst, entry, (PVOID)pResData, &resSize);
	// if(!NT_SUCCESS(status)){
		// DbgPrint("[LoadImageW Hook] LdrAccessResource falhou.\n");
	// }

    // /* Get the relevant resource pointer */
    // // hRes = FindResourceW(
        // // hinst,
        // // MAKEINTRESOURCEW(wResId),
        // // bIcon ? RT_ICON : RT_CURSOR);
		
    // // if (!hRes)
    // // {
        // // DbgPrint("[LoadImageW Hook] Recurso RT_ICON nao encontrado.\n");
        // // return NULL;
    // // }

    // // hGlobal = LoadResource(hinst, hRes);
    // // if (!hGlobal)
    // // {
        // // DbgPrint("[LoadImageW Hook] LoadResource falhou.\n");
        // // return NULL;
    // // }

    // // pResData = (LPBYTE)LockResource(hGlobal);
    // // resSize = SizeofResource(hinst, hRes);
    // // if (!pResData || resSize < 22)
    // // {
        // // DbgPrint("[LoadImageW Hook] Dados do recurso inválidos.\n");
        // // return NULL;
    // // }

    // // Verifica se há assinatura PNG no início da imagem do ícone
    // // No formato .ico embutido, o dado já começa com a imagem (sem header de ícone)
    // if (memcmp(pResData, "\x89PNG\r\n\x1a\n", 8) != 0)
    // {
        // DbgPrint("[LoadImageW Hook] Recurso RT_ICON não contém imagem PNG.\n");
        // return LoadImageW(hinst, lpszName, IMAGE_ICON, cxDesired, cyDesired, fuLoad);
    // }

    // imageOffset = 0;
    // imageSize = resSize;
    // pngData = pResData;

    // hIcon = CreateIconFromPngBits(pngData, imageSize);
    // if (!hIcon)
    // {
        // DbgPrint("[LoadImageW Hook] Falha ao criar ícone PNG do recurso.\n");
    // }

    // return hIcon;
// }

HANDLE LoadImagePngFromResource(
    HINSTANCE hinst,
    LPCWSTR lpszName,
    UINT cxDesired,
    UINT cyDesired,
    UINT fuLoad,
    BOOL bIcon)
{
    HMODULE modBase;
    LDR_RESOURCE_INFO info;
    PIMAGE_RESOURCE_DATA_ENTRY entry;
    PVOID grpData, iconData;
    ULONG grpSize, iconSize;
    BYTE *pngData;
    HANDLE hIcon;
    NTSTATUS status;
	INT iconId;

    modBase = hinst ? hinst : GetModuleHandleW(NULL);

    /* Etapa 1: Localiza RT_GROUP_ICON */
    info.Type = (ULONG_PTR)RT_GROUP_ICON;
    info.Name = (ULONG_PTR)lpszName;
    info.Language = 0;

    entry = NULL;
    status = LdrFindResource_U(modBase, &info, 3, &entry);
    if (status < 0 || !entry)
    {
        OutputDebugStringA("[PNG] RT_GROUP_ICON não encontrado.\n");
        //return NULL;
    }

    grpData = NULL;
    grpSize = 0;
    status = LdrAccessResource(modBase, entry, &grpData, &grpSize);
    if (status < 0 || !grpData)
    {
        OutputDebugStringA("[PNG] Falha ao acessar dados de GRPICON.\n");
        //return NULL;
    }

    /* Etapa 2: Determina melhor ícone (RT_ICON) */
    iconId = LookupIconIdFromDirectoryEx((PBYTE)grpData, TRUE, cxDesired, cyDesired, LR_DEFAULTCOLOR);
    if (iconId == 0)
    {
        OutputDebugStringA("[PNG] LookupIconIdFromDirectoryEx falhou.\n");
        return NULL;
    }

    /* Etapa 3: Localiza RT_ICON com ID retornado */
    info.Type = (ULONG_PTR)RT_ICON;
    info.Name = (ULONG_PTR)(ULONG)iconId;
    info.Language = 0;

    entry = NULL;
    status = LdrFindResource_U(modBase, &info, 3, &entry);
    if (status < 0 || !entry)
    {
        OutputDebugStringA("[PNG] RT_ICON (PNG?) não encontrado.\n");
        return NULL;
    }

    iconData = NULL;
    iconSize = 0;
    status = LdrAccessResource(modBase, entry, &iconData, &iconSize);
    if (status < 0 || !iconData || iconSize < 8)
    {
        OutputDebugStringA("[PNG] Falha ao acessar RT_ICON.\n");
        return NULL;
    }

    pngData = (BYTE *)iconData;
    if (memcmp(pngData, "\x89PNG\r\n\x1a\n", 8) != 0)
    {
        OutputDebugStringA("[Hook] Recurso RT_ICON não é PNG.\n");
        return LoadImageW(hinst, lpszName, IMAGE_ICON, cxDesired, cyDesired, fuLoad);
    }

    hIcon = CreateIconFromPngBits(pngData, iconSize);
    if (!hIcon)
    {
        OutputDebugStringA("[Hook] CreateIconFromPngBits falhou.\n");
    }

    return hIcon;
}


HANDLE WINAPI LoadImageWHook( HINSTANCE hinst, LPCWSTR lpszName, UINT uType,
                INT cxDesired, INT cyDesired, UINT fuLoad )
{
	HICON hIcon = 0;

    switch (uType) {
    case IMAGE_BITMAP:
        return LoadImageW( hinst, lpszName, uType, cxDesired, cyDesired, fuLoad );

    case IMAGE_ICON:
    case IMAGE_CURSOR:
		hIcon = LoadImageW( hinst, lpszName, uType, cxDesired, cyDesired, fuLoad );
		
		if(!hIcon){
			DbgPrint("LoadImageWHook::fuLoad is %d\n", fuLoad);
			if (fuLoad & LR_LOADFROMFILE){
				hIcon = LoadImagePngFromFile(hinst, lpszName, uType, cxDesired, cyDesired, fuLoad);
			}else{
				hIcon = LoadImagePngFromResource(hinst, lpszName, cxDesired, cyDesired, fuLoad, TRUE);
			}
			// if(hIcon){
				// return hIcon;
			// }else{				
				//hIcon =  LoadImagePngFromResource(hinst, lpszName, cxDesired, cyDesired, fuLoad, TRUE, RT_ICON);
			if(hIcon){
				return hIcon;	
			}
			
			return NULL;
			//}			
		}
		
		return hIcon;
    }
    return 0;
}

HANDLE WINAPI LoadImageAHook(
  _In_opt_  HINSTANCE hinst,
  _In_      LPCSTR lpszName,
  _In_      UINT uType,
  _In_      int cxDesired,
  _In_      int cyDesired,
  _In_      UINT fuLoad
)
{
    HANDLE res;
    LPWSTR u_name;
    DWORD len;

    if (IS_INTRESOURCE(lpszName))
        return LoadImageWHook(hinst, (LPCWSTR)lpszName, uType, cxDesired, cyDesired, fuLoad);

    len = MultiByteToWideChar( CP_ACP, 0, lpszName, -1, NULL, 0 );
    u_name = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );
    MultiByteToWideChar( CP_ACP, 0, lpszName, -1, u_name, len );

    res = LoadImageWHook(hinst, u_name, uType, cxDesired, cyDesired, fuLoad);
    HeapFree(GetProcessHeap(), 0, u_name);
    return res;
}

HICON WINAPI LoadIconAHook(
  _In_opt_  HINSTANCE hInstance,
  _In_      LPCSTR lpIconName
)
{
    TRACE("%p, %s\n", hInstance, debugstr_a(lpIconName));

    return LoadImageAHook(hInstance,
        lpIconName,
        IMAGE_ICON,
        0,
        0,
        LR_SHARED | LR_DEFAULTSIZE );
}

HICON WINAPI LoadIconWHook(
  _In_opt_  HINSTANCE hInstance,
  _In_      LPCWSTR lpIconName
)
{
    TRACE("%p, %s\n", hInstance, debugstr_w(lpIconName));

    return LoadImageWHook(hInstance,
        lpIconName,
        IMAGE_ICON,
        0,
        0,
        LR_SHARED | LR_DEFAULTSIZE );
}