#!/usr/bin/env python
# **********************************************************************
#
# Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

import pexpect, sys, os

try:
    import demoscript
except ImportError:
    for toplevel in [".", "..", "../..", "../../..", "../../../.."]:
        toplevel = os.path.normpath(toplevel)
        if os.path.exists(os.path.join(toplevel, "demoscript")):
            break
    else:
        raise "can't find toplevel directory!"
    sys.path.append(os.path.join(toplevel))
    import demoscript

import demoscript.Util
import signal

server = demoscript.Util.spawn('./server --Ice.PrintAdapterReady')
server.expect('.* ready')

print "testing...",
sys.stdout.flush()
client = demoscript.Util.spawn('./client')
client.expect('Contents of root directory:\r\n.*Down to a sunless sea.')
client.expect(pexpect.EOF)
assert client.wait() == 0
print "ok"
server.kill(signal.SIGINT)
server.expect(pexpect.EOF)
assert server.wait() == 0
