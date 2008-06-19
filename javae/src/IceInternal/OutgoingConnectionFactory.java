// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice-E is licensed to you under the terms described in the
// ICEE_LICENSE file included in this distribution.
//
// **********************************************************************

package IceInternal;

public final class OutgoingConnectionFactory
{
    public synchronized void
    destroy()
    {
        if(_destroyed)
        {
            return;
        }

        java.util.Enumeration p = _connections.elements();
        while(p.hasMoreElements())
        {
            java.util.Vector connectionList = (java.util.Vector)p.nextElement();
                
            java.util.Enumeration q = connectionList.elements();
            while(q.hasMoreElements())
            {
                Ice.Connection connection = (Ice.Connection)q.nextElement();
                connection.destroy(Ice.Connection.CommunicatorDestroyed);
            }
        }

        _destroyed = true;
        notifyAll();
    }

    public void
    waitUntilFinished()
    {
        java.util.Hashtable connections;

        synchronized(this)
        {
            //
            // First we wait until the factory is destroyed. We also
            // wait until there are no pending connections
            // anymore. Only then we can be sure the _connections
            // contains all connections.
            //
            while(!_destroyed || !_pending.isEmpty())
            {
                try
                {
                    wait();
                }
                catch(InterruptedException ex)
                {
                }
            }

            //
            // We want to wait until all connections are finished
            // outside the thread synchronization.
            //
            connections = _connections;
            _connections = null;
        }
        
        //
        // Now we wait for until the destruction of each connection is
        // finished.
        //
        java.util.Enumeration p = connections.elements();
        while(p.hasMoreElements())
        {
            java.util.Vector connectionList = (java.util.Vector)p.nextElement();
                
            java.util.Enumeration q = connectionList.elements();
            while(q.hasMoreElements())
            {
                Ice.Connection connection = (Ice.Connection)q.nextElement();
                connection.waitUntilFinished();
            }
        }
    }

    public Ice.Connection
    create(Endpoint[] endpts)
    {
        class ConnectorEndpointPair
        {
             public ConnectorEndpointPair(Connector c, Endpoint e)
             {
                 connector = c;
                 endpoint = e;
             }

             public Connector connector;
             public Endpoint endpoint;
        }

        if(IceUtil.Debug.ASSERT)
        {
            IceUtil.Debug.Assert(endpts.length > 0);
        }
        java.util.Vector connectors = new java.util.Vector();

        DefaultsAndOverrides defaultsAndOverrides = _instance.defaultsAndOverrides();

        synchronized(this)
        {
            if(_destroyed)
            {
                throw new Ice.CommunicatorDestroyedException();
            }

            //
            // Reap connections for which destruction has completed.
            //
            java.util.Enumeration p = _connections.keys();
            while(p.hasMoreElements())
            {
                java.lang.Object key = p.nextElement();
                java.util.Vector connectionList = (java.util.Vector)_connections.get(key);

                for(int i = connectionList.size(); i > 0 ; --i)
                {
                    Ice.Connection con = (Ice.Connection)connectionList.elementAt(i - 1);
                    if(con.isFinished())
                    {
                        connectionList.removeElementAt(i - 1);
                    }
                }

                if(connectionList.isEmpty())
                {
                    _connections.remove(key);
                }
            }

            Endpoint[] endpoints = new Endpoint[endpts.length];
            System.arraycopy(endpts, 0, endpoints, 0, endpts.length);
            for(int i = 0; i < endpoints.length; i++)
            {
                //
                // Modify endpoints with overrides.
                //
                if(defaultsAndOverrides.overrideTimeout)
                {
                    endpoints[i] = endpoints[i].timeout(defaultsAndOverrides.overrideTimeoutValue);
                }

                //
                // Create connectors for the endpoint.
                //
                java.util.Vector cons = endpoints[i].connectors();
                if(IceUtil.Debug.ASSERT)
                {
                    IceUtil.Debug.Assert(cons.size() > 0);
                }

                p = cons.elements();
                while(p.hasMoreElements())
                {
                    connectors.addElement(new ConnectorEndpointPair((Connector)p.nextElement(), endpoints[i]));
                }
            }

            //
            // Search for existing connections.
            //
            p = connectors.elements();
            while(p.hasMoreElements())
            {
                ConnectorEndpointPair cep = (ConnectorEndpointPair)p.nextElement();

                java.util.Vector connectionList = (java.util.Vector)_connections.get(cep.connector);
                if(connectionList != null)
                {
                    java.util.Enumeration q = connectionList.elements();
                        
                    while(q.hasMoreElements())
                    {
                        Ice.Connection connection = (Ice.Connection)q.nextElement();
                        
                        //
                        // Don't return connections for which destruction has
                        // been initiated.
                        //
                        if(!connection.isDestroyed())
                        {
                            return connection;
                        }
                    }
                }
            }

            //
            // If some other thread is currently trying to establish a
            // connection to any of our endpoints, we wait until this
            // thread is finished.
            //
            boolean searchAgain = false;
            while(!_destroyed)
            {
                boolean found = false;
                p = connectors.elements();
                while(p.hasMoreElements())
                {
                    ConnectorEndpointPair cep = (ConnectorEndpointPair)p.nextElement();
                    if(_pending.contains(cep.connector))
                    {
                        found = true;
                        break;
                    }
                }

                if(!found)
                {
                    break;
                }
                
                searchAgain = true;

                try
                {
                    wait();
                }
                catch(InterruptedException ex)
                {
                }
            }

            if(_destroyed)
            {
                throw new Ice.CommunicatorDestroyedException();
            }

            //
            // Search for existing connections again if we waited
            // above, as new connections might have been added in the
            // meantime.
            //
            if(searchAgain)
            {
                p = connectors.elements();
                while(p.hasMoreElements())
                {
                    ConnectorEndpointPair cep = (ConnectorEndpointPair)p.nextElement();

                    java.util.Vector connectionList = (java.util.Vector)_connections.get(cep.connector);
                    if(connectionList != null)
                    {
                        java.util.Enumeration q = connectionList.elements();
                        
                        while(q.hasMoreElements())
                        {
                            Ice.Connection connection = (Ice.Connection)q.nextElement();
                            
                            //
                            // Don't return connections for which destruction has
                            // been initiated.
                            //
                            if(!connection.isDestroyed())
                            {
                                return connection;
                            }
                        }
                    }
                }
            }

            //
            // No connection to any of our endpoints exists yet, so we
            // will try to create one. To avoid that other threads try
            // to create connections to the same endpoints, we add our
            // endpoints to _pending.
            //
            p = connectors.elements();
            while(p.hasMoreElements())
            {
                ConnectorEndpointPair cep = (ConnectorEndpointPair)p.nextElement();
                _pending.put(cep.connector, cep.connector);
            }
        }

        Connector connector = null;
        Ice.Connection connection = null;
        Ice.LocalException exception = null;

        java.util.Enumeration p = connectors.elements();
        while(p.hasMoreElements())
        {
            ConnectorEndpointPair cep = (ConnectorEndpointPair)p.nextElement();
            connector = cep.connector;
            Endpoint endpoint = cep.endpoint;
            
            try
            {
                int timeout;
                if(defaultsAndOverrides.overrideConnectTimeout)
                {
                    timeout = defaultsAndOverrides.overrideConnectTimeoutValue;
                }
                // It is not necessary to check for overrideTimeout,
                // the endpoint has already been modified with this
                // override, if set.
                else
                {
                    timeout = endpoint.timeout();
                }

                Transceiver transceiver = connector.connect(timeout);
                if(IceUtil.Debug.ASSERT)
                {
                    IceUtil.Debug.Assert(transceiver != null);
                }

                connection = new Ice.Connection(_instance, transceiver, endpoint, null);
                //
                // Wait for the connection to be validated by the
                // connection thread.  Once the connection has been
                // validated it will be activated also.
                //
                connection.waitForValidation();
                break;
            }
            catch(Ice.LocalException ex)
            {
                exception = ex;

                //
                // If a connection object was constructed, then validate()
                // must have raised the exception.
                //
                if(connection != null)
                {
                    connection.waitUntilFinished(); // We must call waitUntilFinished() for cleanup.
                    connection = null;
                }
            }
            
            TraceLevels traceLevels = _instance.traceLevels();
            if(traceLevels.retry >= 2)
            {
                StringBuffer s = new StringBuffer();
                s.append("connection to endpoint failed");
                if(p.hasMoreElements())
                {
                    s.append(", trying next endpoint\n");
                }
                else
                {
                    s.append(" and no more endpoints to try\n");
                }
                s.append(exception.toString());
                _instance.initializationData().logger.trace(traceLevels.retryCat, s.toString());
            }
        }
        
        synchronized(this)
        {
            //
            // Signal other threads that we are done with trying to
            // establish connections to our endpoints.
            //
            p = connectors.elements();
            while(p.hasMoreElements())
            {
                ConnectorEndpointPair cep = (ConnectorEndpointPair)p.nextElement();
                _pending.remove(cep.connector);
            }
            notifyAll();
            
            if(connection == null)
            {
                if(IceUtil.Debug.ASSERT)
                {
                    IceUtil.Debug.Assert(exception != null);
                }
                throw exception;
            }
            else
            {
                java.util.Vector connectionList = (java.util.Vector)_connections.get(connector);
                if(connectionList == null)
                {
                    connectionList = new java.util.Vector();
                    _connections.put(connector, connectionList);
                }
                connectionList.addElement(connection);

                if(_destroyed)
                {
                    connection.destroy(Ice.Connection.CommunicatorDestroyed);
                    throw new Ice.CommunicatorDestroyedException();
                }
            }
        }
        
        if(IceUtil.Debug.ASSERT)
        {
            IceUtil.Debug.Assert(connection != null);
        }
        return connection;
    }

    public synchronized void
    setRouterInfo(IceInternal.RouterInfo routerInfo)
    {
        if(_destroyed)
        {
            throw new Ice.CommunicatorDestroyedException();
        }

        if(IceUtil.Debug.ASSERT)
        {
            IceUtil.Debug.Assert(routerInfo != null);
        }

        //
        // Search for connections to the router's client proxy
        // endpoints, and update the object adapter for such
        // connections, so that callbacks from the router can be
        // received over such connections.
        //
        Ice.ObjectAdapter adapter = routerInfo.getAdapter();
        DefaultsAndOverrides defaultsAndOverrides = _instance.defaultsAndOverrides();
        Endpoint[] endpoints = routerInfo.getClientEndpoints();
        for(int i = 0; i < endpoints.length; i++)
        {
            Endpoint endpoint = endpoints[i];

            //
            // Modify endpoints with overrides.
            //
            if(defaultsAndOverrides.overrideTimeout)
            {
                endpoint = endpoint.timeout(defaultsAndOverrides.overrideTimeoutValue);
            }
 
            java.util.Enumeration p = _connections.elements();
            while(p.hasMoreElements())
            {
                java.util.Vector connectionList = (java.util.Vector)p.nextElement();

                java.util.Enumeration q = connectionList.elements();
                while(q.hasMoreElements())
                {
                    Ice.Connection connection = (Ice.Connection)q.nextElement();
                    try
                    {
                        connection.setAdapter(adapter);
                    }
                    catch(Ice.LocalException ex)
                    {
                        //
                        // Ignore, the connection is being closed or closed.
                        //
                    }
                }
            }
        }
    }

    public synchronized void
    removeAdapter(Ice.ObjectAdapter adapter)
    {
        if(_destroyed)
        {
            return;
        }

        java.util.Enumeration p = _connections.elements();
        while(p.hasMoreElements())
        {
            java.util.Vector connectionList = (java.util.Vector)p.nextElement();
                
            java.util.Enumeration q = connectionList.elements();
            while(q.hasMoreElements())
            {
                Ice.Connection connection = (Ice.Connection)q.nextElement();
                if(connection.getAdapter() == adapter)
                {
                    try
                    {
                        connection.setAdapter(null);
                    }
                    catch(Ice.LocalException ex)
                    {
                        //
                        // Ignore, the connection is being closed or closed.
                        //
                    }
                }
            }
        }
    }

    public void
    flushBatchRequests()
    {
        java.util.Vector c = new java.util.Vector();

        synchronized(this)
        {
            java.util.Enumeration p = _connections.elements();
            while(p.hasMoreElements())
            {
                java.util.Vector connectionList = (java.util.Vector)p.nextElement();
                java.util.Enumeration q = connectionList.elements();
                while(q.hasMoreElements())
                {
                    c.addElement(q.nextElement());
                }
            }
        }

        java.util.Enumeration p = c.elements();
        while(p.hasMoreElements())
        {
            Ice.Connection conn = (Ice.Connection)p.nextElement();
            try
            {
                conn.flushBatchRequests();
            }
            catch(Ice.LocalException ex)
            {
                // Ignore.
            }
        }
    }

    //
    // Only for use by Instance.
    //
    OutgoingConnectionFactory(Instance instance)
    {
        _instance = instance;
        _destroyed = false;
    }

    protected synchronized void
    finalize()
        throws Throwable
    {
        IceUtil.Debug.FinalizerAssert(_destroyed);
        IceUtil.Debug.FinalizerAssert(_connections == null);

        //
        // Cannot call parent's finalizer here. java.lang.Object in CLDC doesn't have a finalize call.
        //
    }

    private /*final*/ Instance _instance;
    private boolean _destroyed;
    private java.util.Hashtable _connections = new java.util.Hashtable();
    private java.util.Hashtable _pending = new java.util.Hashtable();
}
