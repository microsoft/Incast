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

#ifndef _INCAST_H
#define _INCAST_H

#include <stdio.h>
#include <stdlib.h>
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <process.h>
#include <conio.h>
#include <sys/stat.h>
#include <vector>
#include <algorithm>
#include <map>
#include <limits>
#include <memory>
#include <string>
#include <fstream>

#ifndef _M_ARM
    #include <mmsystem.h>
#endif

#include "barrier.h"

const unsigned PORT = 27779;
const int DEFAULT_ITERS = 10000;
const int WARMUP_ITERS = 10;
const int DEFAULT_FO_MSG_SIZE = 256;
const int DEFAULT_FI_MSG_SIZE = 4096;

enum DelayMethod
{
        NONE,
        RANDOM_JITTER,
        UNIFORM_SCHED
};

struct GlobalTestParameters
{
    int clients;
    int iters;
    bool rate_limited;
    int target_rate;
    int fo_msg_size;
    int fi_msg_size;
    bool clients_limited;
    int client_limit;
    int delay;
    DelayMethod delay_method;

    // ISSUE-REVIEW
    // Should these be broken down into distinct
    // client and server options?
    bool nagle;
    int send_buffer;
    int recv_buffer;

    bool histogram;

    GlobalTestParameters()
        : clients(0)
        , iters(DEFAULT_ITERS)
        , rate_limited(false)
        , target_rate(0)
        , fo_msg_size(DEFAULT_FO_MSG_SIZE)
        , fi_msg_size(DEFAULT_FI_MSG_SIZE)
        , clients_limited(false)
        , client_limit(0)
        , delay(0)
        , delay_method(NONE)
        , nagle(true)
        , send_buffer(-1)
        , recv_buffer(-1)
        , histogram(false)
    {};
} gtp;

struct ClientSpecificTestParameters
{
    int client_num;
    ClientSpecificTestParameters()
        : client_num(-1)
    {};
};

struct ClientResultData
{
    int retransmits;

    ClientResultData()
        : retransmits(0)
    {};
};

struct Measurement
{
    __int64 actual_delay;
    __int64 start;
    __int64 stop;
};

typedef std::vector<Measurement> Measurements;

struct TestResult
{
    ClientResultData crd;
    Measurements measurements;
};

std::vector<TestResult> clientResults;
std::vector<HANDLE> clientThreads;
std::vector<SOCKET> clientSockets;

barrier *pb;

std::ofstream histfile;
    
MIB_TCPSTATS tcpStatsBefore, tcpStatsAfter;

typedef std::map<std::string,std::vector<int>> ClientAddressMap;
ClientAddressMap clientAddressMap;

#endif // _INCAST_H
