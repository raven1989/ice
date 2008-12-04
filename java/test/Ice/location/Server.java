// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package test.Ice.location;

public class Server extends test.Util.Application
{
    private Ice.InitializationData _initData;

    public int run(String[] args)
    {
        Ice.Communicator communicator = communicator();
        Ice.Properties properties = communicator.getProperties();
        properties.setProperty("Ice.ThreadPool.Server.Size", "2");
        properties.setProperty("Ice.ThreadPool.Server.SizeWarn", "0");
        properties.setProperty("ServerManagerAdapter.Endpoints", "default -p 12010 -t 30000:udp");

        //
        // Register the server manager. The server manager creates a new
        // 'server' (a server isn't a different process, it's just a new
        // communicator and object adapter).
        //
        Ice.ObjectAdapter adapter = communicator.createObjectAdapter("ServerManagerAdapter");

        //
        // We also register a sample server locator which implements the
        // locator interface, this locator is used by the clients and the
        // 'servers' created with the server manager interface.
        //
        ServerLocatorRegistry registry = new ServerLocatorRegistry();
        registry.addObject(adapter.createProxy(communicator.stringToIdentity("ServerManager")));
        Ice.Object object = new ServerManagerI(adapter, registry, _initData);
        adapter.add(object, communicator.stringToIdentity("ServerManager"));

        Ice.LocatorRegistryPrx registryPrx = Ice.LocatorRegistryPrxHelper.uncheckedCast(adapter.add(registry,
                communicator.stringToIdentity("registry")));

        ServerLocator locator = new ServerLocator(registry, registryPrx);
        adapter.add(locator, communicator.stringToIdentity("locator"));

        adapter.activate();
        communicator.waitForShutdown();

        return 0;
    }

    protected Ice.InitializationData getInitData(Ice.StringSeqHolder argsH)
    {
        Ice.InitializationData initData = new Ice.InitializationData();
        initData.properties = Ice.Util.createProperties(argsH);
        initData.properties.setProperty("Ice.Package.Test", "test.Ice.location");

        _initData = initData;
        return initData;
    }

    public static void main(String[] args)
    {
        Server app = new Server();
        int result = app.main("Server", args);
        System.gc();
        System.exit(result);
    }
}
