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

for toplevel in [".", "..", "../..", "../../..", "../../../.."]:
    toplevel = os.path.normpath(toplevel)
    if os.path.exists(os.path.join(toplevel, "config", "DemoUtil.py")):
        break
else:
    raise "can't find toplevel directory!"
sys.path.append(os.path.join(toplevel, "config"))
import DemoUtil

server = DemoUtil.spawn('./server --Ice.PrintAdapterReady')
server.expect('.* ready')
client = DemoUtil.spawn('./client')
client.expect('.*==>')

print "testing...",
sys.stdout.flush()
client.sendline('1')
server.expect("Printing string `The streaming API works!'")
client.sendline('2')
server.expect("Printing string sequence \\{'The', 'streaming', 'API', 'works!'\\}");
client.sendline('3')
server.expect("Printing dictionary \\{API=works!, The=streaming\\}")
client.sendline('4')
server.expect("Printing enum green")
client.sendline('5')
server.expect("Printing struct: name=red, value=red")
client.sendline('6')
server.expect("Printing struct sequence: \\{red=red, green=green, blue=blue\\}")
client.sendline('7')
server.expect("Printing class: s\\.name=blue, s\\.value=blue")
client.sendline('8')
client.expect("Got string `hello' and class: s\\.name=green, s\\.value=green")
print "ok"

client.sendline('s')
server.expect(pexpect.EOF)

client.sendline('x')
client.expect(pexpect.EOF)
