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

#include "incast.h"
#include "utils.h"
#include "tcpstats.h"
#include "histogram.h"

using namespace std;

unsigned int __stdcall serverThread( void *p )
{
    int client_num = (int) p;
    SOCKET s = clientSockets[client_num];
    int bytes;

    if( (gtp.delay > 0) && (gtp.delay_method == RANDOM_JITTER) )
    {
        // each thread needs a unique seed
        unsigned junk = 0xBFBFBFB;
        srand( (client_num+1) * junk );
    }
    
    // send global test parameters to client
    if ((bytes = send(s, (char*) &gtp, sizeof(GlobalTestParameters), 0)) == SOCKET_ERROR)
    {
        fprintf(stderr, "send() global test parameters failed: %d\n", WSAGetLastError());
        exit(-1);
    }
    HARD_ASSERT(bytes == sizeof(GlobalTestParameters));
    
    // send client-specific test parameters to client
    ClientSpecificTestParameters cstp;
    cstp.client_num = client_num;

    if ((bytes = send(s, (char*) &cstp, sizeof(ClientSpecificTestParameters), 0)) == SOCKET_ERROR)
    {
        fprintf(stderr, "send() client-specific test parameters failed: %d\n", WSAGetLastError());
        exit(-1);
    }
    HARD_ASSERT(bytes == sizeof(ClientSpecificTestParameters));
    
    unique_ptr<char[]> fobuf( new char[gtp.fo_msg_size] );
    unique_ptr<char[]> fibuf( new char[gtp.fi_msg_size] );

    if( client_num == 0 )
    {
        printf( "\nWarming up..." );
    }
    
    // warm-up
    for( int i = 0; i < WARMUP_ITERS; ++i )
    {
        // synchronize with the other serverThreads
        pb->wait();
        
        // send the fan-out
        if ((bytes = send(s, fobuf.get(), gtp.fo_msg_size, 0)) == SOCKET_ERROR)
        {
            fprintf(stderr, "send() fan-out failed: %d\n", WSAGetLastError());
            exit(-1);
        }
        HARD_ASSERT(bytes == gtp.fo_msg_size);

        // expect the fan-in
        if ((bytes = recv(s, fibuf.get(), gtp.fi_msg_size, MSG_WAITALL)) == SOCKET_ERROR)
        {
            fprintf(stderr, "recv() fan-in failed: %d\n", WSAGetLastError());
            exit(-1);
        }
        HARD_ASSERT(bytes == gtp.fi_msg_size);
    }

    if( client_num == 0 )
    {
        printf( "done!\nTesting..." );
        GetTcpStatistics(&tcpStatsBefore);
    }

    Measurement m;
    
    __int64 qpcStartTime = qpc();

    for( int i = 0; i < gtp.iters; ++i )
    {
        // synchronize with the other serverThreads
        pb->wait();
        
        m.start = qpc();

        if( gtp.delay > 0 )
        {
            double target_delay = 0;

            if( gtp.delay_method == RANDOM_JITTER )
            {
                target_delay = gtp.delay * ((double) rand()) / RAND_MAX;
            }
            else if( gtp.delay_method == UNIFORM_SCHED )
            {
                target_delay = gtp.delay * ((double) client_num / gtp.clients);
            }
            else
            {
                HARD_ASSERT( UNREACHED );
            }

            m.actual_delay = mySleep( target_delay );
        }

        // send the fan-out
        if ((bytes = send(s, fobuf.get(), gtp.fo_msg_size, 0)) == SOCKET_ERROR)
        {
            fprintf(stderr, "send() fan-out failed: %d\n", WSAGetLastError());
            exit(-1);
        }
        HARD_ASSERT(bytes == gtp.fo_msg_size);

        // expect the fan-in
        if ((bytes = recv(s, fibuf.get(), gtp.fi_msg_size, MSG_WAITALL)) == SOCKET_ERROR)
        {
            fprintf(stderr, "recv() fan-in failed: %d\n", WSAGetLastError());
            exit(-1);
        }
        HARD_ASSERT(bytes == gtp.fi_msg_size);
    
        m.stop = qpc();

        if( gtp.rate_limited )
        {
            double expectedElapsedSeconds = ((double) i) / gtp.target_rate;
            double expectedElapsedQpcTicks = expectedElapsedSeconds * freq;

            double expectedQpc = qpcStartTime + expectedElapsedQpcTicks;

            while( qpc() < expectedQpc )
            {
                // slow down
                
                // ISSUE-REVIEW
                // Sleep is 1 ms min.  Is a spin wait warranted?
                Sleep(0);
            }
        }

        clientResults[client_num].measurements.push_back(m);
    }
    
    // expect client results
    ClientResultData * crd = &clientResults[client_num].crd;
    if ((bytes = recv(s, (char*) crd, sizeof(ClientResultData), MSG_WAITALL)) == SOCKET_ERROR)
    {
        fprintf(stderr, "recv() client results failed: %d\n", WSAGetLastError());
        exit(-1);
    }
    HARD_ASSERT(bytes == sizeof(ClientResultData));

    return 0;
}
    
void reportGlobalTestParameters()
{
    printf( "\n" );
    printf( "Test parameters:\n" );
    printf( "\tclients:              %d\n", gtp.clients );
    printf( "\titerations:           %d\n", gtp.iters );
    if( gtp.rate_limited )
    {
        printf( "\trate limit:           %d\n", gtp.target_rate );
    }
    else
    {
        printf( "\trate limit:           none\n" );
    }
    printf( "\tfan-out msg bytes:    %d\n", gtp.fo_msg_size );
    printf( "\tfan-in msg bytes:     %d\n", gtp.fi_msg_size );
   
    printf( "\tNagle's algorithm:    %s\n", gtp.nagle ? "enabled" : "disabled" );

    if( gtp.send_buffer >= 0 )
    {
        printf( "\tsend buffer size:     %d\n", gtp.send_buffer );
    }
    else
    {
        printf( "\tsend buffer size:     OS default\n" );
    }

    if( gtp.recv_buffer >= 0 )
    {
        printf( "\treceive buffer size:  %d\n", gtp.recv_buffer );
    }
    else
    {
        printf( "\treceive buffer size:  OS default\n" );
    }
    
    if( gtp.delay > 0 )
    {
        printf( "\tdelay:                %d\n", gtp.delay );
        printf( "\tdelay method:         %s\n", 
            gtp.delay_method == RANDOM_JITTER ? "random jitter" : "uniform sched" );
    }
    else
    {
        printf( "\tdelay:               none\n" );
    }
} 
   
void reportLatencyThroughput()
{
    try {

    Histogram<__int64> hist;
    __int64 globalFirstStart, globalLastStop;

    int clients = clientResults.size();

    for( int i = 0; i < gtp.iters; ++i )
    {
        __int64 firstStart = numeric_limits<__int64>::max();
        __int64 lastStop = numeric_limits<__int64>::min();

        for( int c = 0; c < clients; ++c )
        {
            Measurements &m = clientResults[c].measurements;
            firstStart = min( firstStart, m[i].start );
            lastStop = max( lastStop, m[i].stop );
        }
    
        HARD_ASSERT(lastStop>firstStart);
        
        hist.add(lastStop-firstStart);

        if( i == 0 ) globalFirstStart = firstStart;
        if( i == gtp.iters-1 ) globalLastStop = lastStop;
    }
   
    // for delay, we also calculate the exclusive latency
    // by subtracting the actual per-client, per-iteration
    // delay, thus effectively pretending that all the
    // tests began at exactly the same time
    
    Histogram<__int64> exclusive_hist;

#ifdef REPORT_DELAY
    Histogram<__int64> delay_hist;
#endif

    if( gtp.delay )
    {
        for( int i = 0; i < gtp.iters; ++i )
        {
            for( int c = 0; c < clients; ++c )
            {
                Measurements &m = clientResults[c].measurements;
#ifdef REPORT_DELAY
                delay_hist.add(m[i].actual_delay);
#endif
                m[i].stop -= m[i].actual_delay;

            }
            
            __int64 firstStart = numeric_limits<__int64>::max();
            __int64 lastStop = numeric_limits<__int64>::min();

            for( int c = 0; c < clients; ++c )
            {
                Measurements &m = clientResults[c].measurements;
                
                firstStart = min( firstStart, m[i].start );
                lastStop = max( lastStop, m[i].stop );
            }

            HARD_ASSERT(lastStop>firstStart);

            exclusive_hist.add(lastStop-firstStart);
        }
    }

    if( gtp.histogram )
    {
        histfile << freq << endl << endl;
        if( gtp.delay )
        {
#ifdef REPORT_DELAY
            histfile << "Delay" << endl;
            histfile << delay_hist.get_histogram_csv( 10000 ) << endl;
#endif
            histfile << "Exclusive" << endl;
            histfile << exclusive_hist.get_histogram_csv( 10000 );
            histfile << endl << "Inclusive" << endl;
        }
        histfile << hist.get_histogram_csv( 10000 );
        histfile.close();
    }
   
    if( gtp.delay )
        printf( "\nLatency (inclusive):\n" );
    else
        printf( "\nLatency:\n" );

    double lmin = hist.get_min() * 1.0e6 / freq;
    printf( "\tminimum usec/iter:    %10.3f\n", lmin );
    
    double lmax = hist.get_max() * 1.0e6 / freq;
    printf( "\tmaximum usec/iter:    %10.3f\n", lmax );
    
    double avg = hist.get_avg() * 1.0e6 / freq;
    printf( "\taverage usec/iter:    %10.3f\n", avg );
    
    double median = hist.get_median() * 1.0e6 / freq;
    printf( "\tmedian usec/iter:     %10.3f\n", median );
    
    double p95 = hist.get_percentile(0.95) * 1.0e6 / freq;
    printf( "\t95th %%ile usec/iter:  %10.3f\n", p95 );
    
    double p99 = hist.get_percentile(0.99) * 1.0e6 / freq;
    printf( "\t99th %%ile usec/iter:  %10.3f\n", p99 );
    
    if( gtp.delay )
    {
        printf( "\nLatency (exclusive):\n" );

        double lmin = exclusive_hist.get_min() * 1.0e6 / freq;
        printf( "\tminimum usec/iter:    %10.3f\n", lmin );

        double lmax = exclusive_hist.get_max() * 1.0e6 / freq;
        printf( "\tmaximum usec/iter:    %10.3f\n", lmax );

        double avg = exclusive_hist.get_avg() * 1.0e6 / freq;
        printf( "\taverage usec/iter:    %10.3f\n", avg );

        double median = exclusive_hist.get_median() * 1.0e6 / freq;
        printf( "\tmedian usec/iter:     %10.3f\n", median );

        double p95 = exclusive_hist.get_percentile(0.95) * 1.0e6 / freq;
        printf( "\t95th %%ile usec/iter:  %10.3f\n", p95 );

        double p99 = exclusive_hist.get_percentile(0.99) * 1.0e6 / freq;
        printf( "\t99th %%ile usec/iter:  %10.3f\n", p99 );
       
    }

#ifdef REPORT_DELAY
    if( gtp.delay )
    {
        printf( "\nJitter:\n" );

        double lmin = delay_hist.get_min() * 1.0e6 / freq;
        printf( "\tminimum usec/iter:    %10.3f\n", lmin );

        double lmax = delay_hist.get_max() * 1.0e6 / freq;
        printf( "\tmaximum usec/iter:    %10.3f\n", lmax );

        double avg = delay_hist.get_avg() * 1.0e6 / freq;
        printf( "\taverage usec/iter:    %10.3f\n", avg );

        double median = delay_hist.get_median() * 1.0e6 / freq;
        printf( "\tmedian usec/iter:     %10.3f\n", median );

        double p95 = delay_hist.get_percentile(0.95) * 1.0e6 / freq;
        printf( "\t95th %%ile usec/iter:  %10.3f\n", p95 );

        double p99 = delay_hist.get_percentile(0.99) * 1.0e6 / freq;
        printf( "\t99th %%ile usec/iter:  %10.3f\n", p99 );
    }
#endif

    printf( "\nThroughput:\n" );
    
    double totalSeconds = ((double)(globalLastStop-globalFirstStart)) / freq;
    double sendMBytes = gtp.fo_msg_size / 1.0e6 * clients * gtp.iters;
    double recvMBytes = gtp.fi_msg_size / 1.0e6 * clients * gtp.iters;
    double totalMBytes = sendMBytes + recvMBytes;

    double sendMbps = sendMBytes * 8 / totalSeconds;
    printf( "\tmbit/sec send:        %10.3f\n", sendMbps );
    
    double recvMbps = recvMBytes * 8 / totalSeconds;
    printf( "\tmbit/sec recv:        %10.3f\n", recvMbps );
    
    double totalMbps = totalMBytes * 8 / totalSeconds;
    printf( "\tmbit/sec tot:         %10.3f\n", totalMbps );
  
    double ips = gtp.iters / totalSeconds;
    printf( "\titer/sec:             %10.3f\n", ips );
    
    const double FUDGE_FACTOR = 0.95;
    if( gtp.rate_limited && (ips < gtp.target_rate * FUDGE_FACTOR ) )
    {
        printf( "\nWarning: missed target of %d iterations/sec\n", gtp.target_rate );
    }
    
    }
    catch( exception& e )
    {
        fprintf(stderr, "\nException caught: %s\n", e.what());
    }
}

void serverMain()
{
    printf( "Server mode\n\n" );

    SOCKET ls;

    if ((ls = socket(PF_INET,SOCK_STREAM,0)) == INVALID_SOCKET)
    {
        fprintf(stderr, "socket() failed: %d\n", WSAGetLastError());
        exit(-1);
    }

    SOCKADDR_IN sin = {0};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = INADDR_ANY;
       
    if (bind(ls, (SOCKADDR*) &sin, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        fprintf(stderr, "bind() failed: %d\n", WSAGetLastError());
        exit(-1);
    }

    if (listen(ls, SOMAXCONN) == SOCKET_ERROR)
    {
        fprintf(stderr, "listen() failed: %d\n", WSAGetLastError());
        exit(-1);
    }

    printf( "Start clients and then press any key to begin test.\n" );

    while (!_kbhit())
    {
        if (isConnectionPending(ls))
        {
            SOCKET cs;

            int nlen = sizeof(SOCKADDR);
            if ((cs = accept(ls, (SOCKADDR*) &sin, &nlen)) == INVALID_SOCKET)
            {
                fprintf(stderr, "accept() failed: %d\n", WSAGetLastError());
                exit(-1);
            }

            const int client_num = gtp.clients++;

            nlen = sizeof(sockaddr);
            getpeername( cs, (struct sockaddr *)&sin, &nlen );

            string ip( inet_ntoa(sin.sin_addr) );

            clientAddressMap[ip].push_back(client_num);

            printf("\tClient %3d connected from %15.15s\n", client_num, ip.c_str());	

            clientSockets.push_back(cs);
    
            if( gtp.nagle == false )
                disableNagle(cs);

            if( gtp.send_buffer >= 0 )
                setSocketBufferSize(cs, SO_SNDBUF, gtp.send_buffer );
            
            if( gtp.recv_buffer >= 0 )
                setSocketBufferSize(cs, SO_RCVBUF, gtp.recv_buffer );

            if( gtp.clients_limited && (gtp.clients == gtp.client_limit) )
            {
                printf( "Reached limit of %d clients.\n", gtp.client_limit );
                break;
            }
        }
    }

    // stop listening for clients
    closesocket(ls);
    
    if (gtp.clients <= 0)
    {
        printf("No clients, exiting...\n");
        exit(0);
    }
    
    if( !gtp.clients_limited || (gtp.clients < gtp.client_limit) )
    {
        printf( "%d clients connected.\n", gtp.clients );
    }

#ifdef REPORT_ESTATS
    bool estats = enableTcpEStats();
    if( !estats )
    {
        printf( "\nCould not enable TCP EStats.  Run server as admin?\n" );
    }
#endif

    barrier b(gtp.clients);
    pb = &b;
  
    clientResults.resize(gtp.clients);

    for( int c = 0; c < gtp.clients; ++c )
    {
        clientThreads.push_back( 
            (HANDLE) _beginthreadex( NULL, 0, serverThread, (void*) c, 0, NULL ) );
    }

    HARD_ASSERT( clientSockets.size() == gtp.clients );
    HARD_ASSERT( clientResults.size() == gtp.clients );
    HARD_ASSERT( clientThreads.size() == gtp.clients );
    
    // wait for all serverThreads to complete the test and exit
    WaitForMultipleObjectsEx( gtp.clients, &clientThreads[0], true, INFINITE, FALSE );
    
    printf( "done!\n" );
    
    GetTcpStatistics(&tcpStatsAfter);
    
    reportGlobalTestParameters();

    reportLatencyThroughput();

    reportTcpStats();

#ifdef REPORT_ESTATS
    if( estats )
    {
        reportTcpEStats();
    }
#endif

    for( int c = 0; c < gtp.clients; ++c )
    {
        gracefulShutdown( clientSockets[c] );
    }
}

void clientMain( char* server )
{
    printf("Client mode\n");
    
    ULONG addr = inet_addr( server );
    if (addr == INADDR_NONE)
    {
        PADDRINFOA pai;
        if( getaddrinfo( server, NULL, NULL, &pai ) != 0 )
        {
            fprintf(stderr, "getaddrinfo() failed: %d\n", WSAGetLastError());
            exit(-1);
        }

        for( PADDRINFOA p = pai; p != NULL; p=p->ai_next )
        {
            if( p->ai_family == AF_INET )
            {
                PSOCKADDR_IN sai = (PSOCKADDR_IN) p->ai_addr;
                addr = *(ULONG*) &(sai->sin_addr); 
                break;
            }
        }

        freeaddrinfo( pai );
    }
    
    SOCKET s;

beginTest:
    printf("\nCTRL-C to quit.\n");

    if ((s = socket(PF_INET,SOCK_STREAM,0)) == INVALID_SOCKET)
    {
        fprintf(stderr, "socket() failed: %d\n", WSAGetLastError());
        exit(-1);
    }
    
    SOCKADDR_IN sin = {0};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = addr;

    char *ip = inet_ntoa(sin.sin_addr);
   
    if (strcmp(server,ip) == 0)
        printf("Connecting to %s port %d...", server, PORT);	
    else
        printf("Connecting to %s (%s) port %d...", server, ip, PORT);	

    while (true)
    {
        if (connect(s, (SOCKADDR*) &sin, sizeof(SOCKADDR)) != SOCKET_ERROR)
        {
            break;
        }

        int err = WSAGetLastError();

        if( (err == WSAETIMEDOUT) || (err == WSAECONNREFUSED) )
        {
            //printf(".");
            Sleep(100);
            continue;
        }

        fprintf(stderr, "Error: connect() failed: %d\n", WSAGetLastError());
        exit(-1);
    }

    printf("connected!\n");
            
    if( gtp.nagle == false )
        disableNagle(s);
            
    if( gtp.send_buffer >= 0 )
        setSocketBufferSize(s, SO_SNDBUF, gtp.send_buffer );

    if( gtp.recv_buffer >= 0 )
        setSocketBufferSize(s, SO_RCVBUF, gtp.recv_buffer );

    int bytes;

    // get global test parameters from server
    if ((bytes = recv(s, (char*) &gtp, sizeof(GlobalTestParameters), MSG_WAITALL)) == SOCKET_ERROR)
    {
        fprintf(stderr, "recv() global test parameters failed: %d\n", WSAGetLastError());
        exit(-1);
    }
    HARD_ASSERT(bytes == sizeof(GlobalTestParameters));
    
    // get client-specific test parameters from server
    ClientSpecificTestParameters cstp;
    if ((bytes = recv(s, (char*) &cstp, sizeof(ClientSpecificTestParameters), MSG_WAITALL)) == SOCKET_ERROR)
    {
        fprintf(stderr, "recv() client-specific test parameters failed: %d\n", WSAGetLastError());
        exit(-1);
    }
    HARD_ASSERT(bytes == sizeof(ClientSpecificTestParameters));
   
    unique_ptr<char[]> fobuf( new char[gtp.fo_msg_size] );
    unique_ptr<char[]> fibuf( new char[gtp.fi_msg_size] );

    printf( "\nWarming Up..." );
    
    for( int i = 0; i < WARMUP_ITERS; ++i )
    {
        // expect the fan-out
        if ((bytes = recv(s, fobuf.get(), gtp.fo_msg_size, MSG_WAITALL)) == SOCKET_ERROR)
        {
            fprintf(stderr, "recv() fan-out failed: %d\n", WSAGetLastError());
            exit(-1);
        }
        HARD_ASSERT(bytes == gtp.fo_msg_size);
       
        // send the fan-in
        if ((bytes = send(s, fibuf.get(), gtp.fi_msg_size, 0)) == SOCKET_ERROR)
        {
            fprintf(stderr, "send() fan-in failed: %d\n", WSAGetLastError());
            exit(-1);
        }
        HARD_ASSERT(bytes == gtp.fi_msg_size);
    }
    
    printf( "done!\nTesting..." );

    MIB_TCPSTATS tcpStatsBefore, tcpStatsAfter;
    GetTcpStatistics(&tcpStatsBefore);

    for( int i = 0; i < gtp.iters; ++i )
    {
        // expect the fan-out
        if ((bytes = recv(s, fobuf.get(), gtp.fo_msg_size, MSG_WAITALL)) == SOCKET_ERROR)
        {
            fprintf(stderr, "recv() fan-out failed: %d\n", WSAGetLastError());
            exit(-1);
        }
        HARD_ASSERT(bytes == gtp.fo_msg_size);
      
        // send the fan-in
        if ((bytes = send(s, fibuf.get(), gtp.fi_msg_size, 0)) == SOCKET_ERROR)
        {
            fprintf(stderr, "send() fan-in failed: %d\n", WSAGetLastError());
            exit(-1);
        }
        HARD_ASSERT(bytes == gtp.fi_msg_size);

        //printf( "." );
    }

    printf( "done!\n" );

    GetTcpStatistics(&tcpStatsAfter);

    // ISSUE-REVIEW
    // This is a system-wide statistic for all TCP connections.  Can I get a
    // per-connection equivalent with GetPerTcpConnectionEStats or another API?
    ClientResultData crd;
    crd.retransmits = tcpStatsAfter.dwRetransSegs - tcpStatsBefore.dwRetransSegs;

    // send client results
    if ((bytes = send(s, (char*) &crd, sizeof(ClientResultData), 0)) == SOCKET_ERROR)
    {
        fprintf(stderr, "send() client results failed: %d\n", WSAGetLastError());
        exit(-1);
    }
    HARD_ASSERT(bytes == sizeof(ClientResultData));

    gracefulShutdown(s);

    goto beginTest;
}

void usage()
{
    fprintf(stderr, "\
INCAST: Simulates the incast network traffic pattern.\n\
\n\
Copyright (c) Microsoft Corporation 2011\n\
Mark Santaniello (marksan)\n\
\n\
Incast can be run in two modes, client or server.  There is only one server,\n\
but arbitrarily many clients.  Clients launch a coordinated incast \"volley\"\n\
at the server.\n\
\n\
Clients will connect to the server, run a test, and loop forever. Each server\n\
invocation represents a new test.\n\
\n\
For client mode, the only argument is the server IP or name:\n\
    INCAST.EXE <server>\n\
\n\
Test options are specified only on the server side:\n\
    INCAST.EXE <options>\n\
\n\
Available <options> and their default values:\n\
    -n  ITERS  Number of iterations (%d)\n\
    -r  RATE   Iteration rate limit (no limit)\n\
    -c  NUM    Number of clients limit (no limit)\n\
    -d         Disable Nagle's algorithm (enabled)\n\
    -sb SIZE   Socket send buffer size (OS default)\n\
    -rb SIZE   Socket receive buffer size (OS default)\n\
    -o  SIZE   Fan-out message size (%d)\n\
    -i  SIZE   Fan-in message size (%d)\n\
    -f  FILE   Dump full histogram to file\n\
    -j  MSEC   Delay clients via random jitter (disabled)\n\
    -s  MSEC   Delay clients via uniform scheduling (disabled)\n", 
    DEFAULT_ITERS, DEFAULT_FO_MSG_SIZE, DEFAULT_FI_MSG_SIZE );

    exit(-1);
}

int __cdecl main( int argc, char** argv )
{
    setHighPriority();

    // this improves the accuracy of the Sleep calls in the jitter code
    // ISSUE-REVIEW: what about ARM?
#ifndef _M_ARM
    TIMECAPS tc;
    HRESULT hr;
    hr = timeGetDevCaps( &tc, sizeof(tc) );
    HARD_ASSERT( hr == MMSYSERR_NOERROR);
    hr = timeBeginPeriod( tc.wPeriodMin );
    HARD_ASSERT( hr == TIMERR_NOERROR );
#endif

    WSADATA WSAData;

    if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
    {
        fprintf(stderr, "WSAStartup() failed with error code %d", WSAGetLastError());
        exit(-1);
    }

    // ISSUE-REVIEW: Switch to something standard like getopt
    if ((argc == 2) && (argv[1][0] != '-') && (argv[1][0] != '/'))
    {
        clientMain( argv[1] );
    }
    else
    {
        for( int a = 1; a < argc; ++a )
        {
            if ((argv[a][0] != '-') && (argv[a][0] != '/')) 
            {
                usage();
            }

            switch (argv[a][1])
            {
                case '?':
                case 'h':
                    usage();

                case 'i':
                    a++;
                    gtp.fi_msg_size = atoi(argv[a]);
                    if( gtp.fi_msg_size <= 0 )
                    {
                        fprintf(stderr, "-i parameter invalid\n");
                        exit(-1);
                    }
                    break;
                
                case 'o':
                    a++;
                    gtp.fo_msg_size = atoi(argv[a]);
                    if( gtp.fo_msg_size <= 0 )
                    {
                        fprintf(stderr, "-o parameter invalid\n");
                        exit(-1);
                    }
                    break;
                
                case 'r':
                    {
                        if( argv[a][2] == NULL )
                        {
                            a++;
                            gtp.rate_limited = true;
                            gtp.target_rate = atoi(argv[a]);
                            if( gtp.target_rate <= 0 )
                            {
                                fprintf(stderr, "-r parameter invalid\n");
                                exit(-1);
                            }
                        }
                        else if( argv[a][2] == 'b' )
                        {
                            a++;
                            gtp.recv_buffer = atoi(argv[a]);
                            if( gtp.recv_buffer < 0 )
                            {
                                fprintf(stderr, "-rb parameter invalid\n");
                                exit(-1);
                            }
                        }
                        else
                        {
                            fprintf(stderr, "Unknown command line option\n\n");
                            usage();
                        }
                    }
                    break;
                
                case 's':
                    {
                        if( argv[a][2] == NULL )
                        {
                            a++;
                            gtp.delay = atoi(argv[a]);
                            gtp.delay_method = UNIFORM_SCHED;
                            if( gtp.delay <= 0 )
                            {
                                fprintf(stderr, "-s parameter invalid\n");
                                exit(-1);
                            }
                        } 
                        else if( argv[a][2] == 'b' )
                        {
                            a++;
                            gtp.send_buffer = atoi(argv[a]);
                            if( gtp.send_buffer < 0 )
                            {
                                fprintf(stderr, "-sb parameter invalid\n");
                                exit(-1);
                            }
                        }
                        else
                        {
                            fprintf(stderr, "Unknown command line option\n\n");
                            usage();
                        }
                    }
                    break;
                
                case 'c':
                    a++;
                    gtp.clients_limited = true;
                    gtp.client_limit = atoi(argv[a]);
                    if( gtp.client_limit <= 0 )
                    {
                        fprintf(stderr, "-c parameter invalid\n");
                        exit(-1);
                    }
                    break;

                case 'n':
                    a++;
                    gtp.iters = atoi(argv[a]);
                    if( gtp.iters <= 0 )
                    {
                        fprintf(stderr, "-n parameter invalid\n");
                        exit(-1);
                    }
                    break;
                
                case 'd':
                    gtp.nagle = false;
                    break;
                
                case 'f':
                    a++;
                    gtp.histogram = true;
                    // ISSUE-REVIEW: Overwriting existing files?
                    histfile.open(argv[a]);
                    if( !histfile.good() )
                    {
                        fprintf(stderr, "-f parameter invalid\n");
                        exit(-1);
                    }
                    break;
                
                case 'j':
                    a++;
                    gtp.delay = atoi(argv[a]);
                    gtp.delay_method = RANDOM_JITTER;
                    if( gtp.delay <= 0 )
                    {
                        fprintf(stderr, "-j parameter invalid\n");
                        exit(-1);
                    }
                    break;
                   
                default:
                    fprintf(stderr, "Unknown command line option\n\n");
                    usage();
            }
        }

        serverMain();
    }

#ifndef _M_ARM
    hr = timeEndPeriod( tc.wPeriodMin );
    HARD_ASSERT( hr == TIMERR_NOERROR );
#endif

    WSACleanup();
}
