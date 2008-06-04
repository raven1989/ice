// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice-E is licensed to you under the terms described in the
// ICEE_LICENSE file included in this distribution.
//
// **********************************************************************

package IceInternal;

public final class RouterInfo
{
    RouterInfo(Ice.RouterPrx router)
    {
        _router = router;
        _identities = new java.util.Hashtable();

        if(IceUtil.Debug.ASSERT)
        {
            IceUtil.Debug.Assert(_router != null);
        }
    }

    synchronized public void
    destroy()
    {
        _clientEndpoints = new Endpoint[0];
        _serverEndpoints = new Endpoint[0];
        _adapter = null;
        _identities.clear();
    }

    public boolean
    equals(java.lang.Object obj)
    {
        if(this == obj)
        {
            return true;
        }

        if(obj instanceof RouterInfo)
        {
            return _router.equals(((RouterInfo)obj)._router);
        }

        return false;
    }

    public Ice.RouterPrx
    getRouter()
    {
        //
        // No mutex lock necessary, _router is immutable.
        //
        return _router;
    }

    public synchronized Endpoint[]
    getClientEndpoints()
    {
        if(_clientEndpoints == null) // Lazy initialization.
        {
            Ice.ObjectPrx clientProxy = _router.getClientProxy();
            if(clientProxy == null)
            {
                //
                // If getClientProxy() return nil, use router endpoints.
                //
                _clientEndpoints = ((Ice.ObjectPrxHelperBase)_router).__reference().getEndpoints();
            }
            else
            {
                clientProxy = clientProxy.ice_router(null); // The client proxy cannot be routed.

                //
                // In order to avoid creating a new connection to the
                // router, we must use the same timeout as the already
                // existing connection.
                //
                clientProxy = clientProxy.ice_timeout(_router.ice_getConnection().timeout());

                _clientEndpoints = ((Ice.ObjectPrxHelperBase)clientProxy).__reference().getEndpoints();
            }
        }

        return _clientEndpoints;
    }

    public synchronized Endpoint[]
    getServerEndpoints()
    {
        if(_serverEndpoints == null) // Lazy initialization.
        {
            Ice.ObjectPrx serverProxy = _router.getServerProxy();
            if(serverProxy == null)
            {
                throw new Ice.NoEndpointException();
            }

            serverProxy = serverProxy.ice_router(null); // The server proxy cannot be routed.
            _serverEndpoints = ((Ice.ObjectPrxHelperBase)serverProxy).__reference().getEndpoints();
        }

        return _serverEndpoints;
    }

    public void
    addProxy(Ice.ObjectPrx proxy)
    {
        IceUtil.Debug.Assert(proxy != null);

        if(!_identities.containsKey(proxy.ice_getIdentity()))
        {
            //
            // Only add the proxy to the router if it's not already in our local map.
            //
            Ice.ObjectPrx[] proxies = new Ice.ObjectPrx[1];
            proxies[0] = proxy;
            Ice.ObjectPrx[] evictedProxies = _router.addProxies(proxies);

            //
            // If we successfully added the proxy to the router, we add it to our local map.
            //
            _identities.put(proxy.ice_getIdentity(), new java.lang.Integer(0));

            //
            // We also must remove whatever proxies the router evicted.
            //
            for(int i = 0; i < evictedProxies.length; ++i)
            {
                _identities.remove(evictedProxies[i].ice_getIdentity());
            }
        }

    }

    public synchronized void
    setAdapter(Ice.ObjectAdapter adapter)
    {
        _adapter = adapter;
    }

    public synchronized Ice.ObjectAdapter
    getAdapter()
    {
        return _adapter;
    }

    private /*final*/ Ice.RouterPrx _router;
    private Endpoint[] _clientEndpoints;
    private Endpoint[] _serverEndpoints;
    private Ice.ObjectAdapter _adapter;
    private java.util.Hashtable _identities;
}
