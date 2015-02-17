Incast: A benchmark which simulates the "incast" network traffic pattern
==========

Incast.EXE is a client-server network benchmark that simulates the “incast” traffic pattern which is common to map-reduce style cloud apps.

Incast.EXE makes it easy to create this traffic pattern in order to evaluate the performance of switches, NICs, and other networking gear.

Incast can be run in two modes, client or server.  There is only one server, but arbitrarily many clients.  A server sends a “fan-out” request to many clients, which respond simultaneously with a “fan-in” response.  The response is the incast event.

Clients will connect to the server, run a test, and loop forever. Each server invocation represents a new test.

Usage
------

    For client mode, the only argument is the server IP or name:
    
        INCAST.EXE <server>
    
    Test options are specified only on the server side:
    
         INCAST.EXE <options>
    
    Available <options> and their default values:
        
        -n  ITERS  Number of iterations (%d)
        -r  RATE   Iteration rate limit (no limit)
        -c  NUM    Number of clients limit (no limit)
        -d         Disable Nagle's algorithm (enabled)
        -sb SIZE   Socket send buffer size (OS default)
        -rb SIZE   Socket receive buffer size (OS default)
        -o  SIZE   Fan-out message size (%d)
        -i  SIZE   Fan-in message size (%d)
        -f  FILE   Dump full histogram to file
        -j  MSEC   Delay clients via random jitter (disabled)
        -s  MSEC   Delay clients via uniform scheduling (disabled)
