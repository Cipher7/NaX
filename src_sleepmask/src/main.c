/* sleepmask BOF — WFSO PoC.
 * Intercepts Sleep/WFSO/WFMO via BeaconGate and converts them
 * to NtWaitForSingleObject on a dummy event.
 * This is a PoC demonstrating the gate architecture — extend it
 * with your own sleep obfuscation technique. */

#include "Imports.h"
#include "Gate.h"

/* ========= [ per-API handlers ] ========= */

static void HandleSleep( PFUNCTION_CALL FnCall ) {
    DWORD ms = (DWORD)FnCall->Args[0];

#ifdef DEBUG
    printf( "[sleepmask] Sleep(%lu ms)\n", (unsigned long)ms );
#endif

    HANDLE hEvent = NULL;
    NtCreateEvent( &hEvent, EVENT_ALL_ACCESS, NULL, 1, FALSE );
    if ( hEvent ) {
        LARGE_INTEGER timeout;
        timeout.QuadPart = -(LONGLONG)ms * 10000;
        NtWaitForSingleObject( hEvent, FALSE, &timeout );
        NtClose( hEvent );
    }
}

static void HandleWaitForSingleObject( PFUNCTION_CALL FnCall ) {
    HANDLE hHandle = (HANDLE)FnCall->Args[0];
    DWORD  ms      = (DWORD)FnCall->Args[1];

#ifdef DEBUG
    printf( "[sleepmask] WFSO(handle=%p ms=%lu)\n", hHandle, (unsigned long)ms );
#endif

    LARGE_INTEGER timeout;
    timeout.QuadPart = -(LONGLONG)ms * 10000;
    NTSTATUS ret = NtWaitForSingleObject( hHandle, FALSE, &timeout );
    FnCall->RetValue = (ULONG_PTR)ret;
}

static void HandleWaitForMultipleObjects( PFUNCTION_CALL FnCall ) {
    DWORD ms = (DWORD)FnCall->Args[3];

#ifdef DEBUG
    printf( "[sleepmask] WFMO(n=%lu waitAll=%d ms=%lu)\n",
            (unsigned long)(DWORD)FnCall->Args[0],
            (int)(BOOL)FnCall->Args[2],
            (unsigned long)ms );
#endif

    DWORD ret = WaitForMultipleObjects( (DWORD)FnCall->Args[0], (HANDLE*)FnCall->Args[1], (BOOL)FnCall->Args[2], ms );
    FnCall->RetValue = (ULONG_PTR)ret;
}

static void HandleVirtualProtect( PFUNCTION_CALL FnCall ) {
    LPVOID lpAddress      = (LPVOID)FnCall->Args[0];
    SIZE_T dwSize         = (SIZE_T)FnCall->Args[1];
    DWORD  flNewProtect   = (DWORD)FnCall->Args[2];
    PDWORD lpflOldProtect = (PDWORD)FnCall->Args[3];

    BOOL ret = VirtualProtect( lpAddress, dwSize, flNewProtect, lpflOldProtect );
    FnCall->RetValue = (ULONG_PTR)ret;
}

static void HandleGeneric( PFUNCTION_CALL FnCall ) {
    switch ( FnCall->NumArgs ) {
    case 0:  FnCall->RetValue = ((FN0) FnCall->FunctionPtr)(); break;
    case 1:  FnCall->RetValue = ((FN1) FnCall->FunctionPtr)( FnCall->Args[0] ); break;
    case 2:  FnCall->RetValue = ((FN2) FnCall->FunctionPtr)( FnCall->Args[0], FnCall->Args[1] ); break;
    case 3:  FnCall->RetValue = ((FN3) FnCall->FunctionPtr)( FnCall->Args[0], FnCall->Args[1], FnCall->Args[2] ); break;
    case 4:  FnCall->RetValue = ((FN4) FnCall->FunctionPtr)( FnCall->Args[0], FnCall->Args[1], FnCall->Args[2], FnCall->Args[3] ); break;
    case 5:  FnCall->RetValue = ((FN5) FnCall->FunctionPtr)( FnCall->Args[0], FnCall->Args[1], FnCall->Args[2], FnCall->Args[3], FnCall->Args[4] ); break;
    case 6:  FnCall->RetValue = ((FN6) FnCall->FunctionPtr)( FnCall->Args[0], FnCall->Args[1], FnCall->Args[2], FnCall->Args[3], FnCall->Args[4], FnCall->Args[5] ); break;
    case 7:  FnCall->RetValue = ((FN7) FnCall->FunctionPtr)( FnCall->Args[0], FnCall->Args[1], FnCall->Args[2], FnCall->Args[3], FnCall->Args[4], FnCall->Args[5], FnCall->Args[6] ); break;
    case 8:  FnCall->RetValue = ((FN8) FnCall->FunctionPtr)( FnCall->Args[0], FnCall->Args[1], FnCall->Args[2], FnCall->Args[3], FnCall->Args[4], FnCall->Args[5], FnCall->Args[6], FnCall->Args[7] ); break;
    case 9:  FnCall->RetValue = ((FN9) FnCall->FunctionPtr)( FnCall->Args[0], FnCall->Args[1], FnCall->Args[2], FnCall->Args[3], FnCall->Args[4], FnCall->Args[5], FnCall->Args[6], FnCall->Args[7], FnCall->Args[8] ); break;
    case 10: FnCall->RetValue = ((FN10)FnCall->FunctionPtr)( FnCall->Args[0], FnCall->Args[1], FnCall->Args[2], FnCall->Args[3], FnCall->Args[4], FnCall->Args[5], FnCall->Args[6], FnCall->Args[7], FnCall->Args[8], FnCall->Args[9] ); break;
    }
}

/* ========= [ entry point ] ========= */

__attribute__((aligned(16)))
void sleep_mask( void* NaxPtr, PFUNCTION_CALL FnCall ) {
#ifdef DEBUG
    printf( "[sleepmask] GateApi=0x%02x NumArgs=%lu FunctionPtr=%p\n",
            FnCall->GateApi, (unsigned long)FnCall->NumArgs, FnCall->FunctionPtr );
#endif

    switch ( FnCall->GateApi ) {
    case GATE_API_SLEEP:                     HandleSleep( FnCall );                   return;
    case GATE_API_WAIT_FOR_SINGLE_OBJECT:    HandleWaitForSingleObject( FnCall );     return;
    case GATE_API_WAIT_FOR_MULTIPLE_OBJECTS: HandleWaitForMultipleObjects( FnCall );   return;
    case GATE_API_VIRTUAL_PROTECT:           HandleVirtualProtect( FnCall );           return;
    default:                                 HandleGeneric( FnCall );                  return;
    }
}
