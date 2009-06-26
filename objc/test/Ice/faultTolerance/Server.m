// **********************************************************************
//
// Copyright (c) 2003-2009 ZeroC, Inc. All rights reserved.
//
// This copy of Ice Touch is licensed to you under the terms described in the
// ICE_TOUCH_LICENSE file included in this distribution.
//
// **********************************************************************

#import <Ice/Ice.h>
#import <faultTolerance/TestI.h>

#import <Foundation/NSAutoreleasePool.h>
#if ICE_OBJC_GC
  #import <objc/objc-auto.h>
  #import <Foundation/NSGarbageCollector.h>
#endif

#import <stdio.h>

static void
usage(const char* n)
{
    printf("Usage: %s port\n", n);
}

static int
run(int argc, char** argv, id<ICECommunicator> communicator)
{
    int port = 0;
    int i;
    for(i = 1; i < argc; ++i)
    {
        if(argv[i][0] == '-')
        {
            fprintf(stderr, "%s: unknown option `%s'", argv[0], argv[i]);
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        if(port > 0)
        {
            fprintf(stderr, "%s: only one port can be specified", argv[0]);
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        port = atoi(argv[i]);
    }

    if(port <= 0)
    {
        fprintf(stderr, "%s: no port specified", argv[0]);
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    NSString* endpts = [NSString stringWithFormat:@"default  -p %d:udp", port];
    [[communicator getProperties] setProperty:@"TestAdapter.Endpoints" value:endpts];
    id<ICEObjectAdapter> adapter = [communicator createObjectAdapter:@"TestAdapter"];
    ICEObject* object = [[[TestI alloc] init] autorelease];
    [adapter add:object identity:[communicator stringToIdentity:@"test"]];
    [adapter activate];
    [communicator waitForShutdown];
    return EXIT_SUCCESS;
}

int
main(int argc, char* argv[])
{
#if ICE_OBJC_GC
    objc_startCollectorThread();
#endif
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    int status;
    id<ICECommunicator> communicator = nil;

    @try
    {
        //
        // In this test, we need a longer server idle time, otherwise
        // our test servers may time out before they are used in the
        // test.
        //
        ICEInitializationData* initData = [ICEInitializationData initializationData];
        initData.properties = [ICEUtil createProperties:&argc argv:argv];
        [initData.properties setProperty:@"Ice.ServerIdleTime" value:@"120"]; // Two minutes.

        communicator = [ICEUtil createCommunicator:&argc argv:argv initData:initData];
        status = run(argc, argv, communicator);
    }
    @catch(ICEException* ex)
    {
        NSLog(@"%@", ex);
        status = EXIT_FAILURE;
    }

    if(communicator)
    {
        @try
        {
            [communicator destroy];
        }
        @catch(ICEException* ex)
        {
            NSLog(@"%@", ex);
            status = EXIT_FAILURE;
        }
    }

    [pool release];
#if ICE_OBJC_GC
    [[NSGarbageCollector defaultCollector] collectExhaustively];
#endif
    return status;
}
