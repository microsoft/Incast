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

#ifndef _INCAST_TCPSTATS_H
#define _INCAST_TCPSTATS_H
void reportTcpStats()
{
    using namespace std;

    // ISSUE-REVIEW
    // This is a system-wide statistic for all TCP connections.  Can I get a
    // per-connection equivalent with GetPerTcpConnectionEStats or another API?
    //
    // ISSUE-REVIEW
    // I aggregate this system-wide value per-ip.  I am conflating ip with system.
    // This is broken on multi-homed machines.

    int serverRetransmits =
        tcpStatsAfter.dwRetransSegs - tcpStatsBefore.dwRetransSegs;
    
    printf( "\n" );
    printf( "Retransmits (system-wide):\n" );
    printf( "\tserver:                      %3d\n", serverRetransmits );
  
    int clientRetransmits = 0;

    ClientAddressMap::const_iterator i;
    for( i = clientAddressMap.begin(); i != clientAddressMap.end(); ++i )
    {
        int maxRetransmits = numeric_limits<int>::min();
        int num_clients = i->second.size();

        for( int j = 0; j < num_clients; ++j )
        {
            int client_num = i->second[j];
            maxRetransmits = 
                max( maxRetransmits, clientResults[client_num].crd.retransmits );
        }
       
        clientRetransmits += maxRetransmits;

        //printf( "\tall %3d clients from %15.15s: %d\n", 
        //        num_clients,
        //        i->first.c_str(),
        //        maxRetransmits );
    }
    
    printf( "\tclients:                     %3d\n", clientRetransmits );
    printf( "\ttotal:                       %3d\n", clientRetransmits + serverRetransmits );
}

bool enableTcpEStats()
{
    using namespace std;

    DWORD tcpTableSize = 0;

    DWORD r = GetTcpTable( NULL, &tcpTableSize, 0 );
    
    HARD_ASSERT( r == ERROR_INSUFFICIENT_BUFFER );
    HARD_ASSERT( tcpTableSize > 0 );

    unique_ptr<MIB_TCPTABLE, void(__cdecl *)(void*)> tcpTable( (PMIB_TCPTABLE) malloc(tcpTableSize), free );

    r = GetTcpTable( tcpTable.get(), &tcpTableSize, TRUE );
 
    // ISSUE-REVIEW
    // Could get a new TCP connection between GetTcpTable calls.
    HARD_ASSERT( r != ERROR_INSUFFICIENT_BUFFER );

    for( unsigned i = 0; i < tcpTable->dwNumEntries; ++i )
    {
        PMIB_TCPROW tr = &tcpTable->table[i];
        if( ntohs((u_short) tr->dwLocalPort) == PORT )
        {
            TCP_ESTATS_SND_CONG_RW_v0 snd_rw;
            snd_rw.EnableCollection = 1;

            r = SetPerTcpConnectionEStats( 
                tr,
                TcpConnectionEstatsSndCong,
                (PUCHAR) &snd_rw,
                0,
                sizeof(snd_rw),
                0 );

            if( r != NO_ERROR )
                return false;
        }
    }

    return true;
}

void reportTcpEStats()
{
    using namespace std;

    DWORD tcpTableSize = 0;

    DWORD r = GetTcpTable( NULL, &tcpTableSize, 0 );
    
    HARD_ASSERT( r == ERROR_INSUFFICIENT_BUFFER );
    HARD_ASSERT( tcpTableSize > 0 );

    unique_ptr<MIB_TCPTABLE, void(__cdecl *)(void*)> tcpTable( (PMIB_TCPTABLE) malloc(tcpTableSize), free );
    
    r = GetTcpTable( tcpTable.get(), &tcpTableSize, TRUE );
 
    // ISSUE-REVIEW
    // Could get a new TCP connection between GetTcpTable calls.
    HARD_ASSERT( r != ERROR_INSUFFICIENT_BUFFER );

#ifndef PRINT_PER_CLIENT_ESTATS
    printf( "\nCongestion %%age:\n" );
    
    ULONG totalRecvTime = 0;
    ULONG totalNetTime = 0;
    ULONG totalSendTime = 0;

    for( unsigned i = 0; i < tcpTable->dwNumEntries; ++i )
    {
        TCP_ESTATS_SND_CONG_ROD_v0 snd_rod;

        PMIB_TCPROW tr = &tcpTable->table[i];
        if( ntohs((u_short) tr->dwLocalPort) == PORT )
        {
            r = GetPerTcpConnectionEStats(
                tr,
                TcpConnectionEstatsSndCong,
                NULL, 0, 0,
                NULL, 0, 0,
                (PUCHAR) &snd_rod,
                0, 
                sizeof(snd_rod) );
            
            HARD_ASSERT( r == NO_ERROR );
           
            totalRecvTime += snd_rod.SndLimTimeRwin;
            totalNetTime += snd_rod.SndLimTimeCwnd;
            totalSendTime += snd_rod.SndLimTimeSnd;
        }
    }
    
    ULONG totalTime = totalRecvTime + totalNetTime + totalSendTime;

    printf( "\treceive:                  %5.4f\n", totalRecvTime*100.0/totalTime );
    printf( "\tnetwork:                  %5.4f\n", totalNetTime*100.0/totalTime );
    printf( "\tsend:                     %5.4f\n", totalSendTime*100.0/totalTime );
    
#else
    printf( "\nCongestion msec (recv/net/send):\n" );
    //printf( "\nCongestion %%age (recv/net/send):\n" );

    for( unsigned i = 0; i < tcpTable->dwNumEntries; ++i )
    {
        TCP_ESTATS_SND_CONG_ROD_v0 snd_rod;

        PMIB_TCPROW tr = &tcpTable->table[i];
        if( ntohs((u_short) tr->dwLocalPort) == PORT )
        {
            r = GetPerTcpConnectionEStats(
                tr,
                TcpConnectionEstatsSndCong,
                NULL, 0, 0,
                NULL, 0, 0,
                (PUCHAR) &snd_rod,
                0, 
                sizeof(snd_rod) );
            
            HARD_ASSERT( r == NO_ERROR );
            
            printf( "\tclient from %15.15s: ", inet_ntoa( *(IN_ADDR*) &(tr->dwRemoteAddr) ) );
           
            printf( "\t%u / %u / %u\n", 
                snd_rod.SndLimTimeRwin,
                snd_rod.SndLimTimeCwnd,
                snd_rod.SndLimTimeSnd );

//            ULONG totalTime = 
//                snd_rod.SndLimTimeRwin +
//                snd_rod.SndLimTimeCwnd +
//                snd_rod.SndLimTimeSnd;
//           
//            printf( "\t%5.2f / %5.2f / %5.2f\n", 
//                snd_rod.SndLimTimeRwin*100.0/totalTime,
//                snd_rod.SndLimTimeCwnd*100.0/totalTime,
//                snd_rod.SndLimTimeSnd*100.0/totalTime );
        }
    }
#endif
}
#endif //_INCAST_TCPSTATS_H
