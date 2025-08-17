/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    exticon.c

Abstract:

        This file implements the NT routines for icons extraction.

Author:

    Skulltrail 01-August-2025

Revision History:

--*/


#include <main.h>
#include <wingdi.h>

/* Start of Hack section */

WINE_DEFAULT_DEBUG_CHANNEL(icon);

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define IMAGE_ICO_SIGNATURE 1
#define IMAGE_CUR_SIGNATURE 2

#define __REACTOS__ 1

#ifdef __REACTOS__

#include <pshpack1.h>

typedef struct
{
    BYTE        bWidth;          /* Width, in pixels, of the image	*/
    BYTE        bHeight;         /* Height, in pixels, of the image	*/
    BYTE        bColorCount;     /* Number of colors in image (0 if >=8bpp) */
    BYTE        bReserved;       /* Reserved ( must be 0)		*/
    WORD        wPlanes;         /* Color Planes			*/
    WORD        wBitCount;       /* Bits per pixel			*/
    DWORD       dwBytesInRes;    /* How many bytes in this resource?	*/
    DWORD       dwImageOffset;   /* Where in the file is this image?	*/
} icoICONDIRENTRY, *LPicoICONDIRENTRY;

typedef struct
{
    WORD            idReserved;   /* Reserved (must be 0) */
    WORD            idType;       /* Resource Type (RES_ICON or RES_CURSOR) */
    WORD            idCount;      /* How many images */
    icoICONDIRENTRY idEntries[1]; /* An entry for each image (idCount of 'em) */
} icoICONDIR, *LPicoICONDIR;

typedef struct
{
    WORD offset;
    WORD length;
    WORD flags;
    WORD id;
    WORD handle;
    WORD usage;
} NE_NAMEINFO;

typedef struct
{
    WORD  type_id;
    WORD  count;
    DWORD resloader;
} NE_TYPEINFO;

//  From: James Houghtaling
//  https://www.moon-soft.com/program/FORMAT/windows/ani.htm
typedef struct taganiheader
{
    DWORD cbsizeof;  // num bytes in aniheader (36 bytes)
    DWORD cframes;   // number of unique icons in this cursor
    DWORD csteps;    // number of blits before the animation cycles
    DWORD cx;        // reserved, must be zero.
    DWORD cy;        // reserved, must be zero.
    DWORD cbitcount; // reserved, must be zero.
    DWORD cplanes;   // reserved, must be zero.
    DWORD jifrate;   // default jiffies (1/60th sec) if rate chunk not present.
    DWORD flags;     // animation flag
} aniheader;
#endif

typedef struct _ICONIMAGE
{
    BITMAPINFOHEADER icHeader;      // DIB header
    RGBQUAD icColors[1];            // Color table
    BYTE icXOR[1];                  // DIB bits for XOR mask
    BYTE icAND[1];                  // DIB bits for AND mask
} ICONIMAGE, *LPICONIMAGE;

#define NE_RSCTYPE_ICON        0x8003
#define NE_RSCTYPE_GROUP_ICON  0x800e

#if 0
static void dumpIcoDirEnty ( LPicoICONDIRENTRY entry )
{
	TRACE("width = 0x%08x height = 0x%08x\n", entry->bWidth, entry->bHeight);
	TRACE("colors = 0x%08x planes = 0x%08x\n", entry->bColorCount, entry->wPlanes);
	TRACE("bitcount = 0x%08x bytesinres = 0x%08lx offset = 0x%08lx\n",
	entry->wBitCount, entry->dwBytesInRes, entry->dwImageOffset);
}
static void dumpIcoDir ( LPicoICONDIR entry )
{
	TRACE("type = 0x%08x count = 0x%08x\n", entry->idType, entry->idCount);
}
#endif

#ifndef WINE
DWORD get_best_icon_file_offset(const LPBYTE dir,
                                DWORD dwFileSize,
                                int cxDesired,
                                int cyDesired,
                                BOOL bIcon,
                                DWORD fuLoad,
                                POINT *ptHotSpot);
#endif

/**********************************************************************
 *  find_entry_by_id
 *
 * Find an entry by id in a resource directory
 * Copied from loader/pe_resource.c (FIXME: should use exported resource functions)
 */
static const IMAGE_RESOURCE_DIRECTORY *find_entry_by_id( const IMAGE_RESOURCE_DIRECTORY *dir,
                                                         WORD id, const void *root )
{
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;
    int min, max, pos;

    entry = (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
    min = dir->NumberOfNamedEntries;
    max = min + dir->NumberOfIdEntries - 1;
    while (min <= max)
    {
        pos = (min + max) / 2;
        if (entry[pos].Id == id)
            return (const IMAGE_RESOURCE_DIRECTORY *)((const char *)root + entry[pos].OffsetToDirectory);
        if (entry[pos].Id > id) max = pos - 1;
        else min = pos + 1;
    }
    return NULL;
}

/**********************************************************************
 *  find_entry_default
 *
 * Find a default entry in a resource directory
 * Copied from loader/pe_resource.c (FIXME: should use exported resource functions)
 */
static const IMAGE_RESOURCE_DIRECTORY *find_entry_default( const IMAGE_RESOURCE_DIRECTORY *dir,
                                                           const void *root )
{
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;
    entry = (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
    return (const IMAGE_RESOURCE_DIRECTORY *)((const char *)root + entry->OffsetToDirectory);
}

/*************************************************************************
 *				USER32_GetResourceTable
 */
static DWORD USER32_GetResourceTable(LPBYTE peimage,DWORD pesize,LPBYTE *retptr)
{
	IMAGE_DOS_HEADER	* mz_header;

	TRACE("%p %p\n", peimage, retptr);

	*retptr = NULL;

	mz_header = (IMAGE_DOS_HEADER*) peimage;

	if (mz_header->e_magic != IMAGE_DOS_SIGNATURE)
	{
	  if (mz_header->e_cblp == 1 || mz_header->e_cblp == 2)	/* .ICO or .CUR file ? */
	  {
	    *retptr = (LPBYTE)-1;	/* ICONHEADER.idType, must be 1 */
	    return mz_header->e_cblp;
	  }
	  else
	    return 0; /* failed */
	}
	if (mz_header->e_lfanew >= pesize) {
	    return 0; /* failed, happens with PKZIP DOS Exes for instance. */
	}
	if (*((DWORD*)(peimage + mz_header->e_lfanew)) == IMAGE_NT_SIGNATURE )
	  return IMAGE_NT_SIGNATURE;

	if (*((WORD*)(peimage + mz_header->e_lfanew)) == IMAGE_OS2_SIGNATURE )
	{
	  IMAGE_OS2_HEADER	* ne_header;

	  ne_header = (IMAGE_OS2_HEADER*)(peimage + mz_header->e_lfanew);

	  if (ne_header->ne_magic != IMAGE_OS2_SIGNATURE)
	    return 0;

	  if( (ne_header->ne_restab - ne_header->ne_rsrctab) <= sizeof(NE_TYPEINFO) )
	    *retptr = (LPBYTE)-1;
	  else
	    *retptr = peimage + mz_header->e_lfanew + ne_header->ne_rsrctab;

	  return IMAGE_OS2_SIGNATURE;
	}
	return 0; /* failed */
}
/*************************************************************************
 *			USER32_LoadResource
 */
static BYTE * USER32_LoadResource( LPBYTE peimage, NE_NAMEINFO* pNInfo, WORD sizeShift, ULONG *uSize)
{
	TRACE("%p %p 0x%08x\n", peimage, pNInfo, sizeShift);

	*uSize = (DWORD)pNInfo->length << sizeShift;
	return peimage + ((DWORD)pNInfo->offset << sizeShift);
}

/*************************************************************************
 *	ICO_ExtractIconExW		[internal]
 *
 * NOTES
 *  nIcons = 0: returns number of Icons in file
 *
 * returns
 *  invalid file: -1
 *  failure:0;
 *  success: number of icons in file (nIcons = 0) or nr of icons retrieved
 */
static UINT ICO_ExtractIconExW(
	LPCWSTR lpszExeFileName,
	HICON * RetPtr,
	INT nIconIndex,
	UINT nIcons,
	UINT cxDesired,
	UINT cyDesired,
	UINT *pIconId,
#ifdef __REACTOS__
    UINT flags,
    /* This function is called from two different code paths.
     * One is from Shell32 using the ExtractIconEx function.
     * The other is from User32 using PrivateExtractIcons.
     * Based on W2K3SP2 testing, the count of icons returned
     * is zero (0) for PNG ones using ExtractIconEx and
     * one (1) for PNG icons using PrivateExtractIcons. 
     * We can handle the difference using the fIconEx flag.*/
    BOOL fIconEx)
#else
	UINT flags)
#endif
{
	UINT		ret = 0;
	UINT		cx1, cx2, cy1, cy2;
	LPBYTE		pData;
	DWORD		sig;
	HANDLE		hFile;
	UINT16		iconDirCount = 0,iconCount = 0;
	LPBYTE		peimage;
	HANDLE		fmapping;
	DWORD		fsizeh,fsizel;
#ifdef __REACTOS__
    WCHAR		szExpandedExePath[MAX_PATH];
#endif
    WCHAR		szExePath[MAX_PATH];
    DWORD		dwSearchReturn;
	BYTE *idata, *igdata;
	const IMAGE_RESOURCE_DIRECTORY *rootresdir, *iconresdir, *icongroupresdir;
	const IMAGE_RESOURCE_DATA_ENTRY *idataent, *igdataent;
	const IMAGE_RESOURCE_DIRECTORY_ENTRY *xresent;
	ULONG size;
	UINT i;	
	//DWORD cbSize;

#ifdef __REACTOS__
    TRACE("%s, %d, %d, %p, 0x%08x, %d\n", debugstr_w(lpszExeFileName), nIconIndex, nIcons, pIconId, flags, fIconEx);
#else
	TRACE("%s, %d, %d %p 0x%08x\n", debugstr_w(lpszExeFileName), nIconIndex, nIcons, pIconId, flags);
#endif

#ifdef __REACTOS__
    if (RetPtr)
        *RetPtr = NULL;

    if (ExpandEnvironmentStringsW(lpszExeFileName, szExpandedExePath, ARRAY_SIZE(szExpandedExePath)))
        lpszExeFileName = szExpandedExePath;
#endif

        dwSearchReturn = SearchPathW(NULL, lpszExeFileName, NULL, ARRAY_SIZE(szExePath), szExePath, NULL);
        if ((dwSearchReturn == 0) || (dwSearchReturn > ARRAY_SIZE(szExePath)))
        {
#ifdef __REACTOS__
            WARN("File %s not found or path too long and fIconEx is '%d'\n",
                 debugstr_w(lpszExeFileName), fIconEx);
            if (fIconEx && !RetPtr && !pIconId)
                return 0;
            else
                return -1;
#else
            WARN("File %s not found or path too long\n", debugstr_w(lpszExeFileName));
            return -1;
#endif
        }

	hFile = CreateFileW(szExePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE) return ret;
	fsizel = GetFileSize(hFile,&fsizeh);
#ifdef __REACTOS__
    if (!(fsizel | fsizeh))
    {
        /* Cannot map empty file */
        CloseHandle(hFile);
        return 0; /* No icons */
    }
#endif

	/* Map the file */
	fmapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY | SEC_COMMIT, 0, 0, NULL);
	CloseHandle(hFile);
	if (!fmapping)
	{
          WARN("CreateFileMapping error %ld\n", GetLastError() );
	  return 0xFFFFFFFF;
	}

	if (!(peimage = MapViewOfFile(fmapping, FILE_MAP_READ, 0, 0, 0)))
	{
          WARN("MapViewOfFile error %ld\n", GetLastError() );
	  CloseHandle(fmapping);
	  return 0xFFFFFFFF;
	}
	CloseHandle(fmapping);

#ifdef __REACTOS__
    /* Check if we have a min size of 2 headers RIFF & 'icon'
     * at 8 chars each plus an anih header of 36 byptes.
     * Also, is this resource an animjated icon/cursor (RIFF) */
    if ((fsizel >= 52) && !memcmp(peimage, "RIFF", 4))
    {
        UINT anihOffset;
        UINT anihMax;
        /* Get size of the animation data */
        ULONG uSize = MAKEWORD(peimage[4], peimage[5]);

        /* Check if uSize is reasonable with respect to fsizel */
        if ((uSize < strlen("anih")) || (uSize > fsizel))
            goto end;

        /* Look though the reported size less search string length */
        anihMax = uSize - strlen("anih"); 
        /* Search for 'anih' indicating animation header */
        for (anihOffset = 0; anihOffset < anihMax; anihOffset++)
        {
            if (memcmp(&peimage[anihOffset], "anih", 4) == 0)
                break;
        }

        if (anihOffset + sizeof(aniheader) > fsizel)
            goto end;

        /* Get count of images for return value */
        ret = MAKEWORD(peimage[anihOffset + 12], peimage[anihOffset + 13]);

        TRACE("RIFF File with '%u' images at Offset '%u'.\n", ret, anihOffset);

        cx1 = LOWORD(cxDesired);
        cy1 = LOWORD(cyDesired);

        if (RetPtr)
        {
            RetPtr[0] = CreateIconFromResourceExHook(peimage, uSize, TRUE, 0x00030000, cx1, cy1, flags);
        }
        goto end;
    }
#endif

	cx1 = LOWORD(cxDesired);
	cx2 = HIWORD(cxDesired);
	cy1 = LOWORD(cyDesired);
	cy2 = HIWORD(cyDesired);

	if (pIconId) /* Invalidate first icon identifier */
		*pIconId = 0xFFFFFFFF;

	if (!pIconId) /* if no icon identifier array present use the icon handle array as intermediate storage */
	  pIconId = (UINT*)RetPtr;

	sig = USER32_GetResourceTable(peimage, fsizel, &pData);
	
	switch(sig){
		case IMAGE_OS2_SIGNATURE:
			return PrivateExtractIconsW(lpszExeFileName, nIconIndex, cxDesired, cyDesired, RetPtr, pIconId, nIcons, flags);
		case IMAGE_ICO_SIGNATURE:
			TRACE("-- icon Signature (0x%08x)\n", sig);
			
			if (pData == (BYTE*)-1)
			{
				INT cx[2] = {cx1, cx2}, cy[2] = {cy1, cy2};
				INT index;

				for(index = 0; index < (cx2 || cy2 ? 2 : 1); index++)
				{
					DWORD dataOffset;
					LPBYTE imageData;
					POINT hotSpot;
	#ifndef __REACTOS__
					LPICONIMAGE entry;
	#endif

					dataOffset = get_best_icon_file_offset(peimage, fsizel, cx[index], cy[index], sig == 1, flags, sig == 1 ? NULL : &hotSpot);

					if (dataOffset)
					{
						HICON icon;
						WORD *cursorData = NULL;
	#ifdef __REACTOS__
						BITMAPINFOHEADER bi;
						DWORD cbColorTable = 0, cbTotal;
	#endif

						imageData = peimage + dataOffset;
	#ifdef __REACTOS__
						/* Calculate the size of color table */
						ZeroMemory(&bi, sizeof(bi));
						CopyMemory(&bi, imageData, sizeof(BITMAPCOREHEADER));
						if (bi.biBitCount <= 8)
						{
							if (bi.biSize >= sizeof(BITMAPINFOHEADER))
							{
								CopyMemory(&bi, imageData, sizeof(BITMAPINFOHEADER));
								if (bi.biClrUsed)
									cbColorTable = bi.biClrUsed * sizeof(RGBQUAD);
								else
									cbColorTable = (1 << bi.biBitCount) * sizeof(RGBQUAD);
							}
							else if (bi.biSize == sizeof(BITMAPCOREHEADER))
							{
								cbColorTable = (1 << bi.biBitCount) * sizeof(RGBTRIPLE);
							}
						}

						/* biSizeImage is the size of the raw bitmap data.
						 * https://en.wikipedia.org/wiki/BMP_file_format */
						if (bi.biSizeImage == 0)
						{
							 /* Calculate image size */
	#define WIDTHBYTES(width, bits) (((width) * (bits) + 31) / 32 * 4)
							bi.biSizeImage = WIDTHBYTES(bi.biWidth, bi.biBitCount) * (bi.biHeight / 2);
							bi.biSizeImage += WIDTHBYTES(bi.biWidth, 1) * (bi.biHeight / 2);
	#undef WIDTHBYTES
						}

						/* Calculate total size */
						cbTotal = bi.biSize + cbColorTable + bi.biSizeImage;
	#else
						entry = (LPICONIMAGE)(imageData);
	#endif

	#ifdef __REACTOS__
						icon = CreateIconFromResourceExHook(imageData, cbTotal, sig == 1, 0x00030000, cx[index], cy[index], flags);
						if (fIconEx && sig == 1)
							iconCount = 1;
	#else
						icon = CreateIconFromResourceExHook(imageData, entry->icHeader.biSizeImage, sig == 1, 0x00030000, cx[index], cy[index], flags);
	#endif

						HeapFree(GetProcessHeap(), 0, cursorData);

						if (icon)
						{
							if (RetPtr)
								RetPtr[index] = icon;
							else
								DestroyIcon(icon);

							iconCount = 1;
							break;
						}
					}
				}
			}
			ret = iconCount;	/* return number of retrieved icons */	
			break;
		case IMAGE_CUR_SIGNATURE:
			return PrivateExtractIconsW(lpszExeFileName, nIconIndex, cxDesired, cyDesired, RetPtr, pIconId, nIcons, flags);
		case IMAGE_NT_SIGNATURE:


			rootresdir = RtlImageDirectoryEntryToData((HMODULE)peimage, FALSE, IMAGE_DIRECTORY_ENTRY_RESOURCE, &size);
			if (!rootresdir)
			{
				WARN("haven't found section for resource directory.\n");
				goto end;
			}

		  /* search for the group icon directory */
		  if (!(icongroupresdir = find_entry_by_id(rootresdir, LOWORD(RT_GROUP_ICON), rootresdir)))
		  {
			WARN("No Icongroupresourcedirectory!\n");
			goto end;		/* failure */
		  }
		  iconDirCount = icongroupresdir->NumberOfNamedEntries + icongroupresdir->NumberOfIdEntries;

		  /* only number of icons requested */
		  if( !pIconId )
		  {
			ret = iconDirCount;
			goto end;		/* success */
		  }

		  if( nIconIndex < 0 )
		  {
			/* search resource id */
			int n = 0;
			int iId = abs(nIconIndex);
			const IMAGE_RESOURCE_DIRECTORY_ENTRY* xprdeTmp = (const IMAGE_RESOURCE_DIRECTORY_ENTRY*)(icongroupresdir+1);

			while(n<iconDirCount && xprdeTmp)
			{
				  if(xprdeTmp->Id ==  iId)
				  {
					  nIconIndex = n;
					  break;
				  }
				  n++;
				  xprdeTmp++;
			}
			if (nIconIndex < 0)
			{
			  WARN("resource id %d not found\n", iId);
			  goto end;		/* failure */
			}
		  }
		  else
		  {
			/* check nIconIndex to be in range */
			if (nIconIndex >= iconDirCount)
			{
			  WARN("nIconIndex %d is larger than iconDirCount %d\n",nIconIndex,iconDirCount);
			  goto end;		/* failure */
			}
		  }

		  /* assure we don't get too much */
		  if( nIcons > iconDirCount - nIconIndex )
			nIcons = iconDirCount - nIconIndex;

		  /* starting from specified index */
		  xresent = (const IMAGE_RESOURCE_DIRECTORY_ENTRY*)(icongroupresdir+1) + nIconIndex;

		  for (i=0; i < nIcons; i++,xresent++)
		  {
			const IMAGE_RESOURCE_DIRECTORY *resdir;

			/* go down this resource entry, name */
				resdir = (const IMAGE_RESOURCE_DIRECTORY *)((const char *)rootresdir + xresent->OffsetToDirectory);

			/* default language (0) */
			resdir = find_entry_default(resdir,rootresdir);
			igdataent = (const IMAGE_RESOURCE_DATA_ENTRY*)resdir;

			/* lookup address in mapped image for virtual address */
			igdata = RtlImageRvaToVa(RtlImageNtHeader((HMODULE)peimage), (HMODULE)peimage, igdataent->OffsetToData, NULL);
			if (!igdata)
			{
			  FIXME("no matching real address for icongroup!\n");
			  goto end;	/* failure */
			}
			
			pIconId[i] = LookupIconIdFromDirectoryEx(igdata, TRUE, cx1, cy1, flags);
				if (cx2 && cy2) pIconId[++i] = LookupIconIdFromDirectoryEx(igdata, TRUE, cx2, cy2, flags);
		  }

		  if (!(iconresdir=find_entry_by_id(rootresdir,LOWORD(RT_ICON),rootresdir)))
		  {
			WARN("No Iconresourcedirectory!\n");
			goto end;		/* failure */
		  }

		  for (i=0; i<nIcons; i++)
		  {
			const IMAGE_RESOURCE_DIRECTORY *xresdir;
			xresdir = find_entry_by_id(iconresdir, LOWORD(pIconId[i]), rootresdir);
				if( !xresdir )
				{
				  WARN("icon entry %d not found\n", LOWORD(pIconId[i]));
			  RetPtr[i]=0;
			  continue;
				}
			xresdir = find_entry_default(xresdir, rootresdir);
			idataent = (const IMAGE_RESOURCE_DATA_ENTRY*)xresdir;

			idata = RtlImageRvaToVa(RtlImageNtHeader((HMODULE)peimage), (HMODULE)peimage, idataent->OffsetToData, NULL);
			if (!idata)
			{
			  WARN("no matching real address found for icondata!\n");
			  RetPtr[i]=0;
			  continue;
			}
			RetPtr[i] = CreateIconFromResourceExHook(idata, idataent->Size, TRUE, 0x00030000, cx1, cy1, flags);
				if (cx2 && cy2)
					RetPtr[++i] = CreateIconFromResourceExHook(idata, idataent->Size, TRUE, 0x00030000, cx2, cy2, flags);
		  }		
		  ret = i;	/* return number of retrieved icons */		  
		  break;
		default:
			return PrivateExtractIconsW(lpszExeFileName, nIconIndex, cxDesired, cyDesired, RetPtr, pIconId, nIcons, flags); 
	}

end:
	UnmapViewOfFile(peimage);	/* success */
	return ret;
}

/***********************************************************************
 *           PrivateExtractIconsW			[USER32.@]
 *
 * NOTES
 *  If HIWORD(sizeX) && HIWORD(sizeY) 2 * ((nIcons + 1) MOD 2) icons are
 *  returned, with the LOWORD size icon first and the HIWORD size icon
 *  second.
 *  Also the Windows equivalent does extract icons in a strange way if
 *  nIndex is negative. Our implementation treats a negative nIndex as
 *  looking for that resource identifier for the first icon to retrieve.
 *
 * FIXME:
 *  should also support 16 bit EXE + DLLs, cursor and animated cursor as
 *  well as bitmap files.
 */

UINT WINAPI PrivateExtractIconsWHook (
	LPCWSTR lpwstrFile,
	int nIndex,
	int sizeX,
	int sizeY,
	HICON * phicon, /* [out] pointer to array of nIcons HICON handles */
	UINT* pIconId,  /* [out] pointer to array of nIcons icon identifiers or NULL */
	UINT nIcons,    /* [in] number of icons to retrieve */
	UINT flags )    /* [in] LR_* flags used by LoadImage */
{
	UINT resp;
	
	TRACE("%s %d %dx%d %p %p %d 0x%08x\n",
	      debugstr_w(lpwstrFile), nIndex, sizeX, sizeY, phicon, pIconId, nIcons, flags);
		  
	//Ugly hack for bypass reactos/wine problem with some icons, mainly quick launch		  
    resp = PrivateExtractIconsW(lpwstrFile, nIndex, sizeX, sizeY, phicon, pIconId, nIcons, flags);
    if(resp == nIcons || IsNativePNGConversor){
         return resp;
    }

	if ((nIcons & 1) && HIWORD(sizeX) && HIWORD(sizeY))
	{
	  WARN("Uneven number %d of icons requested for small and large icons!\n", nIcons);
	}
	
#ifdef __REACTOS__
    return ICO_ExtractIconExW(lpwstrFile, phicon, nIndex, nIcons, sizeX, sizeY,
                              pIconId, flags, TRUE);
#else
	return ICO_ExtractIconExW(lpwstrFile, phicon, nIndex, nIcons, sizeX, sizeY, pIconId, flags);
#endif
}

/***********************************************************************
 *           PrivateExtractIconsA			[USER32.@]
 */

UINT WINAPI PrivateExtractIconsAHook (
	LPCSTR lpstrFile,
	int nIndex,
	int sizeX,
	int sizeY,
	HICON * RetPtr, /* [out] pointer to array of nIcons HICON handles */
	UINT* pIconId,  /* [out] pointer to array of nIcons icon identifiers or NULL */
	UINT nIcons,    /* [in] number of icons to retrieve */
	UINT flags )    /* [in] LR_* flags used by LoadImage */
{
    UINT ret;
    INT len = MultiByteToWideChar(CP_ACP, 0, lpstrFile, -1, NULL, 0);
    LPWSTR lpwstrFile = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
	
	if(IsNativePNGConversor){
		return PrivateExtractIconsA(lpstrFile, nIndex, sizeX, sizeY, RetPtr, pIconId, nIcons, flags);
	}
	
#ifdef __REACTOS__
    if (lpwstrFile == NULL)
        return 0;
#endif

    MultiByteToWideChar(CP_ACP, 0, lpstrFile, -1, lpwstrFile, len);
    ret = PrivateExtractIconsWHook(lpwstrFile, nIndex, sizeX, sizeY, RetPtr, pIconId, nIcons, flags);

    HeapFree(GetProcessHeap(), 0, lpwstrFile);
    return ret;
}

/***********************************************************************
 *           PrivateExtractIconExW			[USER32.@]
 * NOTES
 *  if nIndex == -1 it returns the number of icons in any case !!!
 */
UINT WINAPI PrivateExtractIconExWHook (
	LPCWSTR lpwstrFile,
	int nIndex,
	HICON * phIconLarge,
	HICON * phIconSmall,
	UINT nIcons )
{
	DWORD cyicon, cysmicon, cxicon, cxsmicon;
	UINT ret = 0;

	TRACE("%s %d %p %p %d\n",
	debugstr_w(lpwstrFile),nIndex,phIconLarge, phIconSmall, nIcons);
	
	//Ugly hack for bypass reactos/wine problem with some icons, mainly quick launch		  
    ret = PrivateExtractIconExW(lpwstrFile, nIndex, phIconLarge, phIconSmall, nIcons);
    if(ret == nIcons || IsNativePNGConversor){
         DbgPrint("PrivateExtractIconExWHook:: resp: %d\n", ret);
         return ret;
    }	

#ifdef __REACTOS__
    if (nIndex == -1 || (!phIconSmall && !phIconLarge))
      /* get the number of icons */
      return ICO_ExtractIconExW(lpwstrFile, NULL, 0, 0, 0, 0, NULL,
                                LR_DEFAULTCOLOR, FALSE);
#else
	if (nIndex == -1)
	  /* get the number of icons */
	  return ICO_ExtractIconExW(lpwstrFile, NULL, 0, 0, 0, 0, NULL, LR_DEFAULTCOLOR);
#endif

	if (nIcons == 1 && phIconSmall && phIconLarge)
	{
	  HICON hIcon[2];
	  cxicon = GetSystemMetrics(SM_CXICON);
	  cyicon = GetSystemMetrics(SM_CYICON);
	  cxsmicon = GetSystemMetrics(SM_CXSMICON);
	  cysmicon = GetSystemMetrics(SM_CYSMICON);

#ifdef __REACTOS__
      ret = ICO_ExtractIconExW(lpwstrFile, hIcon, nIndex, 2,
                               cxicon | (cxsmicon<<16),
                               cyicon | (cysmicon<<16), NULL,
                               LR_DEFAULTCOLOR, FALSE);
#else
          ret = ICO_ExtractIconExW(lpwstrFile, hIcon, nIndex, 2, cxicon | (cxsmicon<<16),
	                           cyicon | (cysmicon<<16), NULL, LR_DEFAULTCOLOR);
#endif
	  *phIconLarge = hIcon[0];
	  *phIconSmall = hIcon[1];
 	  return ret;
	}

	if (phIconSmall)
	{
	  /* extract n small icons */
	  cxsmicon = GetSystemMetrics(SM_CXSMICON);
	  cysmicon = GetSystemMetrics(SM_CYSMICON);
#ifdef __REACTOS__
      ret = ICO_ExtractIconExW(lpwstrFile, phIconSmall, nIndex, nIcons, cxsmicon,
                               cysmicon, NULL, LR_DEFAULTCOLOR, FALSE);
#else
	  ret = ICO_ExtractIconExW(lpwstrFile, phIconSmall, nIndex, nIcons, cxsmicon,
	                           cysmicon, NULL, LR_DEFAULTCOLOR);
#endif
	}
       if (phIconLarge)
	{
	  /* extract n large icons */
	  cxicon = GetSystemMetrics(SM_CXICON);
	  cyicon = GetSystemMetrics(SM_CYICON);
#ifdef __REACTOS__
       ret = ICO_ExtractIconExW(lpwstrFile, phIconLarge, nIndex, nIcons, cxicon,
                                cyicon, NULL, LR_DEFAULTCOLOR, FALSE);
#else
         ret = ICO_ExtractIconExW(lpwstrFile, phIconLarge, nIndex, nIcons, cxicon,
	                           cyicon, NULL, LR_DEFAULTCOLOR);
#endif
	}
	return ret;
}

/***********************************************************************
 *           PrivateExtractIconExA			[USER32.@]
 */
UINT WINAPI PrivateExtractIconExAHook (
	LPCSTR lpstrFile,
	int nIndex,
	HICON * phIconLarge,
	HICON * phIconSmall,
	UINT nIcons )
{
	UINT ret;
	INT len = MultiByteToWideChar(CP_ACP, 0, lpstrFile, -1, NULL, 0);
	LPWSTR lpwstrFile = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
	
	if(IsNativePNGConversor){
		return PrivateExtractIconExA(lpstrFile, nIndex, phIconLarge, phIconSmall, nIcons);
	}	
	
#ifdef __REACTOS__
    if (lpwstrFile == NULL)
        return 0;
#endif

	TRACE("%s %d %p %p %d\n", lpstrFile, nIndex, phIconLarge, phIconSmall, nIcons);

	MultiByteToWideChar(CP_ACP, 0, lpstrFile, -1, lpwstrFile, len);
	ret = PrivateExtractIconExWHook(lpwstrFile, nIndex, phIconLarge, phIconSmall, nIcons);
	HeapFree(GetProcessHeap(), 0, lpwstrFile);
	return ret;
}