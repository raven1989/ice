// **********************************************************************
//
// Copyright (c) 2003-2012 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_TOUCH_LICENSE file included in this distribution.
//
// **********************************************************************

#include <IceUtil/Random.h>
#include <Ice/Ice.h>
#include <TestCommon.h>
#include <BindingTest.h>
#include <set>

#include <functional>

using namespace std;
using namespace Test::Binding;

namespace 
{

struct RandomNumberGenerator : public std::unary_function<ptrdiff_t, ptrdiff_t>
{
    ptrdiff_t operator()(ptrdiff_t d)
    {
        return IceUtilInternal::random(static_cast<int>(d));
    }
};

class GetAdapterNameCB : public IceUtil::Shared, public IceUtil::Monitor<IceUtil::Mutex>
{
public:
    
    void
    response(const string& name)
    {
        Lock sync(*this);
        assert(!name.empty());
        _name = name;
        notify();
    }
    
    void
    exception(const Ice::Exception&)
    {
        test(false);
    }
    
    virtual string
    getResult()
    {
        Lock sync(*this);
        while(_name.empty())
        {
            wait();
        }
        return _name;
    }
    
private:
    
    string _name;
};
typedef IceUtil::Handle<GetAdapterNameCB> GetAdapterNameCBPtr;

}

string
getAdapterNameWithAMI(const TestIntfPrx& test)
{
    GetAdapterNameCBPtr cb = new GetAdapterNameCB();
    test->begin_getAdapterName(
                               newCallback_TestIntf_getAdapterName(cb, &GetAdapterNameCB::response,  &GetAdapterNameCB::exception));
    return cb->getResult();
}

TestIntfPrx
createTestIntfPrx(vector<RemoteObjectAdapterPrx>& adapters)
{
    Ice::EndpointSeq endpoints;
    TestIntfPrx test;
    for(vector<RemoteObjectAdapterPrx>::const_iterator p = adapters.begin(); p != adapters.end(); ++p)
    {
        test = (*p)->getTestIntf();
        Ice::EndpointSeq edpts = test->ice_getEndpoints();
        endpoints.insert(endpoints.end(), edpts.begin(), edpts.end());
    }
    return TestIntfPrx::uncheckedCast(test->ice_endpoints(endpoints));
}

void
deactivate(const RemoteCommunicatorPrx& com, vector<RemoteObjectAdapterPrx>& adapters)
{
    for(vector<RemoteObjectAdapterPrx>::const_iterator p = adapters.begin(); p != adapters.end(); ++p)
    {
        com->deactivateObjectAdapter(*p);
    }
}

void
bindingAllTests(const Ice::CommunicatorPtr& communicator)
{
    string ref = "communicator:default -p 12010";
    RemoteCommunicatorPrx com = RemoteCommunicatorPrx::uncheckedCast(communicator->stringToProxy(ref));
    
    RandomNumberGenerator rng;
    
    tprintf("testing binding with single endpoint... ");
    {
        RemoteObjectAdapterPrx adapter = com->createObjectAdapter("Adapter", "default");
        
        TestIntfPrx test1 = adapter->getTestIntf();
        TestIntfPrx test2 = adapter->getTestIntf();
        test(test1->ice_getConnection() == test2->ice_getConnection());
        
        test1->ice_ping();
        test2->ice_ping();
        
        com->deactivateObjectAdapter(adapter);
        
        TestIntfPrx test3 = TestIntfPrx::uncheckedCast(test1);
        test(test3->ice_getConnection() == test1->ice_getConnection());
        test(test3->ice_getConnection() == test2->ice_getConnection());
        
        try
        {
            test3->ice_ping();
            test(false);
        }
        catch(const Ice::ConnectionRefusedException&)
        {
        }
    }
    tprintf("ok\n");
    
    tprintf("testing binding with multiple endpoints... ");
    {
        vector<RemoteObjectAdapterPrx> adapters;
        adapters.push_back(com->createObjectAdapter("Adapter11", "default"));
        adapters.push_back(com->createObjectAdapter("Adapter12", "default"));
        adapters.push_back(com->createObjectAdapter("Adapter13", "default"));
        
        //
        // Ensure that when a connection is opened it's reused for new
        // proxies and that all endpoints are eventually tried.
        //
        set<string> names;
        names.insert("Adapter11");
        names.insert("Adapter12");
        names.insert("Adapter13");
        while(!names.empty())
        {
            vector<RemoteObjectAdapterPrx> adpts = adapters;
            
            TestIntfPrx test1 = createTestIntfPrx(adpts);
            random_shuffle(adpts.begin(), adpts.end(), rng);
            TestIntfPrx test2 = createTestIntfPrx(adpts);
            random_shuffle(adpts.begin(), adpts.end(), rng);
            TestIntfPrx test3 = createTestIntfPrx(adpts);
            
            test(test1->ice_getConnection() == test2->ice_getConnection());
            test(test2->ice_getConnection() == test3->ice_getConnection());
            
            names.erase(test1->getAdapterName());
            test1->ice_getConnection()->close(false);
        }
        
        //
        // Ensure that the proxy correctly caches the connection (we
        // always send the request over the same connection.)
        //
        {
            for(vector<RemoteObjectAdapterPrx>::const_iterator p = adapters.begin(); p != adapters.end(); ++p)
            {
                (*p)->getTestIntf()->ice_ping();
            }
            
            TestIntfPrx test = createTestIntfPrx(adapters);
            string name = test->getAdapterName();
            const int nRetry = 10;
            int i;
            for(i = 0; i < nRetry &&  test->getAdapterName() == name; i++);
            test(i == nRetry);
            
            for(vector<RemoteObjectAdapterPrx>::const_iterator q = adapters.begin(); q != adapters.end(); ++q)
            {
                (*q)->getTestIntf()->ice_getConnection()->close(false);
            }
        }           
        
        //
        // Deactivate an adapter and ensure that we can still
        // establish the connection to the remaining adapters.
        //
        com->deactivateObjectAdapter(adapters[0]);
        names.insert("Adapter12");
        names.insert("Adapter13");
        while(!names.empty())
        {
            vector<RemoteObjectAdapterPrx> adpts = adapters;
            
            TestIntfPrx test1 = createTestIntfPrx(adpts);
            random_shuffle(adpts.begin(), adpts.end(), rng);
            TestIntfPrx test2 = createTestIntfPrx(adpts);
            random_shuffle(adpts.begin(), adpts.end(), rng);
            TestIntfPrx test3 = createTestIntfPrx(adpts);
            
            test(test1->ice_getConnection() == test2->ice_getConnection());
            test(test2->ice_getConnection() == test3->ice_getConnection());
            
            names.erase(test1->getAdapterName());
            test1->ice_getConnection()->close(false);
        }
        
        //
        // Deactivate an adapter and ensure that we can still
        // establish the connection to the remaining adapter.
        //
        com->deactivateObjectAdapter(adapters[2]);      
        TestIntfPrx test = createTestIntfPrx(adapters);
        test(test->getAdapterName() == "Adapter12");    
        
        deactivate(com, adapters);
    }
    tprintf("ok\n");
    
    tprintf("testing binding with multiple random endpoints... ");
    {
        vector<RemoteObjectAdapterPrx> adapters;
        adapters.push_back(com->createObjectAdapter("AdapterRandom11", "default"));
        adapters.push_back(com->createObjectAdapter("AdapterRandom12", "default"));
        adapters.push_back(com->createObjectAdapter("AdapterRandom13", "default"));
        adapters.push_back(com->createObjectAdapter("AdapterRandom14", "default"));
        adapters.push_back(com->createObjectAdapter("AdapterRandom15", "default"));
        
#ifdef _WIN32
        int count = 60;
#else
        int count = 20;
#endif
        int adapterCount = static_cast<int>(adapters.size());
        while(--count > 0)
        {
#ifdef _WIN32
            if(count == 10)
            {
                com->deactivateObjectAdapter(adapters[4]);
                --adapterCount;
            }
            vector<TestIntfPrx> proxies;
            proxies.resize(10);
#else
            if(count < 60 && count % 10 == 0)
            {
                com->deactivateObjectAdapter(adapters[count / 10 - 1]);
                --adapterCount;
            }
            vector<TestIntfPrx> proxies;
            proxies.resize(40);
#endif
            unsigned int i;
            for(i = 0; i < proxies.size(); ++i)
            {
                vector<RemoteObjectAdapterPrx> adpts;
                adpts.resize(IceUtilInternal::random(static_cast<int>(adapters.size())));
                if(adpts.empty())
                {
                    adpts.resize(1);
                }
                for(vector<RemoteObjectAdapterPrx>::iterator p = adpts.begin(); p != adpts.end(); ++p)
                {
                    *p = adapters[IceUtilInternal::random(static_cast<int>(adapters.size()))];
                }
                proxies[i] = createTestIntfPrx(adpts);
            }
            
            for(i = 0; i < proxies.size(); i++)
            {
                proxies[i]->begin_getAdapterName();
            }
            for(i = 0; i < proxies.size(); i++)
            {
                try
                {
                    proxies[i]->ice_ping();
                }
                catch(const Ice::LocalException&)
                {
                }
            }
            set<Ice::ConnectionPtr> connections;
            for(i = 0; i < proxies.size(); i++)
            {
                if(proxies[i]->ice_getCachedConnection())
                {
                    connections.insert(proxies[i]->ice_getCachedConnection());
                }
            }
            test(static_cast<int>(connections.size()) <= adapterCount);
            
            for(vector<RemoteObjectAdapterPrx>::const_iterator q = adapters.begin(); q != adapters.end(); ++q)
            {
                try
                {
                    (*q)->getTestIntf()->ice_getConnection()->close(false);
                }
                catch(const Ice::LocalException&)
                {
                    // Expected if adapter is down.
                }
            }
        }
    }
    tprintf("ok\n");
    
    tprintf("testing binding with multiple endpoints and AMI... ");
    {
        vector<RemoteObjectAdapterPrx> adapters;
        adapters.push_back(com->createObjectAdapter("AdapterAMI11", "default"));
        adapters.push_back(com->createObjectAdapter("AdapterAMI12", "default"));
        adapters.push_back(com->createObjectAdapter("AdapterAMI13", "default"));
        
        //
        // Ensure that when a connection is opened it's reused for new
        // proxies and that all endpoints are eventually tried.
        //
        set<string> names;
        names.insert("AdapterAMI11");
        names.insert("AdapterAMI12");
        names.insert("AdapterAMI13");
        while(!names.empty())
        {
            vector<RemoteObjectAdapterPrx> adpts = adapters;
            
            TestIntfPrx test1 = createTestIntfPrx(adpts);
            random_shuffle(adpts.begin(), adpts.end(), rng);
            TestIntfPrx test2 = createTestIntfPrx(adpts);
            random_shuffle(adpts.begin(), adpts.end(), rng);
            TestIntfPrx test3 = createTestIntfPrx(adpts);
            
            test(test1->ice_getConnection() == test2->ice_getConnection());
            test(test2->ice_getConnection() == test3->ice_getConnection());
            
            names.erase(getAdapterNameWithAMI(test1));
            test1->ice_getConnection()->close(false);
        }
        
        //
        // Ensure that the proxy correctly caches the connection (we
        // always send the request over the same connection.)
        //
        {
            for(vector<RemoteObjectAdapterPrx>::const_iterator p = adapters.begin(); p != adapters.end(); ++p)
            {
                (*p)->getTestIntf()->ice_ping();
            }
            
            TestIntfPrx test = createTestIntfPrx(adapters);
            string name = getAdapterNameWithAMI(test);
            const int nRetry = 10;
            int i;
            for(i = 0; i < nRetry && getAdapterNameWithAMI(test) == name; i++);
            test(i == nRetry);
            
            for(vector<RemoteObjectAdapterPrx>::const_iterator q = adapters.begin(); q != adapters.end(); ++q)
            {
                (*q)->getTestIntf()->ice_getConnection()->close(false);
            }
        }           
        
        //
        // Deactivate an adapter and ensure that we can still
        // establish the connection to the remaining adapters.
        //
        com->deactivateObjectAdapter(adapters[0]);
        names.insert("AdapterAMI12");
        names.insert("AdapterAMI13");
        while(!names.empty())
        {
            vector<RemoteObjectAdapterPrx> adpts = adapters;
            
            TestIntfPrx test1 = createTestIntfPrx(adpts);
            random_shuffle(adpts.begin(), adpts.end(), rng);
            TestIntfPrx test2 = createTestIntfPrx(adpts);
            random_shuffle(adpts.begin(), adpts.end(), rng);
            TestIntfPrx test3 = createTestIntfPrx(adpts);
            
            test(test1->ice_getConnection() == test2->ice_getConnection());
            test(test2->ice_getConnection() == test3->ice_getConnection());
            
            names.erase(test1->getAdapterName());
            test1->ice_getConnection()->close(false);
        }
        
        //
        // Deactivate an adapter and ensure that we can still
        // establish the connection to the remaining adapter.
        //
        com->deactivateObjectAdapter(adapters[2]);      
        TestIntfPrx test = createTestIntfPrx(adapters);
        test(test->getAdapterName() == "AdapterAMI12"); 
        
        deactivate(com, adapters);
    }
    tprintf("ok\n");
    
    tprintf("testing random endpoint selection... ");
    {
        vector<RemoteObjectAdapterPrx> adapters;
        adapters.push_back(com->createObjectAdapter("Adapter21", "default"));
        adapters.push_back(com->createObjectAdapter("Adapter22", "default"));
        adapters.push_back(com->createObjectAdapter("Adapter23", "default"));
        
        TestIntfPrx test = createTestIntfPrx(adapters);
        test(test->ice_getEndpointSelection() == Ice::Random);
        
        set<string> names;
        names.insert("Adapter21");
        names.insert("Adapter22");
        names.insert("Adapter23");
        while(!names.empty())
        {
            names.erase(test->getAdapterName());
            test->ice_getConnection()->close(false);
        }
        
        test = TestIntfPrx::uncheckedCast(test->ice_endpointSelection(Ice::Random));
        test(test->ice_getEndpointSelection() == Ice::Random);
        
        names.insert("Adapter21");
        names.insert("Adapter22");
        names.insert("Adapter23");
        while(!names.empty())
        {
            names.erase(test->getAdapterName());
            test->ice_getConnection()->close(false);
        }
        
        deactivate(com, adapters);
    }
    tprintf("ok\n");
    
    tprintf("testing ordered endpoint selection... ");
    {
        vector<RemoteObjectAdapterPrx> adapters;
        adapters.push_back(com->createObjectAdapter("Adapter31", "default"));
        adapters.push_back(com->createObjectAdapter("Adapter32", "default"));
        adapters.push_back(com->createObjectAdapter("Adapter33", "default"));
        
        TestIntfPrx test = createTestIntfPrx(adapters);
        test = TestIntfPrx::uncheckedCast(test->ice_endpointSelection(Ice::Ordered));
        test(test->ice_getEndpointSelection() == Ice::Ordered);
        const int nRetry = 5;
        int i;
        
        //
        // Ensure that endpoints are tried in order by deactiving the adapters
        // one after the other.
        //
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter31"; i++);
        test(i == nRetry);
        com->deactivateObjectAdapter(adapters[0]);
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter32"; i++);
        test(i == nRetry);
        com->deactivateObjectAdapter(adapters[1]);
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter33"; i++);
        test(i == nRetry);
        com->deactivateObjectAdapter(adapters[2]);
        
        try
        {
            test->getAdapterName();
        }
        catch(const Ice::ConnectionRefusedException&)
        {
        }
        
        Ice::EndpointSeq endpoints = test->ice_getEndpoints();
        
        adapters.clear();
        
        //
        // Now, re-activate the adapters with the same endpoints in the opposite
        // order.
        // 
        adapters.push_back(com->createObjectAdapter("Adapter36", endpoints[2]->toString()));
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter36"; i++);
        test(i == nRetry);
        test->ice_getConnection()->close(false);
        adapters.push_back(com->createObjectAdapter("Adapter35", endpoints[1]->toString()));
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter35"; i++);
        test(i == nRetry);
        test->ice_getConnection()->close(false);
        adapters.push_back(com->createObjectAdapter("Adapter34", endpoints[0]->toString()));
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter34"; i++);
        test(i == nRetry);
        
        deactivate(com, adapters);
    }
    tprintf("ok\n");
    
    tprintf("testing per request binding with single endpoint... ");
    {
        RemoteObjectAdapterPrx adapter = com->createObjectAdapter("Adapter41", "default");
        
        TestIntfPrx test1 = TestIntfPrx::uncheckedCast(adapter->getTestIntf()->ice_connectionCached(false));
        TestIntfPrx test2 = TestIntfPrx::uncheckedCast(adapter->getTestIntf()->ice_connectionCached(false));
        test(!test1->ice_isConnectionCached());
        test(!test2->ice_isConnectionCached());
        test(test1->ice_getConnection() == test2->ice_getConnection());
        
        test1->ice_ping();
        
        com->deactivateObjectAdapter(adapter);
        
        TestIntfPrx test3 = TestIntfPrx::uncheckedCast(test1);
        try
        {
            test(test3->ice_getConnection() == test1->ice_getConnection());
            test(false);
        }
        catch(const Ice::ConnectionRefusedException&)
        {
        }
    }
    tprintf("ok\n");
    
    tprintf("testing per request binding with multiple endpoints... ");
    {
        vector<RemoteObjectAdapterPrx> adapters;
        adapters.push_back(com->createObjectAdapter("Adapter51", "default"));
        adapters.push_back(com->createObjectAdapter("Adapter52", "default"));
        adapters.push_back(com->createObjectAdapter("Adapter53", "default"));
        
        TestIntfPrx test = TestIntfPrx::uncheckedCast(createTestIntfPrx(adapters)->ice_connectionCached(false));
        test(!test->ice_isConnectionCached());
        
        set<string> names;
        names.insert("Adapter51");
        names.insert("Adapter52");
        names.insert("Adapter53");
        while(!names.empty())
        {
            names.erase(test->getAdapterName());
        }
        
        com->deactivateObjectAdapter(adapters[0]);
        
        names.insert("Adapter52");
        names.insert("Adapter53");
        while(!names.empty())
        {
            names.erase(test->getAdapterName());
        }
        
        com->deactivateObjectAdapter(adapters[2]);
        
        test(test->getAdapterName() == "Adapter52");
        
        deactivate(com, adapters);
    }
    tprintf("ok\n");
    
    tprintf("testing per request binding with multiple endpoints and AMI... ");
    {
        vector<RemoteObjectAdapterPrx> adapters;
        adapters.push_back(com->createObjectAdapter("AdapterAMI51", "default"));
        adapters.push_back(com->createObjectAdapter("AdapterAMI52", "default"));
        adapters.push_back(com->createObjectAdapter("AdapterAMI53", "default"));
        
        TestIntfPrx test = TestIntfPrx::uncheckedCast(createTestIntfPrx(adapters)->ice_connectionCached(false));
        test(!test->ice_isConnectionCached());
        
        set<string> names;
        names.insert("AdapterAMI51");
        names.insert("AdapterAMI52");
        names.insert("AdapterAMI53");
        while(!names.empty())
        {
            names.erase(getAdapterNameWithAMI(test));
        }
        
        com->deactivateObjectAdapter(adapters[0]);
        
        names.insert("AdapterAMI52");
        names.insert("AdapterAMI53");
        while(!names.empty())
        {
            names.erase(getAdapterNameWithAMI(test));
        }
        
        com->deactivateObjectAdapter(adapters[2]);
        
        test(test->getAdapterName() == "AdapterAMI52");
        
        deactivate(com, adapters);
    }
    tprintf("ok\n");
    
    tprintf("testing per request binding and ordered endpoint selection... ");
    {
        vector<RemoteObjectAdapterPrx> adapters;
        adapters.push_back(com->createObjectAdapter("Adapter61", "default"));
        adapters.push_back(com->createObjectAdapter("Adapter62", "default"));
        adapters.push_back(com->createObjectAdapter("Adapter63", "default"));
        
        TestIntfPrx test = createTestIntfPrx(adapters);
        test = TestIntfPrx::uncheckedCast(test->ice_endpointSelection(Ice::Ordered));
        test(test->ice_getEndpointSelection() == Ice::Ordered);
        test = TestIntfPrx::uncheckedCast(test->ice_connectionCached(false));
        test(!test->ice_isConnectionCached());
        const int nRetry = 5;
        int i;
        
        //
        // Ensure that endpoints are tried in order by deactiving the adapters
        // one after the other.
        //
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter61"; i++);
        test(i == nRetry);
        com->deactivateObjectAdapter(adapters[0]);
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter62"; i++);
        test(i == nRetry);
        com->deactivateObjectAdapter(adapters[1]);
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter63"; i++);
        test(i == nRetry);
        com->deactivateObjectAdapter(adapters[2]);
        
        try
        {
            test->getAdapterName();
        }
        catch(const Ice::ConnectionRefusedException&)
        {
        }
        
        Ice::EndpointSeq endpoints = test->ice_getEndpoints();
        
        adapters.clear();
        
        //
        // Now, re-activate the adapters with the same endpoints in the opposite
        // order.
        // 
        adapters.push_back(com->createObjectAdapter("Adapter66", endpoints[2]->toString()));
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter66"; i++);
        test(i == nRetry);
        adapters.push_back(com->createObjectAdapter("Adapter65", endpoints[1]->toString()));
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter65"; i++);
        test(i == nRetry);
        adapters.push_back(com->createObjectAdapter("Adapter64", endpoints[0]->toString()));
        for(i = 0; i < nRetry && test->getAdapterName() == "Adapter64"; i++);
        test(i == nRetry);
        
        deactivate(com, adapters);
    }
    tprintf("ok\n");
    
    tprintf("testing per request binding and ordered endpoint selection and AMI... ");
    {
        vector<RemoteObjectAdapterPrx> adapters;
        adapters.push_back(com->createObjectAdapter("AdapterAMI61", "default"));
        adapters.push_back(com->createObjectAdapter("AdapterAMI62", "default"));
        adapters.push_back(com->createObjectAdapter("AdapterAMI63", "default"));
        
        TestIntfPrx test = createTestIntfPrx(adapters);
        test = TestIntfPrx::uncheckedCast(test->ice_endpointSelection(Ice::Ordered));
        test(test->ice_getEndpointSelection() == Ice::Ordered);
        test = TestIntfPrx::uncheckedCast(test->ice_connectionCached(false));
        test(!test->ice_isConnectionCached());
        const int nRetry = 5;
        int i;
        
        //
        // Ensure that endpoints are tried in order by deactiving the adapters
        // one after the other.
        //
        for(i = 0; i < nRetry && getAdapterNameWithAMI(test) == "AdapterAMI61"; i++);
        test(i == nRetry);
        com->deactivateObjectAdapter(adapters[0]);
        for(i = 0; i < nRetry && getAdapterNameWithAMI(test) == "AdapterAMI62"; i++);
        test(i == nRetry);
        com->deactivateObjectAdapter(adapters[1]);
        for(i = 0; i < nRetry && getAdapterNameWithAMI(test) == "AdapterAMI63"; i++);
        test(i == nRetry);
        com->deactivateObjectAdapter(adapters[2]);
        
        try
        {
            test->getAdapterName();
        }
        catch(const Ice::ConnectionRefusedException&)
        {
        }
        
        Ice::EndpointSeq endpoints = test->ice_getEndpoints();
        
        adapters.clear();
        
        //
        // Now, re-activate the adapters with the same endpoints in the opposite
        // order.
        // 
        adapters.push_back(com->createObjectAdapter("AdapterAMI66", endpoints[2]->toString()));
        for(i = 0; i < nRetry && getAdapterNameWithAMI(test) == "AdapterAMI66"; i++);
        test(i == nRetry);
        adapters.push_back(com->createObjectAdapter("AdapterAMI65", endpoints[1]->toString()));
        for(i = 0; i < nRetry && getAdapterNameWithAMI(test) == "AdapterAMI65"; i++);
        test(i == nRetry);
        adapters.push_back(com->createObjectAdapter("AdapterAMI64", endpoints[0]->toString()));
        for(i = 0; i < nRetry && getAdapterNameWithAMI(test) == "AdapterAMI64"; i++);
        test(i == nRetry);
        
        deactivate(com, adapters);
    }
    tprintf("ok\n");
    
    tprintf("testing endpoint mode filtering... ");
    {
        vector<RemoteObjectAdapterPrx> adapters;
        adapters.push_back(com->createObjectAdapter("Adapter71", "default"));
        adapters.push_back(com->createObjectAdapter("Adapter72", "udp"));
        
        TestIntfPrx test = createTestIntfPrx(adapters);
        test(test->getAdapterName() == "Adapter71");
        
        TestIntfPrx testUDP = TestIntfPrx::uncheckedCast(test->ice_datagram());
        test(test->ice_getConnection() != testUDP->ice_getConnection());
        try
        {
            testUDP->getAdapterName();
        }
        catch(const Ice::TwowayOnlyException&)
        {
        }
    }
    tprintf("ok\n");
    
    com->shutdown();
}
