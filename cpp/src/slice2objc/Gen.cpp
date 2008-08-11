// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <IceUtil/DisableWarnings.h>
#include <IceUtil/Functional.h>
#include <Gen.h>
#include <limits>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#endif
#include <IceUtil/Iterator.h>
#include <IceUtil/UUID.h>
#include <Slice/Checksum.h>
#include <Slice/DotNetNames.h>
#include <Slice/SignalHandler.h>
#include <Slice/Util.h>

using namespace std;
using namespace Slice;

//
// Don't use "using namespace IceUtil", or VC++ 6.0 complains about
// ambigious symbols for constructs like
// "IceUtil::constMemFun(&Slice::Exception::isLocal)".
//
using IceUtilInternal::Output;
using IceUtilInternal::nl;
using IceUtilInternal::sp;
using IceUtilInternal::sb;
using IceUtilInternal::eb;
using IceUtilInternal::spar;
using IceUtilInternal::epar;

//
// Callback for Crtl-C signal handling
//
static Gen* _gen = 0;

static void closeCallback()
{
    if(_gen != 0)
    {
        _gen->closeOutput();
    }
}

static string // Should be an anonymous namespace, but VC++ 6 can't handle that.
sliceModeToIceMode(Operation::Mode opMode)
{
    string mode;
    switch(opMode)
    {
        case Operation::Normal:
        {
            mode = "Ice.OperationMode.Normal";
            break;
        }
        case Operation::Nonmutating:
        {
            mode = "Ice.OperationMode.Nonmutating";
            break;
        }
        case Operation::Idempotent:
        {
            mode = "Ice.OperationMode.Idempotent";
            break;
        }
        default:
        {
            assert(false);
            break;
        }
    }
    return mode;
}

static void
emitDeprecate(const ContainedPtr& p1, const ContainedPtr& p2, Output& out, const string& type)
{
    string deprecateMetadata;
    if(p1->findMetaData("deprecate", deprecateMetadata) || 
       (p2 != 0 && p2->findMetaData("deprecate", deprecateMetadata)))
    {
        string deprecateReason = "This " + type + " has been deprecated.";
        if(deprecateMetadata.find("deprecate:") == 0 && deprecateMetadata.size() > 10)
        {
            deprecateReason = deprecateMetadata.substr(10);
        }
        out << nl << "[System.Obsolete(\"" << deprecateReason << "\")]";
    }
}

Slice::ObjCVisitor::ObjCVisitor(Output& h, Output& m) : _H(h), _M(m)
{
}

Slice::ObjCVisitor::~ObjCVisitor()
{
}

void
Slice::ObjCVisitor::writeInheritedOperations(const ClassDefPtr& p)
{
    ClassList bases = p->bases();
    if(!bases.empty() && !bases.front()->isInterface())
    {
        bases.pop_front();
    }
    if(!bases.empty())
    {
        _M << sp << nl << "#region Inherited Slice operations";

        OperationList allOps;
        for(ClassList::const_iterator q = bases.begin(); q != bases.end(); ++q)
        {
            OperationList tmp = (*q)->allOperations();
            allOps.splice(allOps.end(), tmp);
        }
        allOps.sort();
        allOps.unique();
        for(OperationList::const_iterator op = allOps.begin(); op != allOps.end(); ++op)
        {
            ClassDefPtr containingClass = ClassDefPtr::dynamicCast((*op)->container());
            bool amd = containingClass->hasMetaData("amd") || (*op)->hasMetaData("amd");
            string name = fixId((*op)->name(), DotNet::ICloneable, true);
            if(!amd)
            {
                vector<string> params;// = getParams(*op);
                vector<string> args = getArgs(*op);
                string retS = typeToString((*op)->returnType());

                _M << sp << nl << "public " << retS << ' ' << name << spar << params << epar;
                _M << sb;
                _M << nl;
                if((*op)->returnType())
                {
                    _M << "return ";
                }
                _M << name << spar << args << "Ice.ObjectImpl.defaultCurrent" << epar << ';';
                _M << eb;

                _M << sp << nl << "public abstract " << retS << ' ' << name << spar << params;
                if(!containingClass->isLocal())
                {
                    _M << "Ice.Current current__";
                }
                _M << epar << ';';
            }
            else
            {
                vector<string> params = getParamsAsync(*op, true);
                vector<string> args = getArgsAsync(*op);

                _M << sp << nl << "public void " << name << "_async" << spar << params << epar;
                _M << sb;
                _M << nl << name << "_async" << spar << args << epar << ';';
                _M << eb;

                _M << sp << nl << "public abstract void " << name << "_async"
                     << spar << params << "Ice.Current current__" << epar << ';';
            }
        }

        _M << sp << nl << "#endregion"; // Inherited Slice operations
    }
}

void
Slice::ObjCVisitor::writeDispatchAndMarshalling(const ClassDefPtr& p, bool stream)
{
#if 0
    string name = fixId(p->name());
    string scoped = p->scoped();
    ClassList allBases = p->allBases();
    StringList ids;

#if defined(__IBMCPP__) && defined(NDEBUG)
    //
    // VisualAge C++ 6.0 does not see that ClassDef is a Contained,
    // when inlining is on. The code below issues a warning: better
    // than an error!
    //
    transform(allBases.begin(), allBases.end(), back_inserter(ids), ::IceUtil::constMemFun<string,ClassDef>(&Contained::scoped));
#else
    transform(allBases.begin(), allBases.end(), back_inserter(ids), ::IceUtil::constMemFun(&Contained::scoped));
#endif

    StringList other;
    other.push_back(p->scoped());
    other.push_back("::Ice::Object");
    other.sort();
    ids.merge(other);
    ids.unique();

    StringList::const_iterator firstIter = ids.begin();
    StringList::const_iterator scopedIter = find(ids.begin(), ids.end(), scoped);
    assert(scopedIter != ids.end());
    StringList::difference_type scopedPos = IceUtilInternal::distance(firstIter, scopedIter);
    
    _M << sp << nl << "#region Slice type-related members";

    _M << sp << nl << "public static new string[] ids__ = ";
    _M << sb;

    {
        StringList::const_iterator q = ids.begin();
        while(q != ids.end())
        {
            _M << nl << '"' << *q << '"';
            if(++q != ids.end())
            {
                _M << ',';
            }
        }
    }
    _M << eb << ";";

    _M << sp << nl << "public override bool ice_isA(string s)";
    _M << sb;
    _M << nl << "return _System.Array.BinarySearch(ids__, s, IceUtilInternal.StringUtil.OrdinalStringComparer) >= 0;";
    _M << eb;

    _M << sp << nl << "public override bool ice_isA(string s, Ice.Current current__)";
    _M << sb;
    _M << nl << "return _System.Array.BinarySearch(ids__, s, IceUtilInternal.StringUtil.OrdinalStringComparer) >= 0;";
    _M << eb;

    _M << sp << nl << "public override string[] ice_ids()";
    _M << sb;
    _M << nl << "return ids__;";
    _M << eb;

    _M << sp << nl << "public override string[] ice_ids(Ice.Current current__)";
    _M << sb;
    _M << nl << "return ids__;";
    _M << eb;

    _M << sp << nl << "public override string ice_id()";
    _M << sb;
    _M << nl << "return ids__[" << scopedPos << "];";
    _M << eb;

    _M << sp << nl << "public override string ice_id(Ice.Current current__)";
    _M << sb;
    _M << nl << "return ids__[" << scopedPos << "];";
    _M << eb;

    _M << sp << nl << "public static new string ice_staticId()";
    _M << sb;
    _M << nl << "return ids__[" << scopedPos << "];";
    _M << eb;

    _M << sp << nl << "#endregion"; // Slice type-related members

    OperationList ops = p->operations();
    if(!p->isInterface() || ops.size() != 0)
    {
        _M << sp << nl << "#region Operation dispatch";
    }

    OperationList::const_iterator r;
    for(r = ops.begin(); r != ops.end(); ++r)
    {
        OperationPtr op = *r;
        ContainerPtr container = op->container();
        ClassDefPtr cl = ClassDefPtr::dynamicCast(container);
        assert(cl);

        string opName = op->name();
        _M << sp << nl << "public static Ice.DispatchStatus " << opName << "___(" << name
             << " obj__, IceInternal.Incoming inS__, Ice.Current current__)";
        _M << sb;

        bool amd = p->hasMetaData("amd") || op->hasMetaData("amd");
        if(!amd)
        {
            TypePtr ret = op->returnType();
            
            TypeStringList inParams;
            TypeStringList outParams;
            ParamDeclList paramList = op->parameters();
            for(ParamDeclList::const_iterator pli = paramList.begin(); pli != paramList.end(); ++pli)
            {
                if((*pli)->isOutParam())
                {
                    outParams.push_back(make_pair((*pli)->type(), (*pli)->name()));
                }
                else
                {
                    inParams.push_back(make_pair((*pli)->type(), (*pli)->name()));
                }
            }
            
            ExceptionList throws = op->throws();
            throws.sort();
            throws.unique();

            //
            // Arrange exceptions into most-derived to least-derived order. If we don't
            // do this, a base exception handler can appear before a derived exception
            // handler, causing compiler warnings and resulting in the base exception
            // being marshaled instead of the derived exception.
            //
#if defined(__SUNPRO_CC)
            throws.sort(Slice::derivedToBaseCompare);
#else
            throws.sort(Slice::DerivedToBaseCompare());
#endif

            TypeStringList::const_iterator q;
            
            _M << nl << "checkMode__(" << sliceModeToIceMode(op->mode()) << ", current__.mode);";
            if(!inParams.empty())
            {
                //
                // Unmarshal 'in' parameters.
                //
                _M << nl << "IceInternal.BasicStream is__ = inS__.istr();";
                _M << nl << "is__.startReadEncaps();";
                for(q = inParams.begin(); q != inParams.end(); ++q)
                {
                    string param = fixId(q->second);
                    string typeS = typeToString(q->first);
                    BuiltinPtr builtin = BuiltinPtr::dynamicCast(q->first);
                    bool isClass = (builtin && builtin->kind() == Builtin::KindObject)
                        || ClassDeclPtr::dynamicCast(q->first);
                    if(!isClass)
                    {
                        _M << nl << typeS << ' ' << param << ';';
                        StructPtr st = StructPtr::dynamicCast(q->first);
                        if(st)
                        {
                            if(isValueType(q->first))
                            {
                                _M << nl << param << " = new " << typeS << "();";
                            }
                            else
                            {
                                _M << nl << param << " = null;";
                            }
                        }
                    }
                    writeMarshalUnmarshalCode(_M, q->first, param, false, false, true);
                }
                if(op->sendsClasses())
                {
                    _M << nl << "is__.readPendingObjects();";
                }
                _M << nl << "is__.endReadEncaps();";
            }
            else
            {
                _M << nl << "inS__.istr().skipEmptyEncaps();";
            }

            for(q = outParams.begin(); q != outParams.end(); ++q)
            {
                string typeS = typeToString(q->first);
                _M << nl << typeS << ' ' << fixId(q->second) << ";";
            }

            if(!outParams.empty() || ret || !throws.empty())
            {
                _M << nl << "IceInternal.BasicStream os__ = inS__.ostr();";
            }
            
            //
            // Call on the servant.
            //
            if(!throws.empty())
            {
                _M << nl << "try";
                _M << sb;
            }
            _M << nl;
            if(ret)
            {
                string retS = typeToString(ret);
                _M << retS << " ret__ = ";
            }
            _M << "obj__." << fixId(opName, DotNet::ICloneable, true) << spar;
            for(q = inParams.begin(); q != inParams.end(); ++q)
            {
                BuiltinPtr builtin = BuiltinPtr::dynamicCast(q->first);
                bool isClass = (builtin && builtin->kind() == Builtin::KindObject)
                               || ClassDeclPtr::dynamicCast(q->first);
                
                string arg;
                if(isClass)
                {
                    arg += "(" + typeToString(q->first) + ")";
                }
                arg += fixId(q->second);
                if(isClass)
                {
                    arg += "_PP.value";
                }
                _M << arg;
            }
            for(q = outParams.begin(); q != outParams.end(); ++q)
            {
                _M << "out " + fixId(q->second);
            }
            _M << "current__" << epar << ';';
            
            //
            // Marshal 'out' parameters and return value.
            //
            for(q = outParams.begin(); q != outParams.end(); ++q)
            {
                writeMarshalUnmarshalCode(_M, q->first, fixId(q->second), true, false, true, "");
            }
            if(ret)
            {
                writeMarshalUnmarshalCode(_M, ret, "ret__", true, false, true, "");
            }
            if(op->returnsClasses())
            {
                _M << nl << "os__.writePendingObjects();";
            }
            _M << nl << "return Ice.DispatchStatus.DispatchOK;";
            
            //
            // Handle user exceptions.
            //
            if(!throws.empty())
            {
                _M << eb;
                ExceptionList::const_iterator t;
                for(t = throws.begin(); t != throws.end(); ++t)
                {
                    string exS = fixId((*t)->scoped());
                    _M << nl << "catch(" << exS << " ex)";
                    _M << sb;
                    _M << nl << "os__.writeUserException(ex);";
                    _M << nl << "return Ice.DispatchStatus.DispatchUserException;";
                    _M << eb;
                }
            }

            _M << eb;
        }
        else
        {
            TypeStringList inParams;
            ParamDeclList paramList = op->parameters();
            for(ParamDeclList::const_iterator pli = paramList.begin(); pli != paramList.end(); ++pli)
            {
                if(!(*pli)->isOutParam())
                {
                    inParams.push_back(make_pair((*pli)->type(), (*pli)->name()));
                }
            }
            
            TypeStringList::const_iterator q;
            _M << nl << "checkMode__(" << sliceModeToIceMode(op->mode()) << ", current__.mode);";
    
            if(!inParams.empty())
            {
                //
                // Unmarshal 'in' parameters.
                //
                _M << nl << "IceInternal.BasicStream is__ = inS__.istr();";
                _M << nl << "is__.startReadEncaps();";
                for(q = inParams.begin(); q != inParams.end(); ++q)
                {
                    string param = fixId(q->second);
                    string typeS = typeToString(q->first);
                    BuiltinPtr builtin = BuiltinPtr::dynamicCast(q->first);
                    bool isClass = (builtin && builtin->kind() == Builtin::KindObject)
                        || ClassDeclPtr::dynamicCast(q->first);
                    if(!isClass)
                    {
                        _M << nl << typeS << ' ' << param << ';';
                        StructPtr st = StructPtr::dynamicCast(q->first);
                        if(st)
                        {
                            if(isValueType(q->first))
                            {
                                _M << nl << param << " = new " << typeS << "();";
                            }
                            else
                            {
                                _M << nl << param << " = null;";
                            }
                        }
                    }
                    writeMarshalUnmarshalCode(_M, q->first, fixId(q->second), false, false, true);
                }
                if(op->sendsClasses())
                {
                    _M << nl << "is__.readPendingObjects();";
                }
                _M << nl << "is__.endReadEncaps();";
            }
            else
            {
                _M << nl << "inS__.istr().skipEmptyEncaps();";
            }

            //
            // Call on the servant.
            //
            string classNameAMD = "AMD_" + p->name();
            _M << nl << classNameAMD << '_' << opName << " cb__ = new _" << classNameAMD << '_' << opName
                 << "(inS__);";
            _M << nl << "try";
            _M << sb;
            _M << nl << "obj__.";
            if(amd)
            {
                _M << opName << "_async";
            }
            else
            {
                _M << fixId(opName, DotNet::ICloneable, true);
            }
            _M << spar;
            if(amd)
            {
                _M << "cb__";
            }
            for(q = inParams.begin(); q != inParams.end(); ++q)
            {
                BuiltinPtr builtin = BuiltinPtr::dynamicCast(q->first);
                bool isClass = (builtin && builtin->kind() == Builtin::KindObject)
                               || ClassDeclPtr::dynamicCast(q->first);
                string arg;
                if(isClass)
                {
                    arg += "(" + typeToString(q->first) + ")";
                }
                arg += fixId(q->second);
                if(isClass)
                {
                    arg += "_PP.value";
                }
                _M << arg;
            }
            _M << "current__" << epar << ';';
            _M << eb;
            _M << nl << "catch(_System.Exception ex)";
            _M << sb;
            _M << nl << "cb__.ice_exception(ex);";
            _M << eb;
            _M << nl << "return Ice.DispatchStatus.DispatchAsync;";

            _M << eb;
        }
    }

    OperationList allOps = p->allOperations();
    if(!allOps.empty())
    {
        StringList allOpNames;
#if defined(__IBMCPP__) && defined(NDEBUG)
        //
        // See comment for transform above
        //
        transform(allOps.begin(), allOps.end(), back_inserter(allOpNames), ::IceUtil::constMemFun<string,Operation>(&Contained::name));
#else
        transform(allOps.begin(), allOps.end(), back_inserter(allOpNames), ::IceUtil::constMemFun(&Contained::name));
#endif
        allOpNames.push_back("ice_id");
        allOpNames.push_back("ice_ids");
        allOpNames.push_back("ice_isA");
        allOpNames.push_back("ice_ping");
        allOpNames.sort();
        allOpNames.unique();

        StringList::const_iterator q;

        _M << sp << nl << "private static string[] all__ =";
        _M << sb;
        q = allOpNames.begin();
        while(q != allOpNames.end())
        {
            _M << nl << '"' << *q << '"';
            if(++q != allOpNames.end())
            {
                _M << ',';
            }
        }
        _M << eb << ';';

        _M << sp << nl << "public override Ice.DispatchStatus "
             << "dispatch__(IceInternal.Incoming inS__, Ice.Current current__)";
        _M << sb;
        _M << nl << "int pos = _System.Array.BinarySearch(all__, current__.operation, "
             << "IceUtilInternal.StringUtil.OrdinalStringComparer);";
        _M << nl << "if(pos < 0)";
        _M << sb;
        _M << nl << "throw new Ice.OperationNotExistException(current__.id, current__.facet, current__.operation);";
        _M << eb;
        _M << sp << nl << "switch(pos)";
        _M << sb;
        int i = 0;
        for(q = allOpNames.begin(); q != allOpNames.end(); ++q)
        {
            string opName = *q;

            _M << nl << "case " << i++ << ':';
            _M << sb;
            if(opName == "ice_id")
            {
                _M << nl << "return ice_id___(this, inS__, current__);";
            }
            else if(opName == "ice_ids")
            {
                _M << nl << "return ice_ids___(this, inS__, current__);";
            }
            else if(opName == "ice_isA")
            {
                _M << nl << "return ice_isA___(this, inS__, current__);";
            }
            else if(opName == "ice_ping")
            {
                _M << nl << "return ice_ping___(this, inS__, current__);";
            }
            else
            {
                //
                // There's probably a better way to do this
                //
                for(OperationList::const_iterator t = allOps.begin(); t != allOps.end(); ++t)
                {
                    if((*t)->name() == (*q))
                    {
                        ContainerPtr container = (*t)->container();
                        ClassDefPtr cl = ClassDefPtr::dynamicCast(container);
                        assert(cl);
                        if(cl->scoped() == p->scoped())
                        {
                            _M << nl << "return " << opName << "___(this, inS__, current__);";
                        }
                        else
                        {
                            string base = cl->scoped();
                            if(cl->isInterface())
                            {
                                base += "Disp_";
                            }
                            _M << nl << "return " << fixId(base) << "." << opName << "___(this, inS__, current__);";
                        }
                        break;
                    }
                }
            }
            _M << eb;
        }
        _M << eb;
        _M << sp << nl << "_System.Diagnostics.Debug.Assert(false);";
        _M << nl << "throw new Ice.OperationNotExistException(current__.id, current__.facet, current__.operation);";
        _M << eb;
    }

    if(!p->isInterface() || ops.size() != 0)
    {
        _M << sp << nl << "#endregion"; // Operation dispatch
    }


    // Marshalling support
    DataMemberList allClassMembers = p->allClassDataMembers();
    DataMemberList::const_iterator d;
    DataMemberList members = p->dataMembers();
    DataMemberList classMembers = p->classDataMembers();
    ClassList bases = p->bases();
    bool hasBaseClass = !bases.empty() && !bases.front()->isInterface();

    _M << sp << nl << "#region Marshaling support";

    _M << sp << nl << "public override void write__(IceInternal.BasicStream os__)";
    _M << sb;
    _M << nl << "os__.writeTypeId(ice_staticId());";
    _M << nl << "os__.startWriteSlice();";
    for(d = members.begin(); d != members.end(); ++d)
    {
        writeMarshalUnmarshalCode(_M, (*d)->type(), fixId(*d, DotNet::ICloneable, true),
                                  true, false, false);
    }
    _M << nl << "os__.endWriteSlice();";
    _M << nl << "base.write__(os__);";
    _M << eb;

    if(allClassMembers.size() != 0)
    {
        _M << sp << nl << "public sealed ";
        if(hasBaseClass && bases.front()->declaration()->usesClasses())
        {
            _M << "new ";
        }
        _M << "class Patcher__ : IceInternal.Patcher<" << name << ">";
        _M << sb;
        _M << sp << nl << "internal Patcher__(string type, Ice.ObjectImpl instance";
        if(allClassMembers.size() > 1)
        {
            _M << ", int member";
        }
        _M << ") : base(type)";
        _M << sb;
        _M << nl << "_instance = (" << name << ")instance;";
        if(allClassMembers.size() > 1)
        {
            _M << nl << "_member = member;";
        }
        _M << eb;

        _M << sp << nl << "public override void patch(Ice.Object v)";
        _M << sb;
        _M << nl << "try";
        _M << sb;
        if(allClassMembers.size() > 1)
        {
            _M << nl << "switch(_member)";
            _M << sb;
        }
        int memberCount = 0;
        for(d = allClassMembers.begin(); d != allClassMembers.end(); ++d)
        {
            if(allClassMembers.size() > 1)
            {
                _M.dec();
                _M << nl << "case " << memberCount << ":";
                _M.inc();
            }
            string memberName = fixId((*d)->name(), DotNet::ICloneable, true);
            string memberType = typeToString((*d)->type());
            _M << nl << "_instance." << memberName << " = (" << memberType << ")v;";
            ContainedPtr contained = ContainedPtr::dynamicCast((*d)->type());
            string sliceId = contained ? contained->scoped() : string("::Ice::Object");
            _M << nl << "_typeId = \"" << sliceId << "\";";
            if(allClassMembers.size() > 1)
            {
                _M << nl << "break;";
            }
            memberCount++;
        }
        if(allClassMembers.size() > 1)
        {
            _M << eb;
        }
        _M << eb;
        _M << nl << "catch(System.InvalidCastException)";
        _M << sb;
        _M << nl << "IceInternal.Ex.throwUOE(_typeId, v.ice_id());";
        _M << eb;
        _M << eb;

        _M << sp << nl << "private " << name << " _instance;";
        if(allClassMembers.size() > 1)
        {
            _M << nl << "private int _member;";
        }
        _M << nl << "private string _typeId;";
        _M << eb;
    }

    _M << sp << nl << "public override void read__(IceInternal.BasicStream is__, bool rid__)";
    _M << sb;
    _M << nl << "if(rid__)";
    _M << sb;
    _M << nl << "/* string myId = */ is__.readTypeId();";
    _M << eb;
    _M << nl << "is__.startReadSlice();";
    int classMemberCount = static_cast<int>(allClassMembers.size() - classMembers.size());
    for(d = members.begin(); d != members.end(); ++d)
    {
        ostringstream patchParams;
        patchParams << "this";
        BuiltinPtr builtin = BuiltinPtr::dynamicCast((*d)->type());
        if((builtin && builtin->kind() == Builtin::KindObject) || ClassDeclPtr::dynamicCast((*d)->type()))
        {
            if(classMembers.size() > 1 || allClassMembers.size() > 1)
            {
                patchParams << ", " << classMemberCount++;
            }
        }
        writeMarshalUnmarshalCode(_M, (*d)->type(), fixId(*d, DotNet::ICloneable, true),
                                  false, false, false, patchParams.str());
    }
    _M << nl << "is__.endReadSlice();";
    _M << nl << "base.read__(is__, true);";
    _M << eb;

    //
    // Write streaming API.
    //
    if(stream)
    {
        _M << sp << nl << "public override void write__(Ice.OutputStream outS__)";
        _M << sb;
        _M << nl << "outS__.writeTypeId(ice_staticId());";
        _M << nl << "outS__.startSlice();";
        for(d = members.begin(); d != members.end(); ++d)
        {
            writeMarshalUnmarshalCode(_M, (*d)->type(), fixId(*d, DotNet::ICloneable, true),
                                      true, true, false);
        }
        _M << nl << "outS__.endSlice();";
        _M << nl << "base.write__(outS__);";
        _M << eb;

        _M << sp << nl << "public override void read__(Ice.InputStream inS__, bool rid__)";
        _M << sb;
        _M << nl << "if(rid__)";
        _M << sb;
        _M << nl << "/* string myId = */ inS__.readTypeId();";
        _M << eb;
        _M << nl << "inS__.startSlice();";
        for(d = members.begin(); d != members.end(); ++d)
        {
            ostringstream patchParams;
            patchParams << "this";
            BuiltinPtr builtin = BuiltinPtr::dynamicCast((*d)->type());
            if((builtin && builtin->kind() == Builtin::KindObject) || ClassDeclPtr::dynamicCast((*d)->type()))
            {
                if(classMembers.size() > 1 || allClassMembers.size() > 1)
                {
                    patchParams << ", " << classMemberCount++;
                }
            }
            writeMarshalUnmarshalCode(_M, (*d)->type(), fixId(*d, DotNet::ICloneable, true),
                                      false, true, false, patchParams.str());
        }
        _M << nl << "inS__.endSlice();";
        _M << nl << "base.read__(inS__, true);";
        _M << eb;
    }
    else
    {
        //
        // Emit placeholder functions to catch errors.
        //
        string scoped = p->scoped();
        _M << sp << nl << "public override void write__(Ice.OutputStream outS__)";
        _M << sb;
        _M << nl << "Ice.MarshalException ex = new Ice.MarshalException();";
        _M << nl << "ex.reason = \"type " << scoped.substr(2) << " was not generated with stream support\";";
        _M << nl << "throw ex;";
        _M << eb;

        _M << sp << nl << "public override void read__(Ice.InputStream inS__, bool rid__)";
        _M << sb;
        _M << nl << "Ice.MarshalException ex = new Ice.MarshalException();";
        _M << nl << "ex.reason = \"type " << scoped.substr(2) << " was not generated with stream support\";";
        _M << nl << "throw ex;";
        _M << eb;
    }

    _M << sp << nl << "#endregion"; // Marshalling support

#endif
}

#if 0
string
Slice::ObjCVisitor::getParamAttributes(const ParamDeclPtr& p)
{
    string result;
    StringList metaData = p->getMetaData();
    for(StringList::const_iterator i = metaData.begin(); i != metaData.end(); ++i)
    {
        static const string prefix = "cs:attribute:";
        if(i->find(prefix) == 0)
        {
            result += "[" + i->substr(prefix.size()) + "] ";
        }
    }
    return result;
}
#endif

string
Slice::ObjCVisitor::getParams(const OperationPtr& op)
{
    string result;
    ParamDeclList paramList = op->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
	TypePtr type = (*q)->type();
	string typeString = typeToString(type);
        string name = fixId((*q)->name());

	if(q != paramList.begin())
	{
	    result += " " + name;
	}
	result += ":(" + typeString;
	if(!isValueType(type))
	{
	    result += " *";
	}
        if((*q)->isOutParam())
	{
	    result += "*";
	}
	result += ")" + name;
    }
    return result;
}

vector<string>
Slice::ObjCVisitor::getParamsAsync(const OperationPtr& op, bool amd)
{
    vector<string> params;

    string name = fixId(op->name());
    ContainerPtr container = op->container();
    ClassDefPtr cl = ClassDefPtr::dynamicCast(container); // Get the class containing the op.
    string scope = fixId(cl->scope());
    params.push_back(scope + (amd ? "AMD_" : "AMI_") + cl->name() + '_' + op->name() + " cb__");

    ParamDeclList paramList = op->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        if(!(*q)->isOutParam())
        {
            //params.push_back(getParamAttributes(*q) + typeToString((*q)->type()) + " " + fixId((*q)->name()));
        }
    }
    return params;
}

vector<string>
Slice::ObjCVisitor::getParamsAsyncCB(const OperationPtr& op)
{
    vector<string> params;

    TypePtr ret = op->returnType();
    if(ret)
    {
        params.push_back(typeToString(ret) + " ret__");
    }

    ParamDeclList paramList = op->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        if((*q)->isOutParam())
        {
            //params.push_back(getParamAttributes(*q) + typeToString((*q)->type()) + ' ' + fixId((*q)->name()));
        }
    }

    return params;
}

vector<string>
Slice::ObjCVisitor::getArgs(const OperationPtr& op)
{
    vector<string> args;
    ParamDeclList paramList = op->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        string arg = fixId((*q)->name());
        if((*q)->isOutParam())
        {
            arg = "out " + arg;
        }
        args.push_back(arg);
    }
    return args;
}

vector<string>
Slice::ObjCVisitor::getArgsAsync(const OperationPtr& op)
{
    vector<string> args;

    args.push_back("cb__");

    ParamDeclList paramList = op->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        if(!(*q)->isOutParam())
        {
            args.push_back(fixId((*q)->name()));
        }
    }
    return args;
}

vector<string>
Slice::ObjCVisitor::getArgsAsyncCB(const OperationPtr& op)
{
    vector<string> args;

    TypePtr ret = op->returnType();
    if(ret)
    {
        args.push_back("ret__");
    }

    ParamDeclList paramList = op->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        if((*q)->isOutParam())
        {
            args.push_back(fixId((*q)->name()));
        }
    }

    return args;
}

void
Slice::ObjCVisitor::emitAttributes(const ContainedPtr& p)
{
    StringList metaData = p->getMetaData();
    for(StringList::const_iterator i = metaData.begin(); i != metaData.end(); ++i)
    {
        static const string prefix = "cs:attribute:";
        if(i->find(prefix) == 0)
        {
            _M << nl << '[' << i->substr(prefix.size()) << ']';
        }
    }
}

string
Slice::ObjCVisitor::writeValue(const TypePtr& type)
{
    assert(type);

    BuiltinPtr builtin = BuiltinPtr::dynamicCast(type);
    if(builtin)
    {
        switch(builtin->kind())
        {
            case Builtin::KindBool:
            {
                return "false";
                break;
            }
            case Builtin::KindByte:
            case Builtin::KindShort:
            case Builtin::KindInt:
            case Builtin::KindLong:
            {
                return "0";
                break;
            }
            case Builtin::KindFloat:
            {
                return "0.0f";
                break;
            }
            case Builtin::KindDouble:
            {
                return "0.0";
                break;
            }
            default:
            {
                return "null";
                break;
            }
        }
    }

    EnumPtr en = EnumPtr::dynamicCast(type);
    if(en)
    {
        return fixId(en->scoped()) + "." + fixId((*en->getEnumerators().begin())->name());
    }

    StructPtr st = StructPtr::dynamicCast(type);
    if(st)
    {
        return st->hasMetaData("clr:class") ? string("null") : "new " + fixId(st->scoped()) + "()";
    }

    return "null";
}


Slice::Gen::Gen(const string& name, const string& base, const string& include, const vector<string>& includePaths,
                const string& dir, bool impl, bool implTie, bool stream)
    : _base(base),
      _include(include),
      _includePaths(includePaths),
      _impl(impl),
      _stream(stream)
{
    _gen = this;
    SignalHandler::setCallback(closeCallback);

    for(vector<string>::iterator p = _includePaths.begin(); p != _includePaths.end(); ++p)
    {
        *p = fullPath(*p);
    }

    string fileBase = base;
    string::size_type pos = base.find_last_of("/\\");
    if(pos != string::npos)
    {
        fileBase = base.substr(pos + 1);
    }
    string fileH = fileBase + ".h";
    string fileM = fileBase + ".m";
    string fileImplH = fileBase + "I.h";
    string fileImplM = fileBase + "I.m";

    if(!dir.empty())
    {
        fileH = dir + '/' + fileH;
        fileM = dir + '/' + fileM;
        fileImplH = dir + '/' + fileImplH;
        fileImplM = dir + '/' + fileImplM;
    }
    SignalHandler::addFile(fileH);
    SignalHandler::addFile(fileM);
    SignalHandler::addFile(fileImplH);
    SignalHandler::addFile(fileImplM);

    _H.open(fileH.c_str());
    if(!_H)
    {
        cerr << name << ": can't open `" << fileH << "' for writing" << endl;
        return;
    }
    printHeader(_H);
    _H << nl << "// Generated from file `" << fileBase << ".ice'";

    _H << sp << nl << "#import <IceObjC/Config.h>";
    _H << nl << "#import <IceObjC/Proxy.h>";

    _M.open(fileM.c_str());
    if(!_M)
    {
        cerr << name << ": can't open `" << fileM << "' for writing" << endl;
        return;
    }
    printHeader(_M);
    _M << nl << "// Generated from file `" << fileBase << ".ice'";

    if(impl || implTie)
    {
        struct stat st;
        if(stat(fileImplH.c_str(), &st) == 0)
        {
            cerr << name << ": `" << fileImplH << "' already exists--will not overwrite" << endl;
            return;
        }
        _implH.open(fileImplH.c_str());
        if(!_implH)
        {
            cerr << name << ": can't open `" << fileImplH << "' for writing" << endl;
            return;
        }

        if(stat(fileImplM.c_str(), &st) == 0)
        {
            cerr << name << ": `" << fileImplM << "' already exists--will not overwrite" << endl;
            return;
        }
        _implM.open(fileImplM.c_str());
        if(!_implM)
        {
            cerr << name << ": can't open `" << fileImplM << "' for writing" << endl;
            return;
        }
    }
}

Slice::Gen::~Gen()
{
    if(_H.isOpen())
    {
        _H << '\n';
        _M << '\n';
    }
    if(_implH.isOpen())
    {
        _implH << '\n';
        _implM << '\n';
    }

    SignalHandler::setCallback(0);
}

bool
Slice::Gen::operator!() const
{
    if(!_H || !_M)
    {
        return true;
    }
    if(_impl && (!_implH || !_implM))
    {
        return true;
    }
    return false;
}

void
Slice::Gen::generate(const UnitPtr& p)
{
    ObjCGenerator::validateMetaData(p);

    _M << nl << "\n#import <";
    if(!_include.empty())
    {
        _M << _include << "/";
    }
    _M << _base << ".h>";

    StringList includes = p->includeFiles();
    for(StringList::const_iterator q = includes.begin(); q != includes.end(); ++q)
    {
        _H << "\n#import <" << changeInclude(*q, _includePaths) << ".h>";
    }

    UnitVisitor unitVisitor(_H, _M, _stream);
    p->visit(&unitVisitor, false);

    TypesVisitor typesVisitor(_H, _M, _stream);
    p->visit(&typesVisitor, false);

    ProxyVisitor proxyVisitor(_H, _M);
    p->visit(&proxyVisitor, false);

#if 0
    OpsVisitor opsVisitor(_M);
    p->visit(&opsVisitor, false);

    HelperVisitor helperVisitor(_M, _stream);
    p->visit(&helperVisitor, false);

    DelegateVisitor delegateVisitor(_M);
    p->visit(&delegateVisitor, false);

    DelegateMVisitor delegateMVisitor(_M);
    p->visit(&delegateMVisitor, false);

    DelegateDVisitor delegateDVisitor(_M);
    p->visit(&delegateDVisitor, false);

    DispatcherVisitor dispatcherVisitor(_M, _stream);
    p->visit(&dispatcherVisitor, false);

    AsyncVisitor asyncVisitor(_M);
    p->visit(&asyncVisitor, false);
#endif
}

void
Slice::Gen::generateTie(const UnitPtr& p)
{
    TieVisitor tieVisitor(_M);
    p->visit(&tieVisitor, false);
}

void
Slice::Gen::generateImpl(const UnitPtr& p)
{
    //ImplVisitor implVisitor(_impl);
    //p->visit(&implVisitor, false);
}

void
Slice::Gen::generateImplTie(const UnitPtr& p)
{
    //ImplTieVisitor implTieVisitor(_impl);
    //p->visit(&implTieVisitor, false);
}

void
Slice::Gen::generateChecksums(const UnitPtr& u)
{
    ChecksumMap map = createChecksums(u);
    if(!map.empty())
    {
        string className = "X" + IceUtil::generateUUID();
        for(string::size_type pos = 1; pos < className.size(); ++pos)
        {
            if(!isalnum(className[pos]))
            {
                className[pos] = '_';
            }
        }

        _M << sp << nl << "namespace IceInternal";
        _M << sb;
        _M << nl << "namespace SliceChecksums";
        _M << sb;
        _M << nl << "public sealed class " << className;
        _M << sb;
        _M << nl << "public readonly static System.Collections.Hashtable map = new System.Collections.Hashtable();";
        _M << sp << nl << "static " << className << "()";
        _M << sb;
        for(ChecksumMap::const_iterator p = map.begin(); p != map.end(); ++p)
        {
            _M << nl << "map.Add(\"" << p->first << "\", \"";
            ostringstream str;
            str.flags(ios_base::hex);
            str.fill('0');
            for(vector<unsigned char>::const_iterator q = p->second.begin(); q != p->second.end(); ++q)
            {
                str << (int)(*q);
            }
            _M << str.str() << "\");";
        }
        _M << eb;
        _M << eb << ';';
        _M << eb;
        _M << eb;
    }
}

void
Slice::Gen::closeOutput()
{
    _H.close();
    _M.close();
    _implH.close();
    _implM.close();
}

void
Slice::Gen::printHeader(Output& o)
{
    static const char* header =
"// **********************************************************************\n"
"//\n"
"// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.\n"
"//\n"
"// This copy of Ice is licensed to you under the terms described in the\n"
"// ICE_LICENSE file included in this distribution.\n"
"//\n"
"// **********************************************************************\n"
        ;

    o << header;
    o << "\n// Ice version " << ICE_STRING_VERSION;
}

Slice::Gen::UnitVisitor::UnitVisitor(IceUtilInternal::Output& H, IceUtilInternal::Output& M, bool stream)
    : ObjCVisitor(H, M), _stream(stream), _globalMetaDataDone(false)
{
}

bool
Slice::Gen::UnitVisitor::visitModuleStart(const ModulePtr& p)
{
#if 0
    if(!_globalMetaDataDone)
    {
        DefinitionContextPtr dc = p->definitionContext();
        StringList globalMetaData = dc->getMetaData();

        static const string attributePrefix = "cs:attribute:";

        if(!globalMetaData.empty())
        {
            _M << sp;
        }
        for(StringList::const_iterator q = globalMetaData.begin(); q != globalMetaData.end(); ++q)
        {
            string::size_type pos = q->find(attributePrefix);
            if(pos == 0)
            {
                string attrib = q->substr(pos + attributePrefix.size());
                _M << nl << '[' << attrib << ']';
            }
        }
        _globalMetaDataDone = true; // Do this only once per source file.
    }
#endif
    return false;
}

Slice::Gen::TypesVisitor::TypesVisitor(IceUtilInternal::Output& H, IceUtilInternal::Output& M, bool stream)
    : ObjCVisitor(H, M), _stream(stream)
{
}

bool
Slice::Gen::TypesVisitor::visitModuleStart(const ModulePtr& p)
{
    string suffix;
    StringList names = splitScopedName(p->scoped());
    for(StringList::const_iterator i = names.begin(); i != names.end(); ++i)
    {
	if(i != names.begin())
	{
	    suffix += "_";
	}
        suffix += *i;
    }
    string symbol = "ICE_MODULE_PREFIX_";
    symbol += suffix;
    // TODO: generate checksum so we can catch mismatched module prefixes across different compilation units
    // that are compiled by separate invocations of slice2objc
    #if 0
    _H << nl << nl << "#if defined(" << symbol << ") && (" << symbol << " != " << moduleName(p) << ")";
    _H << nl << "#error inconsistent prefix metadata for Slice module " << p->scoped();
    _H << nl << "#else";
    _H << nl << "#define " << symbol << " " << moduleName(p);
    _H << nl << "#endif";
    #endif
    return true;
}

void
Slice::Gen::TypesVisitor::visitModuleEnd(const ModulePtr&)
{
}

bool
Slice::Gen::TypesVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    string name = p->name();
    ClassList bases = p->bases();
    bool hasBaseClass = !bases.empty() && !bases.front()->isInterface();

#if 0
    if(_stream)
    {
        _M << sp << nl << "public sealed class " << name << "Helper";
        _M << sb;

        _M << sp << nl << "public " << name << "Helper(Ice.InputStream inS__)";
        _M << sb;
        _M << nl << "_in = inS__;";
        _M << nl << "_pp = new IceInternal.ParamPatcher<" << scoped << ">(\"" << p->scoped() << "\");";
        _M << eb;

        _M << sp << nl << "public static void write(Ice.OutputStream outS__, " << fixId(name) << " v__)";
        _M << sb;
        _M << nl << "outS__.writeObject(v__);";
        _M << eb;

        _M << sp << nl << "public void read()";
        _M << sb;
        _M << nl << "_in.readObject(_pp);";
        _M << eb;

        _M << sp << nl << "public " << scoped << " value";
        _M << sb;
        _M << nl << "get";
        _M << sb;
        _M << nl << "return (" << scoped << ")_pp.value;";
        _M << eb;
        _M << eb;

        _M << sp << nl << "private Ice.InputStream _in;";
        _M << nl << "private IceInternal.ParamPatcher<" << scoped << "> _pp;";

        _M << eb;
    }
#endif

    _M << sp;
    if(p->isInterface())
    {
        if(!bases.empty())
        {
            ClassList::const_iterator q = bases.begin();
            while(q != bases.end())
            {
                _M << ", " << fixId((*q)->scoped());
                q++;
            }
        }
    }
    else
    {
        _M << nl << "public ";
        if(p->isAbstract())
        {
            _M << "abstract ";
        }
        _M << "class " << fixId(name);

        bool baseWritten = false;

        if(!hasBaseClass)
        {
            if(!p->isLocal())
            {
                _M << " : Ice.ObjectImpl";
                baseWritten = true;
            }
        }
        else
        {
            _M << " : " << fixId(bases.front()->scoped());
            baseWritten = true;
            bases.pop_front();
        }
        if(p->isAbstract())
        {
            if(baseWritten)
            {
                _M << ", ";
            }
            else
            {
                _M << " : ";
                baseWritten = true;
            }

            if(!p->isLocal())
            {
                _M << name << "Operations_, ";
            }
            _M << name << "OperationsNC_";
        }
        for(ClassList::const_iterator q = bases.begin(); q != bases.end(); ++q)
        {
            if((*q)->isAbstract())
            {
                if(baseWritten)
                {
                    _M << ", ";
                }
                else
                {
                    _M << " : ";
                    baseWritten = true;
                }

                _M << fixId((*q)->scoped());
            }
        }
    }

    _M << sb;

    if(!p->isInterface())
    {
        if(p->hasDataMembers() && !p->hasOperations())
        {
            _M << sp << nl << "#region Slice data members";
        }
        else if(p->hasDataMembers())
        {
            _M << sp << nl << "#region Slice data members and operations";
        }
        else if(p->hasOperations())
        {
            _M << sp << nl << "#region Slice operations";
        }
    }

    return true;
}

void
Slice::Gen::TypesVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    string name = fixId(p->name());
    DataMemberList classMembers = p->classDataMembers();
    DataMemberList allClassMembers = p->allClassDataMembers();
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList allDataMembers = p->allDataMembers();
    ClassList bases = p->bases();
    bool hasBaseClass = !bases.empty() && !bases.front()->isInterface();
    DataMemberList::const_iterator d;

    if(!p->isInterface())
    {
        if(p->hasDataMembers() && !p->hasOperations())
        {
            _M << sp << nl << "#endregion"; // Slice data members"
        }
        else if(p->hasDataMembers())
        {
            _M << sp << nl << "#endregion"; // Slice data members and operations"
        }
        else if(p->hasOperations())
        {
            _M << sp << nl << "#endregion"; // Slice operations"
        }

        if(!allDataMembers.empty())
        {
            _M << sp << nl << "#region Constructors";

            _M << sp << nl << "public " << name << spar << epar;
            if(hasBaseClass)
            {
                _M << " : base()";
            }
            _M << sb;
            _M << eb;

            _M << sp << nl << "public " << name << spar;
            vector<string> paramDecl;
            for(d = allDataMembers.begin(); d != allDataMembers.end(); ++d)
            {
                string memberName = fixId((*d)->name());
                string memberType = typeToString((*d)->type());
                paramDecl.push_back(memberType + " " + memberName);
            }
            _M << paramDecl << epar;
            if(hasBaseClass && allDataMembers.size() != dataMembers.size())
            {
                _M << " : base" << spar;
                vector<string> baseParamNames;
                DataMemberList baseDataMembers = bases.front()->allDataMembers();
                for(d = baseDataMembers.begin(); d != baseDataMembers.end(); ++d)
                {
                    baseParamNames.push_back(fixId((*d)->name()));
                }
                _M << baseParamNames << epar;
            }
            _M << sb;
            vector<string> paramNames;
            for(d = dataMembers.begin(); d != dataMembers.end(); ++d)
            {
                paramNames.push_back(fixId((*d)->name()));
            }
            for(vector<string>::const_iterator i = paramNames.begin(); i != paramNames.end(); ++i)
            {
                _M << nl << "this." << *i << " = " << *i << ';';
            }
            _M << eb;

            _M << sp << nl << "#endregion"; // Constructors
        }

        writeInheritedOperations(p);
    }

    if(!p->isInterface() && !p->isLocal())
    {
        writeDispatchAndMarshalling(p, _stream);
    }

    _M << eb;
}

void
Slice::Gen::TypesVisitor::visitOperation(const OperationPtr& p)
{

    ClassDefPtr classDef = ClassDefPtr::dynamicCast(p->container());
    if(classDef->isInterface())
    {
        return;
    }

    bool isLocal = classDef->isLocal();
    bool amd = !isLocal && (classDef->hasMetaData("amd") || p->hasMetaData("amd"));

    string name = p->name();
    ParamDeclList paramList = p->parameters();
    vector<string> params;
    vector<string> args;
    string retS;

    if(!amd)
    {
        //params = getParams(p);
        args = getArgs(p);
        name = fixId(name, DotNet::ICloneable, true);
        retS = typeToString(p->returnType());
    }
    else
    {
        params = getParamsAsync(p, true);
        args = getArgsAsync(p);
        retS = "void";
        name = name + "_async";
    }

    _M << sp;
    emitAttributes(p);
    _M << nl << "public ";
    if(isLocal)
    {
        _M << "abstract ";
    }
    _M << retS << " " << name << spar << params << epar;
    if(isLocal)
    {
        _M << ";";
    }
    else
    {
        _M << sb;
        _M << nl;
        if(!amd && p->returnType())
        {
            _M << "return ";
        }
        _M << name << spar << args << "Ice.ObjectImpl.defaultCurrent" << epar << ';';
        _M << eb;
    }

    if(!isLocal)
    {
        _M << nl << "public abstract " << retS << " " << name
             << spar << params << "Ice.Current current__" << epar << ';';
    }
}

void
Slice::Gen::TypesVisitor::visitSequence(const SequencePtr& p)
{
    string name = fixName(p);
    BuiltinPtr builtin = BuiltinPtr::dynamicCast(p->type());
    bool isByteSeq = builtin && builtin->kind() == Builtin::KindByte;

    emitDeprecate(p, 0, _M, "type");

    if(isByteSeq)
    {
	_H << sp << nl << "typedef NSData " << name << ";";
    }
    else
    {
	_H << sp << nl << "typedef NSArray " << name << ";";
    }
}

bool
Slice::Gen::TypesVisitor::visitExceptionStart(const ExceptionPtr& p)
{
    string name = fixName(p);
    ExceptionPtr base = p->base();
    DataMemberList dataMembers = p->dataMembers();

    _M << sp;

    emitDeprecate(p, 0, _M, "type");

    _H << sp << nl << "@interface " << name << " : ";
    if(base)
    {
        _H << fixName(base);
    }
    else
    {
        _H << "Ice.UserException";
    }
    if(!dataMembers.empty())
    {
	_H << sb;
	_H << nl << "@private";
	_H.inc();
    }

    _M << sp << nl << "@implementation " << name << sp;

    return true;
}

void
Slice::Gen::TypesVisitor::visitExceptionEnd(const ExceptionPtr& p)
{
    string name = fixName(p);
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList::const_iterator q;

    // TODO: deprecate metadata

    //
    // Data member declarations.
    //
    writeMembers(dataMembers, 0); // TODO fix second paramater

    if(!dataMembers.empty())
    {
	_H.dec();
	_H << eb;
	_H << sp;
    }

    //
    // @property and @synthesize for each data member.
    //
    writeProperties(dataMembers, 0); // TODO fix second parameter
    writeSynthesize(dataMembers, 0); // TODO fix second parameter

    //
    // Constructors.
    //
    _H << sp << nl << "-(id) init";
    _M << sp << nl << "-(id) init";
    writeMemberSignature(dataMembers, 0); // TODO fix second parameter
    _H << ";";
    _M << sb;
    _M << nl << "return [self init";
    writeMemberCall(dataMembers, WithEscape);
    _M << " copyItems:NO];";
    _M << eb;

    _H << nl << "-(id) init";
    _M << sp << nl << "-(id) init";
    writeMemberSignature(dataMembers, 0); // TODO fix second parameter
    _H << " copyItems:(BOOL)copyItems;";
    _M << " copyItems:(BOOL)copyItems__";
    _M << sb;
    _M << nl << "if(![super init])";
    _M << sb;
    _M << nl << "return nil;";
    _M << eb;
    if(!membersAreValues(dataMembers))
    {
	_M << nl << "SEL sel_ = (copyItems__ ? @selector(copy) : @selector(retain));";
    }
    for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
	TypePtr type = (*q)->type();
	string typeString = typeToString(type);
        string name = fixId((*q)->name());

	_M << nl << name << " = ";
	if(isValueType(type))
	{
	    _M << name << "_;";
	}
	else
	{
	   _M << "[" << name << "_ performSelector:sel_];";
	}
    }

    _M << nl << "return self;";
    _M << eb;

    //
    // Copy constructors.
    //
    _H << nl << "-(id) initWithException:(" << name << " *)s_;";
    _M << sp << nl << "-(id) initWithException:(" << name << " *)s_";
    _M << sb;
    _M << nl << "return [self initWithException:s_ copyItems:NO];";
    _M << eb;

    _H << nl << "-(id) initWithException:(" << name << " *)s_ copyItems:(BOOL)copyItems;";
    _M << sp << nl << "-(id) initWithException:(" << name << " *)s_ copyItems:(BOOL)copyItems__";
    _M << sb;
    _M << nl << "return [self init";
    writeMemberMethodCall(dataMembers, "s_");
    _M << " copyItems:copyItems__];";
    _M << eb;

    //
    // copyWithZone
    //
    _H << nl << "-(id) copyWithZone:(NSZone *)zone;";

    _M << sp << nl << "-(id) copyWithZone:(NSZone *)zone";
    _M << sb;
    _M << nl << name << " *copy_ = [" << name << " allocWithZone:zone];";

    for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        string name = fixId((*q)->name());
	if(isValueType((*q)->type()))
	{
	    _M << nl << "copy_->" << name << " = " << name << ";";
	}
	else
	{
	    _M << nl << "copy_->" << name << " = [" << name << " copy];";
	}
    }
    _M << nl << "return copy_;";
    _M << eb;

    //
    // hash
    //
    writeMemberHashCode(dataMembers, 0); // TODO fix second parameter

    //
    // isEqual
    //
    writeMemberEquals(dataMembers, 0); // TODO fix second parameter

    //
    // dealloc
    //
    writeMemberDealloc(dataMembers, 0); // TODO fix second parameter

    _H << nl << "@end";

    _M << nl << "@end";
}

bool
Slice::Gen::TypesVisitor::visitStructStart(const StructPtr& p)
{
    string name = fixName(p);

    // TODO: if(_stream)

    _H << sp;

    emitDeprecate(p, 0, _M, "type");

    _H << nl << "@interface " << name << " : NSObject <NSCopying>";
    _H << sb;
    _H << nl << "@private";
    _H.inc();

    _M << sp << nl << "@implementation " << name << sp;

    return true;
}

void
Slice::Gen::TypesVisitor::visitStructEnd(const StructPtr& p)
{
    string name = fixName(p);
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList::const_iterator q;

    // TODO: deprecate metadata

    //
    // Data member declarations.
    //
    writeMembers(dataMembers, 0); // TODO fix second paramater

    _H.dec();
    _H << eb;
    _H << sp;

    //
    // @property and @synthesize for each data member.
    //
    writeProperties(dataMembers, 0); // TODO fix second parameter
    writeSynthesize(dataMembers, 0); // TODO fix second parameter

    //
    // Constructors.
    //
    _H << sp << nl << "-(id) init";
    _M << sp << nl << "-(id) init";
    writeMemberSignature(dataMembers, 0); // TODO fix second parameter
    _H << ";";
    _M << sb;
    _M << nl << "return [self init";
    writeMemberCall(dataMembers, WithEscape);
    _M << " copyItems:NO];";
    _M << eb;

    _H << nl << "-(id) init";
    _M << sp << nl << "-(id) init";
    writeMemberSignature(dataMembers, 0); // TODO fix second parameter
    _H << " copyItems:(BOOL)copyItems;";
    _M << " copyItems:(BOOL)copyItems__";
    _M << sb;
    _M << nl << "if(![super init])";
    _M << sb;
    _M << nl << "return nil;";
    _M << eb;
    if(!membersAreValues(dataMembers))
    {
	_M << nl << "SEL sel_ = (copyItems__ ? @selector(copy) : @selector(retain));";
    }
    for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
	TypePtr type = (*q)->type();
	string typeString = typeToString(type);
        string name = fixId((*q)->name());

	_M << nl << name << " = ";
	if(isValueType(type))
	{
	    _M << name << "_;";
	}
	else
	{
	   _M << "[" << name << "_ performSelector:sel_];";
	}
    }

    _M << nl << "return self;";
    _M << eb;

    //
    // Copy constructors.
    //
    _H << nl << "-(id) initWithStruct:(" << name << " *)s;";
    _M << sp << nl << "-(id) initWithStruct:(" << name << " *)s_";
    _M << sb;
    _M << nl << "return [self init";
    writeMemberMethodCall(dataMembers, "s_");
    _M << " copyItems:NO];";
    _M << eb;

    _H << nl << "-(id) initWithStruct:(" << name << " *)s copyItems:(BOOL)copyItems;";
    _M << sp << nl << "-(id) initWithStruct:(" << name << " *)s_ copyItems:(BOOL)copyItems__";
    _M << sb;
    _M << nl << "return [self init";
    writeMemberMethodCall(dataMembers, "s_");
    _M << " copyItems:copyItems__];";
    _M << eb;

    //
    // Convenience constructors.
    //
    _H << nl << "+(id) " << name;
    _M << sp << nl << "+(id) " << name;
    writeMemberSignature(dataMembers, 0); // TODO fix second parameter
    _H << ";";
    _M << sb;
    _M << nl << name << " *s__ = [[" << name << " alloc] init";
    writeMemberCall(dataMembers, WithEscape);
    _M << " copyItems:NO];";
    _M << nl << "[s__ autorelease];";
    _M << nl << "return s__;";
    _M << eb;

    _H << nl << "+(id) " << name;
    _M << sp << nl << "+(id) " << name;
    writeMemberSignature(dataMembers, 0); // TODO fix second parameter
    _H << " copyItems:(BOOL)copyItems;";
    _M << " copyItems:(BOOL)copyItems__";
    _M << sb;
    _M << nl << name << " *s__ = [[" << name << " alloc] init";
    writeMemberCall(dataMembers, WithEscape);
    _M << " copyItems:copyItems__];";
    _M << nl << "[s__ autorelease];";
    _M << nl << "return s__;";
    _M << eb;

    _H << nl << "+(id) " << name << "WithStruct:(" << name << " *)s;";
    _M << sp << nl << "+(id) " << name << "WithStruct:(" << name << " *)s_";
    _M << sb;
    _M << nl << name << " *s__ = [[" << name << " alloc] initWithStruct:s_ copyItems:NO];";
    _M << nl << "[s__ autorelease];";
    _M << nl << "return s__;";
    _M << eb;

    _H << nl << "+(id) " << name << "WithStruct:(" << name << " *)s copyItems:(BOOL)copyItems;";
    _M << sp << nl << "+(id) " << name << "WithStruct:(" << name << " *)s_ copyItems:(BOOL)copyItems__";
    _M << sb;
    _M << nl << name << " *s__ = [[" << name << " alloc] initWithStruct:s_ copyItems:copyItems__];";
    _M << nl << "[s__ autorelease];";
    _M << nl << "return s__;";
    _M << eb;

    //
    // copyWithZone
    //
    _H << nl << "-(id) copyWithZone:(NSZone *)zone;";

    _M << sp << nl << "-(id) copyWithZone:(NSZone *)zone";
    _M << sb;
    _M << nl << name << " *copy_ = [" << name << " allocWithZone:zone];";

    for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        string name = fixId((*q)->name());
	if(isValueType((*q)->type()))
	{
	    _M << nl << "copy_->" << name << " = " << name << ";";
	}
	else
	{
	    _M << nl << "copy_->" << name << " = [" << name << " copy];";
	}
    }
    _M << nl << "return copy_;";
    _M << eb;

    //
    // hash
    //
    writeMemberHashCode(dataMembers, 0); // TODO fix second parameter

    //
    // isEqual
    //
    writeMemberEquals(dataMembers, 0); // TODO fix second parameter

    //
    // dealloc
    //
    writeMemberDealloc(dataMembers, 0); // TODO fix second parameter

    _H << nl << "@end";

    _M << nl << "@end";
}

void
Slice::Gen::TypesVisitor::visitDictionary(const DictionaryPtr& p)
{
    string name = fixName(p);

    emitDeprecate(p, 0, _M, "type");

    _H << sp << nl << "typedef NSDictionary " << name << ";";
}

void
Slice::Gen::TypesVisitor::visitEnum(const EnumPtr& p)
{
    string name = fixName(p);
    EnumeratorList enumerators = p->getEnumerators();
    _H << sp;

    emitDeprecate(p, 0, _M, "type");

    _H << nl << "typedef enum";
    _H << sb;
    EnumeratorList::const_iterator en = enumerators.begin();
    while(en != enumerators.end())
    {
        _H << nl << fixName(*en);
        if(++en != enumerators.end())
        {
            _H << ',';
        }
    }
    _H << eb << " " << name << ";";

    // TODO: if(_stream)
}

void
Slice::Gen::TypesVisitor::visitConst(const ConstPtr& p)
{
    _H << sp;
    _H << nl << "const " << typeToString(p->type()) << " ";
    if(!isValueType(p->type()))
    {
        _H << "*";
    }
    _H << fixName(p) << " = ";

    BuiltinPtr bp = BuiltinPtr::dynamicCast(p->type());
    if(bp && bp->kind() == Builtin::KindString)
    {
        //
        // Expand strings into the basic source character set. We can't use isalpha() and the like
        // here because they are sensitive to the current locale.
        //
        static const string basicSourceChars = "abcdefghijklmnopqrstuvwxyz"
                                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                               "0123456789"
                                               "_{}[]#()<>%:;.?*+-/^&|~!=,\\\"' ";
        static const set<char> charSet(basicSourceChars.begin(), basicSourceChars.end());

        _H << "@\"";                                      // Opening @"

        const string val = p->value();
        for(string::const_iterator c = val.begin(); c != val.end(); ++c)
        {
            if(charSet.find(*c) == charSet.end())
            {
                unsigned char uc = *c;                  // char may be signed, so make it positive
                ostringstream s;
                s << "\\";                              // Print as octal if not in basic source character set
                s.width(3);
                s.fill('0');
                s << oct;
                s << static_cast<unsigned>(uc);
                _H << s.str();
            }
            else
            {
                _H << *c;                                // Print normally if in basic source character set
            }
        }

        _H << "\"";                                      // Closing "
    }
    else
    {
        EnumPtr ep = EnumPtr::dynamicCast(p->type());
        if(ep)
        {
	    string prefix = moduleName(findModule(ep));
            _H << prefix << p->value();
        }
        else
        {
            _H << p->value();
        }
    }

    _H << ';';
}

void
Slice::Gen::TypesVisitor::visitDataMember(const DataMemberPtr& p)
{
#if 0
    int baseTypes = 0;
    bool isClass = false;
    bool propertyMapping = false;
    bool isValue = false;
    bool isProtected = false;
    ContainedPtr cont = ContainedPtr::dynamicCast(p->container());
    assert(cont);
    if(StructPtr::dynamicCast(cont))
    {
        isValue = isValueType(StructPtr::dynamicCast(cont));
        if(!isValue || cont->hasMetaData("clr:class"))
        {
            baseTypes = DotNet::ICloneable;
        }
        if(cont->hasMetaData("clr:property"))
        {
            propertyMapping = true;
        }
    }
    else if(ExceptionPtr::dynamicCast(cont))
    {
        baseTypes = DotNet::Exception;
    }
    else if(ClassDefPtr::dynamicCast(cont))
    {
        baseTypes = DotNet::ICloneable;
        isClass = true;
        if(cont->hasMetaData("clr:property"))
        {
            propertyMapping = true;
        }
        isProtected = cont->hasMetaData("protected") || p->hasMetaData("protected");
    }

    _M << sp;

    emitDeprecate(p, cont, _M, "member");

    emitAttributes(p);

    string type = typeToString(p->type());
    string propertyName = fixId(p->name(), baseTypes, isClass);
    string dataMemberName = propertyName;
    if(propertyMapping)
    {
        dataMemberName += "_prop";
    }

    _M << nl;
    if(propertyMapping)
    {
        _M << "private";
    }
    else if(isProtected)
    {
        _M << "protected";
    }
    else
    {
        _M << "public";
    }
    _M << ' ' << type << ' ' << dataMemberName << ';';

    if(!propertyMapping)
    {
        return;
    }

    _M << nl << (isProtected ? "protected" : "public");
    if(!isValue)
    {
        _M << " virtual";
    }
    _M << ' ' << type << ' ' << propertyName;
    _M << sb;
    _M << nl << "get";
    _M << sb;
    _M << nl << "return " << dataMemberName << ';';
    _M << eb;
    _M << nl << "set";
    _M << sb;
    _M << nl << dataMemberName << " = value;";
    _M << eb;
    _M << eb;
#endif
}

bool
Slice::Gen::TypesVisitor::membersAreValues(const DataMemberList& dataMembers) const
{
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        if(!isValueType((*q)->type()))
	{
	    return false;
	}
    }
    return true;
}

void
Slice::Gen::TypesVisitor::writeMembers(const DataMemberList& dataMembers, int baseTypes) const
{
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
	TypePtr type = (*q)->type();
	string typeString = typeToString(type);
        string name = fixId((*q)->name());

	_H << nl << typeString << " ";
	if(!isValueType(type))
	{
	    _H << "*";
	}
	_H << name << ";";
    }
}

void
Slice::Gen::TypesVisitor::writeMemberSignature(const DataMemberList& dataMembers, int baseTypes) const
{
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
	TypePtr type = (*q)->type();
	string typeString = typeToString(type);
        string name = fixId((*q)->name());
	bool isValue = isValueType(type);

	if(q != dataMembers.begin())
	{
	    _H << " ";
	    _M << " ";
	}
	if(q != dataMembers.begin())
	{
	    _H << name;
	    _M << name;
	}
	_H << ":(" << typeString;
	_M << ":(" << typeString;
	if(!isValue)
	{
	    _H << " *";
	    _M << " *";
	}
	_H << ")" << name;
	_M << ")" << name << "_";
    }
}

void
Slice::Gen::TypesVisitor::writeMemberCall(const DataMemberList& dataMembers, Escape esc) const
{
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        string name = fixId((*q)->name());

	if(q != dataMembers.begin())
	{
	    _M << " " << name;
	}
	_M << ":" << name;
	if(esc == WithEscape)
	{
	    _M << "_";
	}
    }
}

void
Slice::Gen::TypesVisitor::writeMemberMethodCall(const DataMemberList& dataMembers, const string& container) const
{
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        string name = fixId((*q)->name());

	if(q != dataMembers.begin())
	{
	    _M << " " << name;
	}
	_M << ":[" << container << " " << name << "]";
    }
}

void
Slice::Gen::TypesVisitor::writeProperties(const DataMemberList& dataMembers, int baseTypes) const
{
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
	TypePtr type = (*q)->type();
        string name = fixId((*q)->name());
	string typeString = typeToString(type);
	bool isValue = isValueType(type);

	_H << nl << "@property(nonatomic, ";
	if(isValue)
	{
	    _H << "assign";
	}
	else
	{
	    _H << "retain";
	}
	_H << ") " << typeString << " ";
	if(!isValue)
	{
	    _H << "*";
	}
	_H << name << ";";
    }
}

void
Slice::Gen::TypesVisitor::writeSynthesize(const DataMemberList& dataMembers, int baseTypes) const
{
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        string name = fixId((*q)->name());
	_M << nl << "@synthesize " << name << ";";
    }
}

void
Slice::Gen::TypesVisitor::writeMemberHashCode(const DataMemberList& dataMembers, int baseTypes) const
{
    _H << nl << "-(NSUInteger) hash;";

    _M << sp << nl << "-(NSUInteger) hash";
    _M << sb;
    _M << nl << "NSUInteger h_ = 0;";
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
	TypePtr type = (*q)->type();
        string name = fixId((*q)->name());

	_M << nl << "h_ = (h_ << 1) ^ ";
	if(isValueType(type))
	{
	    _M << name << ";";
	}
	else
	{
	    _M << "(!" << name << " ? 0 : [" << name << " hash]);";
	}
    }
    _M << nl << "return h_;";
    _M << eb;
}

void
Slice::Gen::TypesVisitor::writeMemberEquals(const DataMemberList& dataMembers, int baseTypes) const
{
    _H << nl << "-(BOOL) isEqual:(id)anObject;";

    _M << sp << nl << "-(BOOL) isEqual:(id)o_";
    _M << sb;
    _M << nl << "if(self == o_)";
    _M << sb;
    _M << nl << "return YES;";
    _M << eb;
    _M << nl << "if(!o_ || ![o_ isKindOfClass:[self class]])";
    _M << sb;
    _M << nl << "return NO;";
    _M << eb;
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
	TypePtr type = (*q)->type();
        string name = fixId((*q)->name());

	if(isValueType(type))
	{
	    _M << nl << "if(" << name << " != [o_ " << name << "])";
	    _M << sb;
	    _M << nl << "return NO;";
	    _M << eb;
	}
	else
	{
	    _M << nl << "if(!" << name << ")";
	    _M << sb;
	    _M << nl << "if([o_ " << name << "])";
	    _M << sb;
	    _M << nl << "return NO;";
	    _M << eb;
	    _M << eb;
	    _M << nl << "else";
	    _M << sb;
	    _M << nl << "if(![" << name << " isEqual:[o_ " << name << "]])";
	    _M << sb;
	    _M << nl << "return NO;";
	    _M << eb;
	    _M << eb;
	}
    }
    _M << nl << "return YES;";
    _M << eb;
}

void
Slice::Gen::TypesVisitor::writeMemberDealloc(const DataMemberList& dataMembers, int baseTypes) const
{
    _H << nl << "-(void) dealloc;";

    _M << sp << nl << "-(void) dealloc;";
    _M << sb;
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
	TypePtr type = (*q)->type();
        string name = fixId((*q)->name());
	bool isValue = isValueType(type);
	if(!isValue)
	{
	    _M << nl << "[" << name << " release];";
	}
    }
    _M << nl << "[super dealloc];";
    _M << eb;
}

Slice::Gen::ProxyVisitor::ProxyVisitor(IceUtilInternal::Output& H, IceUtilInternal::Output& M)
    : ObjCVisitor(H, M)
{
}

bool
Slice::Gen::ProxyVisitor::visitModuleStart(const ModulePtr& p)
{
    return true;
}

void
Slice::Gen::ProxyVisitor::visitModuleEnd(const ModulePtr&)
{
}

bool
Slice::Gen::ProxyVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    string name = fixName(p);
    ClassList bases = p->bases();

    _H << sp << nl << "@protocol " << name << "Prx <";
    if(bases.empty())
    {
        _H << "ICEObjectPrx";
    }
    else
    {
        ClassList::const_iterator q = bases.begin();
        while(q != bases.end())
        {
            _H << fixName(*q) + "Prx";
            if(++q != bases.end())
            {
                _H << ", ";
            }
        }
    }
    _H << ">";

    return true;
}

void
Slice::Gen::ProxyVisitor::visitClassDefEnd(const ClassDefPtr&)
{
    _H << nl << "@end;";
}

void
Slice::Gen::ProxyVisitor::visitOperation(const OperationPtr& p)
{
    // TODO deal with deprecate metadata.

    string name = fixId(p->name());
    TypePtr returnType = p->returnType();
    string retString = typeToString(returnType);
    bool retIsValue = isValueType(returnType);
    string params = getParams(p);

    //
    // Write two versions of the operation--with and without a
    // context parameter.
    //
    _H << nl << "-(" << retString;
    if(!retIsValue)
    {
        _H << " *";
    }
    _H << ") " << name << params << ";";

    _H << nl << "-(" << retString;
    if(!retIsValue)
    {
        _H << " *";
    }
    _H << ") " << name << params;
    if(!params.empty())
    {
        _H << " ctx_";
    }
    _H << ":(ICEContext *)ctx_;";

    // TODO: deal with AMI
}

Slice::Gen::OpsVisitor::OpsVisitor(IceUtilInternal::Output& out)
    : ObjCVisitor(out, out)
{
}

bool
Slice::Gen::OpsVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasAbstractClassDefs())
    {
        return false;
    }

    _M << sp << nl << "namespace " << fixId(p->name());
    _M << sb;
    return true;
}

void
Slice::Gen::OpsVisitor::visitModuleEnd(const ModulePtr&)
{
    _M << eb;
}

bool
Slice::Gen::OpsVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    //
    // Don't generate Operations interfaces for non-abstract classes.
    //
    if(!p->isAbstract())
    {
        return false;
    }

    if(!p->isLocal())
    {
        writeOperations(p, false);
    }
    writeOperations(p, true);

    return false;
}

void
Slice::Gen::OpsVisitor::writeOperations(const ClassDefPtr& p, bool noCurrent)
{
    string name = p->name();
    string scoped = fixId(p->scoped());
    ClassList bases = p->bases();
    string opIntfName = "Operations";
    if(noCurrent || p->isLocal())
    {
        opIntfName += "NC";
    }

    _M << sp << nl << "public interface " << name << opIntfName << '_';
    if((bases.size() == 1 && bases.front()->isAbstract()) || bases.size() > 1)
    {
        _M << " : ";
        ClassList::const_iterator q = bases.begin();
        bool first = true;
        while(q != bases.end())
        {
            if((*q)->isAbstract())
            {
                if(!first)
                {
                    _M << ", ";
                }
                else
                {
                    first = false;
                }
                string s = (*q)->scoped();
                s += "Operations";
                if(noCurrent)
                {
                    s += "NC";
                }
                _M << fixId(s) << '_';
            }
            ++q;
        }
    }
    _M << sb;

    OperationList ops = p->operations();
    OperationList::const_iterator r;
    for(r = ops.begin(); r != ops.end(); ++r)
    {
        OperationPtr op = *r;
        bool amd = !p->isLocal() && (p->hasMetaData("amd") || op->hasMetaData("amd"));
        string opname = amd ? (op->name() + "_async") : fixId(op->name(), DotNet::ICloneable, true);
        
        TypePtr ret;
        vector<string> params;

        if(amd)
        {
            params = getParamsAsync(op, true);
        }
        else
        {
            //params = getParams(op);
            ret = op->returnType();
        }

        _M << sp;

        emitDeprecate(op, p, _M, "operation");

        emitAttributes(op);
        string retS = typeToString(ret);
        _M << nl << retS << ' ' << opname << spar << params;
        if(!noCurrent && !p->isLocal())
        { 
            _M << "Ice.Current current__";
        }
        _M << epar << ';';
    }

    _M << eb;
}

Slice::Gen::HelperVisitor::HelperVisitor(IceUtilInternal::Output& out, bool stream)
    : ObjCVisitor(out, out), _stream(stream)
{
}

bool
Slice::Gen::HelperVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDecls() && !p->hasNonLocalSequences() && !p->hasDictionaries())
    {
        return false;
    }

    _M << sp << nl << "namespace " << fixId(p->name());
    _M << sb;
    return true;
}

void
Slice::Gen::HelperVisitor::visitModuleEnd(const ModulePtr&)
{
    _M << eb;
}

bool
Slice::Gen::HelperVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal())
        return false;

    string name = p->name();
    ClassList bases = p->bases();

    _M << sp << nl << "public sealed class " << name << "PrxHelper : Ice.ObjectPrxHelperBase, " << name << "Prx";
    _M << sb;

    OperationList ops = p->allOperations();

    if(!ops.empty())
    {
        _M << sp << nl << "#region Synchronous operations";
    }

    OperationList::const_iterator r;
    for(r = ops.begin(); r != ops.end(); ++r)
    {
        OperationPtr op = *r;
        string opName = fixId(op->name(), DotNet::ICloneable, true);
        TypePtr ret = op->returnType();
        string retS = typeToString(ret);

        vector<string> params;// = getParams(op);
        vector<string> args = getArgs(op);

        _M << sp << nl << "public " << retS << " " << opName << spar << params << epar;
        _M << sb;
        _M << nl;
        if(ret)
        {
            _M << "return ";
        }
        _M << opName << spar << args << "null" << "false" << epar << ';';
        _M << eb;

        _M << sp << nl << "public " << retS << " " << opName << spar << params 
             << "_System.Collections.Generic.Dictionary<string, string> context__" << epar;
        _M << sb;
        _M << nl;
        if(ret)
        {
            _M << "return ";
        }
        _M << opName << spar << args << "context__" << "true" << epar << ';';
        _M << eb;

        _M << sp << nl << "private " << retS << " " << opName << spar << params 
             << "_System.Collections.Generic.Dictionary<string, string> context__"
             << "bool explicitContext__" << epar;
        _M << sb;

        _M << nl << "if(explicitContext__ && context__ == null)";
        _M << sb;
        _M << nl << "context__ = emptyContext_;";
        _M << eb;
        _M << nl << "int cnt__ = 0;";
        _M << nl << "while(true)";
        _M << sb;
        _M << nl << "Ice.ObjectDel_ delBase__ = null;";
        _M << nl << "try";
        _M << sb;
        if(op->returnsData())
        {
            _M << nl << "checkTwowayOnly__(\"" << op->name() << "\");";
        }
        _M << nl << "delBase__ = getDelegate__(false);";
        _M << nl << name << "Del_ del__ = (" << name << "Del_)delBase__;";
        _M << nl;
        if(ret)
        {
            _M << "return ";
        }
        _M << "del__." << opName << spar << args << "context__" << epar << ';';
        if(!ret)
        {
            _M << nl << "return;";
        }
        _M << eb;
        _M << nl << "catch(IceInternal.LocalExceptionWrapper ex__)";
        _M << sb;
        if(op->mode() == Operation::Idempotent || op->mode() == Operation::Nonmutating)
        {
            _M << nl << "handleExceptionWrapperRelaxed__(delBase__, ex__, null, ref cnt__);";
        }
        else
        {
            _M << nl << "handleExceptionWrapper__(delBase__, ex__, null);";
        }
        _M << eb;
        _M << nl << "catch(Ice.LocalException ex__)";
        _M << sb;
        _M << nl << "handleException__(delBase__, ex__, null, ref cnt__);";
        _M << eb;
        _M << eb;

        _M << eb;
    }

    if(!ops.empty())
    {
        _M << sp << nl << "#endregion"; // Synchronous operations
    }

    bool hasAsyncOps = false;

    for(r = ops.begin(); r != ops.end(); ++r)
    {
        OperationPtr op = *r;
        
        ClassDefPtr containingClass = ClassDefPtr::dynamicCast(op->container());
        if(containingClass->hasMetaData("ami") || op->hasMetaData("ami"))
        {
            if(!hasAsyncOps)
            {
                _M << sp << nl << "#region Asynchronous operations";
                hasAsyncOps = true;
            }
            vector<string> paramsAMI = getParamsAsync(op, false);
            vector<string> argsAMI = getArgsAsync(op);

            string opName = op->name();

            //
            // Write two versions of the operation - with and without a
            // context parameter
            //
            _M << sp;
            _M << nl << "public bool " << opName << "_async" << spar << paramsAMI << epar;
            _M << sb;
            _M << nl << "return " << opName << "_async" << spar << argsAMI << "null" << "false" << epar << ';';
            _M << eb;

            _M << sp;
            _M << nl << "public bool " << opName << "_async" << spar << paramsAMI
                 << "_System.Collections.Generic.Dictionary<string, string> ctx__" << epar;
            _M << sb;
            _M << nl << "return " << opName << "_async" << spar << argsAMI << "ctx__" << "true" << epar << ';';
            _M << eb;

            _M << sp;
            _M << nl << "public bool " << opName << "_async" << spar << paramsAMI
                 << "_System.Collections.Generic.Dictionary<string, string> ctx__"
                 << "bool explicitContext__" << epar;
            _M << sb;
            _M << nl << "if(explicitContext__ && ctx__ == null)";
            _M << sb;
            _M << nl << "ctx__ = emptyContext_;";
            _M << eb;
            _M << nl << "return cb__.invoke__" << spar << "this" << argsAMI << "ctx__" << epar << ';';
            _M << eb;
        }
    }

    if(hasAsyncOps)
    {
        _M << sp << nl << "#endregion"; // Asynchronous operations
    }

    _M << sp << nl << "#region Checked and unchecked cast operations";

    _M << sp << nl << "public static " << name << "Prx checkedCast(Ice.ObjectPrx b)";
    _M << sb;
    _M << nl << "if(b == null)";
    _M << sb;
    _M << nl << "return null;";
    _M << eb;
    _M << nl << name << "Prx r = b as " << name << "Prx;";
    _M << nl << "if((r == null) && b.ice_isA(\"" << p->scoped() << "\"))";
    _M << sb;
    _M << nl << name << "PrxHelper h = new " << name << "PrxHelper();";
    _M << nl << "h.copyFrom__(b);";
    _M << nl << "r = h;";
    _M << eb;
    _M << nl << "return r;";
    _M << eb;

    _M << sp << nl << "public static " << name
         << "Prx checkedCast(Ice.ObjectPrx b, _System.Collections.Generic.Dictionary<string, string> ctx)";
    _M << sb;
    _M << nl << "if(b == null)";
    _M << sb;
    _M << nl << "return null;";
    _M << eb;
    _M << nl << name << "Prx r = b as " << name << "Prx;";
    _M << nl << "if((r == null) && b.ice_isA(\"" << p->scoped() << "\", ctx))";
    _M << sb;
    _M << nl << name << "PrxHelper h = new " << name << "PrxHelper();";
    _M << nl << "h.copyFrom__(b);";
    _M << nl << "r = h;";
    _M << eb;
    _M << nl << "return r;";
    _M << eb;

    _M << sp << nl << "public static " << name << "Prx checkedCast(Ice.ObjectPrx b, string f)";
    _M << sb;
    _M << nl << "if(b == null)";
    _M << sb;
    _M << nl << "return null;";
    _M << eb;
    _M << nl << "Ice.ObjectPrx bb = b.ice_facet(f);";
    _M << nl << "try";
    _M << sb;
    _M << nl << "if(bb.ice_isA(\"" << p->scoped() << "\"))";
    _M << sb;
    _M << nl << name << "PrxHelper h = new " << name << "PrxHelper();";
    _M << nl << "h.copyFrom__(bb);";
    _M << nl << "return h;";
    _M << eb;
    _M << eb;
    _M << nl << "catch(Ice.FacetNotExistException)";
    _M << sb;
    _M << eb;
    _M << nl << "return null;";
    _M << eb;

    _M << sp << nl << "public static " << name
         << "Prx checkedCast(Ice.ObjectPrx b, string f, "
         << "_System.Collections.Generic.Dictionary<string, string> ctx)";
    _M << sb;
    _M << nl << "if(b == null)";
    _M << sb;
    _M << nl << "return null;";
    _M << eb;
    _M << nl << "Ice.ObjectPrx bb = b.ice_facet(f);";
    _M << nl << "try";
    _M << sb;
    _M << nl << "if(bb.ice_isA(\"" << p->scoped() << "\", ctx))";
    _M << sb;
    _M << nl << name << "PrxHelper h = new " << name << "PrxHelper();";
    _M << nl << "h.copyFrom__(bb);";
    _M << nl << "return h;";
    _M << eb;
    _M << eb;
    _M << nl << "catch(Ice.FacetNotExistException)";
    _M << sb;
    _M << eb;
    _M << nl << "return null;";
    _M << eb;

    _M << sp << nl << "public static " << name << "Prx uncheckedCast(Ice.ObjectPrx b)";
    _M << sb;
    _M << nl << "if(b == null)";
    _M << sb;
    _M << nl << "return null;";
    _M << eb;
    _M << nl << name << "Prx r = b as " << name << "Prx;";
    _M << nl << "if(r == null)";
    _M << sb;
    _M << nl << name << "PrxHelper h = new " << name << "PrxHelper();";
    _M << nl << "h.copyFrom__(b);";
    _M << nl << "r = h;";
    _M << eb;
    _M << nl << "return r;";
    _M << eb;

    _M << sp << nl << "public static " << name << "Prx uncheckedCast(Ice.ObjectPrx b, string f)";
    _M << sb;
    _M << nl << "if(b == null)";
    _M << sb;
    _M << nl << "return null;";
    _M << eb;
    _M << nl << "Ice.ObjectPrx bb = b.ice_facet(f);";
    _M << nl << name << "PrxHelper h = new " << name << "PrxHelper();";
    _M << nl << "h.copyFrom__(bb);";
    _M << nl << "return h;";
    _M << eb;

    _M << sp << nl << "#endregion"; // Checked and unchecked cast operations

    _M << sp << nl << "#region Marshaling support";

    _M << sp << nl << "protected override Ice.ObjectDelM_ createDelegateM__()";
    _M << sb;
    _M << nl << "return new " << name << "DelM_();";
    _M << eb;

    _M << sp << nl << "protected override Ice.ObjectDelD_ createDelegateD__()";
    _M << sb;
    _M << nl << "return new " << name << "DelD_();";
    _M << eb;

    _M << sp << nl << "public static void write__(IceInternal.BasicStream os__, " << name << "Prx v__)";
    _M << sb;
    _M << nl << "os__.writeProxy(v__);";
    _M << eb;

    _M << sp << nl << "public static " << name << "Prx read__(IceInternal.BasicStream is__)";
    _M << sb;
    _M << nl << "Ice.ObjectPrx proxy = is__.readProxy();";
    _M << nl << "if(proxy != null)";
    _M << sb;
    _M << nl << name << "PrxHelper result = new " << name << "PrxHelper();";
    _M << nl << "result.copyFrom__(proxy);";
    _M << nl << "return result;";
    _M << eb;
    _M << nl << "return null;";
    _M << eb;

    if(_stream)
    {
        _M << sp << nl << "public static void write(Ice.OutputStream outS__, " << name << "Prx v__)";
        _M << sb;
        _M << nl << "outS__.writeProxy(v__);";
        _M << eb;

        _M << sp << nl << "public static " << name << "Prx read(Ice.InputStream inS__)";
        _M << sb;
        _M << nl << "Ice.ObjectPrx proxy = inS__.readProxy();";
        _M << nl << "if(proxy != null)";
        _M << sb;
        _M << nl << name << "PrxHelper result = new " << name << "PrxHelper();";
        _M << nl << "result.copyFrom__(proxy);";
        _M << nl << "return result;";
        _M << eb;
        _M << nl << "return null;";
        _M << eb;
    }

    _M << sp << nl << "#endregion"; // Marshaling support

    return true;
}

void
Slice::Gen::HelperVisitor::visitClassDefEnd(const ClassDefPtr&)
{
    _M << eb;
}

void
Slice::Gen::HelperVisitor::visitSequence(const SequencePtr& p)
{
    //
    // Don't generate helper for sequence of a local type.
    //
    if(p->isLocal())
    {
        return;
    }

    string typeS = typeToString(p);

    _M << sp << nl << "public sealed class " << p->name() << "Helper";
    _M << sb;

    _M << sp << nl << "public static void write(IceInternal.BasicStream os__, " << typeS << " v__)";
    _M << sb;
    writeSequenceMarshalUnmarshalCode(_M, p, "v__", true, false);
    _M << eb;

    _M << sp << nl << "public static " << typeS << " read(IceInternal.BasicStream is__)";
    _M << sb;
    _M << nl << typeS << " v__;";
    writeSequenceMarshalUnmarshalCode(_M, p, "v__", false, false);
    _M << nl << "return v__;";
    _M << eb;

    if(_stream)
    {
        _M << sp << nl << "public static void write(Ice.OutputStream outS__, " << typeS << " v__)";
        _M << sb;
        writeSequenceMarshalUnmarshalCode(_M, p, "v__", true, true);
        _M << eb;

        _M << sp << nl << "public static " << typeS << " read(Ice.InputStream inS__)";
        _M << sb;
        _M << nl << typeS << " v__;";
        writeSequenceMarshalUnmarshalCode(_M, p, "v__", false, true);
        _M << nl << "return v__;";
        _M << eb;
    }

    _M << eb;

    string prefix = "clr:generic:";
    string meta;
    if(p->findMetaData(prefix, meta))
    {
        string type = meta.substr(prefix.size());
        if(type == "List" || type == "LinkedList" || type == "Queue" || type == "Stack")
        {
            return;
        }
        BuiltinPtr builtin = BuiltinPtr::dynamicCast(p->type());
        bool isClass = (builtin && builtin->kind() == Builtin::KindObject)
                        || ClassDeclPtr::dynamicCast(p->type());

        if(!isClass)
        {
            return;
        }

        //
        // The sequence is a custom sequence with elements of class type.
        // Emit a dummy class that causes a compile-time error if the
        // custom sequence type does not implement an indexer.
        //
        _M << sp << nl << "public class " << p->name() << "_Tester";
        _M << sb;
        _M << nl << p->name() << "_Tester()";
        _M << sb;
        _M << nl << typeS << " test = new " << typeS << "();";
        _M << nl << "test[0] = null;";
        _M << eb;
        _M << eb;
    }
}

void
Slice::Gen::HelperVisitor::visitDictionary(const DictionaryPtr& p)
{
    //
    // Don't generate helper for a dictionary containing a local type
    //
    if(p->isLocal())
    {
        return;
    }

    TypePtr key = p->keyType();
    TypePtr value = p->valueType();

    string meta;
    bool isNewMapping = !p->hasMetaData("clr:collection");

    string prefix = "clr:generic:";
    string genericType;
    if(!p->findMetaData(prefix, meta))
    {
        genericType = "Dictionary";
    }
    else
    {
        genericType = meta.substr(prefix.size());
    }

    string keyS = typeToString(key);
    string valueS = typeToString(value);
    string name = isNewMapping
                        ? "_System.Collections.Generic." + genericType + "<" + keyS + ", " + valueS + ">"
                        : fixId(p->name());

    _M << sp << nl << "public sealed class " << p->name() << "Helper";
    _M << sb;

    _M << sp << nl << "public static void write(";
    _M.useCurrentPosAsIndent();
    _M << "IceInternal.BasicStream os__,";
    _M << nl << name << " v__)";
    _M.restoreIndent();
    _M << sb;
    _M << nl << "if(v__ == null)";
    _M << sb;
    _M << nl << "os__.writeSize(0);";
    _M << eb;
    _M << nl << "else";
    _M << sb;
    _M << nl << "os__.writeSize(v__.Count);";
    _M << nl << "foreach(_System.Collections.";
    if(isNewMapping)
    {
        _M << "Generic.KeyValuePair<" << keyS << ", " << valueS << ">";
    }
    else
    {
        _M << "DictionaryEntry";
    }
    _M << " e__ in v__)";
    _M << sb;
    string keyArg = isNewMapping ? string("e__.Key") : "((" + keyS + ")e__.Key)";
    writeMarshalUnmarshalCode(_M, key, keyArg, true, false, false);
    string valueArg = isNewMapping ? string("e__.Value") : "((" + valueS + ")e__.Value)";
    writeMarshalUnmarshalCode(_M, value, valueArg, true, false, false);
    _M << eb;
    _M << eb;
    _M << eb;

    BuiltinPtr builtin = BuiltinPtr::dynamicCast(value);
    bool hasClassValue = (builtin && builtin->kind() == Builtin::KindObject) || ClassDeclPtr::dynamicCast(value);
    if(hasClassValue)
    {
        string expectedType = ContainedPtr::dynamicCast(value)->scoped();
        _M << sp << nl << "public sealed class Patcher__ : IceInternal.Patcher<" << valueS << ">";
        _M << sb;
        _M << sp << nl << "internal Patcher__(string type, " << name << " m, " << keyS << " key) : base(type)";
        _M << sb;
        _M << nl << "_m = m;";
        _M << nl << "_key = key;";
        _M << eb;

        _M << sp << nl << "public override void" << nl << "patch(Ice.Object v)";
        _M << sb;
        _M << nl << "try";
        _M << sb;
        _M << nl << "_m[_key] = (" << valueS << ")v;";
        _M << eb;
        _M << nl << "catch(System.InvalidCastException)";
        _M << sb;
        _M << nl << "IceInternal.Ex.throwUOE(type(), v.ice_id());";
        _M << eb;
        _M << eb;

        _M << sp << nl << "private " << name << " _m;";
        _M << nl << "private " << keyS << " _key;";
        _M << eb;
    }

    _M << sp << nl << "public static " << name << " read(IceInternal.BasicStream is__)";
    _M << sb;
    _M << nl << "int sz__ = is__.readSize();";
    _M << nl << name << " r__ = new " << name << "();";
    _M << nl << "for(int i__ = 0; i__ < sz__; ++i__)";
    _M << sb;
    _M << nl << keyS << " k__;";
    StructPtr st = StructPtr::dynamicCast(key);
    if(st)
    {
        if(isValueType(key))
        {
            _M << nl << "v__ = new " << typeToString(key) << "();";
        }
        else
        {
            _M << nl << "k__ = null;";
        }
    }
    writeMarshalUnmarshalCode(_M, key, "k__", false, false, false);
    if(!hasClassValue)
    {
        _M << nl << valueS << " v__;";

        StructPtr st = StructPtr::dynamicCast(value);
        if(st)
        {
            if(isValueType(value))
            {
                _M << nl << "v__ = new " << typeToString(value) << "();";
            }
            else
            {
                _M << nl << "v__ = null;";
            }
        }
    }
    writeMarshalUnmarshalCode(_M, value, "v__", false, false, false, "r__, k__");
    if(!hasClassValue)
    {
        _M << nl << "r__[k__] = v__;";
    }
    _M << eb;
    _M << nl << "return r__;";
    _M << eb;

    if(_stream)
    {
        _M << sp << nl << "public static void write(Ice.OutputStream outS__, " << name << " v__)";
        _M << sb;
        _M << nl << "if(v__ == null)";
        _M << sb;
        _M << nl << "outS__.writeSize(0);";
        _M << eb;
        _M << nl << "else";
        _M << sb;
        _M << nl << "outS__.writeSize(v__.Count);";
        _M << nl << "foreach(_System.Collections.";
        if(isNewMapping)
        {
            _M << nl << "Generic.KeyValuePair<" << keyS << ", " << valueS << ">";
        }
        else
        {
            _M << nl << "DictionaryEntry";
        }
        _M << " e__ in v__)";
        _M << sb;
        writeMarshalUnmarshalCode(_M, key, keyArg, true, true, false);
        writeMarshalUnmarshalCode(_M, value, valueArg, true, true, false);
        _M << eb;
        _M << eb;
        _M << eb;

        _M << sp << nl << "public static " << name << " read(Ice.InputStream inS__)";
        _M << sb;
        _M << nl << "int sz__ = inS__.readSize();";
        _M << nl << name << " r__ = new " << name << "();";
        _M << nl << "for(int i__ = 0; i__ < sz__; ++i__)";
        _M << sb;
        _M << nl << keyS << " k__;";
        StructPtr st = StructPtr::dynamicCast(key);
        if(st)
        {
            if(isValueType(key))
            {
                _M << nl << "v__ = new " << typeToString(key) << "();";
            }
            else
            {
                _M << nl << "k__ = null;";
            }
        }
        writeMarshalUnmarshalCode(_M, key, "k__", false, true, false);
        if(!hasClassValue)
        {
            _M << nl << valueS << " v__;";
            StructPtr st = StructPtr::dynamicCast(value);
            if(st)
            {
                if(isValueType(value))
                {
                    _M << nl << "v__ = new " << typeToString(value) << "();";
                }
                else
                {
                    _M << nl << "v__ = null;";
                }
            }
        }
        writeMarshalUnmarshalCode(_M, value, "v__", false, true, false, "r__, k__");
        if(!hasClassValue)
        {
            _M << nl << "r__[k__] = v__;";
        }
        _M << eb;
        _M << nl << "return r__;";
        _M << eb;
    }

    _M << eb;
}

Slice::Gen::DelegateVisitor::DelegateVisitor(IceUtilInternal::Output& out)
    : ObjCVisitor(out, out)
{
}

bool
Slice::Gen::DelegateVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
        return false;
    }

    _M << sp << nl << "namespace " << fixId(p->name());
    _M << sb;
    return true;
}

void
Slice::Gen::DelegateVisitor::visitModuleEnd(const ModulePtr&)
{
    _M << eb;
}

bool
Slice::Gen::DelegateVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal())
    {
        return false;
    }

    string name = p->name();
    ClassList bases = p->bases();

    _M << sp << nl << "public interface " << name << "Del_ : ";
    if(bases.empty())
    {
        _M << "Ice.ObjectDel_";
    }
    else
    {
        ClassList::const_iterator q = bases.begin();
        while(q != bases.end())
        {
            string s = (*q)->scoped();
            s += "Del_";
            _M << fixId(s);
            if(++q != bases.end())
            {
                _M << ", ";
            }
        }
    }

    _M << sb;

    OperationList ops = p->operations();

    OperationList::const_iterator r;
    for(r = ops.begin(); r != ops.end(); ++r)
    {
        OperationPtr op = *r;
        string opName = fixId(op->name(), DotNet::ICloneable, true);
        TypePtr ret = op->returnType();
        string retS = typeToString(ret);
        vector<string> params;// = getParams(op);

        _M << sp << nl << retS << ' ' << opName << spar << params
             << "_System.Collections.Generic.Dictionary<string, string> context__" << epar << ';';
    }

    return true;
}

void
Slice::Gen::DelegateVisitor::visitClassDefEnd(const ClassDefPtr&)
{
    _M << eb;
}

Slice::Gen::DelegateMVisitor::DelegateMVisitor(IceUtilInternal::Output& out)
    : ObjCVisitor(out, out)
{
}

bool
Slice::Gen::DelegateMVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
        return false;
    }

    _M << sp << nl << "namespace " << fixId(p->name());
    _M << sb;
    return true;
}

void
Slice::Gen::DelegateMVisitor::visitModuleEnd(const ModulePtr&)
{
    _M << eb;
}

bool
Slice::Gen::DelegateMVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal())
    {
       return false;
    }

    string name = p->name();
    ClassList bases = p->bases();

    _M << sp << nl << "public sealed class " << name << "DelM_ : Ice.ObjectDelM_, " << name << "Del_";
    _M << sb;

    OperationList ops = p->allOperations();

    OperationList::const_iterator r;
    for(r = ops.begin(); r != ops.end(); ++r)
    {
        OperationPtr op = *r;
        string opName = fixId(op->name(), DotNet::ICloneable, true);
        TypePtr ret = op->returnType();
        string retS = typeToString(ret);

        TypeStringList inParams;
        TypeStringList outParams;
        ParamDeclList paramList = op->parameters();
        for(ParamDeclList::const_iterator pli = paramList.begin(); pli != paramList.end(); ++pli)
        {
            if((*pli)->isOutParam())
            {
               outParams.push_back(make_pair((*pli)->type(), (*pli)->name()));
            }
            else
            {
               inParams.push_back(make_pair((*pli)->type(), (*pli)->name()));
            }
        }

        TypeStringList::const_iterator q;

        ExceptionList throws = op->throws();
        throws.sort();
        throws.unique();

        //
        // Arrange exceptions into most-derived to least-derived order. If we don't
        // do this, a base exception handler can appear before a derived exception
        // handler, causing compiler warnings and resulting in the base exception
        // being marshaled instead of the derived exception.
        //
#if defined(__SUNPRO_CC)
        throws.sort(Slice::derivedToBaseCompare);
#else
        throws.sort(Slice::DerivedToBaseCompare());
#endif

        vector<string> params;// = getParams(op);

        _M << sp << nl << "public " << retS << ' ' << opName << spar << params
             << "_System.Collections.Generic.Dictionary<string, string> context__" << epar;
        _M << sb;

        _M << nl << "IceInternal.Outgoing og__ = handler__.getOutgoing(\"" << op->name() << "\", " 
             << sliceModeToIceMode(op->sendMode()) << ", context__);";
        _M << nl << "try";
        _M << sb;
        if(!inParams.empty())
        {
            _M << nl << "try";
            _M << sb;
            _M << nl << "IceInternal.BasicStream os__ = og__.ostr();";
            for(q = inParams.begin(); q != inParams.end(); ++q)
            {
                writeMarshalUnmarshalCode(_M, q->first, fixId(q->second), true, false, false);
            }
            if(op->sendsClasses())
            {
                _M << nl << "os__.writePendingObjects();";
            }
            _M << eb;
            _M << nl << "catch(Ice.LocalException ex__)";
            _M << sb;
            _M << nl << "og__.abort(ex__);";
            _M << eb;
        }
        _M << nl << "bool ok__ = og__.invoke();";
        if(!op->returnsData())
        {
            _M << nl << "if(!og__.istr().isEmpty())";
            _M << sb;
        }
        _M << nl << "try";
        _M << sb;
        _M << nl << "if(!ok__)";
        _M << sb;
        //
        // The try/catch block is necessary because throwException()
        // can raise UserException.
        //
        _M << nl << "try";
        _M << sb;
        _M << nl << "og__.throwUserException();";
        _M << eb;
        for(ExceptionList::const_iterator t = throws.begin(); t != throws.end(); ++t)
        {
            _M << nl << "catch(" << fixId((*t)->scoped()) << ')';
            _M << sb;
            _M << nl << "throw;";
            _M << eb;
        }
        _M << nl << "catch(Ice.UserException ex)";
        _M << sb;
        _M << nl << "throw new Ice.UnknownUserException(ex.ice_name(), ex);";
        _M << eb;
        _M << eb;
        if(op->returnsData())
        {
            _M << nl << "IceInternal.BasicStream is__ = og__.istr();";
            _M << nl << "is__.startReadEncaps();";
            for(q = outParams.begin(); q != outParams.end(); ++q)
            {
                string param = fixId(q->second);
                StructPtr st = StructPtr::dynamicCast(q->first);
                if(st)
                {
                    if(isValueType(q->first))
                    {
                        _M << nl << param << " = new " << typeToString(q->first) << "();";
                    }
                    else
                    {
                        _M << nl << param << " = null;";
                    }
                }
                writeMarshalUnmarshalCode(_M, q->first, param, false, false, true, "");
            }
            if(ret)
            {
                BuiltinPtr builtin = BuiltinPtr::dynamicCast(ret);
                if((builtin && builtin->kind() == Builtin::KindObject) || ClassDeclPtr::dynamicCast(ret))
                {
                    _M << nl << retS << " ret__;";
                    ContainedPtr contained = ContainedPtr::dynamicCast(ret);
                    string sliceId = contained ? contained->scoped() : string("::Ice::Object");
                    _M << nl << "IceInternal.ParamPatcher<" << retS << "> ret___PP = new IceInternal.ParamPatcher<"
                         << retS << ">(\"" << sliceId << "\");";
                    _M << nl << "is__.readObject(ret___PP);";
                }
                else
                {
                    _M << nl << retS << " ret__;";
                    StructPtr st = StructPtr::dynamicCast(ret);
                    if(st)
                    {
                        if(isValueType(st))
                        {
                            _M << nl << "ret__ = new " << retS << "();";
                        }
                        else
                        {
                            _M << nl << "ret__ = null;";
                        }
                    }
                    writeMarshalUnmarshalCode(_M, ret, "ret__", false, false, true, "");
                }
            }
            if(op->returnsClasses())
            {
                _M << nl << "is__.readPendingObjects();";
                for(q = outParams.begin(); q != outParams.end(); ++q)
                {
                    string param = fixId(q->second);
                    BuiltinPtr builtin = BuiltinPtr::dynamicCast(q->first);
                    if((builtin && builtin->kind() == Builtin::KindObject) || ClassDeclPtr::dynamicCast(q->first))
                    {           
                        string type = typeToString(q->first);
                        _M << nl << "try";
                        _M << sb;
                        _M << nl << param << " = (" << type << ")" << param << "_PP.value;";
                        _M << eb;
                        _M << nl << "catch(System.InvalidCastException)";
                        _M << sb;
                        _M << nl << param << " = null;";
                        _M << nl << "IceInternal.Ex.throwUOE(" << param << "_PP.type(), "
                             << param << "_PP.value.ice_id());";
                        _M << eb;
                    }
                }
            }
            _M << nl << "is__.endReadEncaps();";
        }
        else
        {
            _M << nl << "og__.istr().skipEmptyEncaps();";
        }

        if(ret)
        {
            BuiltinPtr builtin = BuiltinPtr::dynamicCast(ret);
            if((builtin && builtin->kind() == Builtin::KindObject) || ClassDeclPtr::dynamicCast(ret))
            {
                _M << nl << "try";
                _M << sb;
                _M << nl << "ret__ = (" << retS << ")ret___PP.value;";
                _M << eb;
                _M << nl << "catch(System.InvalidCastException)";
                _M << sb;
                _M << nl << "ret__ = null;";
                _M << nl << "IceInternal.Ex.throwUOE(ret___PP.type(), ret___PP.value.ice_id());";
                _M << eb;
            }
            _M << nl << "return ret__;";
        }
        _M << eb;
        _M << nl << "catch(Ice.LocalException ex__)";
        _M << sb;
        _M << nl << "throw new IceInternal.LocalExceptionWrapper(ex__, false);";
        _M << eb;
        if(!op->returnsData())
        {
            _M << eb;
        }
        _M << eb;
        _M << nl << "finally";
        _M << sb;
        _M << nl << "handler__.reclaimOutgoing(og__);";
        _M << eb;
        _M << eb;
    }

    return true;
}

void
Slice::Gen::DelegateMVisitor::visitClassDefEnd(const ClassDefPtr&)
{
    _M << eb;
}

Slice::Gen::DelegateDVisitor::DelegateDVisitor(IceUtilInternal::Output& out)
    : ObjCVisitor(out, out)
{
}

bool
Slice::Gen::DelegateDVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
        return false;
    }

    _M << sp << nl << "namespace " << fixId(p->name());
    _M << sb;
    return true;
}

void
Slice::Gen::DelegateDVisitor::visitModuleEnd(const ModulePtr&)
{
    _M << eb;
}

bool
Slice::Gen::DelegateDVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal())
    {
        return false;
    }

    string name = p->name();
    ClassList bases = p->bases();

    _M << sp << nl << "public sealed class " << name << "DelD_ : Ice.ObjectDelD_, " << name << "Del_";
    _M << sb;

    OperationList ops = p->allOperations();

    OperationList::const_iterator r;
    for(r = ops.begin(); r != ops.end(); ++r)
    {
        OperationPtr op = *r;
        string opName = fixId(op->name(), DotNet::ICloneable, true);
        TypePtr ret = op->returnType();
        string retS = typeToString(ret);
        ClassDefPtr containingClass = ClassDefPtr::dynamicCast(op->container());

        ExceptionList throws = op->throws();
        throws.sort();
        throws.unique();

        //
        // Arrange exceptions into most-derived to least-derived order. If we don't
        // do this, a base exception handler can appear before a derived exception
        // handler, causing compiler warnings.
        //
#if defined(__SUNPRO_CC)
        throws.sort(Slice::derivedToBaseCompare);
#else
        throws.sort(Slice::DerivedToBaseCompare());
#endif

        vector<string> params;// = getParams(op);
        vector<string> args = getArgs(op);

        _M << sp;
        _M << nl << "public " << retS << ' ' << opName << spar << params
             << "_System.Collections.Generic.Dictionary<string, string> context__" << epar;
        _M << sb;
        if(containingClass->hasMetaData("amd") || op->hasMetaData("amd"))
        {
            _M << nl << "throw new Ice.CollocationOptimizationException();";
        }
        else
        {
            _M << nl << "Ice.Current current__ = new Ice.Current();";
            _M << nl << "initCurrent__(ref current__, \"" << op->name() << "\", " 
                 << sliceModeToIceMode(op->sendMode())
                 << ", context__);";
            
          
            //
            // Create out holders and delArgs
            //
            vector<string> delArgs;
            vector<string> outHolders;

            const ParamDeclList paramList = op->parameters();
            for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
            {
                string arg = fixId((*q)->name());
                if((*q)->isOutParam())
                {
                    _M << nl << typeToString((*q)->type()) << " " << arg << "Holder__ = " << writeValue((*q)->type()) << ";";
                    outHolders.push_back(arg);
                    arg = "out " + arg + "Holder__";
                }
                delArgs.push_back(arg);
            }
            
            if(ret)
            {
                _M << nl << retS << " result__ = " << writeValue(ret) << ";";
            }
       
            if(!throws.empty())
            {
                _M << nl << "Ice.UserException userException__ = null;";
            }

            _M << nl << "IceInternal.Direct.RunDelegate run__ = delegate(Ice.Object obj__)";
            _M << sb;
            _M << nl << fixId(name) << " servant__ = null;";
            _M << nl << "try";
            _M << sb;
            _M << nl << "servant__ = (" << fixId(name) << ")obj__;";
            _M << eb;
            _M << nl << "catch(_System.InvalidCastException)";
            _M << sb;
            _M << nl << "throw new Ice.OperationNotExistException(current__.id, current__.facet, current__.operation);";
            _M << eb;

            if(!throws.empty())
            {
                _M << nl << "try";
                _M << sb;
            }

            _M << nl;
            if(ret)
            {
                _M << "result__ = ";
            }
           
            _M << "servant__." << opName << spar << delArgs << "current__" << epar << ';';
            _M << nl << "return Ice.DispatchStatus.DispatchOK;";
            
            if(!throws.empty())
            {
                _M << eb;
                _M << nl << "catch(Ice.UserException ex__)";
                _M << sb;
                _M << nl << "userException__ = ex__;";
                _M << nl << "return Ice.DispatchStatus.DispatchUserException;";
                _M << eb;
            }

            _M << eb;
            _M << ";";

            _M << nl << "IceInternal.Direct direct__ = null;";
            _M << nl << "try";
            _M << sb;
            _M << nl << "direct__ = new IceInternal.Direct(current__, run__);";

            _M << nl << "try";
            _M << sb;
            
            _M << nl << "Ice.DispatchStatus status__ = direct__.servant().collocDispatch__(direct__);";
            if(!throws.empty())
            {
                _M << nl << "if(status__ == Ice.DispatchStatus.DispatchUserException)";
                _M << sb;
                _M << nl << "throw userException__;";
                _M << eb;
            }
            _M << nl << "_System.Diagnostics.Debug.Assert(status__ == Ice.DispatchStatus.DispatchOK);";

            _M << eb;
            _M << nl << "finally";
            _M << sb;
            _M << nl << "direct__.destroy();";
            _M << eb;
            _M << eb;

            for(ExceptionList::const_iterator i = throws.begin(); i != throws.end(); ++i)
            {
                _M << nl << "catch(" << fixId((*i)->scoped()) << ')';
                _M << sb;
                _M << nl << "throw;";
                _M << eb;
            }
            _M << nl << "catch(Ice.SystemException)";
            _M << sb;
            _M << nl << "throw;";
            _M << eb;
            _M << nl << "catch(System.Exception ex__)";
            _M << sb;
            _M << nl << "IceInternal.LocalExceptionWrapper.throwWrapper(ex__);";
            _M << eb;
            
            //
            //
            // Set out parameters
            //
            for(vector<string>::iterator s = outHolders.begin(); s != outHolders.end(); ++s)
            {
                _M << nl << (*s) << " = " << (*s) << "Holder__;";
            }
            if(ret && !containingClass->hasMetaData("amd") && !op->hasMetaData("amd"))
            {
                _M << nl << "return result__;";
            }
        }
        _M << eb;
    }

    return true;
}

void
Slice::Gen::DelegateDVisitor::visitClassDefEnd(const ClassDefPtr&)
{
    _M << eb;
}

Slice::Gen::DispatcherVisitor::DispatcherVisitor(::IceUtilInternal::Output &out, bool stream)
    : ObjCVisitor(out, out), _stream(stream)
{
}

bool
Slice::Gen::DispatcherVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
        return false;
    }

    _M << sp << nl << "namespace " << fixId(p->name());
    _M << sb;
    return true;
}

void
Slice::Gen::DispatcherVisitor::visitModuleEnd(const ModulePtr&)
{
    _M << eb;
}

bool
Slice::Gen::DispatcherVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal() || !p->isInterface())
    {
        return false;
    }

    string name = p->name();

    _M << sp << nl << "public abstract class " << name << "Disp_ : Ice.ObjectImpl, " << fixId(name);
    _M << sb;

    OperationList ops = p->operations();
    if(!ops.empty())
    {
        _M << sp << nl << "#region Slice operations";
    }

    for(OperationList::const_iterator op = ops.begin(); op != ops.end(); ++op)
    {
        bool amd = p->hasMetaData("amd") || (*op)->hasMetaData("amd");

        string opname = (*op)->name();
        vector<string> params;
        vector<string> args;
        TypePtr ret;

        if(amd)
        {
            opname = opname + "_async";
            params = getParamsAsync(*op, true);
            args = getArgsAsync(*op);
        }
        else
        {
            opname = fixId(opname, DotNet::ICloneable, true);
            //params = getParams(*op);
            ret = (*op)->returnType();
            args = getArgs(*op);
        }

        _M << sp << nl << "public " << typeToString(ret) << " " << opname << spar << params << epar;
        _M << sb << nl;
        if(ret)
        {
            _M << "return ";
        }
        _M << opname << spar << args << "Ice.ObjectImpl.defaultCurrent" << epar << ';';
        _M << eb;

        _M << sp << nl << "public abstract " << typeToString(ret) << " " << opname << spar << params;
        if(!p->isLocal())
        {
            _M << "Ice.Current current__";
        }
        _M << epar << ';';
    }

    if(!ops.empty())
    {
        _M << sp << nl << "#endregion"; // Slice operations
    }

    writeInheritedOperations(p);

    writeDispatchAndMarshalling(p, _stream);

    _M << eb;

    return true;
}

Slice::Gen::AsyncVisitor::AsyncVisitor(::IceUtilInternal::Output &out)
    : ObjCVisitor(out, out)
{
}

bool
Slice::Gen::AsyncVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasAsyncOps())
    {
        return false;
    }

    _M << sp << nl << "namespace " << fixId(p->name());
    _M << sb;
    return true;
}

void
Slice::Gen::AsyncVisitor::visitModuleEnd(const ModulePtr&)
{
    _M << eb;
}

bool
Slice::Gen::AsyncVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    return true;
}

void
Slice::Gen::AsyncVisitor::visitClassDefEnd(const ClassDefPtr&)
{
}

void
Slice::Gen::AsyncVisitor::visitOperation(const OperationPtr& p)
{
    ContainerPtr container = p->container();
    ClassDefPtr cl = ClassDefPtr::dynamicCast(container);
    
    if(cl->isLocal())
    {
        return;
    }

    string name = p->name();
    
    if(cl->hasMetaData("ami") || p->hasMetaData("ami"))
    {

        TypePtr ret = p->returnType();
        string retS = typeToString(ret);

        TypeStringList inParams;
        TypeStringList outParams;
        ParamDeclList paramList = p->parameters();
        for(ParamDeclList::const_iterator pli = paramList.begin(); pli != paramList.end(); ++pli)
        {
            if((*pli)->isOutParam())
            {
                outParams.push_back(make_pair((*pli)->type(), (*pli)->name()));
            }
            else
            {
                inParams.push_back(make_pair((*pli)->type(), (*pli)->name()));
            }
        }

        ExceptionList throws = p->throws();
        throws.sort();
        throws.unique();

        //
        // Arrange exceptions into most-derived to least-derived order. If we don't
        // do this, a base exception handler can appear before a derived exception
        // handler, causing compiler warnings and resulting in the base exception
        // being marshaled instead of the derived exception.
        //
#if defined(__SUNPRO_CC)
        throws.sort(Slice::derivedToBaseCompare);
#else
        throws.sort(Slice::DerivedToBaseCompare());
#endif

        TypeStringList::const_iterator q;

        vector<string> params = getParamsAsyncCB(p);
        vector<string> args = getArgsAsyncCB(p);

        vector<string> paramsInvoke = getParamsAsync(p, false);

        _M << sp << nl << "public abstract class AMI_" << cl->name() << '_'
             << name << " : IceInternal.OutgoingAsync";
        _M << sb;
        _M << sp;
        _M << nl << "public abstract void ice_response" << spar << params << epar << ';';
        
        _M << sp << nl << "public bool invoke__" << spar << "Ice.ObjectPrx prx__"
             << paramsInvoke << "_System.Collections.Generic.Dictionary<string, string> ctx__" << epar;
        _M << sb;
        _M << nl << "acquireCallback__(prx__);";
        _M << nl << "try";
        _M << sb;
        if(p->returnsData())
        {
            _M << nl << "((Ice.ObjectPrxHelperBase)prx__).checkTwowayOnly__(\"" << p->name() << "\");";
        }
        _M << nl << "prepare__(prx__, \"" << name << "\", " << sliceModeToIceMode(p->sendMode()) << ", ctx__);";
        for(q = inParams.begin(); q != inParams.end(); ++q)
        {
            string typeS = typeToString(q->first);
            writeMarshalUnmarshalCode(_M, q->first, fixId(q->second), true, false, false);
        }
        if(p->sendsClasses())
        {
            _M << nl << "os__.writePendingObjects();";
        }
        _M << nl << "os__.endWriteEncaps();";
        _M << nl << "return send__();";
        _M << eb;
        _M << nl << "catch(Ice.LocalException ex__)";
        _M << sb;
        _M << nl << "releaseCallback__(ex__);";
        _M << nl << "return false;";
        _M << eb;
        _M << eb;

        _M << sp << nl << "protected override void response__(bool ok__)";
        _M << sb;
        for(q = outParams.begin(); q != outParams.end(); ++q)
        {
            string param = fixId(q->second);
            string typeS = typeToString(q->first);
            _M << nl << typeS << ' ' << param << ';';
        }
        if(ret)
        {
            _M << nl << retS << " ret__;";
        }
        _M << nl << "try";
        _M << sb;
        _M << nl << "if(!ok__)";
        _M << sb;
        _M << nl << "try";
        _M << sb;
        _M << nl << "throwUserException__();";
        _M << eb;
        for(ExceptionList::const_iterator r = throws.begin(); r != throws.end(); ++r)
        {
            _M << nl << "catch(" << fixId((*r)->scoped()) << " ex__)";
            _M << sb;
            _M << nl << "exception__(ex__);";
            _M << eb;
        }
        _M << nl << "catch(Ice.UserException ex__)";
        _M << sb;
        _M << nl << "throw new Ice.UnknownUserException(ex__.ice_name(), ex__);";
        _M << eb;
        _M << "return;";
        _M << eb;
        if(p->returnsData())
        {
            _M << nl << "is__.startReadEncaps();";
            for(q = outParams.begin(); q != outParams.end(); ++q)
            {
                string param = fixId(q->second);
                StructPtr st = StructPtr::dynamicCast(q->first);
                if(st)
		{
                    if(isValueType(st))
                    {
                        _M << nl << param << " = new " << typeToString(q->first) << "();";
                    }
                    else
                    {
                        _M << nl << param << " = null;";
                    }
		}
                writeMarshalUnmarshalCode(_M, q->first, fixId(q->second), false, false, true);
            }
            if(ret)
            {
                StructPtr st = StructPtr::dynamicCast(ret);
                if(st)
                {
                    if(isValueType(ret))
                    {
                        _M << nl << "ret__ = new " << retS << "();";
                    }
                    else
                    {
                        _M << nl << "ret__ = null;";
                    }
                }
                writeMarshalUnmarshalCode(_M, ret, "ret__", false, false, true);
            }
            if(p->returnsClasses())
            {
                _M << nl << "is__.readPendingObjects();";
            }
            _M << nl << "is__.endReadEncaps();";
            for(q = outParams.begin(); q != outParams.end(); ++q)
            {
                string param = fixId(q->second);
                BuiltinPtr builtin = BuiltinPtr::dynamicCast(q->first);
                if((builtin && builtin->kind() == Builtin::KindObject) || ClassDeclPtr::dynamicCast(q->first))
                {
                    string type = typeToString(q->first);
                    _M << nl << param << " = (" << type << ")" << param << "_PP.value;";
                }
            }
            if(ret)
            {
                BuiltinPtr builtin = BuiltinPtr::dynamicCast(ret);
                if((builtin && builtin->kind() == Builtin::KindObject) || ClassDeclPtr::dynamicCast(ret))
                {
                    string type = typeToString(ret);
                    _M << nl << "ret__ = (" << retS << ")ret___PP.value;";
                }
            }
        }
        else
        {
            _M << nl << "is__.skipEmptyEncaps();";
        }
        _M << eb;
        _M << nl << "catch(Ice.LocalException ex__)";
        _M << sb;
        _M << nl << "finished__(ex__);";
        _M << nl << "return;";
        _M << eb;
        _M << nl << "ice_response" << spar << args << epar << ';';
        _M << nl << "releaseCallback__();";
        _M << eb;
        _M << eb;
    }

    if(cl->hasMetaData("amd") || p->hasMetaData("amd"))
    {
        string classNameAMD = "AMD_" + cl->name();
        string classNameAMDI = "_AMD_" + cl->name();

        vector<string> paramsAMD = getParamsAsyncCB(p);

        _M << sp << nl << "public interface " << classNameAMD << '_' << name;
        _M << sb;
        _M << sp << nl << "void ice_response" << spar << paramsAMD << epar << ';';
        _M << sp << nl << "void ice_exception(_System.Exception ex);";
        _M << eb;
    
        TypePtr ret = p->returnType();
        
        TypeStringList outParams;
        ParamDeclList paramList = p->parameters();
        for(ParamDeclList::const_iterator pli = paramList.begin(); pli != paramList.end(); ++pli)
        {
            if((*pli)->isOutParam())
            {
                outParams.push_back(make_pair((*pli)->type(), (*pli)->name()));
            }
        }
        
        ExceptionList throws = p->throws();
        throws.sort();
        throws.unique();

        //
        // Arrange exceptions into most-derived to least-derived order. If we don't
        // do this, a base exception handler can appear before a derived exception
        // handler, causing compiler warnings and resulting in the base exception
        // being marshaled instead of the derived exception.
        //
#if defined(__SUNPRO_CC)
        throws.sort(Slice::derivedToBaseCompare);
#else
        throws.sort(Slice::DerivedToBaseCompare());
#endif

        TypeStringList::const_iterator q;
        _M << sp << nl << "class " << classNameAMDI << '_' << name
            << " : IceInternal.IncomingAsync, " << classNameAMD << '_' << name;
        _M << sb;

        _M << sp << nl << "public " << classNameAMDI << '_' << name << "(IceInternal.Incoming inc) : base(inc)";
        _M << sb;
        _M << eb;

        _M << sp << nl << "public void ice_response" << spar << paramsAMD << epar;
        _M << sb;
        _M << nl << "if(validateResponse__(true))";
        _M << sb;
        if(ret || !outParams.empty())
        {
            _M << nl << "try";
            _M << sb;
            _M << nl << "IceInternal.BasicStream os__ = this.os__();";
            for(q = outParams.begin(); q != outParams.end(); ++q)
            {
                string typeS = typeToString(q->first);
                writeMarshalUnmarshalCode(_M, q->first, fixId(q->second), true, false, false);
            }
            if(ret)
            {
                string retS = typeToString(ret);
                writeMarshalUnmarshalCode(_M, ret, "ret__", true, false, false);
            }
            if(p->returnsClasses())
            {
                _M << nl << "os__.writePendingObjects();";
            }
            _M << eb;
            _M << nl << "catch(Ice.LocalException ex__)";
            _M << sb;
            _M << nl << "ice_exception(ex__);";
            _M << eb;
        }
        _M << nl << "response__(true);";
        _M << eb;
        _M << eb;

        _M << sp << nl << "public void ice_exception(_System.Exception ex)";
        _M << sb;
        if(throws.empty())
        {
            _M << nl << "if(validateException__(ex))";
            _M << sb;
            _M << nl << "exception__(ex);";
            _M << eb;
        }
        else
        {
            _M << nl << "try";
            _M << sb;
            _M << nl << "throw ex;";
            _M << eb;
            ExceptionList::const_iterator r;
            for(r = throws.begin(); r != throws.end(); ++r)
            {
                string exS = fixId((*r)->scoped());
                _M << nl << "catch(" << exS << " ex__)";
                _M << sb;
                _M << nl << "if(validateResponse__(false))";
                _M << sb;
                _M << nl << "os__().writeUserException(ex__);";
                _M << nl << "response__(false);";
                _M << eb;
                _M << eb;
            }
            _M << nl << "catch(_System.Exception ex__)";
            _M << sb;
            _M << nl << "exception__(ex__);";
            _M << eb;
        }
        _M << eb;

        _M << eb;
    }
}

Slice::Gen::TieVisitor::TieVisitor(IceUtilInternal::Output& out)
    : ObjCVisitor(out, out)
{
}

bool
Slice::Gen::TieVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDefs())
    {
        return false;
    }

    _M << sp << nl << "namespace " << fixId(p->name());
    _M << sb;

    return true;
}

void
Slice::Gen::TieVisitor::visitModuleEnd(const ModulePtr&)
{
    _M << eb;
}

bool
Slice::Gen::TieVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(!p->isAbstract())
    {
        return false;
    }
    
    string name = p->name();
    string opIntfName = "Operations";
    if(p->isLocal())
    {
        opIntfName += "NC";
    }

    _M << sp << nl << "public class " << name << "Tie_ : ";
    if(p->isInterface())
    {
        if(p->isLocal())
        {
            _M << fixId(name) << ", Ice.TieBase";
        }
        else
        {
            _M << name << "Disp_, Ice.TieBase";
        }
    }
    else
    {
        _M << fixId(name) << ", Ice.TieBase";
    }
    _M << sb;

    _M << sp << nl << "public " << name << "Tie_()";
    _M << sb;
    _M << eb;

    _M << sp << nl << "public " << name << "Tie_(" << name << opIntfName << "_ del)";
    _M << sb;
    _M << nl << "_ice_delegate = del;";
    _M << eb;

    _M << sp << nl << "public object ice_delegate()";
    _M << sb;
    _M << nl << "return _ice_delegate;";
    _M << eb;

    _M << sp << nl << "public void ice_delegate(object del)";
    _M << sb;
    _M << nl << "_ice_delegate = (" << name << opIntfName << "_)del;";
    _M << eb;

    _M << sp << nl << "public ";
    if(!p->isInterface() || !p->isLocal())
    {
        _M << "override ";
    }
    _M << "int ice_hash()";

    _M << sb;
    _M << nl << "return GetHashCode();";
    _M << eb;

    _M << sp << nl << "public override int GetHashCode()";
    _M << sb;
    _M << nl << "return _ice_delegate == null ? 0 : _ice_delegate.GetHashCode();";
    _M << eb;

    _M << sp << nl << "public override bool Equals(object rhs)";
    _M << sb;
    _M << nl << "if(object.ReferenceEquals(this, rhs))";
    _M << sb;
    _M << nl << "return true;";
    _M << eb;
    _M << nl << "if(!(rhs is " << name << "Tie_))";
    _M << sb;
    _M << nl << "return false;";
    _M << eb;
    _M << nl << "if(_ice_delegate == null)";
    _M << sb;
    _M << nl << "return ((" << name << "Tie_)rhs)._ice_delegate == null;";
    _M << eb;
    _M << nl << "return _ice_delegate.Equals(((" << name << "Tie_)rhs)._ice_delegate);";
    _M << eb;

    OperationList ops = p->operations();
    OperationList::const_iterator r;
    for(r = ops.begin(); r != ops.end(); ++r)
    {
        bool hasAMD = p->hasMetaData("amd") || (*r)->hasMetaData("amd");
        string opName = hasAMD ? (*r)->name() + "_async" : fixId((*r)->name(), DotNet::ICloneable, true);

        TypePtr ret = (*r)->returnType();
        string retS = typeToString(ret);

        vector<string> params;
        vector<string> args;
        if(hasAMD)
        {
            params = getParamsAsync((*r), true);
            args = getArgsAsync(*r);
        }
        else
        {
            //params = getParams(*r);
            args = getArgs(*r);
        }

        _M << sp << nl << "public ";
        if(!p->isInterface() || !p->isLocal())
        {
            _M << "override ";
        }
        _M << (hasAMD ? string("void") : retS) << ' ' << opName << spar << params;
        if(!p->isLocal())
        {
            _M << "Ice.Current current__";
        }
        _M << epar;
        _M << sb;
        _M << nl;
        if(ret && !hasAMD)
        {
            _M << "return ";
        }
        _M << "_ice_delegate." << opName << spar << args;
        if(!p->isLocal())
        {
            _M << "current__";
        }
        _M << epar << ';';
        _M << eb;
    }

    NameSet opNames;
    ClassList bases = p->bases();
    for(ClassList::const_iterator i = bases.begin(); i != bases.end(); ++i)
    {
        writeInheritedOperationsWithOpNames(*i, opNames);
    }

    _M << sp << nl << "private " << name << opIntfName << "_ _ice_delegate;";

    return true;
}

void
Slice::Gen::TieVisitor::visitClassDefEnd(const ClassDefPtr&)
{
    _M << eb;
}

void
Slice::Gen::TieVisitor::writeInheritedOperationsWithOpNames(const ClassDefPtr& p, NameSet& opNames)
{
    OperationList ops = p->operations();
    OperationList::const_iterator r;
    for(r = ops.begin(); r != ops.end(); ++r)
    {
        bool hasAMD = p->hasMetaData("amd") || (*r)->hasMetaData("amd");
        string opName = hasAMD ? (*r)->name() + "_async" : fixId((*r)->name(), DotNet::ICloneable, true);
        if(opNames.find(opName) != opNames.end())
        {
            continue;
        }
        opNames.insert(opName);

        TypePtr ret = (*r)->returnType();
        string retS = typeToString(ret);

        vector<string> params;
        vector<string> args;
        if(hasAMD)
        {
            params = getParamsAsync((*r), true);
            args = getArgsAsync(*r);
        }
        else
        {
            //params = getParams(*r);
            args = getArgs(*r);
        }

        _M << sp << nl << "public ";
        if(!p->isInterface() || !p->isLocal())
        {
            _M << "override ";
        }
        _M << (hasAMD ? string("void") : retS) << ' ' << opName << spar << params;
        if(!p->isLocal())
        {
            _M << "Ice.Current current__";
        }
        _M << epar;
        _M << sb;
        _M << nl;
        if(ret && !hasAMD)
        {
            _M << "return ";
        }
        _M << "_ice_delegate." << opName << spar << args;
        if(!p->isLocal())
        {
            _M << "current__";
        }
        _M << epar << ';';
        _M << eb;
    }

    ClassList bases = p->bases();
    for(ClassList::const_iterator i = bases.begin(); i != bases.end(); ++i)
    {
        writeInheritedOperationsWithOpNames(*i, opNames);
    }
}

Slice::Gen::BaseImplVisitor::BaseImplVisitor(IceUtilInternal::Output& out)
    : ObjCVisitor(out, out)
{
}

void
Slice::Gen::BaseImplVisitor::writeOperation(const OperationPtr& op, bool comment, bool forTie)
{
    ClassDefPtr cl = ClassDefPtr::dynamicCast(op->container());
    string opName = op->name();
    TypePtr ret = op->returnType();
    string retS = typeToString(ret);
    ParamDeclList params = op->parameters();

    if(comment)
    {
        _M << nl << "// ";
    }
    else
    {
        _M << sp << nl;
    }

        ParamDeclList::const_iterator i;
    if(!cl->isLocal() && (cl->hasMetaData("amd") || op->hasMetaData("amd")))
    {
        ParamDeclList::const_iterator i;
        vector<string> pDecl = getParamsAsync(op, true);

        _M << "public ";
        if(!forTie)
        {
            _M << "override ";
        }
        _M << "void " << opName << "_async" << spar << pDecl << "Ice.Current current__" << epar;

        if(comment)
        {
            _M << ';';
            return;
        }

        _M << sb;
        if(ret)
        {
            _M << nl << typeToString(ret) << " ret__ = " << writeValue(ret) << ';';
        }
        for(i = params.begin(); i != params.end(); ++i)
        {
            if((*i)->isOutParam())
            {
                string name = fixId((*i)->name());
                TypePtr type = (*i)->type();
                _M << nl << typeToString(type) << ' ' << name << " = " << writeValue(type) << ';';
            }
        }
        _M << nl << "cb__.ice_response" << spar;
        if(ret)
        {
            _M << "ret__";
        }
        for(i = params.begin(); i != params.end(); ++i)
        {
            if((*i)->isOutParam())
            {
                _M << fixId((*i)->name());
            }
        }
        _M << epar << ';';
        _M << eb;
    }
    else
    {
        vector<string> pDecls;// = getParams(op);

        _M << "public ";
        if(!forTie && !cl->isLocal())
        {
            _M << "override ";
        }
        _M << retS << ' ' << fixId(opName, DotNet::ICloneable, true) << spar << pDecls;
        if(!cl->isLocal())
        {
            _M << "Ice.Current current__";
        }
        _M << epar;
        if(comment)
        {
            _M << ';';
            return;
        }
        _M << sb;
        for(ParamDeclList::const_iterator i = params.begin(); i != params.end(); ++i)
        {
            if((*i)->isOutParam())
            {
                string name = fixId((*i)->name());
                TypePtr type = (*i)->type();
                _M << nl << name << " = " << writeValue(type) << ';';
            }
        }
        if(ret)
        {
            _M << nl << "return " << writeValue(ret) << ';';
        }
        _M << eb;
    }
}


Slice::Gen::ImplVisitor::ImplVisitor(IceUtilInternal::Output& out)
    : BaseImplVisitor(out)
{
}

bool
Slice::Gen::ImplVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDefs())
    {
        return false;
    }

    _M << sp << nl << "namespace " << fixId(p->name());
    _M << sb;

    return true;
}

void
Slice::Gen::ImplVisitor::visitModuleEnd(const ModulePtr&)
{
    _M << eb;
}

bool
Slice::Gen::ImplVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(!p->isAbstract())
    {
        return false;
    }

    string name = p->name();

    _M << sp << nl << "public sealed class " << name << 'I';
    if(p->isInterface())
    {
        if(p->isLocal())
        {
            _M << " : " << fixId(name);
        }
        else
        {
            _M << " : " << name << "Disp_";
        }
    }
    else
    {
        _M << " : " << fixId(name);
    }
    _M << sb;

    OperationList ops = p->allOperations();
    for(OperationList::const_iterator r = ops.begin(); r != ops.end(); ++r)
    {
        writeOperation(*r, false, false);
    }

    return true;
}

void
Slice::Gen::ImplVisitor::visitClassDefEnd(const ClassDefPtr&)
{
    _M << eb;
}

Slice::Gen::ImplTieVisitor::ImplTieVisitor(IceUtilInternal::Output& out)
    : BaseImplVisitor(out)
{
}

bool
Slice::Gen::ImplTieVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDefs())
    {
        return false;
    }

    _M << sp << nl << "namespace " << fixId(p->name());
    _M << sb;

    return true;
}

void
Slice::Gen::ImplTieVisitor::visitModuleEnd(const ModulePtr&)
{
    _M << eb;
}

bool
Slice::Gen::ImplTieVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(!p->isAbstract())
    {
        return false;
    }

    string name = p->name();
    ClassList bases = p->bases();

    //
    // Use implementation inheritance in the following situations:
    //
    // * if a class extends another class
    // * if a class implements a single interface
    // * if an interface extends only one interface
    //
    bool inheritImpl = (!p->isInterface() && !bases.empty() && !bases.front()->isInterface()) || (bases.size() == 1);

    _M << sp << nl << "public class " << name << "I : ";
    if(inheritImpl)
    {
        if(bases.front()->isAbstract())
        {
            _M << bases.front()->name() << 'I';
        }
        else
        {
            _M << fixId(bases.front()->name());
        }
        _M << ", ";
    }
    _M << name << "Operations";
    if(p->isLocal())
    {
        _M << "NC";
    }
    _M << '_';
    _M << sb;

    _M << nl << "public " << name << "I()";
    _M << sb;
    _M << eb;

    OperationList ops = p->allOperations();
    ops.sort();

    OperationList baseOps;
    if(inheritImpl)
    {
        baseOps = bases.front()->allOperations();
        baseOps.sort();
    }

    OperationList::const_iterator r;
    for(r = ops.begin(); r != ops.end(); ++r)
    {
        if(inheritImpl && binary_search(baseOps.begin(), baseOps.end(), *r))
        {
            _M << sp;
            _M << nl << "// ";
            _M << nl << "// Implemented by " << bases.front()->name() << 'I';
            _M << nl << "//";
            writeOperation(*r, true, true);
        }
        else
        {
            writeOperation(*r, false, true);
        }
    }

    _M << eb;

    return true;
}
