/*****************************************************************************
 * unicode.c: Unicode <-> locale functions
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
 * Copyright © 2005-2006 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc_charset.h>

#include <assert.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#  include <dirent.h>
#endif
#ifdef UNDER_CE
#  include <tchar.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef WIN32
# include <io.h>
#else
# include <unistd.h>
#endif

#ifndef HAVE_LSTAT
# define lstat( a, b ) stat(a, b)
#endif

#ifdef __APPLE__
/* Define this if the OS always use UTF-8 internally */
# define ASSUME_UTF8 1
#endif

#ifndef ASSUME_UTF8
# if defined (HAVE_ICONV)
/* libiconv is more powerful than Win32 API (it has translit) */
#  define USE_ICONV 1
# elif defined (WIN32) || defined (UNDER_CE)
#  define USE_MB2MB 1
# else
#  error No UTF8 charset conversion implemented on this platform!
# endif
#endif

typedef struct locale_data_t
{
#if defined (USE_ICONV)
    vlc_iconv_t hd;
    vlc_mutex_t lock;
#elif defined (USE_MB2MB)
    UINT fromCP;
    UINT toCP;
#endif
} locale_data_t;

static locale_data_t from_locale, to_locale;


void LocaleInit( vlc_object_t *p_this )
{
#if defined USE_ICONV
    char *psz_charset;

    if( vlc_current_charset( &psz_charset ) )
        /* UTF-8 */
        from_locale.hd = to_locale.hd = (vlc_iconv_t)(-1);
    else
    {
        /* not UTF-8 */
        char psz_buf[strlen( psz_charset ) + sizeof( "//translit" )];
        const char *psz_conv;

        /*
         * Still allow non-ASCII characters when the locale is not set.
         * Western Europeans are being favored for historical reasons.
         */
        if( strcmp( psz_charset, "ASCII" ) )
        {
            sprintf( psz_buf, "%s//translit", psz_charset );
            psz_conv = psz_buf;
        }
        else
            psz_conv = "ISO-8859-1//translit";

        vlc_mutex_init( p_this, &from_locale.lock );
        vlc_mutex_init( p_this, &to_locale.lock );
        from_locale.hd = vlc_iconv_open( "UTF-8", psz_conv );
        to_locale.hd = vlc_iconv_open( psz_conv, "UTF-8" );
    }

    free( psz_charset );

    assert( (from_locale.hd == (vlc_iconv_t)(-1))
            == (to_locale.hd == (vlc_iconv_t)(-1)) );

#elif defined (USE_MB2MB)
    to_locale.toCP = from_locale.fromCP = CP_ACP;
    from_locale.toCP = to_locale.fromCP = CP_UTF8;
#else
    (void)p_this;
#endif
}

void LocaleDeinit( void )
{
#ifdef USE_ICONV
    if( to_locale.hd != (vlc_iconv_t)(-1) )
    {
        vlc_iconv_close( to_locale.hd );
        vlc_mutex_destroy( &to_locale.lock );
    }

    if( from_locale.hd != (vlc_iconv_t)(-1) )
    {
        vlc_iconv_close( from_locale.hd );
        vlc_mutex_destroy( &from_locale.lock );
    }
#endif
}

static char *locale_fast (const char *string, locale_data_t *p)
{
#if defined (USE_ICONV)
    vlc_iconv_t hd = p->hd;

    if (hd == (vlc_iconv_t)(-1))
        return (char *)string;

    const char *iptr = string;
    size_t inb = strlen (string);
    size_t outb = inb * 6 + 1;
    char output[outb], *optr = output;

    if (string == NULL)
        return NULL;

    vlc_mutex_lock (&p->lock);
    vlc_iconv (hd, NULL, NULL, NULL, NULL);

    while (vlc_iconv (hd, &iptr, &inb, &optr, &outb) == (size_t)(-1))
    {
        *optr++ = '?';
        outb--;
        iptr++;
        inb--;
        vlc_iconv (hd, NULL, NULL, NULL, NULL);
    }
    vlc_mutex_unlock (&p->lock);
    *optr = '\0';

    assert (inb == 0);
    assert (*iptr == '\0');
    assert (*optr == '\0');
    assert (strlen (output) == (size_t)(optr - output));
    return strdup (output);
#elif defined (USE_MB2MB)
    char *out;
    wchar_t *wide;
    int len;

    if (string == NULL)
        return NULL;

    len = MultiByteToWideChar (p->fromCP, 0, string, -1, NULL, 0);
    if (len == 0)
        return NULL;

    wchar_t wide[len];

    MultiByteToWideChar (p->fromCP, 0, string, -1, wide, len);
    len = WideCharToMultiByte (p->toCP, 0, wide, -1, NULL, 0, NULL, NULL);
    if (len == 0)
        return NULL;
    out = malloc (len);

    WideCharToMultiByte (p->toCP, 0, wide, -1, out, len, NULL, NULL);
    return out;
#else
    return (char *)string;
#endif
}


static inline char *locale_dup (const char *string, locale_data_t *p)
{
#if defined (USE_ICONV)
    return (p->hd == (vlc_iconv_t)(-1))
            ? strdup (string)
            : locale_fast (string, p);
#elif defined (USE_MB2MB)
    return locale_fast (string, p);
#else
    return strdup (string);
#endif
}


void LocaleFree (const char *str)
{
#if defined (USE_ICONV)
    assert ((to_locale.hd == (vlc_iconv_t)(-1))
         == (from_locale.hd == (vlc_iconv_t)(-1)));

    if( to_locale.hd != (vlc_iconv_t)(-1) )
        free ((char *)str);
#elif defined (USE_MB2MB)
    free ((char *)str);
#endif
}


/**
 * FromLocale: converts a locale string to UTF-8
 *
 * @param locale nul-terminated string to be converted
 *
 * @return a nul-terminated UTF-8 string, or NULL in case of error.
 * To avoid memory leak, you have to pass the result to LocaleFree()
 * when it is no longer needed.
 */
char *FromLocale (const char *locale)
{
    return locale_fast (locale, &from_locale);
}

char *FromLocaleDup (const char *locale)
{
    return locale_dup (locale, &from_locale);
}


/**
 * ToLocale: converts a UTF-8 string to local system encoding.
 *
 * @param utf8 nul-terminated string to be converted
 *
 * @return a nul-terminated string, or NULL in case of error.
 * To avoid memory leak, you have to pass the result to LocaleFree()
 * when it is no longer needed.
 */
char *ToLocale (const char *utf8)
{
    return locale_fast (utf8, &to_locale);
}


static char *ToLocaleDup (const char *utf8)
{
    return locale_dup (utf8, &to_locale);
}


/**
 * utf8_open: open() wrapper for UTF-8 filenames
 */
int utf8_open (const char *filename, int flags, mode_t mode)
{
#if defined (WIN32) || defined (UNDER_CE)
    if (GetVersion() < 0x80000000)
    {
        /* for Windows NT and above */
        wchar_t wpath[MAX_PATH + 1];

        if (!MultiByteToWideChar (CP_UTF8, 0, filename, -1, wpath, MAX_PATH))
        {
            errno = ENOENT;
            return -1;
        }
        wpath[MAX_PATH] = L'\0';

        /*
         * open() cannot open files with non-“ANSI” characters on Windows.
         * We use _wopen() instead. Same thing for mkdir() and stat().
         */
        return _wopen (wpath, flags, mode);
    }
#endif
    const char *local_name = ToLocale (filename);

    if (local_name == NULL)
    {
        errno = ENOENT;
        return -1;
    }

    int fd = open (local_name, flags, mode);
    LocaleFree (local_name);
    return fd;
}

/**
 * utf8_fopen: fopen() wrapper for UTF-8 filenames
 */
FILE *utf8_fopen (const char *filename, const char *mode)
{
    int rwflags = 0, oflags = 0;
    vlc_bool_t append = VLC_FALSE;

    for (const char *ptr = mode; *ptr; ptr++)
    {
        switch (*ptr)
        {
            case 'r':
                rwflags = O_RDONLY;
                break;

            case 'a':
                rwflags = O_WRONLY;
                oflags |= O_CREAT;
                append = VLC_TRUE;
                break;

            case 'w':
                rwflags = O_WRONLY;
                oflags |= O_CREAT | O_TRUNC;
                break;

            case '+':
                rwflags = O_RDWR;
                break;

#ifdef O_TEXT
            case 't':
                oflags |= O_TEXT;
                break;
#endif
        }
    }

    int fd = utf8_open (filename, rwflags | oflags, 0666);
    if (fd == -1)
        return NULL;

    if (append && (lseek (fd, 0, SEEK_END) == -1))
    {
        close (fd);
        return NULL;
    }

    FILE *stream = fdopen (fd, mode);
    if (stream == NULL)
        close (fd);

    return stream;
}

/**
 * utf8_mkdir: Calls mkdir() after conversion of file name to OS locale
 *
 * @param dirname a UTF-8 string with the name of the directory that you
 *        want to create.
 * @return A 0 return value indicates success. A -1 return value indicates an
 *        error, and an error code is stored in errno
 */
int utf8_mkdir( const char *dirname )
{
#if defined (UNDER_CE) || defined (WIN32)
    wchar_t wname[MAX_PATH + 1];
    char mod[MAX_PATH + 1];
    int i;

    /* Convert '/' into '\' */
    for( i = 0; *dirname; i++ )
    {
        if( i == MAX_PATH )
            return -1; /* overflow */

        if( *dirname == '/' )
            mod[i] = '\\';
        else
            mod[i] = *dirname;
        dirname++;

    }
    mod[i] = 0;

    if( MultiByteToWideChar( CP_UTF8, 0, mod, -1, wname, MAX_PATH ) == 0 )
    {
        errno = ENOENT;
        return -1;
    }
    wname[MAX_PATH] = L'\0';

    if( CreateDirectoryW( wname, NULL ) == 0 )
    {
        if( GetLastError( ) == ERROR_ALREADY_EXISTS )
            errno = EEXIST;
        else
            errno = ENOENT;
        return -1;
    }
    return 0;
#else
    char *locname = ToLocale( dirname );
    int res;

    if( locname == NULL )
    {
        errno = ENOENT;
        return -1;
    }
    res = mkdir( locname, 0755 );

    LocaleFree( locname );
    return res;
#endif
}

/**
 * utf8_opendir: wrapper that converts dirname to the locale in use by the OS
 *
 * @param dirname UTF-8 representation of the directory name
 *
 * @return a pointer to the DIR struct. Release with closedir().
 */
DIR *utf8_opendir( const char *dirname )
{
#ifdef WIN32
    wchar_t wname[MAX_PATH + 1];

    if (MultiByteToWideChar (CP_UTF8, 0, dirname, -1, wname, MAX_PATH))
    {
        wname[MAX_PATH] = L'\0';
        return (DIR *)vlc_wopendir (wname);
    }
#else
    const char *local_name = ToLocale( dirname );

    if( local_name != NULL )
    {
        DIR *dir = opendir( local_name );
        LocaleFree( local_name );
        return dir;
    }
#endif

    errno = ENOENT;
    return NULL;
}

/**
 * utf8_readdir: a readdir wrapper that returns the name of the next entry
 *     in the directory as a UTF-8 string.
 *
 * @param dir The directory that is being read
 *
 * @return a UTF-8 string of the directory entry. Use LocaleFree() to free this memory
 */
char *utf8_readdir( DIR *dir )
{
#ifdef WIN32
    struct _wdirent *ent = vlc_wreaddir (dir);
    if (ent == NULL)
        return NULL;

    return FromWide (ent->d_name);
#else
    struct dirent *ent;

    ent = readdir( (DIR *)dir );
    if( ent == NULL )
        return NULL;

    return vlc_fix_readdir( ent->d_name );
#endif
}

static int dummy_select( const char *str )
{
    (void)str;
    return 1;
}

int utf8_loaddir( DIR *dir, char ***namelist,
                  int (*select)( const char * ),
                  int (*compar)( const char **, const char ** ) )
{
    if( select == NULL )
        select = dummy_select;

    if( dir == NULL )
        return -1;
    else
    {
        char **tab = NULL;
        char *entry;
        unsigned num = 0;

        rewinddir( dir );

        while( ( entry = utf8_readdir( dir ) ) != NULL )
        {
            char **newtab;

            if( !select( entry ) )
            {
                free( entry );
                continue;
            }

            newtab = realloc( tab, sizeof( char * ) * (num + 1) );
            if( newtab == NULL )
            {
                free( entry );
                goto error;
            }
            tab = newtab;
            tab[num++] = entry;
        }

        if( compar != NULL )
            qsort( tab, num, sizeof( tab[0] ),
                   (int (*)( const void *, const void *))compar );

        *namelist = tab;
        return num;

    error:{
        unsigned i;

        for( i = 0; i < num; i++ )
            free( tab[i] );
        if( tab != NULL )
            free( tab );
        }
    }
    return -1;
}

int utf8_scandir( const char *dirname, char ***namelist,
                  int (*select)( const char * ),
                  int (*compar)( const char **, const char ** ) )
{
    DIR *dir = utf8_opendir (dirname);
    int val = -1;

    if (dir != NULL)
    {
        val = utf8_loaddir (dir, namelist, select, compar);
        closedir (dir);
    }
    return val;
}

static int utf8_statEx( const char *filename, struct stat *buf,
                        vlc_bool_t deref )
{
#if defined (WIN32) || defined (UNDER_CE)
    /* retrieve Windows OS version */
    if( GetVersion() < 0x80000000 )
    {
        /* for Windows NT and above */
        wchar_t wpath[MAX_PATH + 1];

        if( !MultiByteToWideChar( CP_UTF8, 0, filename, -1, wpath, MAX_PATH ) )
        {
            errno = ENOENT;
            return -1;
        }
        wpath[MAX_PATH] = L'\0';

        return _wstati64( wpath, buf );
    }
#endif
#ifdef HAVE_SYS_STAT_H
    const char *local_name = ToLocale( filename );

    if( local_name != NULL )
    {
        int res = deref ? stat( local_name, buf )
                       : lstat( local_name, buf );
        LocaleFree( local_name );
        return res;
    }
    errno = ENOENT;
#endif
    return -1;
}


int utf8_stat( const char *filename, struct stat *buf)
{
    return utf8_statEx( filename, buf, VLC_TRUE );
}

int utf8_lstat( const char *filename, struct stat *buf)
{
    return utf8_statEx( filename, buf, VLC_FALSE );
}

/**
 * utf8_*printf: *printf with conversion from UTF-8 to local encoding
 */
static int utf8_vasprintf( char **str, const char *fmt, va_list ap )
{
    char *utf8;
    int res = vasprintf( &utf8, fmt, ap );
    if( res == -1 )
        return -1;

    *str = ToLocaleDup( utf8 );
    free( utf8 );
    return res;
}

int utf8_vfprintf( FILE *stream, const char *fmt, va_list ap )
{
    char *str;
    int res = utf8_vasprintf( &str, fmt, ap );
    if( res == -1 )
        return -1;

    fputs( str, stream );
    free( str );
    return res;
}

int utf8_fprintf( FILE *stream, const char *fmt, ... )
{
    va_list ap;
    int res;

    va_start( ap, fmt );
    res = utf8_vfprintf( stream, fmt, ap );
    va_end( ap );
    return res;
}


static char *CheckUTF8( char *str, char rep )
{
    uint8_t *ptr = (uint8_t *)str;
    assert (str != NULL);

    for (;;)
    {
        uint8_t c = ptr[0];
        int charlen = -1;

        if (c == '\0')
            break;

        for (int i = 0; i < 7; i++)
            if ((c >> (7 - i)) == ((0xff >> (7 - i)) ^ 1))
            {
                charlen = i;
                break;
            }

        switch (charlen)
        {
            case 0: // 7-bit ASCII character -> OK
                ptr++;
                continue;

            case -1: // 1111111x -> error
            case 1: // continuation byte -> error
                goto error;
        }

        assert (charlen >= 2);

        uint32_t cp = c & ~((0xff >> (7 - charlen)) << (7 - charlen));
        for (int i = 1; i < charlen; i++)
        {
            assert (cp < (1 << 26));
            c = ptr[i];

            if ((c == '\0') // unexpected end of string
             || ((c >> 6) != 2)) // not a continuation byte
                goto error;

            cp = (cp << 6) | (ptr[i] & 0x3f);
        }

        if (cp < 128) // overlong (special case for ASCII)
            goto error;
        if (cp < (1u << (5 * charlen - 3))) // overlong
            goto error;

        ptr += charlen;
        continue;

    error:
        if (rep == 0)
            return NULL;
        *ptr++ = rep;
        str = NULL;
    }

    return str;
}

/**
 * EnsureUTF8: replaces invalid/overlong UTF-8 sequences with question marks
 * Note that it is not possible to convert from Latin-1 to UTF-8 on the fly,
 * so we don't try that, even though it would be less disruptive.
 *
 * @return str if it was valid UTF-8, NULL if not.
 */
char *EnsureUTF8( char *str )
{
    return CheckUTF8( str, '?' );
}


/**
 * IsUTF8: checks whether a string is a valid UTF-8 byte sequence.
 *
 * @param str nul-terminated string to be checked
 *
 * @return str if it was valid UTF-8, NULL if not.
 */
const char *IsUTF8( const char *str )
{
    return CheckUTF8( (char *)str, 0 );
}
