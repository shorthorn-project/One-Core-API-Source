/*++

Copyright (c) 2017 Shorthorn Project.

Module Name:

    locale.c

Abstract:

    This file contains functions that return information about a
    language group, a UI language, a locale, or a calendar.

Revision History:

    Skulltrail 22-03-2017

--*/

#include "main.h"
#include "locale.h"

WINE_DEFAULT_DEBUG_CHANNEL(locale);

#define LOCALE_WINDOWS              0x01
#define LOCALE_NEUTRALDATA          0x10
#define LOCALE_SPECIFICDATA         0x20
#define MUI_LANGUAGE_ID             0x04
#define MUI_LANGUAGE_NAME           0x08
#define MUI_MACHINE_LANGUAGE_SETTINGS       0x400
#define MUI_MERGE_USER_FALLBACK 0x20
#define MUI_MERGE_SYSTEM_FALLBACK 0x10
#define MUI_THREAD_LANGUAGES                0x40
#define MUI_UI_FALLBACK                     MUI_MERGE_SYSTEM_FALLBACK | MUI_MERGE_USER_FALLBACK
#define MAX_STRING_LEN 256

#define LOCALE_LOCALEINFOFLAGSMASK (LOCALE_NOUSEROVERRIDE|LOCALE_USE_CP_ACP|\
                                    LOCALE_RETURN_NUMBER|LOCALE_RETURN_GENITIVE_NAMES)
									
#define LOCALE_ALLOW_NEUTRAL_NAMES    0x08000000

#define LINGUISTIC_IGNOREDIACRITIC 0x00000020								
									
static const GUID default_sort_guid = { 0x00000001, 0x57ee, 0x1e5c, { 0x00, 0xb4, 0xd0, 0x00, 0x0b, 0xb1, 0xe1, 0x1e }};									
									
static LPWSTR systemLocale;			

static const struct sortguid *current_locale_sort;						

/* locale ids corresponding to the various Unix locale parameters */
static LCID lcid_LC_COLLATE;
static LCID lcid_LC_CTYPE;
static LCID lcid_LC_MESSAGES;
static LCID lcid_LC_MONETARY;
static LCID lcid_LC_NUMERIC;
static LCID lcid_LC_TIME;
static LCID lcid_LC_PAPER;
static LCID lcid_LC_MEASUREMENT;
static LCID lcid_LC_TELEPHONE;

static HKEY nls_key;
static HKEY tz_key;

static RTL_CRITICAL_SECTION cache_section;

static const NLS_LOCALE_HEADER *locale_table;

struct sortkey
{
    BYTE *buf;
    BYTE *new_buf;  /* allocated buf if static buf is not large enough */
    UINT  size;     /* buffer size */
    UINT  max;      /* max possible size */
    UINT  len;      /* current key length */
};

struct sortkey_state
{
    struct sortkey         key_primary;
    struct sortkey         key_diacritic;
    struct sortkey         key_case;
    struct sortkey         key_special;
    struct sortkey         key_extra[4];
    UINT                   primary_pos;
    BYTE                   buffer[3 * 128];
};

enum sortkey_script
{
    SCRIPT_UNSORTABLE = 0,
    SCRIPT_NONSPACE_MARK = 1,
    SCRIPT_EXPANSION = 2,
    SCRIPT_EASTASIA_SPECIAL = 3,
    SCRIPT_JAMO_SPECIAL = 4,
    SCRIPT_EXTENSION_A = 5,
    SCRIPT_PUNCTUATION = 6,
    SCRIPT_SYMBOL_1 = 7,
    SCRIPT_SYMBOL_2 = 8,
    SCRIPT_SYMBOL_3 = 9,
    SCRIPT_SYMBOL_4 = 10,
    SCRIPT_SYMBOL_5 = 11,
    SCRIPT_SYMBOL_6 = 12,
    SCRIPT_DIGIT = 13,
    SCRIPT_LATIN = 14,
    SCRIPT_GREEK = 15,
    SCRIPT_CYRILLIC = 16,
    SCRIPT_KANA = 34,
    SCRIPT_HEBREW = 40,
    SCRIPT_ARABIC = 41,
    SCRIPT_PUA_FIRST = 169,
    SCRIPT_PUA_LAST = 175,
    SCRIPT_CJK_FIRST = 192,
    SCRIPT_CJK_LAST = 239,
};

struct sort_expansion
{
    WCHAR exp[2];
};

struct jamo_sort
{
    BYTE is_old;
    BYTE leading;
    BYTE vowel;
    BYTE trailing;
    BYTE weight;
    BYTE pad[3];
};

struct sort_compression
{
    UINT  offset;
    WCHAR minchar, maxchar;
    WORD  len[8];
};

static inline int compression_size( int len ) { return 2 + len + (len & 1); }

union char_weights
{
    UINT val;
    struct { BYTE primary, script, diacritic, _case; };
};

/* move to winnls*/

#define LOCALE_SNAN                 0x0069
#define LOCALE_SNEGINFINITY         0x006B
#define LOCALE_SPOSINFINITY         0x006A
#define LOCALE_SDURATION            0x005D
#define LOCALE_SSHORTESTDAYNAME1    0x0060
#define LOCALE_SSHORTESTDAYNAME2    0x0061
#define LOCALE_SSHORTESTDAYNAME3    0x0062
#define LOCALE_SSHORTESTDAYNAME4    0x0063
#define LOCALE_SSHORTESTDAYNAME5    0x0064
#define LOCALE_SSHORTESTDAYNAME6    0x0065
#define LOCALE_SSHORTESTDAYNAME7    0x0066
#define SORT_DIGITSASNUMBERS       0x00000008

/* bits for case weights */
#define CASE_FULLWIDTH   0x01  /* full width kana (vs. half width) */
#define CASE_FULLSIZE    0x02  /* full size kana (vs. small) */
#define CASE_SUBSCRIPT   0x08  /* sub/super script */
#define CASE_UPPER       0x10  /* upper case */
#define CASE_KATAKANA    0x20  /* katakana (vs. hiragana) */
#define CASE_COMPR_2     0x40  /* compression exists for >= 2 chars */
#define CASE_COMPR_4     0x80  /* compression exists for >= 4 chars */
#define CASE_COMPR_6     0xc0  /* compression exists for >= 6 chars */

/* flags for sortguid */
#define FLAG_HAS_3_BYTE_WEIGHTS 0x01
#define FLAG_REVERSEDIACRITICS  0x10
#define FLAG_DOUBLECOMPRESSION  0x20
#define FLAG_INVERSECASING      0x40

NeutralToSpecific NeutralToSpecificMap[] =
{
	{ L"af", L"af-ZA" },
	{ L"am", L"am-ET" },
	{ L"ar", L"ar-SA" },
	{ L"arn", L"arn-CL" },
	{ L"as", L"as-IN" },
	{ L"az", L"az-Latn-AZ" },
	{ L"az-Cyrl", L"az-Cyrl-AZ" },
	{ L"az-Latn", L"az-Latn-AZ" },
	{ L"ba", L"ba-RU" },
	{ L"be", L"be-BY" },
	{ L"bg", L"bg-BG" },
	{ L"bn", L"bn-IN" },
	{ L"bo", L"bo-CN" },
	{ L"br", L"br-FR" },
	{ L"bs-Cyrl", L"bs-Cyrl-BA" },
	{ L"bs-Latn", L"bs-Latn-BA" },
	{ L"ca", L"ca-ES" },
	{ L"co", L"co-FR" },
	{ L"cs", L"cs-CZ" },
	{ L"cy", L"cy-GB" },
	{ L"da", L"da-DK" },
	{ L"de", L"de-DE" },
	{ L"dsb", L"dsb-DE" },
	{ L"dv", L"dv-MV" },
	{ L"el", L"el-GR" },
	{ L"en", L"en-US" },
	{ L"es", L"es-ES" },
	{ L"et", L"et-EE" },
	{ L"eu", L"eu-ES" },
	{ L"fa", L"fa-IR" },
	{ L"fi", L"fi-FI" },
	{ L"fil", L"fil-PH" },
	{ L"fo", L"fo-FO" },
	{ L"fr", L"fr-FR" },
	{ L"fy", L"fy-NL" },
	{ L"ga", L"ga-IE" },
	{ L"gd", L"gd-GB" },
	{ L"gl", L"gl-ES" },
	{ L"gsw", L"gsw-FR" },
	{ L"gu", L"gu-IN" },
	{ L"ha-Latn", L"ha-Latn-NG" },
	{ L"he", L"he-IL" },
	{ L"hi", L"hi-IN" },
	{ L"hr", L"hr-HR" },
	{ L"hsb", L"hsb-DE" },
	{ L"hu", L"hu-HU" },
	{ L"hy", L"hy-AM" },
	{ L"id", L"id-ID" },
	{ L"ig", L"ig-NG" },
	{ L"ii", L"ii-CN" },
	{ L"is", L"is-IS" },
	{ L"it", L"it-IT" },
	{ L"iu-Cans", L"iu-Cans-CA" },
	{ L"iu-Latn", L"iu-Latn-CA" },
	{ L"ja", L"ja-JP" },
	{ L"ka", L"ka-GE" },
	{ L"kk", L"kk-KZ" },
	{ L"kl", L"kl-GL" },
	{ L"km", L"km-KH" },
	{ L"kn", L"kn-IN" },
	{ L"ko", L"ko-KR" },
	{ L"kok", L"kok-IN" },
	{ L"ky", L"ky-KG" },
	{ L"lb", L"lb-LU" },
	{ L"lo", L"lo-LA" },
	{ L"lt", L"lt-LT" },
	{ L"lv", L"lv-LV" },
	{ L"mi", L"mi-NZ" },
	{ L"mk", L"mk-MK" },
	{ L"ml", L"ml-IN" },
	{ L"mn", L"mn-MN" },
	{ L"mn-Cyrl", L"mn-MN" },
	{ L"mn-Mong", L"mn-Mong-CN" },
	{ L"moh", L"moh-CA" },
	{ L"mr", L"mr-IN" },
	{ L"ms", L"ms-MY" },
	{ L"mt", L"mt-MT" },
	{ L"nb", L"nb-NO" },
	{ L"ne", L"ne-NP" },
	{ L"nl", L"nl-NL" },
	{ L"nn", L"nn-NO" },
	{ L"no", L"nb-NO" },
	{ L"nso", L"nso-ZA" },
	{ L"oc", L"oc-FR" },
	{ L"or", L"or-IN" },
	{ L"pa", L"pa-IN" },
	{ L"pl", L"pl-PL" },
	{ L"prs", L"prs-AF" },
	{ L"ps", L"ps-AF" },
	{ L"pt", L"pt-BR" },
	{ L"qut", L"qut-GT" },
	{ L"quz", L"quz-BO" },
	{ L"rm", L"rm-CH" },
	{ L"ro", L"ro-RO" },
	{ L"ru", L"ru-RU" },
	{ L"rw", L"rw-RW" },
	{ L"sa", L"sa-IN" },
	{ L"sah", L"sah-RU" },
	{ L"se", L"se-NO" },
	{ L"si", L"si-LK" },
	{ L"sk", L"sk-SK" },
	{ L"sl", L"sl-SI" },
	{ L"sma", L"sma-SE" },
	{ L"smj", L"smj-SE" },
	{ L"smn", L"smn-FI" },
	{ L"sms", L"sms-FI" },
	{ L"sq", L"sq-AL" },
	{ L"sr", L"sr-Latn-RS" },
	{ L"sr-Cyrl", L"sr-Cyrl-RS" },
	{ L"sr-Latn", L"sr-Latn-RS" },
	{ L"sv", L"sv-SE" },
	{ L"sw", L"sw-KE" },
	{ L"syr", L"syr-SY" },
	{ L"ta", L"ta-IN" },
	{ L"te", L"te-IN" },
	{ L"tg-Cyrl", L"tg-Cyrl-TJ" },
	{ L"th", L"th-TH" },
	{ L"tk", L"tk-TM" },
	{ L"tn", L"tn-ZA" },
	{ L"tr", L"tr-TR" },
	{ L"tt", L"tt-RU" },
	{ L"tzm-Latn", L"tzm-Latn-DZ" },
	{ L"ug", L"ug-CN" },
	{ L"uk", L"uk-UA" },
	{ L"ur", L"ur-PK" },
	{ L"uz", L"uz-Latn-UZ" },
	{ L"uz-Cyrl", L"uz-Cyrl-UZ" },
	{ L"uz-Latn", L"uz-Latn-UZ" },
	{ L"vi", L"vi-VN" },
	{ L"wo", L"wo-SN" },
	{ L"xh", L"xh-ZA" },
	{ L"yo", L"yo-NG" },
	{ L"zh-Hans", L"zh-CN" },
	{ L"zh-Hant", L"zh-HK" },
	{ L"zu", L"zu-ZA" },
};

INT 
WINAPI 
FindNLSStringEx(
	const WCHAR *localename, 
	DWORD flags, 
	const WCHAR *src,
    INT src_size, 
	const WCHAR *value, 
	INT value_size,
    INT *found, 
	NLSVERSIONINFO *version_info, 
	void *reserved,
    LPARAM sort_handle
);

struct sortguid
{
    GUID  id;          /* sort GUID */
    DWORD flags;       /* flags */
    DWORD compr;       /* offset to compression table */
    DWORD except;      /* exception table offset in sortkey table */
    DWORD ling_except; /* exception table offset for linguistic casing */
    DWORD casemap;     /* linguistic casemap table offset */
};

struct locale_name
{
    WCHAR  win_name[128];   /* Windows name ("en-US") */
    WCHAR  lang[128];       /* language ("en") (note: buffer contains the other strings too) */
    WCHAR *country;         /* country ("US") */
    WCHAR *charset;         /* charset ("UTF-8") for Unix format only */
    WCHAR *script;          /* script ("Latn") for Windows format only */
    WCHAR *modifier;        /* modifier or sort order */
    LCID   lcid;            /* corresponding LCID */
    int    matches;         /* number of elements matching LCID (0..4) */
    UINT   codepage;        /* codepage corresponding to charset */
};

static struct
{
    UINT                           version;         /* NLS version */
    UINT                           guid_count;      /* number of sort GUIDs */
    UINT                           exp_count;       /* number of character expansions */
    UINT                           compr_count;     /* number of compression tables */
    const UINT                    *keys;            /* sortkey table, indexed by char */
    const USHORT                  *casemap;         /* casemap table, in l_intl.nls format */
    const WORD                    *ctypes;          /* CT_CTYPE1,2,3 values */
    const BYTE                    *ctype_idx;       /* index to map char to ctypes array entry */
    const struct sortguid         *guids;           /* table of sort GUIDs */
    const struct sort_expansion   *expansions;      /* character expansions */
    const struct sort_compression *compressions;    /* character compression tables */
    const WCHAR                   *compr_data;      /* data for individual compressions */
    const struct jamo_sort        *jamo;            /* table for Jamo compositions */
} sort;

static const WCHAR iCalendarTypeW[] = {'i','C','a','l','e','n','d','a','r','T','y','p','e',0};
static const WCHAR iCountryW[] = {'i','C','o','u','n','t','r','y',0};
static const WCHAR iCurrDigitsW[] = {'i','C','u','r','r','D','i','g','i','t','s',0};
static const WCHAR iCurrencyW[] = {'i','C','u','r','r','e','n','c','y',0};
static const WCHAR iDateW[] = {'i','D','a','t','e',0};
static const WCHAR iDigitsW[] = {'i','D','i','g','i','t','s',0};
static const WCHAR iFirstDayOfWeekW[] = {'i','F','i','r','s','t','D','a','y','O','f','W','e','e','k',0};
static const WCHAR iFirstWeekOfYearW[] = {'i','F','i','r','s','t','W','e','e','k','O','f','Y','e','a','r',0};
static const WCHAR iLDateW[] = {'i','L','D','a','t','e',0};
static const WCHAR iLZeroW[] = {'i','L','Z','e','r','o',0};
static const WCHAR iMeasureW[] = {'i','M','e','a','s','u','r','e',0};
static const WCHAR iNegCurrW[] = {'i','N','e','g','C','u','r','r',0};
static const WCHAR iNegNumberW[] = {'i','N','e','g','N','u','m','b','e','r',0};
static const WCHAR iPaperSizeW[] = {'i','P','a','p','e','r','S','i','z','e',0};
static const WCHAR iTLZeroW[] = {'i','T','L','Z','e','r','o',0};
static const WCHAR iTimePrefixW[] = {'i','T','i','m','e','P','r','e','f','i','x',0};
static const WCHAR iTimeW[] = {'i','T','i','m','e',0};
static const WCHAR s1159W[] = {'s','1','1','5','9',0};
static const WCHAR s2359W[] = {'s','2','3','5','9',0};
static const WCHAR sCountryW[] = {'s','C','o','u','n','t','r','y',0};
static const WCHAR sCurrencyW[] = {'s','C','u','r','r','e','n','c','y',0};
static const WCHAR sDateW[] = {'s','D','a','t','e',0};
static const WCHAR sDecimalW[] = {'s','D','e','c','i','m','a','l',0};
static const WCHAR sGroupingW[] = {'s','G','r','o','u','p','i','n','g',0};
static const WCHAR sLanguageW[] = {'s','L','a','n','g','u','a','g','e',0};
static const WCHAR sListW[] = {'s','L','i','s','t',0};
static const WCHAR sLongDateW[] = {'s','L','o','n','g','D','a','t','e',0};
static const WCHAR sMonDecimalSepW[] = {'s','M','o','n','D','e','c','i','m','a','l','S','e','p',0};
static const WCHAR sMonGroupingW[] = {'s','M','o','n','G','r','o','u','p','i','n','g',0};
static const WCHAR sMonThousandSepW[] = {'s','M','o','n','T','h','o','u','s','a','n','d','S','e','p',0};
static const WCHAR sNativeDigitsW[] = {'s','N','a','t','i','v','e','D','i','g','i','t','s',0};
static const WCHAR sNegativeSignW[] = {'s','N','e','g','a','t','i','v','e','S','i','g','n',0};
static const WCHAR sPositiveSignW[] = {'s','P','o','s','i','t','i','v','e','S','i','g','n',0};
static const WCHAR sShortDateW[] = {'s','S','h','o','r','t','D','a','t','e',0};
static const WCHAR sThousandW[] = {'s','T','h','o','u','s','a','n','d',0};
static const WCHAR sTimeFormatW[] = {'s','T','i','m','e','F','o','r','m','a','t',0};
static const WCHAR sTimeW[] = {'s','T','i','m','e',0};
static const WCHAR sYearMonthW[] = {'s','Y','e','a','r','M','o','n','t','h',0};
static const WCHAR NumShapeW[] = {'N','u','m','s','h','a','p','e',0};

static struct registry_value
{
    DWORD           lctype;
    const WCHAR    *name;
    WCHAR          *cached_value;
} registry_values[] =
{
    { LOCALE_ICALENDARTYPE, iCalendarTypeW },
    { LOCALE_ICURRDIGITS, iCurrDigitsW },
    { LOCALE_ICURRENCY, iCurrencyW },
    { LOCALE_IDIGITS, iDigitsW },
    { LOCALE_IFIRSTDAYOFWEEK, iFirstDayOfWeekW },
    { LOCALE_IFIRSTWEEKOFYEAR, iFirstWeekOfYearW },
    { LOCALE_ILZERO, iLZeroW },
    { LOCALE_IMEASURE, iMeasureW },
    { LOCALE_INEGCURR, iNegCurrW },
    { LOCALE_INEGNUMBER, iNegNumberW },
    { LOCALE_IPAPERSIZE, iPaperSizeW },
    { LOCALE_ITIME, iTimeW },
    { LOCALE_S1159, s1159W },
    { LOCALE_S2359, s2359W },
    { LOCALE_SCURRENCY, sCurrencyW },
    { LOCALE_SDATE, sDateW },
    { LOCALE_SDECIMAL, sDecimalW },
    { LOCALE_SGROUPING, sGroupingW },
    { LOCALE_SLIST, sListW },
    { LOCALE_SLONGDATE, sLongDateW },
    { LOCALE_SMONDECIMALSEP, sMonDecimalSepW },
    { LOCALE_SMONGROUPING, sMonGroupingW },
    { LOCALE_SMONTHOUSANDSEP, sMonThousandSepW },
    { LOCALE_SNEGATIVESIGN, sNegativeSignW },
    { LOCALE_SPOSITIVESIGN, sPositiveSignW },
    { LOCALE_SSHORTDATE, sShortDateW },
    { LOCALE_STHOUSAND, sThousandW },
    { LOCALE_STIME, sTimeW },
    { LOCALE_STIMEFORMAT, sTimeFormatW },
    { LOCALE_SYEARMONTH, sYearMonthW },
    /* The following are not listed under MSDN as supported,
     * but seem to be used and also stored in the registry.
     */
    { LOCALE_ICOUNTRY, iCountryW },
    { LOCALE_IDATE, iDateW },
    { LOCALE_ILDATE, iLDateW },
    { LOCALE_ITLZERO, iTLZeroW },
    { LOCALE_SCOUNTRY, sCountryW },
    { LOCALE_SABBREVLANGNAME, sLanguageW },
    /* The following are used in XP and later */
    { LOCALE_IDIGITSUBSTITUTION, NumShapeW },
    { LOCALE_SNATIVEDIGITS, sNativeDigitsW },
    { LOCALE_ITIMEMARKPOSN, iTimePrefixW }
};


static void free_sortkey_state( struct sortkey_state *s )
{
    RtlFreeHeap( GetProcessHeap(), 0, s->key_primary.new_buf );
    RtlFreeHeap( GetProcessHeap(), 0, s->key_diacritic.new_buf );
    RtlFreeHeap( GetProcessHeap(), 0, s->key_case.new_buf );
    RtlFreeHeap( GetProcessHeap(), 0, s->key_special.new_buf );
    RtlFreeHeap( GetProcessHeap(), 0, s->key_extra[0].new_buf );
    RtlFreeHeap( GetProcessHeap(), 0, s->key_extra[1].new_buf );
    RtlFreeHeap( GetProcessHeap(), 0, s->key_extra[2].new_buf );
    RtlFreeHeap( GetProcessHeap(), 0, s->key_extra[3].new_buf );
}

static union char_weights get_char_weights( WCHAR c, UINT except )
{
    union char_weights ret;

    ret.val = except ? sort.keys[sort.keys[except + (c >> 8)] + (c & 0xff)] : sort.keys[c];
    return ret;
}

static const UINT *find_compression( const WCHAR *src, const WCHAR *table, int count, int len )
{
    int elem_size = compression_size( len ), min = 0, max = count - 1;

    while (min <= max)
    {
        int pos = (min + max) / 2;
        int res = wcsncmp( src, table + pos * elem_size, len );
        if (!res) return (UINT *)(table + (pos + 1) * elem_size) - 1;
        if (res > 0) min = pos + 1;
        else max = pos - 1;
    }
    return NULL;
}

/* find a compression for a char sequence */
/* return the number of extra chars to skip */
static int get_compression_weights( UINT compression, const WCHAR *compr_tables[8],
                                    const WCHAR *src, int srclen, union char_weights *weights )
{
    const struct sort_compression *compr = sort.compressions + compression;
    const UINT *ret;
    BYTE size = weights->_case & CASE_COMPR_6;
    int i, maxlen = 1;

    if (compression >= sort.compr_count) return 0;
    if (size == CASE_COMPR_6) maxlen = 8;
    else if (size == CASE_COMPR_4) maxlen = 5;
    else if (size == CASE_COMPR_2) maxlen = 3;
    maxlen = min( maxlen, srclen );
    for (i = 0; i < maxlen; i++) if (src[i] < compr->minchar || src[i] > compr->maxchar) break;
    maxlen = i;
    if (!compr_tables[0])
    {
        compr_tables[0] = sort.compr_data + compr->offset;
        for (i = 1; i < 8; i++)
            compr_tables[i] = compr_tables[i - 1] + compr->len[i - 1] * compression_size( i + 1 );
    }
    for (i = maxlen - 2; i >= 0; i--)
    {
        if (!(ret = find_compression( src, compr_tables[i], compr->len[i], i + 2 ))) continue;
        weights->val = *ret;
        return i + 1;
    }
    return 0;
}

static void append_sortkey( struct sortkey *key, BYTE val )
{
    if (key->len >= key->max) return;
    if (key->len >= key->size)
    {
        key->new_buf = RtlAllocateHeap( GetProcessHeap(), 0, key->max );
        if (key->new_buf) memcpy( key->new_buf, key->buf, key->len );
        else key->max = 0;
        key->buf = key->new_buf;
        key->size = key->max;
    }
    key->buf[key->len++] = val;
}

static int compare_sortkeys( const struct sortkey *key1, const struct sortkey *key2, BOOL shorter_wins )
{
    int ret = memcmp( key1->buf, key2->buf, min( key1->len, key2->len ));
    if (!ret) ret = shorter_wins ? key2->len - key1->len : key1->len - key2->len;
    return ret;
}

static void append_normal_weights( const struct sortguid *sortid, struct sortkey *key_primary,
                                   struct sortkey *key_diacritic, struct sortkey *key_case,
                                   union char_weights weights, DWORD flags )
{
    append_sortkey( key_primary, weights.script );
    append_sortkey( key_primary, weights.primary );

    if ((weights.script >= SCRIPT_PUA_FIRST && weights.script <= SCRIPT_PUA_LAST) ||
        ((sortid->flags & FLAG_HAS_3_BYTE_WEIGHTS) &&
         (weights.script >= SCRIPT_CJK_FIRST && weights.script <= SCRIPT_CJK_LAST)))
    {
        append_sortkey( key_primary, weights.diacritic );
        append_sortkey( key_case, weights._case );
        return;
    }
    if (weights.script <= SCRIPT_ARABIC && weights.script != SCRIPT_HEBREW)
    {
        if (flags & LINGUISTIC_IGNOREDIACRITIC) weights.diacritic = 2;
        if (flags & LINGUISTIC_IGNORECASE) weights._case = 2;
    }
    append_sortkey( key_diacritic, weights.diacritic );
    append_sortkey( key_case, weights._case );
}

static void append_nonspace_weights( struct sortkey *key, union char_weights weights, DWORD flags )
{
    if (flags & LINGUISTIC_IGNOREDIACRITIC) weights.diacritic = 2;
    if (key->len) key->buf[key->len - 1] += weights.diacritic;
    else append_sortkey( key, weights.diacritic );
}

static void append_expansion_weights( const struct sortguid *sortid, struct sortkey *key_primary,
                                      struct sortkey *key_diacritic, struct sortkey *key_case,
                                      union char_weights weights, DWORD flags, BOOL is_compare )
{
    /* sortkey and comparison behave differently here */
    if (is_compare)
    {
        if (weights.script == SCRIPT_UNSORTABLE) return;
        if (weights.script == SCRIPT_NONSPACE_MARK)
        {
            append_nonspace_weights( key_diacritic, weights, flags );
            return;
        }
    }
    append_normal_weights( sortid, key_primary, key_diacritic, key_case, weights, flags );
}


/* get the zero digit for the digit character range that contains 'ch' */
static WCHAR get_digit_zero_char( WCHAR ch )
{
    static const WCHAR zeroes[] =
    {
        0x0030, 0x0660, 0x06f0, 0x0966, 0x09e6, 0x0a66, 0x0ae6, 0x0b66, 0x0be6, 0x0c66,
        0x0ce6, 0x0d66, 0x0e50, 0x0ed0, 0x0f20, 0x1040, 0x1090, 0x17e0, 0x1810, 0x1946,
        0x1bb0, 0x1c40, 0x1c50, 0xa620, 0xa8d0, 0xa900, 0xaa50, 0xff10
    };
    int min = 0, max = ARRAY_SIZE( zeroes ) - 1;

    while (min <= max)
    {
        int pos = (min + max) / 2;
        if (zeroes[pos] <= ch && zeroes[pos] + 9 >= ch) return zeroes[pos];
        if (zeroes[pos] < ch) min = pos + 1;
        else max = pos - 1;
    }
    return 0;
}

/* append weights for digits when using SORT_DIGITSASNUMBERS */
/* return the number of extra chars to skip */
static int append_digit_weights( struct sortkey *key, const WCHAR *src, UINT srclen )
{
    UINT i, zero, len, lzero;
    BYTE val, values[19];

    if (!(zero = get_digit_zero_char( *src ))) return -1;

    values[0] = *src - zero;
    for (len = 1; len < ARRAY_SIZE(values) && len < srclen; len++)
    {
        if (src[len] < zero || src[len] > zero + 9) break;
        values[len] = src[len] - zero;
    }
    for (lzero = 0; lzero < len; lzero++) if (values[lzero]) break;

    append_sortkey( key, SCRIPT_DIGIT );
    append_sortkey( key, 2 );
    append_sortkey( key, 2 + len - lzero );
    for (i = lzero, val = 2; i < len; i++)
    {
        if ((len - i) % 2) append_sortkey( key, (val << 4) + values[i] + 2 );
        else val = values[i] + 2;
    }
    append_sortkey( key, 0xfe - lzero );
    return len - 1;
}

/* append the extra weights for kana prolonged sound / repeat marks */
static int append_extra_kana_weights( struct sortkey keys[4], const WCHAR *src, int pos, UINT except,
                                      BYTE case_mask, union char_weights *weights )
{
    BYTE extra1 = 3, case_weight = weights->_case;

    if (weights->primary <= 1)
    {
        while (pos > 0)
        {
            union char_weights prev = get_char_weights( src[--pos], except );
            if (prev.script == SCRIPT_UNSORTABLE || prev.script == SCRIPT_NONSPACE_MARK) continue;
            if (prev.script == SCRIPT_EXPANSION) return 0;
            if (prev.script != SCRIPT_EASTASIA_SPECIAL)
            {
                *weights = prev;
                return 1;
            }
            if (prev.primary <= 1) continue;

            case_weight = prev._case & case_mask;
            if (weights->primary == 1)  /* prolonged sound mark */
            {
                prev.primary &= 0x87;
                case_weight &= ~CASE_FULLWIDTH;
                case_weight |= weights->_case & CASE_FULLWIDTH;
            }
            extra1 = 4 + weights->primary;
            weights->primary = prev.primary;
            goto done;
        }
        return 0;
    }
done:
    append_sortkey( &keys[0], 0xc4 | (case_weight & CASE_FULLSIZE) );
    append_sortkey( &keys[1], extra1 );
    append_sortkey( &keys[2], 0xc4 | (case_weight & CASE_KATAKANA) );
    append_sortkey( &keys[3], 0xc4 | (case_weight & CASE_FULLWIDTH) );
    weights->script = SCRIPT_KANA;
    return 1;
}


#define HANGUL_SBASE  0xac00
#define HANGUL_LCOUNT 19
#define HANGUL_VCOUNT 21
#define HANGUL_TCOUNT 28

static int append_hangul_weights( struct sortkey *key, const WCHAR *src, int srclen, UINT except )
{
    int leading_idx = 0x115f - 0x1100;  /* leading filler */
    int vowel_idx = 0x1160 - 0x1100;  /* vowel filler */
    int trailing_idx = -1;
    BYTE leading_off, vowel_off, trailing_off;
    union char_weights weights;
    WCHAR composed;
    BYTE filler_mask = 0;
    int pos = 0;

    /* leading */
    if (src[pos] >= 0x1100 && src[pos] <= 0x115f) leading_idx = src[pos++] - 0x1100;
    else if (src[pos] >= 0xa960 && src[pos] <= 0xa97c) leading_idx = src[pos++] - (0xa960 - 0x100);

    /* vowel */
    if (srclen > pos)
    {
        if (src[pos] >= 0x1160 && src[pos] <= 0x11a7) vowel_idx = src[pos++] - 0x1100;
        else if (src[pos] >= 0xd7b0 && src[pos] <= 0xd7c6) vowel_idx = src[pos++] - (0xd7b0 - 0x11d);
    }

    /* trailing */
    if (srclen > pos)
    {
        if (src[pos] >= 0x11a8 && src[pos] <= 0x11ff) trailing_idx = src[pos++] - 0x1100;
        else if (src[pos] >= 0xd7cb && src[pos] <= 0xd7fb) trailing_idx = src[pos++] - (0xd7cb - 0x134);
    }

    if (!sort.jamo[leading_idx].is_old && !sort.jamo[vowel_idx].is_old &&
        (trailing_idx == -1 || !sort.jamo[trailing_idx].is_old))
    {
        /* not old Hangul, only use leading char; vowel and trailing will be handled in the next pass */
        pos = 1;
        vowel_idx = 0x1160 - 0x1100;
        trailing_idx = -1;
    }

    leading_off = max( sort.jamo[leading_idx].leading, sort.jamo[vowel_idx].leading );
    vowel_off = max( sort.jamo[leading_idx].vowel, sort.jamo[vowel_idx].vowel );
    trailing_off = max( sort.jamo[leading_idx].trailing, sort.jamo[vowel_idx].trailing );
    if (trailing_idx != -1) trailing_off = max( trailing_off, sort.jamo[trailing_idx].trailing );
    composed = HANGUL_SBASE + (leading_off * HANGUL_VCOUNT + vowel_off) * HANGUL_TCOUNT + trailing_off;

    if (leading_idx == 0x115f - 0x1100 || vowel_idx == 0x1160 - 0x1100)
    {
        filler_mask = 0x80;
        composed--;
    }
    if (composed < HANGUL_SBASE) composed = 0x3260;

    weights = get_char_weights( composed, except );
    append_sortkey( key, weights.script );
    append_sortkey( key, weights.primary );
    append_sortkey( key, 0xff );
    append_sortkey( key, sort.jamo[leading_idx].weight | filler_mask );
    append_sortkey( key, 0xff );
    append_sortkey( key, sort.jamo[vowel_idx].weight );
    append_sortkey( key, 0xff );
    append_sortkey( key, trailing_idx != -1 ? sort.jamo[trailing_idx].weight : 2 );
    return pos - 1;
}

static int append_weights( const struct sortguid *sortid, DWORD flags,
                           const WCHAR *src, int srclen, int pos, BYTE case_mask, UINT except,
                           const WCHAR *compr_tables[8], struct sortkey_state *s, BOOL is_compare )
{
    union char_weights weights = get_char_weights( src[pos], except );
    WCHAR idx = (weights.val >> 16) & ~(CASE_COMPR_6 << 8);  /* expansion index */
    int ret = 1;

    if (weights._case & CASE_COMPR_6)
        ret += get_compression_weights( sortid->compr, compr_tables, src + pos, srclen - pos, &weights );

    weights._case &= case_mask;

    switch (weights.script)
    {
    case SCRIPT_UNSORTABLE:
        break;

    case SCRIPT_NONSPACE_MARK:
        append_nonspace_weights( &s->key_diacritic, weights, flags );
        break;

    case SCRIPT_EXPANSION:
        while (weights.script == SCRIPT_EXPANSION)
        {
            weights = get_char_weights( sort.expansions[idx].exp[0], except );
            weights._case &= case_mask;
            append_expansion_weights( sortid, &s->key_primary, &s->key_diacritic,
                                      &s->key_case, weights, flags, is_compare );
            weights = get_char_weights( sort.expansions[idx].exp[1], except );
            idx = weights.val >> 16;
            weights._case &= case_mask;
        }
        append_expansion_weights( sortid, &s->key_primary, &s->key_diacritic,
                                  &s->key_case, weights, flags, is_compare );
        break;

    case SCRIPT_EASTASIA_SPECIAL:
        if (!append_extra_kana_weights( s->key_extra, src, pos, except, case_mask, &weights ))
        {
            append_sortkey( &s->key_primary, 0xff );
            append_sortkey( &s->key_primary, 0xff );
            break;
        }
        weights._case = 2;
        append_normal_weights( sortid, &s->key_primary, &s->key_diacritic, &s->key_case, weights, flags );
        break;

    case SCRIPT_JAMO_SPECIAL:
        ret += append_hangul_weights( &s->key_primary, src + pos, srclen - pos, except );
        append_sortkey( &s->key_diacritic, 2 );
        append_sortkey( &s->key_case, 2 );
        break;

    case SCRIPT_EXTENSION_A:
        append_sortkey( &s->key_primary, 0xfd );
        append_sortkey( &s->key_primary, 0xff );
        append_sortkey( &s->key_primary, weights.primary );
        append_sortkey( &s->key_primary, weights.diacritic );
        append_sortkey( &s->key_diacritic, 2 );
        append_sortkey( &s->key_case, 2 );
        break;

    case SCRIPT_PUNCTUATION:
        if (flags & NORM_IGNORESYMBOLS) break;
        if (!(flags & SORT_STRINGSORT))
        {
            short len = -((s->key_primary.len + s->primary_pos) / 2) - 1;
            if (flags & LINGUISTIC_IGNORECASE) weights._case = 2;
            if (flags & LINGUISTIC_IGNOREDIACRITIC) weights.diacritic = 2;
            append_sortkey( &s->key_special, len >> 8 );
            append_sortkey( &s->key_special, len & 0xff );
            append_sortkey( &s->key_special, weights.primary );
            append_sortkey( &s->key_special, weights._case | (weights.diacritic << 3) );
            break;
        }
        /* fall through */
    case SCRIPT_SYMBOL_1:
    case SCRIPT_SYMBOL_2:
    case SCRIPT_SYMBOL_3:
    case SCRIPT_SYMBOL_4:
    case SCRIPT_SYMBOL_5:
    case SCRIPT_SYMBOL_6:
        if (flags & NORM_IGNORESYMBOLS) break;
        append_sortkey( &s->key_primary, weights.script );
        append_sortkey( &s->key_primary, weights.primary );
        append_sortkey( &s->key_diacritic, weights.diacritic );
        append_sortkey( &s->key_case, weights._case );
        break;

    case SCRIPT_DIGIT:
        if (flags & SORT_DIGITSASNUMBERS)
        {
            int len = append_digit_weights( &s->key_primary, src + pos, srclen - pos );
            if (len >= 0)
            {
                ret += len;
                append_sortkey( &s->key_diacritic, weights.diacritic );
                append_sortkey( &s->key_case, weights._case );
                break;
            }
        }
        /* fall through */
    default:
        append_normal_weights( sortid, &s->key_primary, &s->key_diacritic, &s->key_case, weights, flags );
        break;
    }

    return ret;
}

static void reverse_sortkey( struct sortkey *key )
{
    int i;

    for (i = 0; i < key->len / 2; i++)
    {
        BYTE tmp = key->buf[key->len - i - 1];
        key->buf[key->len - i - 1] = key->buf[i];
        key->buf[i] = tmp;
    }
}

static void init_sortkey_state( struct sortkey_state *s, DWORD flags, UINT srclen,
                                BYTE *primary_buf, UINT primary_size )
{
    /* buffer for secondary weights */
    BYTE *secondary_buf = s->buffer;
    UINT secondary_size;

    memset( s, 0, offsetof( struct sortkey_state, buffer ));

    s->key_primary.buf  = primary_buf;
    s->key_primary.size = primary_size;

    if (!(flags & NORM_IGNORENONSPACE))  /* reserve space for diacritics */
    {
        secondary_size = sizeof(s->buffer) / 3;
        s->key_diacritic.buf = secondary_buf;
        s->key_diacritic.size = secondary_size;
        secondary_buf += secondary_size;
    }
    else secondary_size = sizeof(s->buffer) / 2;

    s->key_case.buf = secondary_buf;
    s->key_case.size = secondary_size;
    s->key_special.buf = secondary_buf + secondary_size;
    s->key_special.size = secondary_size;

    s->key_primary.max = srclen * 8;
    s->key_case.max = srclen * 3;
    s->key_special.max = srclen * 4;
    s->key_extra[2].max = s->key_extra[3].max = srclen;
    if (!(flags & NORM_IGNORENONSPACE))
    {
        s->key_diacritic.max = srclen * 3;
        s->key_extra[0].max = s->key_extra[1].max = srclen;
    }
}

static BOOL remove_unneeded_weights( const struct sortguid *sortid, struct sortkey_state *s )
{
    const BYTE ignore[4] = { 0xc4 | CASE_FULLSIZE, 0x03, 0xc4 | CASE_KATAKANA, 0xc4 | CASE_FULLWIDTH };
    int i, j;

    if (sortid->flags & FLAG_REVERSEDIACRITICS) reverse_sortkey( &s->key_diacritic );

    for (i = s->key_diacritic.len; i > 0; i--) if (s->key_diacritic.buf[i - 1] > 2) break;
    s->key_diacritic.len = i;

    for (i = s->key_case.len; i > 0; i--) if (s->key_case.buf[i - 1] > 2) break;
    s->key_case.len = i;

    if (!s->key_extra[2].len) return FALSE;

    for (i = 0; i < 4; i++)
    {
        for (j = s->key_extra[i].len; j > 0; j--) if (s->key_extra[i].buf[j - 1] != ignore[i]) break;
        s->key_extra[i].len = j;
    }
    return TRUE;
}

static const struct sortguid *find_sortguid( const GUID *guid )
{
    int pos, ret, min = 0, max = sort.guid_count - 1;

    while (min <= max)
    {
        pos = (min + max) / 2;
        ret = memcmp( guid, &sort.guids[pos].id, sizeof(*guid) );
        if (!ret) return &sort.guids[pos];
        if (ret > 0) min = pos + 1;
        else max = pos - 1;
    }
    //ERR( "no sort found for %s\n", debugstr_guid( guid ));
    return NULL;
}

static const struct sortguid *get_language_sort( const WCHAR *locale )
{
    WCHAR *p, *end, buffer[LOCALE_NAME_MAX_LENGTH], guidstr[39];
    const struct sortguid *ret;
    UNICODE_STRING str;
    GUID guid;
    HKEY key = 0;
    DWORD size, type;

    if (locale == LOCALE_NAME_USER_DEFAULT)
    {
        if (current_locale_sort) return current_locale_sort;
        GetUserDefaultLocaleName( buffer, ARRAY_SIZE( buffer ));
    }
    else lstrcpynW( buffer, locale, LOCALE_NAME_MAX_LENGTH );

    if (buffer[0] && !RegOpenKeyExW( nls_key, L"Sorting\\Ids", 0, KEY_READ, &key ))
    {
        for (;;)
        {
            size = sizeof(guidstr);
            if (!RegQueryValueExW( key, buffer, NULL, &type, (BYTE *)guidstr, &size ) && type == REG_SZ)
            {
                RtlInitUnicodeString( &str, guidstr );
                if (!RtlGUIDFromString( &str, &guid ))
                {
                    ret = find_sortguid( &guid );
                    goto done;
                }
                break;
            }
            for (p = end = buffer; *p; p++) if (*p == '-' || *p == '_') end = p;
            if (end == buffer) break;
            *end = 0;
        }
    }
    ret = find_sortguid( &default_sort_guid );
done:
    RegCloseKey( key );
    return ret;
}

/***********************************************************************
 *		init_locale
 */
void init_locale(void)
{
	current_locale_sort = get_language_sort( LOCALE_NAME_USER_DEFAULT );
    RegCreateKeyExW( HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\Nls",
                     0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &nls_key, NULL );	
    RegCreateKeyExW( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones",
                     0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &tz_key, NULL );					
}

struct enum_locale_ex_data
{
    LOCALE_ENUMPROCEX proc;
    DWORD             flags;
    LPARAM            lparam;
};


void InitializeCriticalForLocaleInfo(){
	 RtlInitializeCriticalSection(&cache_section);
}

static void init_sortkeys( DWORD *ptr )
{
    WORD *ctype;
    DWORD *table;

    sort.keys = (DWORD *)((char *)ptr + ptr[0]);
    sort.casemap = (USHORT *)((char *)ptr + ptr[1]);

    ctype = (WORD *)((char *)ptr + ptr[2]);
    sort.ctypes = ctype + 2;
    sort.ctype_idx = (BYTE *)ctype + ctype[1] + 2;

    table = (DWORD *)((char *)ptr + ptr[3]);
    sort.version = table[0];
    sort.guid_count = table[1];
    sort.guids = (struct sortguid *)(table + 2);
}

/***********************************************************************
 *           is_genitive_name_supported
 *
 * Determine could LCTYPE basically support genitive name form or not.
 */
static BOOL is_genitive_name_supported( LCTYPE lctype )
{
    switch(lctype & 0xffff)
    {
    case LOCALE_SMONTHNAME1:
    case LOCALE_SMONTHNAME2:
    case LOCALE_SMONTHNAME3:
    case LOCALE_SMONTHNAME4:
    case LOCALE_SMONTHNAME5:
    case LOCALE_SMONTHNAME6:
    case LOCALE_SMONTHNAME7:
    case LOCALE_SMONTHNAME8:
    case LOCALE_SMONTHNAME9:
    case LOCALE_SMONTHNAME10:
    case LOCALE_SMONTHNAME11:
    case LOCALE_SMONTHNAME12:
    case LOCALE_SMONTHNAME13:
         return TRUE;
    default:
         return FALSE;
    }
}

/***********************************************************************
 *           convert_default_lcid
 *
 * Get the default LCID to use for a given lctype in GetLocaleInfo.
 */
static LCID convert_default_lcid( LCID lcid, LCTYPE lctype )
{
    if (lcid == LOCALE_SYSTEM_DEFAULT ||
        lcid == LOCALE_USER_DEFAULT ||
        lcid == LOCALE_NEUTRAL)
    {
        LCID default_id = 0;

        switch(lctype & 0xffff)
        {
        case LOCALE_SSORTNAME:
            default_id = lcid_LC_COLLATE;
            break;

        case LOCALE_FONTSIGNATURE:
        case LOCALE_IDEFAULTANSICODEPAGE:
        case LOCALE_IDEFAULTCODEPAGE:
        case LOCALE_IDEFAULTEBCDICCODEPAGE:
        case LOCALE_IDEFAULTMACCODEPAGE:
        case LOCALE_IDEFAULTUNIXCODEPAGE:
            default_id = lcid_LC_CTYPE;
            break;

        case LOCALE_ICURRDIGITS:
        case LOCALE_ICURRENCY:
        case LOCALE_IINTLCURRDIGITS:
        case LOCALE_INEGCURR:
        case LOCALE_INEGSEPBYSPACE:
        case LOCALE_INEGSIGNPOSN:
        case LOCALE_INEGSYMPRECEDES:
        case LOCALE_IPOSSEPBYSPACE:
        case LOCALE_IPOSSIGNPOSN:
        case LOCALE_IPOSSYMPRECEDES:
        case LOCALE_SCURRENCY:
        case LOCALE_SINTLSYMBOL:
        case LOCALE_SMONDECIMALSEP:
        case LOCALE_SMONGROUPING:
        case LOCALE_SMONTHOUSANDSEP:
        case LOCALE_SNATIVECURRNAME:
            default_id = lcid_LC_MONETARY;
            break;

        case LOCALE_IDIGITS:
        case LOCALE_IDIGITSUBSTITUTION:
        case LOCALE_ILZERO:
        case LOCALE_INEGNUMBER:
        case LOCALE_SDECIMAL:
        case LOCALE_SGROUPING:
        case LOCALE_SNAN:
        case LOCALE_SNATIVEDIGITS:
        case LOCALE_SNEGATIVESIGN:
        case LOCALE_SNEGINFINITY:
        case LOCALE_SPOSINFINITY:
        case LOCALE_SPOSITIVESIGN:
        case LOCALE_STHOUSAND:
            default_id = lcid_LC_NUMERIC;
            break;

        case LOCALE_ICALENDARTYPE:
        case LOCALE_ICENTURY:
        case LOCALE_IDATE:
        case LOCALE_IDAYLZERO:
        case LOCALE_IFIRSTDAYOFWEEK:
        case LOCALE_IFIRSTWEEKOFYEAR:
        case LOCALE_ILDATE:
        case LOCALE_IMONLZERO:
        case LOCALE_IOPTIONALCALENDAR:
        case LOCALE_ITIME:
        case LOCALE_ITIMEMARKPOSN:
        case LOCALE_ITLZERO:
        case LOCALE_S1159:
        case LOCALE_S2359:
        case LOCALE_SABBREVDAYNAME1:
        case LOCALE_SABBREVDAYNAME2:
        case LOCALE_SABBREVDAYNAME3:
        case LOCALE_SABBREVDAYNAME4:
        case LOCALE_SABBREVDAYNAME5:
        case LOCALE_SABBREVDAYNAME6:
        case LOCALE_SABBREVDAYNAME7:
        case LOCALE_SABBREVMONTHNAME1:
        case LOCALE_SABBREVMONTHNAME2:
        case LOCALE_SABBREVMONTHNAME3:
        case LOCALE_SABBREVMONTHNAME4:
        case LOCALE_SABBREVMONTHNAME5:
        case LOCALE_SABBREVMONTHNAME6:
        case LOCALE_SABBREVMONTHNAME7:
        case LOCALE_SABBREVMONTHNAME8:
        case LOCALE_SABBREVMONTHNAME9:
        case LOCALE_SABBREVMONTHNAME10:
        case LOCALE_SABBREVMONTHNAME11:
        case LOCALE_SABBREVMONTHNAME12:
        case LOCALE_SABBREVMONTHNAME13:
        case LOCALE_SDATE:
        case LOCALE_SDAYNAME1:
        case LOCALE_SDAYNAME2:
        case LOCALE_SDAYNAME3:
        case LOCALE_SDAYNAME4:
        case LOCALE_SDAYNAME5:
        case LOCALE_SDAYNAME6:
        case LOCALE_SDAYNAME7:
        case LOCALE_SDURATION:
        case LOCALE_SLONGDATE:
        case LOCALE_SMONTHNAME1:
        case LOCALE_SMONTHNAME2:
        case LOCALE_SMONTHNAME3:
        case LOCALE_SMONTHNAME4:
        case LOCALE_SMONTHNAME5:
        case LOCALE_SMONTHNAME6:
        case LOCALE_SMONTHNAME7:
        case LOCALE_SMONTHNAME8:
        case LOCALE_SMONTHNAME9:
        case LOCALE_SMONTHNAME10:
        case LOCALE_SMONTHNAME11:
        case LOCALE_SMONTHNAME12:
        case LOCALE_SMONTHNAME13:
        case LOCALE_SSHORTDATE:
        case LOCALE_SSHORTESTDAYNAME1:
        case LOCALE_SSHORTESTDAYNAME2:
        case LOCALE_SSHORTESTDAYNAME3:
        case LOCALE_SSHORTESTDAYNAME4:
        case LOCALE_SSHORTESTDAYNAME5:
        case LOCALE_SSHORTESTDAYNAME6:
        case LOCALE_SSHORTESTDAYNAME7:
        case LOCALE_STIME:
        case LOCALE_STIMEFORMAT:
        case LOCALE_SYEARMONTH:
            default_id = lcid_LC_TIME;
            break;

        case LOCALE_IPAPERSIZE:
            default_id = lcid_LC_PAPER;
            break;

        case LOCALE_IMEASURE:
            default_id = lcid_LC_MEASUREMENT;
            break;

        case LOCALE_ICOUNTRY:
            default_id = lcid_LC_TELEPHONE;
            break;
        }
        if (default_id) lcid = default_id;
    }
    return ConvertDefaultLocale( lcid );
}

/******************************************************************************
 *		get_locale_registry_value
 *
 * Gets the registry value name and cache for a given lctype.
 */
static struct registry_value *get_locale_registry_value( DWORD lctype )
{
    int i;
    for (i=0; i < sizeof(registry_values)/sizeof(registry_values[0]); i++)
        if (registry_values[i].lctype == lctype)
            return &registry_values[i];
    return NULL;
}

/***********************************************************************
 *		create_registry_key
 *
 * Create the Control Panel\\International registry key.
 */
static inline HANDLE create_registry_key(void)
{
    static const WCHAR cplW[] = {'C','o','n','t','r','o','l',' ','P','a','n','e','l',0};
    static const WCHAR intlW[] = {'I','n','t','e','r','n','a','t','i','o','n','a','l',0};
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    HANDLE cpl_key, hkey = 0;

    if (RtlOpenCurrentUser( KEY_ALL_ACCESS, &hkey ) != STATUS_SUCCESS) return 0;

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, cplW );

    if (!NtCreateKey( &cpl_key, KEY_ALL_ACCESS, &attr, 0, NULL, 0, NULL ))
    {
        NtClose( attr.RootDirectory );
        attr.RootDirectory = cpl_key;
        RtlInitUnicodeString( &nameW, intlW );
        if (NtCreateKey( &hkey, KEY_ALL_ACCESS, &attr, 0, NULL, 0, NULL )) hkey = 0;
    }
    NtClose( attr.RootDirectory );
    return hkey;
}

/******************************************************************************
 *		get_registry_locale_info
 *
 * Retrieve user-modified locale info from the registry.
 * Return length, 0 on error, -1 if not found.
 */
static INT get_registry_locale_info( struct registry_value *registry_value, LPWSTR buffer, INT len )
{
    DWORD size;
    INT ret;
    HANDLE hkey;
    NTSTATUS status;
    UNICODE_STRING nameW;
    KEY_VALUE_PARTIAL_INFORMATION *info;
    static const int info_size = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data);

    RtlEnterCriticalSection( &cache_section );

    if (!registry_value->cached_value)
    {
        if (!(hkey = create_registry_key()))
        {
            RtlLeaveCriticalSection( &cache_section );
            return -1;
        }

        RtlInitUnicodeString( &nameW, registry_value->name );
        size = info_size + len * sizeof(WCHAR);

        if (!(info = HeapAlloc( GetProcessHeap(), 0, size )))
        {
            NtClose( hkey );
            SetLastError( ERROR_NOT_ENOUGH_MEMORY );
            RtlLeaveCriticalSection( &cache_section );
            return 0;
        }

        status = NtQueryValueKey( hkey, &nameW, KeyValuePartialInformation, info, size, &size );

        /* try again with a bigger buffer when we have to return the correct size */
        if (status == STATUS_BUFFER_OVERFLOW && !buffer && size > info_size)
        {
            KEY_VALUE_PARTIAL_INFORMATION *new_info;
            if ((new_info = HeapReAlloc( GetProcessHeap(), 0, info, size )))
            {
                info = new_info;
                status = NtQueryValueKey( hkey, &nameW, KeyValuePartialInformation, info, size, &size );
            }
        }

        NtClose( hkey );

        if (!status)
        {
            INT length = (size - info_size) / sizeof(WCHAR);
            LPWSTR cached_value;

            if (!length || ((WCHAR *)&info->Data)[length-1])
                length++;

            cached_value = HeapAlloc( GetProcessHeap(), 0, length * sizeof(WCHAR) );

            if (!cached_value)
            {
                HeapFree( GetProcessHeap(), 0, info );
                SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                RtlLeaveCriticalSection( &cache_section );
                return 0;
            }

            memcpy( cached_value, info->Data, (length-1) * sizeof(WCHAR) );
            cached_value[length-1] = 0;
            HeapFree( GetProcessHeap(), 0, info );
            registry_value->cached_value = cached_value;
        }
        else
        {
            if (status == STATUS_BUFFER_OVERFLOW && !buffer)
            {
                ret = (size - info_size) / sizeof(WCHAR);
            }
            else if (status == STATUS_OBJECT_NAME_NOT_FOUND)
            {
                ret = -1;
            }
            else
            {
                SetLastError( RtlNtStatusToDosError(status) );
                ret = 0;
            }
            HeapFree( GetProcessHeap(), 0, info );
            RtlLeaveCriticalSection( &cache_section );
            return ret;
        }
    }

    ret = lstrlenW( registry_value->cached_value ) + 1;

    if (buffer)
    {
        if (ret > len)
        {
            SetLastError( ERROR_INSUFFICIENT_BUFFER );
            ret = 0;
        }
        else
        {
            lstrcpyW( buffer, registry_value->cached_value );
        }
    }

    RtlLeaveCriticalSection( &cache_section );

    return ret;
}

static int get_value_base_by_lctype( LCTYPE lctype )
{
    return lctype == LOCALE_ILANGUAGE || lctype == LOCALE_IDEFAULTLANGUAGE ? 16 : 10;
}

static LANGID get_default_sublang( LANGID lang )
{
    switch (lang)
    {
    case MAKELANGID( LANG_SPANISH, SUBLANG_NEUTRAL ):
        return MAKELANGID( LANG_SPANISH, SUBLANG_SPANISH_MODERN );
    case MAKELANGID( LANG_CHINESE, SUBLANG_NEUTRAL ):
        return MAKELANGID( LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED );
    case MAKELANGID( LANG_CHINESE, SUBLANG_CHINESE_SINGAPORE ):
        return MAKELANGID( LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED );
    case MAKELANGID( LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL ):
    case MAKELANGID( LANG_CHINESE, SUBLANG_CHINESE_MACAU ):
        return MAKELANGID( LANG_CHINESE, SUBLANG_CHINESE_HONGKONG );
    }
    if (SUBLANGID( lang ) == SUBLANG_NEUTRAL) lang = MAKELANGID( PRIMARYLANGID(lang), SUBLANG_DEFAULT );
    return lang;
}

/******************************************************************************
 *		GetLocaleInfoW (KERNEL32.@)
 *
 * See GetLocaleInfoA.
 */
INT WINAPI GetLocaleInfoInternalW( LCID lcid, LCTYPE lctype, LPWSTR buffer, INT len )
{
    LANGID lang_id;
    HRSRC hrsrc;
    HGLOBAL hmem;
    INT ret;
    UINT lcflags;
    const WCHAR *p;
    unsigned int i;

    if (len < 0 || (len && !buffer))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }
    if (lctype & LOCALE_RETURN_GENITIVE_NAMES &&
       !is_genitive_name_supported( lctype ))
    {
        SetLastError( ERROR_INVALID_FLAGS );
        return 0;
    }

    if (!len) buffer = NULL;

    lcid = convert_default_lcid( lcid, lctype );

    lcflags = lctype & LOCALE_LOCALEINFOFLAGSMASK;
    lctype &= 0xffff;

    TRACE( "(lcid=0x%x,lctype=0x%x,%p,%d)\n", lcid, lctype, buffer, len );

    /* first check for overrides in the registry */

    if (!(lcflags & LOCALE_NOUSEROVERRIDE) &&
        lcid == convert_default_lcid( LOCALE_USER_DEFAULT, lctype ))
    {
        struct registry_value *value = get_locale_registry_value(lctype);

        if (value)
        {
            if (lcflags & LOCALE_RETURN_NUMBER)
            {
                WCHAR tmp[16];
                ret = get_registry_locale_info( value, tmp, sizeof(tmp)/sizeof(WCHAR) );
                if (ret > 0)
                {
                    WCHAR *end;
                    UINT number = strtolW( tmp, &end, get_value_base_by_lctype( lctype ) );
                    if (*end)  /* invalid number */
                    {
                        SetLastError( ERROR_INVALID_FLAGS );
                        return 0;
                    }
                    ret = sizeof(UINT)/sizeof(WCHAR);
                    if (!buffer) return ret;
                    if (ret > len)
                    {
                        SetLastError( ERROR_INSUFFICIENT_BUFFER );
                        return 0;
                    }
                    memcpy( buffer, &number, sizeof(number) );
                }
            }
            else ret = get_registry_locale_info( value, buffer, len );

            if (ret != -1) return ret;
        }
    }

    /* now load it from kernel resources */

    lang_id = LANGIDFROMLCID( lcid );

    /* replace SUBLANG_NEUTRAL by SUBLANG_DEFAULT */
    if (SUBLANGID(lang_id) == SUBLANG_NEUTRAL) lang_id = get_default_sublang( lang_id );

    if (!(hrsrc = FindResourceExW( kernel32_handle, (LPWSTR)RT_STRING,
                                   ULongToPtr((lctype >> 4) + 1), lang_id )))
    {
        SetLastError( ERROR_INVALID_FLAGS );  /* no such lctype */
        return 0;
    }
    if (!(hmem = LoadResource( kernel32_handle, hrsrc )))
        return 0;

    p = LockResource( hmem );
    for (i = 0; i < (lctype & 0x0f); i++) p += *p + 1;

    if (lcflags & LOCALE_RETURN_NUMBER) ret = sizeof(UINT)/sizeof(WCHAR);
    else if (is_genitive_name_supported( lctype ) && *p)
    {
        /* genitive form's stored after a null separator from a nominative */
        for (i = 1; i <= *p; i++) if (!p[i]) break;

        if (i <= *p && (lcflags & LOCALE_RETURN_GENITIVE_NAMES))
        {
            ret = *p - i + 1;
            p += i;
        }
        else ret = i;
    }
    else
        ret = (lctype == LOCALE_FONTSIGNATURE) ? *p : *p + 1;

    if (!buffer) return ret;

    if (ret > len)
    {
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        return 0;
    }

    if (lcflags & LOCALE_RETURN_NUMBER)
    {
        UINT number;
        WCHAR *end, *tmp = HeapAlloc( GetProcessHeap(), 0, (*p + 1) * sizeof(WCHAR) );
        if (!tmp) return 0;
        memcpy( tmp, p + 1, *p * sizeof(WCHAR) );
        tmp[*p] = 0;
        number = strtolW( tmp, &end, get_value_base_by_lctype( lctype ) );
        if (!*end)
            memcpy( buffer, &number, sizeof(number) );
        else  /* invalid number */
        {
            SetLastError( ERROR_INVALID_FLAGS );
            ret = 0;
        }
        HeapFree( GetProcessHeap(), 0, tmp );

        TRACE( "(lcid=0x%x,lctype=0x%x,%p,%d) returning number %d\n",
               lcid, lctype, buffer, len, number );
    }
    else
    {
        memcpy( buffer, p + 1, ret * sizeof(WCHAR) );
        if (lctype != LOCALE_FONTSIGNATURE) buffer[ret-1] = 0;

        TRACE( "(lcid=0x%x,lctype=0x%x,%p,%d) returning %d %s\n",
               lcid, lctype, buffer, len, ret, debugstr_w(buffer) );
    }
    return ret;
}

/***********************************************************************
 *           LocaleNameToLCID  (KERNEL32.@)
 */
LCID 
WINAPI 
LocaleNameToLCID( 
	LPCWSTR name, 
	DWORD flags 
)
{
    int i;

    if (name == LOCALE_NAME_USER_DEFAULT)
        return GetUserDefaultLCID();

	for(i=0;i<LOCALE_TABLE_SIZE;i++){
		if(wcscmp(name, LocaleTable[i].localeName)==0){
			return LocaleTable[i].lcid;
		}
	}
	
    return GetSystemDefaultLCID();
}

// Implements wcsncpmp for ASCII chars only.
// NOTE: We can't use wcsncmp in this context because we may end up trying to modify
// locale data structs or even calling the same function in NLS code.
static int _fastcall __wcsnicmp_ascii(const wchar_t* string1, const wchar_t* string2, size_t count)
{
	wchar_t f, l;
	int result = 0;

	if (count)
	{
		/* validation section */
		do {
			f = towlower(*string1);
			l = towlower(*string2);
			string1++;
			string2++;
		} while ((--count) && f && (f == l));

		result = (int)(f - l);
	}

	return result;
}

//尝试将一个中性语言匹配到实际区域
static LPCWSTR __fastcall DownlevelNeutralToSpecificLocaleName(LPCWSTR szLocaleName)
{
	int bottom = 0;
	int top = _countof(NeutralToSpecificMap) - 1;
	int middle;
	int testIndex;

	while (bottom <= top)
	{
		middle = (bottom + top) / 2;
		testIndex = __wcsnicmp_ascii(szLocaleName, NeutralToSpecificMap[middle].szNeutralLocale, LOCALE_NAME_MAX_LENGTH);

		if (testIndex == 0)
			return NeutralToSpecificMap[middle].szSpecificLocale;

		if (testIndex < 0)
			top = middle - 1;
		else
			bottom = middle + 1;
	}

	//找不到就直接返回本身
	return szLocaleName;
}

/***********************************************************************
 *           LCIDToLocaleName  (KERNEL32.@)
 */
INT 
WINAPI 
LCIDToLocaleName( 
    LCID Locale, 
    LPWSTR lpName, 
    INT cchName, 
    DWORD dwFlags 
)
{
    int i;
    int count = 0;
    LPCWSTR szLocaleName;
    
    szLocaleName = (LPWSTR)HeapAlloc(GetProcessHeap(), 8, MAX_PATH * 2);
    
    if (Locale == 0 || (lpName == NULL && cchName > 0) || cchName < 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
        if (Locale == LOCALE_USER_DEFAULT)
            Locale = GetUserDefaultLCID();
        if (Locale == LOCALE_SYSTEM_DEFAULT)
            Locale = GetSystemDefaultLCID();
            
    for(i=0;i<LOCALE_TABLE_SIZE;i++){
        if(Locale == LocaleTable[i].lcid){
            count = (wcslen(LocaleTable[i].localeName)+1);
            if(lpName){
                memcpy(lpName, LocaleTable[i].localeName, sizeof(WCHAR)*(count));
                lpName[count-1] = 0;
            }            
            return count;
        }
    }
    // if(lpName){
        // if ((LOCALE_ALLOW_NEUTRAL_NAMES & dwFlags) == 0)
        // {
            // szLocaleName = DownlevelNeutralToSpecificLocaleName(szLocaleName);
            // count = wcslen(szLocaleName) + 1;
            // memcpy(lpName, szLocaleName, count * sizeof(szLocaleName[0]));
            // HeapFree(GetProcessHeap(), 0, szLocaleName);
        // }        
    // }
    return count;
}

//TODO MUI_LANGUAGE_ID
BOOL
WINAPI
EnumPreferredUserUILanguages(
  _In_      DWORD   flags,
  _In_		LANGID langid,
  _Out_     PULONG  count,
  _Out_opt_ PZZWSTR buffer,
  _Inout_   PULONG  buffersize 
)
{
    static const WCHAR formathexW[] = { '%','0','4','x',0 };

    static const WCHAR formatstringW[] = { '%','.','2','s',0 };
	

    FIXME( "semi-stub %u, %p, %p %p\n", flags, count, buffer, buffersize );
	
    /* FIXME should we check for too small buffersize too? */
    if (!buffer || *buffersize < 11)
    {
           SetLastError(ERROR_INSUFFICIENT_BUFFER);
           *buffersize = 11;
           *count=2;
           return TRUE;
    }	

    if (!flags)
        flags = MUI_LANGUAGE_NAME;

	if ((flags & (MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME )) == (MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME ))
    {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;

    }
    /* FIXME should we check for too small buffersize too? */
    if (!buffer)
    {
           SetLastError(ERROR_INSUFFICIENT_BUFFER);
           *buffersize = 10;
           *count=2;
           return TRUE;
    }
	
    memset((WCHAR *)buffer,0,*buffersize);
    if ((flags & MUI_LANGUAGE_ID) == MUI_LANGUAGE_ID)  
    { 
           *buffersize = 11; 
           *count=2;
           sprintfW((WCHAR *)buffer, formathexW, langid);
           sprintfW((WCHAR *)buffer+5, formathexW, PRIMARYLANGID(langid)); 
           SetLastError(ERROR_SUCCESS);
    }
    else  
    {
           *buffersize = 10; 
           *count=2;
           //GetLocaleInfoW( MAKELCID(langid, SORT_DEFAULT), LOCALE_SNAME | LOCALE_NOUSEROVERRIDE, (WCHAR *)buffer, *buffersize);
		   LCIDToLocaleName(MAKELCID(langid, SORT_DEFAULT), (WCHAR *)buffer, *buffersize, 0);
           /* FIXME is there no better way to to this? I can't get GetLocaleInfo to return the neutral languagename :( */      
           sprintfW((WCHAR *)buffer+6, formatstringW, buffer);
           SetLastError(ERROR_SUCCESS);

    }
    return TRUE; 	
}

BOOL
WINAPI
EnumPreferredThreadUILanguages(
  _In_      DWORD   flags,
  _In_		LANGID	langid,
  _Out_     PULONG  count,
  _Out_opt_ PZZWSTR buffer,
  _Inout_   PULONG  buffersize 
)
{
    static const WCHAR formathexW[] = { '%','0','4','x',0 };

    static const WCHAR formatstringW[] = { '%','.','2','s',0 };
	

    //FIXME( "EnumPreferredThreadUILanguages :: semi-stub %u, %p, %p %p\n", flags, count, buffer, buffersize );
	
    /* FIXME should we check for too small buffersize too? */
    if (!buffer || *buffersize < 11)
    {
           SetLastError(ERROR_INSUFFICIENT_BUFFER);
           *buffersize = 11;
           *count=2;
           return TRUE;
    }	

    if (!flags)
        flags = MUI_LANGUAGE_NAME;

	if ((flags & (MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME )) == (MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME ))
    {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;

    }
    /* FIXME should we check for too small buffersize too? */
    if (!buffer)
    {
           SetLastError(ERROR_INSUFFICIENT_BUFFER);
           *buffersize = 10;
           *count=2;
           return TRUE;
    }

    memset((WCHAR *)buffer,0,*buffersize);
    if ((flags & MUI_LANGUAGE_ID) == MUI_LANGUAGE_ID)  
    { 
           *buffersize = 11; 
           *count=2;
           sprintfW((WCHAR *)buffer, formathexW, langid);
           sprintfW((WCHAR *)buffer+5, formathexW, PRIMARYLANGID(langid)); 
           SetLastError(ERROR_SUCCESS);
    }
    else  
    {
           *buffersize = 10; 
           *count=2;
           //GetLocaleInfoW( MAKELCID(langid, SORT_DEFAULT), LOCALE_SNAME | LOCALE_NOUSEROVERRIDE, (WCHAR *)buffer, *buffersize);
		   LCIDToLocaleName(MAKELCID(langid, SORT_DEFAULT), (WCHAR *)buffer, *buffersize, 0);
           /* FIXME is there no better way to to this? I can't get GetLocaleInfo to return the neutral languagename :( */      
           sprintfW((WCHAR *)buffer+6, formatstringW, buffer);
           SetLastError(ERROR_SUCCESS);

    }
    return TRUE; 	
}

BOOL
WINAPI
EnumPreferredSystemUILanguages(
  _In_      DWORD   flags,
  _Out_     PULONG  count,
  _Out_opt_ PZZWSTR buffer,
  _Inout_   PULONG  buffersize 
)
{
    static const WCHAR formathexW[] = { '%','0','4','x',0 };

    static const WCHAR formatstringW[] = { '%','.','2','s',0 };
	

    LANGID langid;

    FIXME( "semi-stub %u, %p, %p %p\n", flags, count, buffer, buffersize );
	
    /* FIXME should we check for too small buffersize too? */
    if (!buffer || *buffersize < 11)
    {
           SetLastError(ERROR_INSUFFICIENT_BUFFER);
           *buffersize = 11;
           *count=2;
           return TRUE;
    }		

    if (!flags)
        flags = MUI_LANGUAGE_NAME;

	if ((flags & (MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME )) == (MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME ))
    {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;

    }
    /* FIXME should we check for too small buffersize too? */
    if (!buffer)
    {
           SetLastError(ERROR_INSUFFICIENT_BUFFER);
           *buffersize = 10;
           *count=2;
           return TRUE;
    }

    langid = GetSystemDefaultLangID();
    memset((WCHAR *)buffer,0,*buffersize);
    if ((flags & MUI_LANGUAGE_ID) == MUI_LANGUAGE_ID)  
    { 
           *buffersize = 11; 
           *count=2;
           sprintfW((WCHAR *)buffer, formathexW, langid);
           sprintfW((WCHAR *)buffer+5, formathexW, PRIMARYLANGID(langid)); 
           SetLastError(ERROR_SUCCESS);
    }
    else  
    {
           *buffersize = 10; 
           *count=2;
           //GetLocaleInfoW( MAKELCID(langid, SORT_DEFAULT), LOCALE_SNAME | LOCALE_NOUSEROVERRIDE, (WCHAR *)buffer, *buffersize);
		   LCIDToLocaleName(MAKELCID(langid, SORT_DEFAULT), (WCHAR *)buffer, *buffersize, 0);
           /* FIXME is there no better way to to this? I can't get GetLocaleInfo to return the neutral languagename :( */      
           sprintfW((WCHAR *)buffer+6, formatstringW, buffer);
           SetLastError(ERROR_SUCCESS);

    }
    return TRUE; 	
}

BOOL
WINAPI
EnumPreferredProcessUILanguages(
  _In_      DWORD   flags,
  _Out_     PULONG  count,
  _Out_opt_ PZZWSTR buffer,
  _Inout_   PULONG  buffersize 
)
{
    static const WCHAR formathexW[] = { '%','0','4','x',0 };

    static const WCHAR formatstringW[] = { '%','.','2','s',0 };
	

    LANGID langid;

    FIXME( "semi-stub %u, %p, %p %p\n", flags, count, buffer, buffersize );
	
    /* FIXME should we check for too small buffersize too? */
    if (!buffer || *buffersize < 11)
    {
           SetLastError(ERROR_INSUFFICIENT_BUFFER);
           *buffersize = 11;
           *count=2;
           return TRUE;
    }		

    if (!flags)
        flags = MUI_LANGUAGE_NAME;

	if ((flags & (MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME )) == (MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME ))
    {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;

    }
    /* FIXME should we check for too small buffersize too? */
    if (!buffer)
    {
           SetLastError(ERROR_INSUFFICIENT_BUFFER);
           *buffersize = 10;
           *count=2;
           return TRUE;
    }

    langid = GetSystemDefaultLangID();
    memset((WCHAR *)buffer,0,*buffersize);
    if ((flags & MUI_LANGUAGE_ID) == MUI_LANGUAGE_ID)  
    { 
           *buffersize = 11; 
           *count=2;
           sprintfW((WCHAR *)buffer, formathexW, langid);
           sprintfW((WCHAR *)buffer+5, formathexW, PRIMARYLANGID(langid)); 
           SetLastError(ERROR_SUCCESS);
    }
    else  
    {
           *buffersize = 10; 
           *count=2;
           //GetLocaleInfoW( MAKELCID(langid, SORT_DEFAULT), LOCALE_SNAME | LOCALE_NOUSEROVERRIDE, (WCHAR *)buffer, *buffersize);
		   LCIDToLocaleName(MAKELCID(langid, SORT_DEFAULT), (WCHAR *)buffer, *buffersize, 0);
           /* FIXME is there no better way to to this? I can't get GetLocaleInfo to return the neutral languagename :( */      
           sprintfW((WCHAR *)buffer+6, formatstringW, buffer);
           SetLastError(ERROR_SUCCESS);

    }
    return TRUE; 	
}


/***********************************************************************
 *		GetSystemDefaultLocaleName (KERNEL32.@)
 */
INT 
WINAPI 
GetSystemDefaultLocaleName(
	LPWSTR localename, 
	INT len
)
{
    return LCIDToLocaleName(GetSystemDefaultLCID(), localename, len, 0);
}

/******************************************************************************
 *           IsValidLocaleName   (KERNEL32.@)
 */
BOOL 
WINAPI 
IsValidLocaleName( 
	LPCWSTR locale 
)
{
	int i;
    if (!locale)
        return FALSE;

	for(i=0;i<LOCALE_TABLE_SIZE;i++){
		if(wcscmp(locale, LocaleTable[i].localeName)==0){
			return TRUE;
		}
	}	

    return FALSE;
}

/***********************************************************************
  *              GetThreadUILanguage (KERNEL32.@)
  *
  * Get the current thread's language identifier.
  *
  * PARAMS
  *  None.
  *
  * RETURNS
  *  The current thread's language identifier.
*/
LANGID 
WINAPI 
GetThreadUILanguage( void )
{
     //LANGID lang;
     //NtQueryDefaultUILanguage( &lang );
     //DbgPrint("GetThreadUILanguage is UNIMPLEMENTED, returning default language.\n");
	 //Windows XP and Server 2003 doesn't use really LANGIID passed how paremeter on SetThreadUILanguage, so, we 
	 //can use to get Thread UI Language;	 
     //return SetThreadUILanguage(0);
    LANGID lang;

    FIXME(": stub, returning default language.\n");
    NtQueryDefaultUILanguage( &lang );
    return lang;	 
}

BOOL 
WINAPI 
GetFileMUIPath(
	DWORD dwFlags, 
	PCWSTR pcwszFilePath, 
	PWSTR pwszLanguage, 
	PULONG pcchLanguage, 
	PWSTR pwszFileMUIPath,
	PULONG pcchFileMUIPath, 
	PULONGLONG pululEnumerator
)
{

	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);		
	return FALSE;
}

/* implementation of CompareStringEx */
static int compare_string( const struct sortguid *sortid, DWORD flags,
                           const WCHAR *src1, int srclen1, const WCHAR *src2, int srclen2 )
{
    struct sortkey_state s1;
    struct sortkey_state s2;
    BYTE primary1[32];
    BYTE primary2[32];
    int i, ret, len, pos1 = 0, pos2 = 0;
    BOOL have_extra1, have_extra2;
    BYTE case_mask = 0x3f;
    UINT except = sortid->except;
    const WCHAR *compr_tables[8];

    compr_tables[0] = NULL;
    if (flags & NORM_IGNORECASE) case_mask &= ~(CASE_UPPER | CASE_SUBSCRIPT);
    if (flags & NORM_IGNOREWIDTH) case_mask &= ~CASE_FULLWIDTH;
    if (flags & NORM_IGNOREKANATYPE) case_mask &= ~CASE_KATAKANA;
    if ((flags & NORM_LINGUISTIC_CASING) && except && sortid->ling_except) except = sortid->ling_except;

    init_sortkey_state( &s1, flags, srclen1, primary1, sizeof(primary1) );
    init_sortkey_state( &s2, flags, srclen2, primary2, sizeof(primary2) );

    while (pos1 < srclen1 || pos2 < srclen2)
    {
        while (pos1 < srclen1 && !s1.key_primary.len)
            pos1 += append_weights( sortid, flags, src1, srclen1, pos1,
                                    case_mask, except, compr_tables, &s1, TRUE );

        while (pos2 < srclen2 && !s2.key_primary.len)
            pos2 += append_weights( sortid, flags, src2, srclen2, pos2,
                                    case_mask, except, compr_tables, &s2, TRUE );

        if (!(len = min( s1.key_primary.len, s2.key_primary.len ))) break;
        if ((ret = memcmp( primary1, primary2, len ))) goto done;
        memmove( primary1, primary1 + len, s1.key_primary.len - len );
        memmove( primary2, primary2 + len, s2.key_primary.len - len );
        s1.key_primary.len -= len;
        s2.key_primary.len -= len;
        s1.primary_pos += len;
        s2.primary_pos += len;
    }

    if ((ret = s1.key_primary.len - s2.key_primary.len)) goto done;

    have_extra1 = remove_unneeded_weights( sortid, &s1 );
    have_extra2 = remove_unneeded_weights( sortid, &s2 );

    if ((ret = compare_sortkeys( &s1.key_diacritic, &s2.key_diacritic, FALSE ))) goto done;
    if ((ret = compare_sortkeys( &s1.key_case, &s2.key_case, FALSE ))) goto done;

    if (have_extra1 && have_extra2)
    {
        for (i = 0; i < 4; i++)
            if ((ret = compare_sortkeys( &s1.key_extra[i], &s2.key_extra[i], i != 1 ))) goto done;
    }
    else if ((ret = have_extra1 - have_extra2)) goto done;

    ret = compare_sortkeys( &s1.key_special, &s2.key_special, FALSE );

done:
    free_sortkey_state( &s1 );
    free_sortkey_state( &s2 );
    return ret;
}

/******************************************************************************
 *	CompareStringEx   (kernelex.@)
 */
INT WINAPI CompareStringEx( const WCHAR *locale, DWORD flags, const WCHAR *str1, int len1,
                            const WCHAR *str2, int len2, NLSVERSIONINFO *version,
                            void *reserved, LPARAM handle )
{
    const struct sortguid *sortid;
    const DWORD supported_flags = NORM_IGNORECASE | NORM_IGNORENONSPACE | NORM_IGNORESYMBOLS |
                                  SORT_STRINGSORT | NORM_IGNOREKANATYPE | NORM_IGNOREWIDTH |
                                  NORM_LINGUISTIC_CASING | LINGUISTIC_IGNORECASE |
                                  LINGUISTIC_IGNOREDIACRITIC | SORT_DIGITSASNUMBERS |
                                  0x10000000 | LOCALE_USE_CP_ACP;
    /* 0x10000000 is related to diacritics in Arabic, Japanese, and Hebrew */
    int ret;
    LCID lpLocale = LocaleNameToLCID(locale, 0);
	
	if (flags & 0x8000000)
		flags ^= 0x8000000;	
	
    if (lpLocale == 0)
        return 0;

    ret = CompareStringW(lpLocale, flags, str1, len1, str2, len2);
    if (ret != 0 || GetLastError() == ERROR_INVALID_PARAMETER)
        return ret;	

    if (version) FIXME( "unexpected version parameter\n" );
    if (reserved) FIXME( "unexpected reserved value\n" );
    if (handle) FIXME( "unexpected handle\n" );

    if (flags & ~supported_flags)
    {
        SetLastError( ERROR_INVALID_FLAGS );
        return 0;
    }

    if (!(sortid = get_language_sort( locale ))) return 0;

    if (!str1 || !str2)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }

    if (len1 < 0) len1 = lstrlenW(str1);
    if (len2 < 0) len2 = lstrlenW(str2);

    ret = compare_string( sortid, flags, str1, len1, str2, len2 );
    if (ret < 0) return CSTR_LESS_THAN;
    if (ret > 0) return CSTR_GREATER_THAN;
    return CSTR_EQUAL;
}

/*
 *		get_lcid_codepage
 *
 * Retrieve the ANSI codepage for a given locale.
 */
static inline UINT get_lcid_codepage( LCID lcid )
{
    UINT ret;
    if (!GetLocaleInfoW( lcid, LOCALE_IDEFAULTANSICODEPAGE|LOCALE_RETURN_NUMBER, (WCHAR *)&ret,
                         sizeof(ret)/sizeof(WCHAR) )) ret = 0;
    return ret;
}

// /******************************************************************************
 // *           CompareStringA    (KERNEL32.@)
 // *
 // * Compare two locale sensitive strings.
 // *
 // * PARAMS
 // *  lcid  [I] LCID for the comparison
 // *  flags [I] Flags for the comparison (NORM_ constants from "winnls.h").
 // *  str1  [I] First string to compare
 // *  len1  [I] Length of str1, or -1 if str1 is NUL terminated
 // *  str2  [I] Second string to compare
 // *  len2  [I] Length of str2, or -1 if str2 is NUL terminated
 // *
 // * RETURNS
 // *  Success: CSTR_LESS_THAN, CSTR_EQUAL or CSTR_GREATER_THAN depending on whether
 // *           str1 is less than, equal to or greater than str2 respectively.
 // *  Failure: FALSE. Use GetLastError() to determine the cause.
 // */
// INT WINAPI CompareStringA(LCID lcid, DWORD flags,
                          // LPCSTR str1, INT len1, LPCSTR str2, INT len2)
// {
    // WCHAR *buf1W = NtCurrentTeb()->StaticUnicodeBuffer;
    // WCHAR *buf2W = buf1W + 130;
    // LPWSTR str1W, str2W;
    // INT len1W = 0, len2W = 0, ret;
    // UINT locale_cp = CP_ACP;

    // if (!str1 || !str2)
    // {
        // SetLastError(ERROR_INVALID_PARAMETER);
        // return 0;
    // }
    // if (len1 < 0) len1 = strlen(str1);
    // if (len2 < 0) len2 = strlen(str2);

    // if (!(flags & LOCALE_USE_CP_ACP)) locale_cp = get_lcid_codepage( lcid );

    // if (len1)
    // {
        // if (len1 <= 130) len1W = MultiByteToWideChar(locale_cp, 0, str1, len1, buf1W, 130);
        // if (len1W)
            // str1W = buf1W;
        // else
        // {
            // len1W = MultiByteToWideChar(locale_cp, 0, str1, len1, NULL, 0);
            // str1W = HeapAlloc(GetProcessHeap(), 0, len1W * sizeof(WCHAR));
            // if (!str1W)
            // {
                // SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                // return 0;
            // }
            // MultiByteToWideChar(locale_cp, 0, str1, len1, str1W, len1W);
        // }
    // }
    // else
    // {
        // len1W = 0;
        // str1W = buf1W;
    // }

    // if (len2)
    // {
        // if (len2 <= 130) len2W = MultiByteToWideChar(locale_cp, 0, str2, len2, buf2W, 130);
        // if (len2W)
            // str2W = buf2W;
        // else
        // {
            // len2W = MultiByteToWideChar(locale_cp, 0, str2, len2, NULL, 0);
            // str2W = HeapAlloc(GetProcessHeap(), 0, len2W * sizeof(WCHAR));
            // if (!str2W)
            // {
                // if (str1W != buf1W) HeapFree(GetProcessHeap(), 0, str1W);
                // SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                // return 0;
            // }
            // MultiByteToWideChar(locale_cp, 0, str2, len2, str2W, len2W);
        // }
    // }
    // else
    // {
        // len2W = 0;
        // str2W = buf2W;
    // }

    // ret = CompareStringEx(NULL, flags, str1W, len1W, str2W, len2W, NULL, NULL, 0);

    // if (str1W != buf1W) HeapFree(GetProcessHeap(), 0, str1W);
    // if (str2W != buf2W) HeapFree(GetProcessHeap(), 0, str2W);
    // return ret;
// }

// /******************************************************************************
 // *           CompareStringW    (KERNEL32.@)
 // *
 // * See CompareStringA.
 // */
// INT WINAPI CompareStringW(LCID lcid, DWORD flags,
                          // LPCWSTR str1, INT len1, LPCWSTR str2, INT len2)
// {
    // return CompareStringEx(NULL, flags, str1, len1, str2, len2, NULL, NULL, 0);
// }

static inline void map_byterev(const WCHAR *src, int len, WCHAR *dst)
{
    while (len--)
        *dst++ = RtlUshortByteSwap(*src++);
}

static int map_to_hiragana(const WCHAR *src, int srclen, WCHAR *dst, int dstlen)
{
    int pos;
    for (pos = 0; srclen; src++, srclen--, pos++)
    {
        /*
         * U+30A1 ... U+30F3: Katakana
         * U+30F4: Katakana Letter VU
         * U+30F5: Katakana Letter Small KA
         * U+30FD: Katakana Iteration Mark
         * U+30FE: Katakana Voiced Iteration Mark
         */
        WCHAR wch = *src;
        if ((0x30A1 <= wch && wch <= 0x30F3) ||
            wch == 0x30F4 || wch == 0x30F5 || wch == 0x30FD || wch == 0x30FE)
        {
            wch -= 0x60; /* Katakana to Hiragana */
        }
        if (pos < dstlen)
            dst[pos] = wch;
    }
    return pos;
}

static int map_to_katakana(const WCHAR *src, int srclen, WCHAR *dst, int dstlen)
{
    int pos;
    for (pos = 0; srclen; src++, srclen--, pos++)
    {
        /*
         * U+3041 ... U+3093: Hiragana
         * U+3094: Hiragana Letter VU
         * U+3095: Hiragana Letter Small KA
         * U+309D: Hiragana Iteration Mark
         * U+309E: Hiragana Voiced Iteration Mark
         */
        WCHAR wch = *src;
        if ((0x3041 <= wch && wch <= 0x3093) ||
            wch == 3094 || wch == 0x3095 || wch == 0x309D || wch == 0x309E)
        {
            wch += 0x60; /* Hiragana to Katakana */
        }
        if (pos < dstlen)
            dst[pos] = wch;
    }
    return pos;
}

/* The table that contains fullwidth characters and halfwidth characters */
typedef WCHAR FULL2HALF_ENTRY[3];
static const FULL2HALF_ENTRY full2half_table[] =
{
#define DEFINE_FULL2HALF(full, half1, half2) { full, half1, half2 },
#include "full2half.h"
#undef DEFINE_FULL2HALF
};
#define GET_FULL(table, index)  ((table)[index][0])
#define GET_HALF1(table, index) ((table)[index][1])
#define GET_HALF2(table, index) ((table)[index][2])

/* The table that contains dakuten entries */
typedef WCHAR DAKUTEN_ENTRY[3];
static const DAKUTEN_ENTRY dakuten_table[] =
{
#define DEFINE_DAKUTEN(voiced, single1, single2, half1, half2) { voiced, single1, single2 },
#include "dakuten.h"
#undef DEFINE_DAKUTEN
};
#define GET_VOICED(table, index) ((table)[index][0])
#define GET_SINGLE1(table, index) ((table)[index][1])
#define GET_SINGLE2(table, index) ((table)[index][2])

static int map_to_halfwidth(DWORD flags, const WCHAR *src, int srclen, WCHAR *dst, int dstlen)
{
    int pos, i;
    const int count1 = (int)ARRAY_SIZE(full2half_table);
    const FULL2HALF_ENTRY *table1 = full2half_table;

    for (pos = 0; srclen; src++, srclen--, pos++)
    {
        WCHAR ch = *src;

        if (flags & LCMAP_KATAKANA)
            map_to_katakana(&ch, 1, &ch, 1);
        else if (flags & LCMAP_HIRAGANA)
            map_to_hiragana(&ch, 1, &ch, 1);

        if (ch < 0x3000) /* Quick judgment */
        {
            if (pos < dstlen)
                dst[pos] = ch;
            continue;
        }

        if (0xFF01 <= ch && ch <= 0xFF5E) /* U+FF01 ... U+FF5E */
        {
            if (pos < dstlen)
                dst[pos] = ch - 0xFEE0; /* Fullwidth ASCII to halfwidth ASCII */
            continue;
        }

        /* Search in table1 (full/half) */
        for (i = count1 - 1; i >= 0; --i) /* In reverse order */
        {
            if (GET_FULL(table1, i) != ch)
                continue;

            if (GET_HALF2(table1, i) == 0)
            {
                if (pos < dstlen)
                    dst[pos] = GET_HALF1(table1, i);
            }
            else if (!dstlen)
            {
                pos++;
            }
            else if (pos + 1 < dstlen)
            {
                dst[pos++] = GET_HALF1(table1, i);
                dst[pos  ] = GET_HALF2(table1, i);
            }
            else
            {
                dst[pos] = ch;
            }
            break;
        }

        if (i >= 0)
            continue;

        if (pos < dstlen)
            dst[pos] = ch;
    }

    return pos;
}

static int map_to_fullwidth(const WCHAR *src, int srclen, WCHAR *dst, int dstlen)
{
    int pos, i;
    const FULL2HALF_ENTRY *table1 = full2half_table;
    const DAKUTEN_ENTRY *table2 = dakuten_table;
    const int count1 = (int)ARRAY_SIZE(full2half_table);
    const int count2 = (int)ARRAY_SIZE(dakuten_table);

    for (pos = 0; srclen; src++, srclen--, pos++)
    {
        WCHAR ch = *src;

        if (ch == 0x20) /* U+0020: Space */
        {
            if (pos < dstlen)
                dst[pos] = 0x3000; /* U+3000: Ideographic Space */
            continue;
        }

        if (0x21 <= ch && ch <= 0x7E) /* Mappable halfwidth ASCII */
        {
            if (pos < dstlen)
                dst[pos] = ch + 0xFEE0; /* U+FF01 ... U+FF5E */
            continue;
        }

        if (ch < 0xFF00) /* Quick judgment */
        {
            if (pos < dstlen)
                dst[pos] = ch;
            continue;
        }

        /* Search in table1 (full/half) */
        for (i = count1 - 1; i >= 0; --i) /* In reverse order */
        {
            if (GET_HALF1(table1, i) != ch)
                continue; /* Mismatched */

            if (GET_HALF2(table1, i) == 0)
            {
                if (pos < dstlen)
                    dst[pos] = GET_FULL(table1, i);
                break;
            }

            if (srclen <= 1 || GET_HALF2(table1, i) != src[1])
                continue; /* Mismatched */

            --srclen;
            ++src;

            if (pos < dstlen)
                dst[pos] = GET_FULL(table1, i);
            break;
        }

        if (i >= 0)
            continue;

        /* Search in table2 (dakuten) */
        for (i = count2 - 1; i >= 0; --i) /* In reverse order */
        {
            if (GET_SINGLE1(table2, i) != ch)
                continue; /* Mismatched */

            if (srclen <= 1 || GET_SINGLE2(table2, i) != src[1])
                continue; /* Mismatched */

            --srclen;
            ++src;

            if (pos < dstlen)
                dst[pos] = GET_VOICED(table2, i);
            break;
        }

        if (i >= 0)
            continue;

        if (pos < dstlen)
            dst[pos] = ch;
    }

    return pos;
}

static int map_to_lowercase(DWORD flags, const WCHAR *src, int srclen, WCHAR *dst, int dstlen)
{
    int pos;
    for (pos = 0; srclen; src++, srclen--)
    {
        WCHAR wch = *src;
        if ((flags & NORM_IGNORESYMBOLS) && (get_char_typeW(wch) & (C1_PUNCT | C1_SPACE)))
            continue;
        if (pos < dstlen)
            dst[pos] = tolowerW(wch);
        pos++;
    }
    return pos;
}

static int map_to_uppercase(DWORD flags, const WCHAR *src, int srclen, WCHAR *dst, int dstlen)
{
    int pos;
    for (pos = 0; srclen; src++, srclen--)
    {
        WCHAR wch = *src;
        if ((flags & NORM_IGNORESYMBOLS) && (get_char_typeW(wch) & (C1_PUNCT | C1_SPACE)))
            continue;
        if (pos < dstlen)
            dst[pos] = toupperW(wch);
        pos++;
    }
    return pos;
}

typedef struct tagWCHAR_PAIR
{
    WCHAR from, to;
} WCHAR_PAIR, *PWCHAR_PAIR;

/* The table to convert Simplified Chinese to Traditional Chinese */
static const WCHAR_PAIR s_sim2tra[] =
{
#define DEFINE_SIM2TRA(from, to) { from, to },
#include "sim2tra.h"
#undef DEFINE_SIM2TRA
};

/* The table to convert Traditional Chinese to Simplified Chinese */
static const WCHAR_PAIR s_tra2sim[] =
{
#define DEFINE_TRA2SIM(from, to) { from, to },
#include "tra2sim.h"
#undef DEFINE_TRA2SIM
};

/* The comparison function to do bsearch */
static int compare_wchar_pair(const void *x, const void *y)
{
    const WCHAR_PAIR *a = x;
    const WCHAR_PAIR *b = y;
    if (a->from < b->from)
        return -1;
    if (a->from > b->from)
        return +1;
    return 0;
}

static WCHAR find_wchar_pair(const WCHAR_PAIR *pairs, size_t count, WCHAR ch)
{
    PWCHAR_PAIR found = bsearch(&ch, pairs, count, sizeof(WCHAR_PAIR), compare_wchar_pair);
    if (found)
        return found->to;
    return ch;
}

static int map_to_simplified_chinese(DWORD flags, const WCHAR *src, int srclen, WCHAR *dst, int dstlen)
{
    int pos;
    for (pos = 0; srclen; src++, srclen--)
    {
        WCHAR wch = *src;
        if (pos < dstlen)
            dst[pos] = find_wchar_pair(s_tra2sim, ARRAY_SIZE(s_tra2sim), wch);
        pos++;
    }
    return pos;
}

static int map_to_traditional_chinese(DWORD flags, const WCHAR *src, int srclen, WCHAR *dst, int dstlen)
{
    int pos;
    for (pos = 0; srclen; src++, srclen--)
    {
        WCHAR wch = *src;
        if (pos < dstlen)
            dst[pos] = find_wchar_pair(s_sim2tra, ARRAY_SIZE(s_sim2tra), wch);
        pos++;
    }
    return pos;
}

static int map_remove_ignored(DWORD flags, const WCHAR *src, int srclen, WCHAR *dst, int dstlen)
{
    int pos;
    WORD wC1, wC2, wC3;
    for (pos = 0; srclen; src++, srclen--)
    {
        WCHAR wch = *src;
        GetStringTypeW(CT_CTYPE1, &wch, 1, &wC1);
        GetStringTypeW(CT_CTYPE2, &wch, 1, &wC2);
        GetStringTypeW(CT_CTYPE3, &wch, 1, &wC3);
        if (flags & NORM_IGNORESYMBOLS)
        {
            if ((wC1 & C1_PUNCT) || (wC3 & C3_SYMBOL))
                continue;
        }
        if (flags & NORM_IGNORENONSPACE)
        {
            if ((wC2 & C2_OTHERNEUTRAL) && (wC3 & (C3_NONSPACING | C3_DIACRITIC)))
                continue;
        }
        if (pos < dstlen)
            dst[pos] = wch;
        pos++;
    }
    return pos;
}

static int lcmap_string(DWORD flags, const WCHAR *src, int srclen, WCHAR *dst, int dstlen)
{
    int ret = 0;

    if ((flags & (LCMAP_LOWERCASE | LCMAP_UPPERCASE)) == (LCMAP_LOWERCASE | LCMAP_UPPERCASE))
    {
        SetLastError(ERROR_INVALID_FLAGS);
        return 0;
    }

    switch (flags & ~(LCMAP_BYTEREV | LCMAP_LOWERCASE | LCMAP_UPPERCASE | LCMAP_LINGUISTIC_CASING))
    {
    case LCMAP_HIRAGANA:
        ret = map_to_hiragana(src, srclen, dst, dstlen);
        break;
    case LCMAP_KATAKANA:
        ret = map_to_katakana(src, srclen, dst, dstlen);
        break;
    case LCMAP_HALFWIDTH:
        ret = map_to_halfwidth(flags, src, srclen, dst, dstlen);
        break;
    case LCMAP_HIRAGANA | LCMAP_HALFWIDTH:
        ret = map_to_halfwidth(flags, src, srclen, dst, dstlen);
        break;
    case LCMAP_KATAKANA | LCMAP_HALFWIDTH:
        ret = map_to_halfwidth(flags, src, srclen, dst, dstlen);
        break;
    case LCMAP_FULLWIDTH:
        ret = map_to_fullwidth(src, srclen, dst, dstlen);
        break;
    case LCMAP_HIRAGANA | LCMAP_FULLWIDTH:
        ret = map_to_fullwidth(src, srclen, dst, dstlen);
        if (dstlen && ret)
            map_to_hiragana(dst, ret, dst, dstlen);
        break;
    case LCMAP_KATAKANA | LCMAP_FULLWIDTH:
        ret = map_to_fullwidth(src, srclen, dst, dstlen);
        if (dstlen && ret)
            map_to_katakana(dst, ret, dst, dstlen);
        break;
    case LCMAP_SIMPLIFIED_CHINESE:
        ret = map_to_simplified_chinese(flags, src, srclen, dst, dstlen);
        break;
    case LCMAP_TRADITIONAL_CHINESE:
        ret = map_to_traditional_chinese(flags, src, srclen, dst, dstlen);
        break;
    case NORM_IGNORENONSPACE:
    case NORM_IGNORESYMBOLS:
    case NORM_IGNORENONSPACE | NORM_IGNORESYMBOLS:
        if (flags & ~(NORM_IGNORENONSPACE | NORM_IGNORESYMBOLS | LCMAP_BYTEREV))
        {
            SetLastError(ERROR_INVALID_FLAGS);
            return 0;
        }
        ret = map_remove_ignored(flags, src, srclen, dst, dstlen);
        break;
    case 0:
        if (flags & LCMAP_LOWERCASE)
        {
            ret = map_to_lowercase(flags, src, srclen, dst, dstlen);
            flags &= ~LCMAP_LOWERCASE;
            break;
        }
        if (flags & LCMAP_UPPERCASE)
        {
            ret = map_to_uppercase(flags, src, srclen, dst, dstlen);
            flags &= ~LCMAP_UPPERCASE;
            break;
        }
        if (flags & LCMAP_BYTEREV)
        {
            if (dstlen == 0)
            {
                ret = srclen;
                break;
            }
            ret = min(srclen, dstlen);
            RtlCopyMemory(dst, src, ret * sizeof(WCHAR));
            break;
        }
        /* fall through */
    default:
        SetLastError(ERROR_INVALID_FLAGS);
        return 0;
    }

    if (dstlen)
    {
        if (flags & LCMAP_LOWERCASE)
            map_to_lowercase(flags, dst, ret, dst, dstlen);
        if (flags & LCMAP_UPPERCASE)
            map_to_uppercase(flags, dst, ret, dst, dstlen);
        if (flags & LCMAP_BYTEREV)
            map_byterev(dst, min(ret, dstlen), dst);

        if (dstlen < ret)
        {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return 0;
        }
    }

    return ret;
}

/*************************************************************************
 *           LCMapStringEx   (KERNEL32.@)
 *
 * Map characters in a locale sensitive string.
 *
 * PARAMS
 *  name     [I] Locale name for the conversion.
 *  flags    [I] Flags controlling the mapping (LCMAP_ constants from "winnls.h")
 *  src      [I] String to map
 *  srclen   [I] Length of src in chars, or -1 if src is NUL terminated
 *  dst      [O] Destination for mapped string
 *  dstlen   [I] Length of dst in characters
 *  version  [I] reserved, must be NULL
 *  reserved [I] reserved, must be NULL
 *  lparam   [I] reserved, must be 0
 *
 * RETURNS
 *  Success: The length of the mapped string in dst, including the NUL terminator.
 *  Failure: 0. Use GetLastError() to determine the cause.
 */
/*************************************************************************
 *           LCMapStringEx   (KERNEL32.@)
 *
 * Map characters in a locale sensitive string.
 *
 * PARAMS
 *  locale   [I] Locale name for the conversion.
 *  flags    [I] Flags controlling the mapping (LCMAP_ constants from "winnls.h")
 *  src      [I] String to map
 *  srclen   [I] Length of src in chars, or -1 if src is NUL terminated
 *  dst      [O] Destination for mapped string
 *  dstlen   [I] Length of dst in characters
 *  version  [I] reserved, must be NULL
 *  reserved [I] reserved, must be NULL
 *  lparam   [I] reserved, must be 0
 *
 * RETURNS
 *  Success: The length of the mapped string in dst, including the NUL terminator.
 *  Failure: 0. Use GetLastError() to determine the cause.
 */
INT WINAPI LCMapStringEx(LPCWSTR locale, DWORD flags, LPCWSTR src, INT srclen, LPWSTR dst, INT dstlen,
                         LPNLSVERSIONINFO version, LPVOID reserved, LPARAM handle)
{
   LCID lpLocale = LocaleNameToLCID(locale, 0);
   INT res;
   
  	if (flags & 0x8000000)
		flags ^= 0x8000000; 

    if (lpLocale == 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    // First use Native API
    res = LCMapStringW(lpLocale, flags, src, srclen, dst, dstlen);
    if (res != 0 || (GetLastError() != ERROR_INVALID_FLAGS && GetLastError() != ERROR_INVALID_PARAMETER)){ // incase Win8 API is used
        return res;
	}
		
    if (version) FIXME("unsupported version structure %p\n", version);
    if (reserved) FIXME("unsupported reserved pointer %p\n", reserved);
    if (handle)
    {
        static int once;
        if (!once++) FIXME("unsupported lparam %Ix\n", handle);
    }

    if (!src || !srclen || dstlen < 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    if (srclen < 0) srclen = lstrlenW(src) + 1;

    TRACE( "(%s,0x%08lx,%s,%d,%p,%d)\n",
           debugstr_w(locale), flags, debugstr_wn(src, srclen), srclen, dst, dstlen );

    flags &= ~LOCALE_USE_CP_ACP;

    if (src == dst && (flags & ~(LCMAP_LOWERCASE | LCMAP_UPPERCASE)))
    {
        SetLastError(ERROR_INVALID_FLAGS);
        return 0;
    }

    if (!dstlen) dst = NULL;

    if (flags & LCMAP_SORTKEY)
    {
        INT ret;

        if (srclen < 0)
            srclen = strlenW(src);

        ret = wine_get_sortkey(flags, src, srclen, (char *)dst, dstlen);
        if (ret == 0)
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
        else
            ret++;
        return ret;
    }

    /* SORT_STRINGSORT must be used exclusively with LCMAP_SORTKEY */
    if (flags & SORT_STRINGSORT)
    {
        SetLastError(ERROR_INVALID_FLAGS);
        return 0;
    }

    return lcmap_string(flags, src, srclen, dst, dstlen);
}

INT 
WINAPI 
GetUserDefaultLocaleName(
	LPWSTR localename, 
	int buffersize
)
{
	int ret;
	
    ret = LCIDToLocaleName(GetUserDefaultLCID(), localename, buffersize, 0);
	
	// DbgPrint("GetUserDefaultLocaleName :: Default Locale: %s\n", localename);
	
	// DbgPrint("GetUserDefaultLocaleName :: Return value: %d\n", ret);
	
	return ret;
}

static BOOL CALLBACK enum_locale_ex_proc( HMODULE module, LPCWSTR type,
                                          LPCWSTR name, WORD lang, LONG_PTR lparam )
{
    struct enum_locale_ex_data *data = (struct enum_locale_ex_data *)lparam;
    WCHAR buffer[256];
    DWORD neutral;
    unsigned int flags;

    GetLocaleInfoW( MAKELCID( lang, SORT_DEFAULT ), LOCALE_SNAME | LOCALE_NOUSEROVERRIDE,
                    buffer, sizeof(buffer) / sizeof(WCHAR) );
    if (!GetLocaleInfoW( MAKELCID( lang, SORT_DEFAULT ),
                         LOCALE_INEUTRAL | LOCALE_NOUSEROVERRIDE | LOCALE_RETURN_NUMBER,
                         (LPWSTR)&neutral, sizeof(neutral) / sizeof(WCHAR) ))
        neutral = 0;
    flags = LOCALE_WINDOWS;
    flags |= neutral ? LOCALE_NEUTRALDATA : LOCALE_SPECIFICDATA;
    if (data->flags && !(data->flags & flags)) return TRUE;
    return data->proc( buffer, flags, data->lparam );
}

/* retrieve the resource name to pass to the ntdll functions */
static NTSTATUS get_res_nameW( LPCWSTR name, UNICODE_STRING *str )
{
    if (IS_INTRESOURCE(name))
    {
        str->Buffer = ULongToPtr(LOWORD(name));
        return STATUS_SUCCESS;
    }
    if (name[0] == '#')
    {
        ULONG value;
        RtlInitUnicodeString( str, name + 1 );
        if (RtlUnicodeStringToInteger( str, 10, &value ) != STATUS_SUCCESS || HIWORD(value))
            return STATUS_INVALID_PARAMETER;
        str->Buffer = ULongToPtr(value);
        return STATUS_SUCCESS;
    }
    RtlCreateUnicodeString( str, name );
    RtlUpcaseUnicodeString( str, str, FALSE );
    return STATUS_SUCCESS;
}

/* retrieve the resource name to pass to the ntdll functions */
static NTSTATUS get_res_nameA( LPCSTR name, UNICODE_STRING *str )
{
    if (IS_INTRESOURCE(name))
    {
        str->Buffer = ULongToPtr(LOWORD(name));
        return STATUS_SUCCESS;
    }
    if (name[0] == '#')
    {
        ULONG value;
        if (RtlCharToInteger( name + 1, 10, &value ) != STATUS_SUCCESS || HIWORD(value))
            return STATUS_INVALID_PARAMETER;
        str->Buffer = ULongToPtr(value);
        return STATUS_SUCCESS;
    }
    RtlCreateUnicodeStringFromAsciiz( str, name );
    RtlUpcaseUnicodeString( str, str, FALSE );
    return STATUS_SUCCESS;
}

/**********************************************************************
 *	EnumResourceLanguagesExA	(KERNEL32.@)
 */
BOOL WINAPI EnumResourceLanguagesExA( HMODULE hmod, LPCSTR type, LPCSTR name,
                                      ENUMRESLANGPROCA lpfun, LONG_PTR lparam,
                                      DWORD flags, LANGID lang )
{
    int i;
    BOOL ret = FALSE;
    NTSTATUS status;
    UNICODE_STRING typeW, nameW;
    LDR_RESOURCE_INFO info;
    const IMAGE_RESOURCE_DIRECTORY *basedir, *resdir;
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *et;

    TRACE( "%p %s %s %p %lx %x %d\n", hmod, debugstr_a(type), debugstr_a(name),
           lpfun, lparam, flags, lang );

    if (flags & (RESOURCE_ENUM_MUI | RESOURCE_ENUM_MUI_SYSTEM | RESOURCE_ENUM_VALIDATE))
        FIXME( "unimplemented flags: %x\n", flags );

    if (!flags) flags = RESOURCE_ENUM_LN | RESOURCE_ENUM_MUI;

    if (!(flags & RESOURCE_ENUM_LN)) return ret;

    if (!hmod) hmod = GetModuleHandleA( NULL );
    typeW.Buffer = nameW.Buffer = NULL;
    if ((status = LdrFindResourceDirectory_U( hmod, (const LDR_RESOURCE_INFO *)NULL, 0, &basedir )) != STATUS_SUCCESS)
        goto done;
    if ((status = get_res_nameA( type, &typeW )) != STATUS_SUCCESS)
        goto done;
    if ((status = get_res_nameA( name, &nameW )) != STATUS_SUCCESS)
        goto done;
    info.Type = (ULONG_PTR)typeW.Buffer;
    info.Name = (ULONG_PTR)nameW.Buffer;
    if ((status = LdrFindResourceDirectory_U( hmod, &info, 2, &resdir )) != STATUS_SUCCESS)
        goto done;

    et = (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(resdir + 1);
    _SEH2_TRY
    {
        for (i = 0; i < resdir->NumberOfNamedEntries + resdir->NumberOfIdEntries; i++)
        {
            ret = lpfun( hmod, type, name, et[i].Id, lparam );
            if (!ret) break;
        }
    }
    _SEH2_EXCEPT(UnhandledExceptionFilter(_SEH2_GetExceptionInformation()))
    {
        ret = FALSE;
        status = STATUS_ACCESS_VIOLATION;
    }
    _SEH2_END
done:
    if (!IS_INTRESOURCE(typeW.Buffer)) HeapFree( GetProcessHeap(), 0, typeW.Buffer );
    if (!IS_INTRESOURCE(nameW.Buffer)) HeapFree( GetProcessHeap(), 0, nameW.Buffer );
    if (status != STATUS_SUCCESS) SetLastError( RtlNtStatusToDosError(status) );
    return ret;
}

/**********************************************************************
 *	EnumResourceLanguagesExW	(KERNEL32.@)
 */
BOOL WINAPI EnumResourceLanguagesExW( 
	HMODULE hmod, 
	LPCWSTR type, 
	LPCWSTR name,
    ENUMRESLANGPROCW lpfun, 
	LONG_PTR lparam,
    DWORD flags, 
	LANGID lang 
)
{
    int i;
    BOOL ret = FALSE;
    NTSTATUS status;
    UNICODE_STRING typeW, nameW;
    LDR_RESOURCE_INFO info;
    const IMAGE_RESOURCE_DIRECTORY *basedir, *resdir;
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *et;

    TRACE( "%p %s %s %p %lx %x %d\n", hmod, debugstr_w(type), debugstr_w(name),
           lpfun, lparam, flags, lang );

    if (flags & (RESOURCE_ENUM_MUI | RESOURCE_ENUM_MUI_SYSTEM | RESOURCE_ENUM_VALIDATE))
        FIXME( "unimplemented flags: %x\n", flags );

    if (!flags) flags = RESOURCE_ENUM_LN | RESOURCE_ENUM_MUI;

    if (!(flags & RESOURCE_ENUM_LN)) return ret;

    if (!hmod) hmod = GetModuleHandleW( NULL );
    typeW.Buffer = nameW.Buffer = NULL;
    if ((status = LdrFindResourceDirectory_U( hmod, NULL, 0, &basedir )) != STATUS_SUCCESS)
        goto done;
    if ((status = get_res_nameW( type, &typeW )) != STATUS_SUCCESS)
        goto done;
    if ((status = get_res_nameW( name, &nameW )) != STATUS_SUCCESS)
        goto done;
    info.Type = (ULONG_PTR)typeW.Buffer;
    info.Name = (ULONG_PTR)nameW.Buffer;
    if ((status = LdrFindResourceDirectory_U( hmod, &info, 2, &resdir )) != STATUS_SUCCESS)
        goto done;

    et = (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(resdir + 1);
    _SEH2_TRY
    {
        for (i = 0; i < resdir->NumberOfNamedEntries + resdir->NumberOfIdEntries; i++)
        {
            ret = lpfun( hmod, type, name, et[i].Id, lparam );
            if (!ret) break;
        }
    }
     _SEH2_EXCEPT(UnhandledExceptionFilter(_SEH2_GetExceptionInformation()))
    {
        ret = FALSE;
        status = STATUS_ACCESS_VIOLATION;
    }
    _SEH2_END
done:
    if (!IS_INTRESOURCE(typeW.Buffer)) HeapFree( GetProcessHeap(), 0, typeW.Buffer );
    if (!IS_INTRESOURCE(nameW.Buffer)) HeapFree( GetProcessHeap(), 0, nameW.Buffer );
    if (status != STATUS_SUCCESS) SetLastError( RtlNtStatusToDosError(status) );
    return ret;
}

BOOL CALLBACK EnumLocalesProc(
  _In_ LPTSTR lpLocaleString
)
{
	DbgPrint("EnumLocalesProc called\n");
	
	if(ARGUMENT_PRESENT(lpLocaleString))
	{
		DbgPrint("EnumLocalesProc:: lpLocaleString is %s\n", lpLocaleString);
		LCIDToLocaleName((LCID)lpLocaleString, systemLocale, 0, 0);
		return TRUE;
	}	
	return FALSE;
}

/******************************************************************************
 *	EnumSystemLocalesEx   (kernelex.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH EnumSystemLocalesEx( LOCALE_ENUMPROCEX proc, DWORD wanted_flags,
                                                   LPARAM param, void *reserved )
{
    WCHAR buffer[256], name[10];
    DWORD name_len, type, neutral, flags, index = 0, alt = 0;
    HKEY key, altkey;
    LCID lcid;

    if (reserved)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    if (RegOpenKeyExW( nls_key, L"Locale", 0, KEY_READ, &key )) return FALSE;
    if (RegOpenKeyExW( key, L"Alternate Sorts", 0, KEY_READ, &altkey )) altkey = 0;

    for (;;)
    {
        name_len = ARRAY_SIZE(name);
        if (RegEnumValueW( alt ? altkey : key, index++, name, &name_len, NULL, &type, NULL, NULL ))
        {
            if (alt++) break;
            index = 0;
            continue;
        }
        if (type != REG_SZ) continue;
        if (!(lcid = wcstoul( name, NULL, 16 ))) continue;

        GetLocaleInfoW( lcid, LOCALE_SNAME | LOCALE_NOUSEROVERRIDE, buffer, ARRAY_SIZE( buffer ));
        if (!GetLocaleInfoW( lcid, LOCALE_INEUTRAL | LOCALE_NOUSEROVERRIDE | LOCALE_RETURN_NUMBER,
                             (LPWSTR)&neutral, sizeof(neutral) / sizeof(WCHAR) ))
            neutral = 0;

        if (alt)
            flags = LOCALE_ALTERNATE_SORTS;
        else
            flags = LOCALE_WINDOWS | (neutral ? LOCALE_NEUTRALDATA : LOCALE_SPECIFICDATA);

        if (wanted_flags && !(flags & wanted_flags)) continue;
        if (!proc( buffer, flags, param )) break;
    }
    RegCloseKey( altkey );
    RegCloseKey( key );
    return TRUE;
}

//Wrapper to special cases of GetLocaleInfoW
int 
WINAPI 
GetpLocaleInfoW(
    LCID Locale,
    LCTYPE LCType,
    LPWSTR lpLCData,
    int cchData)
{
	switch(LCType){
		case ( LOCALE_SNAME ) :
			return LCIDToLocaleName(Locale, lpLCData, LOCALE_NAME_MAX_LENGTH, 0);
		default:
			return GetLocaleInfoW(Locale, LCType, lpLCData, cchData);
	}
}

int 
WINAPI
GeptLocaleInfoA(
  _In_      LCID   Locale,
  _In_      LCTYPE LCType,
  _Out_opt_ LPTSTR lpLCData,
  _In_      int    cchData
)
{
    WCHAR pDTmp[MAX_STRING_LEN];  // tmp Unicode buffer (destination)
    LPWSTR pBuf;                  // ptr to destination buffer	
	int numberCharacters;
	
	pBuf = pDTmp;
	switch(LCType){
		case ( LOCALE_SNAME ) :
			numberCharacters = LCIDToLocaleName(Locale, pBuf, LOCALE_NAME_MAX_LENGTH, 0);
			numberCharacters *= sizeof(WCHAR);
            if (lpLCData)
            {
                if (numberCharacters <= cchData) 
				{
					memcpy( lpLCData, pBuf, numberCharacters );
				}					
			}
			return numberCharacters;
		default:
			return GetLocaleInfoA(Locale, LCType, lpLCData, cchData);
	}	
}

/******************************************************************************
 *           GetLocaleInfoEx (KERNEL32.@)
 */
INT 
WINAPI 
GetLocaleInfoEx(
	LPCWSTR locale, 
	LCTYPE info, 
	LPWSTR buffer, 
	INT len
)
{
    LCID lcid = LocaleNameToLCID(locale, 0);

    TRACE("%s, lcid=0x%x, 0x%x\n", debugstr_w(locale), lcid, info);

    if (!lcid) return 0;
	
	if (info == 0x20000071){ // Hack to fix .NET Core
		*buffer = 1;
		return TRUE;
	}

    /* special handling for neutral locale names */
    if (locale && strlenW(locale) == 2)
    {
        switch (info)
        {
        case LOCALE_SNAME:
            if (len && len < 3)
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return 0;
            }
            if (len) strcpyW(buffer, locale);
            return 3;
        case LOCALE_SPARENT:
            if (len) buffer[0] = 0;
            return 1;
        }
    }

    return GetpLocaleInfoW(lcid, info, buffer, len);
}

/******************************************************************************
 *	GetNLSVersionEx   (kernelex.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetNLSVersionEx( NLS_FUNCTION func, const WCHAR *locale,
                                               NLSVERSIONINFOEX *info )
{
    LCID lcid = 0;

    if (func != COMPARE_STRING)
    {
        SetLastError( ERROR_INVALID_FLAGS );
        return FALSE;
    }
    if (info->dwNLSVersionInfoSize < sizeof(*info) &&
        (info->dwNLSVersionInfoSize != offsetof( NLSVERSIONINFO, dwNLSVersionInfoSize )))
    {
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        return FALSE;
    }

    if (!(lcid = LocaleNameToLCID( locale, 0 ))) return FALSE;

    info->dwNLSVersion = info->dwDefinedVersion = sort.version;
    if (info->dwNLSVersionInfoSize >= sizeof(*info))
    {
        const struct sortguid *sortid = get_language_sort( locale );
        info->dwEffectiveId = lcid;
        info->guidCustomVersion = sortid ? sortid->id : default_sort_guid;
    }
    return TRUE;
}

BOOL 
WINAPI 
GetNLSVersion(
    NLS_FUNCTION     func,
    LCID             lcid,
    LPNLSVERSIONINFO info)
{
    WCHAR locale[LOCALE_NAME_MAX_LENGTH];	
	
    if (info->dwNLSVersionInfoSize < offsetof( NLSVERSIONINFO, dwNLSVersionInfoSize ))
    {
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        return FALSE;
    }
    if (!LCIDToLocaleName( lcid, locale, LOCALE_NAME_MAX_LENGTH, LOCALE_ALLOW_NEUTRAL_NAMES ))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    return GetNLSVersionEx( func, locale, (NLSVERSIONINFOEX *)info );
}

int 
WINAPI 
FindNLSString(
  _In_ 		 LCID Locale,
  _In_       DWORD dwFindNLSStringFlags,
  _In_       LPCWSTR lpStringSource,
  _In_       int cchSource,
  _In_       LPCWSTR lpStringValue,
  _In_       int cchValue,
  _Out_opt_  LPINT pcchFound
)
{
	const WCHAR localename[10]; // VS2012 Toolbox Crash bugfix
	
	LCIDToLocaleName(Locale, localename, MAX_STRING_LEN, 0);
	return FindNLSStringEx(localename,
						   dwFindNLSStringFlags,
						   lpStringSource,
						   cchSource,
						   lpStringValue,
						   cchValue,
						   pcchFound,
						   NULL,
						   NULL,
						   0);
}

/******************************************************************************
 *           FindNLSStringEx (KERNEL32.@)
 */

INT 
WINAPI 
FindNLSStringEx(
	const WCHAR *localename, 
	DWORD flags, 
	const WCHAR *src,
    INT src_size, 
	const WCHAR *value, 
	INT value_size,
    INT *found, 
	NLSVERSIONINFO *version_info, 
	void *reserved,
    LPARAM sort_handle
)
{

    /* FIXME: this function should normalize strings before calling CompareStringEx() */
    DWORD mask = flags;
    int offset, inc, count;

    TRACE("%s %x %s %d %s %d %p %p %p %ld\n", wine_dbgstr_w(localename), flags,
          wine_dbgstr_w(src), src_size, wine_dbgstr_w(value), value_size, found,
          version_info, reserved, sort_handle);

    if (version_info != NULL || reserved != NULL || sort_handle != 0 ||
        !IsValidLocaleName(localename) || src == NULL || src_size == 0 ||
        src_size < -1 || value == NULL || value_size == 0 || value_size < -1)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    if (src_size == -1)
        src_size = strlenW(src);
    if (value_size == -1)
        value_size = strlenW(value);

    src_size -= value_size;
    if (src_size < 0) return -1;

    mask = flags & ~(FIND_FROMSTART | FIND_FROMEND | FIND_STARTSWITH | FIND_ENDSWITH);
    count = flags & (FIND_FROMSTART | FIND_FROMEND) ? src_size + 1 : 1;
    offset = flags & (FIND_FROMSTART | FIND_STARTSWITH) ? 0 : src_size;
    inc = flags & (FIND_FROMSTART | FIND_STARTSWITH) ? 1 : -1;
    while (count--)
    {
        if (CompareStringEx(localename, mask, src + offset, value_size, value, value_size, NULL, NULL, 0) == CSTR_EQUAL)
        {
            if (found)
                *found = value_size;
            return offset;
        }
        offset += inc;
    }

    return -1;
}

    // int
    // WINAPI
    // FindNLSStringEx(
        // _In_opt_ LPCWSTR _szLocaleName,
        // _In_ DWORD _fFindNLSStringFlags,
        // _In_reads_(_cchSource) LPCWSTR _szStringSource,
        // _In_ int _cchSource,
        // _In_reads_(_cchValue) LPCWSTR _szStringValue,
        // _In_ int _cchValue,
        // _Out_opt_ LPINT _pcchFound,
        // _In_opt_ LPNLSVERSIONINFO _pVersionInformation,
        // _In_opt_ LPVOID _pReserved,
        // _In_opt_ LPARAM _hSortHandle
        // )
    // {
        // // if (auto const _pfnFindNLSStringEx = try_get_FindNLSStringEx())
        // // {
            // // return _pfnFindNLSStringEx(_szLocaleName, _fFindNLSStringFlags, _szStringSource, _cchSource, _szStringValue, _cchValue, _pcchFound, _pVersionInformation, _pReserved, _hSortHandle);
        // // }

        // //__WarningMessage__("FindNLSStringEx 暂时只支持搜索 _cchValue 的子字符串。");

        // const DWORD _fFindFlags = _fFindNLSStringFlags & (FIND_STARTSWITH | FIND_ENDSWITH | FIND_FROMSTART | FIND_FROMEND);
        // const LCID _Locale = LocaleNameToLCID(_szLocaleName, 0);
		// const DWORD _fCmpFlags = _fFindNLSStringFlags & ~(FIND_STARTSWITH | FIND_ENDSWITH | FIND_FROMSTART | FIND_FROMEND);
		// INT _nResult;
        // LPCWSTR _szStr;
        // LPCWSTR _szStrEnd;		
		
		// if (_fFindFlags & (_fFindFlags - 1))
        // {
            // SetLastError(ERROR_INVALID_FLAGS);
            // return -1;
        // }

        // if (_pVersionInformation || _pReserved|| _hSortHandle
            // || _szStringSource == NULL || _cchSource == 0 || _cchSource < -1
            // || _szStringValue == NULL || _cchValue == 0 || _cchValue < -1)
        // {
            // SetLastError(ERROR_INVALID_PARAMETER);
            // return -1;
        // }

        
        // if (_Locale == 0)
        // {
            // SetLastError(ERROR_INVALID_PARAMETER);
            // return -1;
        // }

        // if (_cchSource == -1)
        // {
            // _cchSource = wcslen(_szStringSource);
        // }

        // if (_cchValue == -1)
        // {
            // _cchValue = wcslen(_szStringValue);
        // }

        // if (_cchSource < _cchValue)
            // return -1;

       
        // if ((_fFindNLSStringFlags & (FIND_ENDSWITH | FIND_FROMEND)) == 0)
        // {
            // // 从头开始搜索
            // if (_fFindNLSStringFlags & FIND_STARTSWITH)
            // {
                // _nResult = CompareStringW(_Locale, _fCmpFlags, _szStringSource, _cchValue, _szStringValue, _cchValue);
                // if (_nResult == CSTR_EQUAL)
                // {
                    // if (_pcchFound)
                        // *_pcchFound = _cchValue;

                    // return 0;
                // }
            // }
            // else
            // {
                // _szStr = _szStringSource;
                // _szStrEnd = _szStringSource + _cchSource - _cchValue + 1;
                // for (; _szStr != _szStrEnd;++_szStr)
                // {
                    // _nResult = CompareStringW(_Locale, _fCmpFlags, _szStr, _cchValue, _szStringValue, _cchValue);
                    // if (_nResult == 0)
                        // return -1;

                    // if (_nResult == CSTR_EQUAL)
                    // {
                        // if (_pcchFound)
                            // *_pcchFound = _cchValue;

                        // return _szStr - _szStringSource;
                    // }
                // }
            // }
        // }
        // else
        // {
            // // 反向搜索
            // _szStr = _szStringSource + _cchSource - _cchValue;
            // if (_fFindNLSStringFlags & FIND_ENDSWITH)
            // {
                // _nResult = CompareStringW(_Locale, _fCmpFlags, _szStr, _cchValue, _szStringValue, _cchValue);
                // if (_nResult == CSTR_EQUAL)
                // {
                    // if (_pcchFound)
                        // *_pcchFound = _cchValue;

                    // return _szStr - _szStringSource;
                // }
            // }
            // else
            // {
                // for(;; --_szStr)
                // {
                    // _nResult = CompareStringW(_Locale, _fCmpFlags, _szStr, _cchValue, _szStringValue, _cchValue);
                    // if (_nResult == 0)
                        // return -1;

                    // if (_nResult == CSTR_EQUAL)
                    // {
                        // if (_pcchFound)
                            // *_pcchFound = _cchValue;

                        // return _szStr - _szStringSource;
                    // }
                    
                    // if (_szStr == _szStringSource)
                        // break;
                // } 
            // }
        // }

        // return -1;
    // }

/******************************************************************************
 *	ResolveLocaleName   (kernelex.@)
 */
int
WINAPI
ResolveLocaleName(
	_In_opt_                        LPCWSTR lpNameToResolve,
	_Out_writes_opt_(cchLocaleName) LPWSTR  lpLocaleName,
	_In_                            int     cchLocaleName
)
{
	LCID lcid = 0;
	wchar_t Buffer[LOCALE_NAME_MAX_LENGTH];
	unsigned i = 0;
	int result;

	if (lpNameToResolve == NULL)
	{
		lcid = GetUserDefaultLCID();
	}
	else
	{
		for (; i != _countof(Buffer) && lpNameToResolve[i]; ++i)
		{
			Buffer[i] = lpNameToResolve[i];

			if (i == _countof(Buffer))
			{
				//输入异常
				SetLastError(ERROR_INVALID_PARAMETER);
				return 0;
			}

			//保证 '\0' 截断
			Buffer[i] = L'\0';

			for (;;)
			{
				lcid = LocaleNameToLCID(Buffer, 0);

				//成功找到了区域，那么停止搜索
				if (lcid != 0 && lcid != LOCALE_CUSTOM_UNSPECIFIED)
					break;

				while(i)
				{
					--i;

					if (Buffer[i] == L'-')
					{
						Buffer[i] = L'\0';
						break;
					}
				}

				//字符串已经达到开始位置，则停止搜索
				if (i == 0)
					break;
			}
		}
	}

	if (lcid !=0 && lcid != LOCALE_CUSTOM_UNSPECIFIED)
	{
		if (lpLocaleName == NULL || cchLocaleName == 0)
		{
			lpLocaleName = NULL;
			cchLocaleName = 0;
		}

		//删除排序规则，然后重新转换名称。
		result = LCIDToLocaleName(LANGIDFROMLCID(lcid), lpLocaleName, cchLocaleName, 0);

		if (result)
			return result;
	}
			
	//如果找不到，那么直接设置为空字符串
	if (lpLocaleName && cchLocaleName)
	{
		lpLocaleName[0] = L'\0';
	}

	return 1;
}

/******************************************************************************
 *             GetUserPreferredUILanguages (KERNEL32.@)
 */
BOOL 
WINAPI 
GetUserPreferredUILanguages( 
  _In_      DWORD   dwFlags,
  _Out_     PULONG  pulNumLanguages,
  _Out_opt_ PZZWSTR pwszLanguagesBuffer,
  _Inout_   PULONG  pcchLanguagesBuffer
)
{
	LANGID ui_language;
	
	NtQueryDefaultUILanguage( &ui_language );
	// return set_ntstatus( RtlGetUserPreferredUILanguages( dwFlags, 0, pulNumLanguages, pwszLanguagesBuffer, pcchLanguagesBuffer ));
	return EnumPreferredUserUILanguages(dwFlags,
										ui_language,
									    pulNumLanguages,
									    pwszLanguagesBuffer,
									    pcchLanguagesBuffer);
}

/***********************************************************************
 *      GetThreadPreferredUILanguages   (kernelex.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetThreadPreferredUILanguages( DWORD flags, ULONG *count,
                                                             WCHAR *buffer, ULONG *size )
{
	LANGID ui_language;
	
	NtQueryDefaultUILanguage( &ui_language );
    // return set_ntstatus( RtlGetThreadPreferredUILanguages( flags, count, buffer, size ));
	return EnumPreferredThreadUILanguages(flags,
										  ui_language,
										  count,
										  buffer,
										  size);	
}

/***********************************************************************
 *      GetSystemPreferredUILanguages   (kernelex.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetSystemPreferredUILanguages( DWORD flags, ULONG *count,
                                                             WCHAR *buffer, ULONG *size )
{
	LANGID ui_language;
	
	NtQueryInstallUILanguage( &ui_language );	
    // return set_ntstatus( RtlGetSystemPreferredUILanguages( flags, 0, count, buffer, size ));
	return EnumPreferredThreadUILanguages(flags,
										  ui_language,
										  count,
										  buffer,
										  size);		
}

/***********************************************************************
 *      GetProcessPreferredUILanguages   (kernelex.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetProcessPreferredUILanguages( DWORD flags, ULONG *count,
                                                              WCHAR *buffer, ULONG *size )
{
    return set_ntstatus( RtlGetProcessPreferredUILanguages( flags, count, buffer, size ));
}

/***********************************************************************
 *      SetThreadPreferredUILanguages   (kernelex.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH SetThreadPreferredUILanguages( DWORD flags, PCZZWSTR buffer, ULONG *count )
{
    return set_ntstatus( RtlSetThreadPreferredUILanguages( flags, buffer, count ));
}

/***********************************************************************
 *      SetProcessPreferredUILanguages   (kernelex.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH SetProcessPreferredUILanguages( DWORD flags, PCZZWSTR buffer, ULONG *count )
{
    return set_ntstatus( RtlSetProcessPreferredUILanguages( flags, buffer, count ));
}

/******************************************************************************
  *           GetFileMUIInfo (KERNEL32.@)
  */

BOOL WINAPI GetFileMUIInfo(DWORD flags, PCWSTR path, FILEMUIINFO *info, DWORD *size)
{
    FIXME("stub: %u, %s, %p, %p\n", flags, debugstr_w(path), info, size);
 
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}


/******************************************************************************
 *	IsValidNLSVersion   (kernelex.@)
 */
DWORD WINAPI DECLSPEC_HOTPATCH IsValidNLSVersion( NLS_FUNCTION func, const WCHAR *locale,
                                                  NLSVERSIONINFOEX *info )
{
    static const GUID GUID_NULL;
    NLSVERSIONINFOEX infoex;
    DWORD ret;

    if (func != COMPARE_STRING)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    if (info->dwNLSVersionInfoSize < sizeof(*info) &&
        (info->dwNLSVersionInfoSize != offsetof( NLSVERSIONINFOEX, dwEffectiveId )))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    infoex.dwNLSVersionInfoSize = sizeof(infoex);
    if (!GetNLSVersionEx( func, locale, &infoex )) return FALSE;

    ret = (infoex.dwNLSVersion & ~0xff) == (info->dwNLSVersion & ~0xff);
    if (ret && !IsEqualGUID( &info->guidCustomVersion, &GUID_NULL ))
        ret = find_sortguid( &info->guidCustomVersion ) != NULL;

    if (!ret) SetLastError( ERROR_SUCCESS );
    return ret;
}

/******************************************************************************
 *	IdnToAscii   (kernelex.@)
 */
INT WINAPI DECLSPEC_HOTPATCH IdnToAscii( DWORD flags, const WCHAR *src, INT srclen,
                                         WCHAR *dst, INT dstlen )
{
    NTSTATUS status = RtlIdnToAscii( flags, src, srclen, dst, &dstlen );
    if (!NT_SUCCESS( status )){
		SetLastError( ERROR_SUCCESS );
		return 0;
	}		
    return dstlen;
}

/******************************************************************************
 *	IdnToNameprepUnicode   (kernelex.@)
 */
INT WINAPI DECLSPEC_HOTPATCH IdnToNameprepUnicode( DWORD flags, const WCHAR *src, INT srclen,
                                                   WCHAR *dst, INT dstlen )
{
    NTSTATUS status = RtlIdnToNameprepUnicode( flags, src, srclen, dst, &dstlen );
    if (!NT_SUCCESS( status )){
		SetLastError( ERROR_SUCCESS );
		return 0;
	}		
    return dstlen;
}

/******************************************************************************
 *	IdnToUnicode   (kernelex.@)
 */
INT WINAPI DECLSPEC_HOTPATCH IdnToUnicode( DWORD flags, const WCHAR *src, INT srclen,
                                           WCHAR *dst, INT dstlen )
{
    NTSTATUS status = RtlIdnToUnicode( flags, src, srclen, dst, &dstlen );
    if (!NT_SUCCESS( status )){
		SetLastError( ERROR_SUCCESS );
		return 0;
	}		
    return dstlen;
}

/******************************************************************************
 *	IsNormalizedString   (kernelex.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH IsNormalizedString( NORM_FORM form, const WCHAR *str, INT len )
{
    BOOLEAN res = TRUE;
    if (!NT_SUCCESS(RtlIsNormalizedString( form, str, len, &res ))){
		SetLastError( ERROR_SUCCESS );
		res = FALSE;
	}		
    return res;
}

/******************************************************************************
 *	NormalizeString   (kernelex.@)
 */
INT WINAPI DECLSPEC_HOTPATCH NormalizeString(NORM_FORM form, const WCHAR *src, INT src_len,
                                             WCHAR *dst, INT dst_len)
{
    NTSTATUS status = RtlNormalizeString( form, src, src_len, dst, &dst_len );

    switch (status)
    {
    case STATUS_OBJECT_NAME_NOT_FOUND:
        status = STATUS_INVALID_PARAMETER;
        break;
    case STATUS_BUFFER_TOO_SMALL:
    case STATUS_NO_UNICODE_TRANSLATION:
        dst_len = -dst_len;
        break;
    }
    SetLastError( RtlNtStatusToDosError( status ));
    return dst_len;
}

/******************************************************************************
 *	EnumDynamicTimeZoneInformation   (kernelex.@)
 */
DWORD WINAPI DECLSPEC_HOTPATCH EnumDynamicTimeZoneInformation( DWORD index,
                                                               DYNAMIC_TIME_ZONE_INFORMATION *info )
{
    DYNAMIC_TIME_ZONE_INFORMATION tz;
    LSTATUS ret;
    DWORD size;

    if (!info) return ERROR_INVALID_PARAMETER;

    size = ARRAY_SIZE(tz.TimeZoneKeyName);
    ret = RegEnumKeyExW( tz_key, index, tz.TimeZoneKeyName, &size, NULL, NULL, NULL, NULL );
    if (ret) return ret;

    tz.DynamicDaylightTimeDisabled = TRUE;
    if (!GetTimeZoneInformationForYear( 0, &tz, (TIME_ZONE_INFORMATION *)info )) return GetLastError();

    lstrcpyW( info->TimeZoneKeyName, tz.TimeZoneKeyName );
    info->DynamicDaylightTimeDisabled = FALSE;
    return 0;
}

/******************************************************************************
 *	GetDynamicTimeZoneInformationEffectiveYears   (kernelex.@)
 */
DWORD WINAPI DECLSPEC_HOTPATCH GetDynamicTimeZoneInformationEffectiveYears( const DYNAMIC_TIME_ZONE_INFORMATION *info,
                                                                            DWORD *first, DWORD *last )
{
    HKEY key, dst_key = 0;
    DWORD type, count, ret = ERROR_FILE_NOT_FOUND;

    if (RegOpenKeyExW( tz_key, info->TimeZoneKeyName, 0, KEY_ALL_ACCESS, &key )) return ret;

    if (RegOpenKeyExW( key, L"Dynamic DST", 0, KEY_ALL_ACCESS, &dst_key )) goto done;
    count = sizeof(DWORD);
    if (RegQueryValueExW( dst_key, L"FirstEntry", NULL, &type, (BYTE *)first, &count )) goto done;
    if (type != REG_DWORD) goto done;
    count = sizeof(DWORD);
    if (RegQueryValueExW( dst_key, L"LastEntry", NULL, &type, (BYTE *)last, &count )) goto done;
    if (type != REG_DWORD) goto done;
    ret = 0;

done:
    RegCloseKey( dst_key );
    RegCloseKey( key );
    return ret;
}

static int wcstombs_utf8( DWORD flags, const WCHAR *src, int srclen, char *dst, int dstlen,
                          const char *defchar, BOOL *used )
{
    DWORD reslen;
    NTSTATUS status;

    if (used) *used = FALSE;
    if (!dstlen) dst = NULL;
    status = RtlUnicodeToUTF8N( dst, dstlen, &reslen, src, srclen * sizeof(WCHAR) );
    if (status == STATUS_SOME_NOT_MAPPED)
    {
        if (flags & WC_ERR_INVALID_CHARS)
        {
            SetLastError( ERROR_NO_UNICODE_TRANSLATION );
            return 0;
        }
        if (used) *used = TRUE;
    }
    else if (!set_ntstatus( status )) reslen = 0;
    return reslen;
}

/***********************************************************************
 *	WideCharToMultiByte   (kernelex.@)
 */
INT WINAPI DECLSPEC_HOTPATCH WideCharToMultiByteInternal( UINT codepage, DWORD flags, LPCWSTR src, INT srclen,
                                                  LPSTR dst, INT dstlen, LPCSTR defchar, BOOL *used )
{
    int ret = 0;

	if(flags & WC_ERR_INVALID_CHARS){
		ret = wcstombs_utf8( flags, src, srclen, dst, dstlen, defchar, used );
	}else{
		ret = WideCharToMultiByte(codepage, flags, src, srclen, dst, dstlen, defchar, used);
	}	
	
	return ret;
}

// based on VxKex 1.1.2
INT WINAPI GetUserDefaultGeoName(
    OUT    PWSTR    Buffer,
    IN    INT        BufferCch)
{
    GEOID GeoId;

    //
    // Validate parameters.
    //

    if (Buffer == NULL && BufferCch != 0) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return 0;
    }

    //
    // Get the user's default GeoID.
    //

    GeoId = GetUserGeoID(GEOCLASS_NATION);

    if (GeoId == GEOID_NOT_AVAILABLE) {
        GeoId = GetUserGeoID(GEOCLASS_REGION);
    }

    ASSERT (GeoId != GEOID_NOT_AVAILABLE);

    if (GeoId == GEOID_NOT_AVAILABLE) {
        RtlSetLastWin32Error(ERROR_BADDB);
        return 0;
    }

    //
    // Convert the GeoID into a two-letter ISO country code.
    //

    return GetGeoInfoW(GeoId, GEO_ISO2, Buffer, BufferCch, 0);
}