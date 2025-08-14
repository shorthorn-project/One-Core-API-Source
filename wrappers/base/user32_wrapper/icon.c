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

#define ICO_PNG_SIGNATURE "\x89PNG\r\n\x1a\n"
#define ICO_HEADER_SIZE 6
#define ICO_ENTRY_SIZE 16

static const WCHAR DISPLAYW[] = L"DISPLAY";

#include <pshpack1.h>
typedef struct _CURSORICONFILEDIRENTRY
{
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    union
    {
        WORD wPlanes; /* For icons */
        WORD xHotspot; /* For cursors */
    };
    union
    {
        WORD wBitCount; /* For icons */
        WORD yHotspot; /* For cursors */
    };
    DWORD dwDIBSize;
    DWORD dwDIBOffset;
} CURSORICONFILEDIRENTRY;

typedef struct _CURSORICONFILEDIR
{
    WORD idReserved;
    WORD idType;
    WORD idCount;
    CURSORICONFILEDIRENTRY idEntries[1];
} CURSORICONFILEDIR;
#include <poppack.h>

/* libpng helpers */
typedef struct _PNG_READER_STATE
{
    png_bytep buffer;
    size_t bufSize;
    size_t currentPos;
} PNG_READER_STATE;

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

static int get_display_bpp(void)
{
    HDC hdc = CreateDCW(DISPLAYW, NULL, NULL, NULL);
    int ret = GetDeviceCaps( hdc, BITSPIXEL );
    return ret;
}

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

HICON CreateIconFromPngBits(BYTE *data, DWORD size)
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
	
    if (!LibPngExists())
    {
        ERR("No libpng.dll\n");
        return NULL;
    }	

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

/* Função para redimensionar uma DIB Section */
static HBITMAP ResizeBitmap(HDC hdc, HBITMAP hBitmap, int newWidth, int newHeight)
{
    BITMAP bm;
    HDC hdcSrc, hdcDst;
    HBITMAP hbmResized;
    BITMAPV4HEADER bi;

    /* Obtém informações do bitmap original */
    if (!GetObjectW(hBitmap, sizeof(bm), &bm))
        return NULL;

    /* Configura BITMAPV4HEADER para RGBA top-down */
    ZeroMemory(&bi, sizeof(bi));
    bi.bV4Size          = sizeof(bi);
    bi.bV4Width         = newWidth;
    bi.bV4Height        = -newHeight; /* negativo = top-down */
    bi.bV4Planes        = 1;
    bi.bV4BitCount      = 32;
    bi.bV4V4Compression = BI_BITFIELDS;
    bi.bV4RedMask       = 0x00FF0000;
    bi.bV4GreenMask     = 0x0000FF00;
    bi.bV4BlueMask      = 0x000000FF;
    bi.bV4AlphaMask     = 0xFF000000;

    /* Cria DCs compatíveis */
    hdcSrc = CreateCompatibleDC(hdc);
    hdcDst = CreateCompatibleDC(hdc);

    /* Cria bitmap RGBA */
    hbmResized = CreateDIBSection(hdcDst, (BITMAPINFO*)&bi, DIB_RGB_COLORS, NULL, NULL, 0);

    if (!hbmResized)
    {
        DeleteDC(hdcSrc);
        DeleteDC(hdcDst);
        return NULL;
    }

    /* Seleciona bitmaps nos DCs */
    SelectObject(hdcSrc, hBitmap);
    SelectObject(hdcDst, hbmResized);

    /* Melhor qualidade de redimensionamento */
    //SetStretchBltMode(hdcDst, HALFTONE);
    SetBrushOrgEx(hdcDst, 0, 0, NULL);

    /* Redimensiona preservando alpha */
    StretchBlt(hdcDst, 0, 0, newWidth, newHeight,
               hdcSrc, 0, 0, bm.bmWidth, bm.bmHeight,
               SRCCOPY);

    /* Libera DCs temporários */
    DeleteDC(hdcSrc);
    DeleteDC(hdcDst);

    return hbmResized;
}

static void FixBGRAtoRGBA(BYTE *pixels, int width, int height)
{
    int x, y;
    DWORD *p = (DWORD*)pixels;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            DWORD px = p[y * width + x];
            p[y * width + x] = (px & 0xFF00FF00) | ((px & 0x000000FF) << 16) | ((px & 0x00FF0000) >> 16);
        }
    }
}

HICON CreateIconFromPngBitsEx(BYTE *data, DWORD size, int cxDesired, int cyDesired, BOOL fIcon)
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
	HBITMAP hResized;

    png_ptr = NULL;
    info_ptr = NULL;
    row_pointers = NULL;
    dibBits = NULL;
    hBitmap = NULL;
    hMask = NULL;
    hIcon = NULL;
	
    if (!LibPngExists())
    {
        ERR("No libpng.dll\n");
        return NULL;
    }	

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
	
    /* Use tamanhos padrão se cx/cy forem 0 */
    if (cxDesired == 0)
        cxDesired = GetSystemMetrics(fIcon ? SM_CXICON : SM_CXCURSOR);
    if (cyDesired == 0)
        cyDesired = GetSystemMetrics(fIcon ? SM_CYICON : SM_CYCURSOR);	

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
	
    /* Redimensionar */
    if (cxDesired == 16 || cyDesired == 16) {
        hResized = ResizeBitmap(GetDC(NULL), hBitmap, cxDesired, cyDesired);
        DeleteObject(hBitmap);
        hBitmap = hResized;
		hMask = CreateBitmap(cxDesired, cyDesired, 1, 1, NULL);
    }else{
		hMask = CreateBitmap(width, height, 1, 1, NULL);
	}
    
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

HANDLE LoadImagePngFromFile(
    HINSTANCE hinst,
    LPCWSTR lpFileName,
    UINT cxDesired,
    UINT cyDesired,
    UINT fuLoad,
    BOOL bIcon)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
    LPBYTE lpBase = NULL;
    HICON hIcon = NULL;
    ICONDIR* iconDir;
    ICONDIRENTRY* entries;
    DWORD i, bestIndex = 0;
    DWORD bestScore = 0;
    LPBYTE lpRes;
    DWORD dwSize;	

    hFile = CreateFileW(lpFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return NULL;

    hMapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping)
        goto cleanup;

    lpBase = (LPBYTE) MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!lpBase)
        goto cleanup;

    iconDir = (ICONDIR*)lpBase;
    if (iconDir->idReserved != 0 || iconDir->idType != 1 || iconDir->idCount == 0)
        goto cleanup;

    entries = (ICONDIRENTRY*)(lpBase + sizeof(ICONDIR));

    for (i = 0; i < iconDir->idCount; ++i)
    {
        DWORD width = entries[i].bWidth == 0 ? 256 : entries[i].bWidth;
        DWORD height = entries[i].bHeight == 0 ? 256 : entries[i].bHeight;
        DWORD depth = entries[i].wBitCount;

        DWORD score = width * height * depth;
        if (score > bestScore)
        {
            bestScore = score;
            bestIndex = i;
        }
    }

    // Pega o melhor recurso
    lpRes = lpBase + entries[bestIndex].dwImageOffset;
    dwSize = entries[bestIndex].dwBytesInRes;

    hIcon = CreateIconFromResourceExHook(lpRes, dwSize, TRUE, 0x00030000,
                                     cxDesired, cyDesired, LR_DEFAULTCOLOR);

cleanup:
    if (lpBase) UnmapViewOfFile(lpBase);
    if (hMapping) CloseHandle(hMapping);
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);

    return hIcon;
}

HICON LoadImagePngFromResource(HINSTANCE hInstance, LPCWSTR name,
                             INT width, INT height, INT depth,
                             BOOL fCursor, UINT loadflags)
{
    HANDLE handle = 0;
    HICON hIcon = 0;
    HRSRC hRsrc;
    DWORD size;
    const CURSORICONDIR *dir;
    const BYTE *bits;
	POINT hotspot;
	LPWSTR  lpszGroupType;
	LPWSTR  type;
	UINT idIcon;
	PBYTE lpBytes;
	
	if(fCursor){
		type = RT_CURSOR;
	}else{
		type = RT_ICON;
	}
	
    lpszGroupType = RT_GROUP_CURSOR + (type - RT_CURSOR);	

    /* Get directory resource ID */

    if (!(hRsrc = FindResourceW( hInstance, name,
                                 (LPCWSTR)lpszGroupType)))
    {
		DbgPrint("Cannot resolve icon/cursor group on exe/dll\n");
        /* try animated resource */
		return 0;
    }

    /* Find the best entry in the directory */
    if (!(handle = LoadResource( hInstance, hRsrc ))){ 
		DbgPrint("Cannot LoadResource icon/cursor group on exe/dll\n");
		return 0;
	}
    if (!(dir = LockResource( handle ))){ 
		DbgPrint("Cannot LockResource icon/cursor group on exe/dll\n");
		return 0;
	}
    size = SizeofResource( hInstance, hRsrc );
	if(!size){
		DbgPrint("Cannot SizeofResource icon/cursor group on exe/dll\n");
	}
	
    if (lpBytes = (PBYTE)LockResource(dir)) {

        idIcon = LookupIconIdFromDirectoryEx((PBYTE)lpBytes,
                                             (type == RT_ICON),
                                             width,
                                             height,
                                             loadflags);	
	}
							  
    if (!idIcon){ 
		DbgPrint("Cannot get idIcon\n");
		return 0;
	}

    FreeResource( handle );
    /* Load the resource */

    if (!(hRsrc = FindResourceW(hInstance,MAKEINTRESOURCEW(idIcon),
                                (LPWSTR)(fCursor ? RT_CURSOR : RT_ICON) ))){
		DbgPrint("FindResourceW for RT_ICON or RT_CURSOR failed\n");							
		return 0;
	}

    if (!(handle = LoadResource( hInstance, hRsrc ))){ 
		DbgPrint("LoadResource for RT_ICON or RT_CURSOR failed\n");
		return 0;
	}
    size = SizeofResource( hInstance, hRsrc );
	
	if(!size){
		DbgPrint("SizeofResource for RT_ICON or RT_CURSOR failed\n");
		return 0;
	}
	
    bits = LockResource( handle );
	
	if(!bits){
		DbgPrint("LockResource for RT_ICON or RT_CURSOR failed\n");
	}

    if (!fCursor)
    {
        hotspot.x = width / 2;
        hotspot.y = height / 2;
    }
    else /* get the hotspot */
    {
        bits += 2 * sizeof(SHORT);
        size -= 2 * sizeof(SHORT);
    }
	
	hIcon = CreateIconFromPngBits(bits, size);
    FreeResource( handle );
    return hIcon;
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

HICON WINAPI CreateIconFromResourceHook(
  _In_  PBYTE presbits,
  _In_  DWORD dwResSize,
  _In_  BOOL fIcon,
  _In_  DWORD dwVer
)
{
	HICON hIcon = NULL;
	
	if(memcmp(presbits, ICO_PNG_SIGNATURE, 8) == 0){
		hIcon = CreateIconFromPngBits(presbits, dwResSize);
	}else{
		hIcon = CreateIconFromResource(presbits, dwResSize, fIcon, dwVer);	
	}
	
	return hIcon;
}

HICON WINAPI CreateIconFromResourceExHook(
  _In_  PBYTE pbIconBits,
  _In_  DWORD cbIconBits,
  _In_  BOOL fIcon,
  _In_  DWORD dwVersion,
  _In_  int cxDesired,
  _In_  int cyDesired,
  _In_  UINT uFlags
)
{
	HICON hIcon = NULL;
	
	if(memcmp(pbIconBits, ICO_PNG_SIGNATURE, 8) == 0){
		hIcon = CreateIconFromPngBitsEx(pbIconBits, cbIconBits, cxDesired, cyDesired, fIcon);
	}else{
		hIcon = CreateIconFromResourceEx(pbIconBits, cbIconBits, fIcon, dwVersion, cxDesired, cyDesired, uFlags);
	}
	
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

HANDLE WINAPI LoadImageWHook( HINSTANCE hinst, LPCWSTR lpszName, UINT uType,
                INT cxDesired, INT cyDesired, UINT fuLoad )
{
	HICON hIcon = 0;
	int depth;

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
			}
			else{	
				depth = 1;
				if (!(fuLoad & LR_MONOCHROME)) depth = get_display_bpp();	

		
				hIcon = LoadImagePngFromResource(hinst, lpszName, cxDesired, cyDesired, depth, 
											(uType == IMAGE_CURSOR), fuLoad);
			}
			
			if(hIcon){
				return hIcon;	
			}
			
			return NULL;		
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

const CURSORICONFILEDIRENTRY*
get_best_icon_file_entry(
    _In_ const CURSORICONFILEDIR* dir,
    _In_ DWORD dwFileSize,
    _In_ int cxDesired,
    _In_ int cyDesired,
    _In_ BOOL bIcon,
    _In_ DWORD fuLoad
)
{
    CURSORICONDIR* fakeDir;
    CURSORICONDIRENTRY* fakeEntry;
    WORD i;
    const CURSORICONFILEDIRENTRY* entry;

    /* Check our file is what it claims to be */
    if ( dwFileSize < sizeof(*dir) )
        return NULL;

    if (dwFileSize < FIELD_OFFSET(CURSORICONFILEDIR, idEntries[dir->idCount]))
        return NULL;

    /*
     * Cute little hack:
     * We allocate a buffer, fake it as if it was a pointer to a resource in a module,
     * pass it to LookupIconIdFromDirectoryExHook and get back the index we have to use
     */
    fakeDir = HeapAlloc(GetProcessHeap(), 0, FIELD_OFFSET(CURSORICONDIR, idEntries[dir->idCount]));
    if(!fakeDir)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    fakeDir->idReserved = 0;
    fakeDir->idType = dir->idType;
    fakeDir->idCount = dir->idCount;
    for(i = 0; i<dir->idCount; i++)
    {
        fakeEntry = &fakeDir->idEntries[i];
        entry = &dir->idEntries[i];
        /* Take this as an occasion to perform a size check */
        if ((entry->dwDIBOffset > dwFileSize)
                || ((entry->dwDIBOffset + entry->dwDIBSize) > dwFileSize))
        {
            ERR("Corrupted icon file?.\n");
            HeapFree(GetProcessHeap(), 0, fakeDir);
            return NULL;
        }
        /* File icon/cursors are not like resource ones */
        if(bIcon)
        {
            fakeEntry->ResInfo.icon.bWidth = entry->bWidth;
            fakeEntry->ResInfo.icon.bHeight = entry->bHeight;
            fakeEntry->ResInfo.icon.bColorCount = 0;
            fakeEntry->ResInfo.icon.bReserved = 0;
        }
        else
        {
            fakeEntry->ResInfo.cursor.wWidth = entry->bWidth;
            fakeEntry->ResInfo.cursor.wHeight = entry->bHeight;
        }
        /* Let's assume there's always one plane */
        fakeEntry->wPlanes = 1;
        /* We must get the bitcount from the BITMAPINFOHEADER itself */
        if (((BITMAPINFOHEADER *)((char *)dir + entry->dwDIBOffset))->biSize == sizeof(BITMAPCOREHEADER))
            fakeEntry->wBitCount = ((BITMAPCOREHEADER *)((char *)dir + entry->dwDIBOffset))->bcBitCount;
        else
            fakeEntry->wBitCount = ((BITMAPINFOHEADER *)((char *)dir + entry->dwDIBOffset))->biBitCount;
        fakeEntry->dwBytesInRes = entry->dwDIBSize;
        fakeEntry->wResId = i + 1;
    }

    /* Now call LookupIconIdFromResourceEx */
    i = LookupIconIdFromDirectoryExHook((PBYTE)fakeDir, bIcon, cxDesired, cyDesired, fuLoad & LR_MONOCHROME);
    /* We don't need this anymore */
    HeapFree(GetProcessHeap(), 0, fakeDir);
    if(i == 0)
    {
        WARN("Unable to get a fit entry index.\n");
        return NULL;
    }

    /* We found it */
    return &dir->idEntries[i-1];
}

DWORD
get_best_icon_file_offset(
    _In_ const LPBYTE dir,
    _In_ DWORD dwFileSize,
    _In_ int cxDesired,
    _In_ int cyDesired,
    _In_ BOOL bIcon,
    _In_ DWORD fuLoad,
    _Out_ POINT *ptHotSpot
)
{
    const CURSORICONFILEDIRENTRY *entry;

    entry = get_best_icon_file_entry((CURSORICONFILEDIR *) dir, dwFileSize, cxDesired, cyDesired, bIcon, fuLoad);

    if(ptHotSpot)
    {
        ptHotSpot->x = entry->xHotspot;
        ptHotSpot->y = entry->yHotspot;
    }

    if(entry)
        return entry->dwDIBOffset;

    return 0;
}