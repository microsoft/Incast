// Incast
//
// Copyright (c) Microsoft Corporation
//
// All rights reserved. 
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#ifndef __INCAST_UTILS_H
#define __INCAST_UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>

// an assertion that persists even in non-debug builds
__declspec(noinline)
void HARD_ASSERT( bool cond, char *msg )
{
    if( !cond )
    {
        fprintf( stderr, "Assertion failed: %s\n", msg );
        abort();
    }
}
#define HARD_ASSERT(x) (HARD_ASSERT(x,#x));
#define UNREACHED false

__int64 qpf() 
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f.QuadPart;
}
    
static __int64 freq = qpf();

__int64 qpc()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

double qpc_to_msec( __int64 x )
{
    return x * 1000.0 / freq;
}

__int64 msec_to_qpc( double x )
{
    return (__int64) (x * freq / 1000);
}

// takes desired sleep in msec
// returns actual sleep in qpc ticks
__int64 mySleep( const double target_msec )
{
    __int64 start = qpc();
    __int64 actual;
   
    double remaining_msec = target_msec;

    while(1)
    {
        Sleep( (int) (remaining_msec + 0.5) );
        
        actual = qpc() - start;

        double actual_msec = qpc_to_msec( actual );

        if( actual_msec >= target_msec )
            break;
        
        remaining_msec = target_msec - actual_msec;
    }

    return actual;
}

void setHighPriority()
{
    SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
}

void gracefulShutdown( SOCKET s )
{
    char buf[256];
    int rv;

    if( shutdown(s, SD_SEND) == SOCKET_ERROR )
    {
        fprintf(stderr, "in gracefulShutdown, shutdown() failed: %d\n", WSAGetLastError());
        exit(-1);
    }
    
    do
    {
        if( (rv = recv(s, buf, 256, 0 ) ) == SOCKET_ERROR )
        {
            fprintf(stderr, "in gracefulShutdown, recv() failed: %d\n", WSAGetLastError());
            exit(-1);
        }
    } while( rv != 0 );

    closesocket(s);
}

bool isConnectionPending( SOCKET ls ) 
{
    fd_set fds;
    TIMEVAL tv = {0, 0};
    
    FD_ZERO(&fds);
    FD_SET(ls, &fds);

    int rv = select( 0, &fds, NULL, NULL, &tv );

    if (rv == SOCKET_ERROR) 
    {
        fprintf(stderr, "select() failed: %d\n", WSAGetLastError());
        exit(-1);
    }

    return rv ? true : false;
}

void setSocketBufferSize( SOCKET s, int optname, int size )
{
    if (setsockopt(s, SOL_SOCKET, optname, (char*) &size, sizeof(size)) != 0)
    {
        fprintf(stderr, "setsockopt() failed: %d\n", WSAGetLastError());
    }

    int rb, rbs = sizeof(rb);
    getsockopt(s, SOL_SOCKET, optname, (char*) &rb, &rbs);

    if (rb != size)
    {
        fprintf(stderr, "setsockopt() didn't take effect\n");
    }
}

void disableNagle( SOCKET s )
{
    int flag = 1;
    if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(flag)) != 0)
    {
        fprintf(stderr, "setsockopt() failed: %d\n", WSAGetLastError());
    }
}
#endif // __INCAST_UTILS_H
