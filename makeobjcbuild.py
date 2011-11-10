#!/usr/bin/env python
# **********************************************************************
#
# Copyright (c) 2003-2011 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

import os, sys, fnmatch, re, getopt, stat

if not os.geteuid() == 0:
    print "Script must be run as root - prefix command with 'sudo'"
    sys.exit(1)


rootDir = os.path.abspath(os.path.dirname(__file__))

sys.path.append(os.path.join(rootDir, "distribution", "lib"))
from DistUtils import *

def writeBinDistReport(product, version):

    cwd = os.getcwd()
    os.chdir(cwd)

    print "Writing report in README...",
    readme = open("README", "w")
    print >>readme, "This directory contains the binary distributions of " + product + " " + version + ".\n"
    print >>readme, "Creation time: " + time.strftime("%a %b %d %Y, %I:%M:%S %p (%Z)")
    (sysname, nodename, release, ver, machine) = os.uname();
    print >>readme, "Host: " + nodename
    print >>readme, "Platform: " + sysname + " " + release
    print >>readme, "User: " + os.environ["USER"]
    print >>readme, ""

    print "ok"
        
    readme.close()

    os.chdir(cwd)

#
# Program usage.
#
def usage():
    print "Usage: " + sys.argv[0] + " [options] tag"
    print
    print "Options:"
    print "-h                   Show this message."
    print "-v                   Be verbose."
    print "--xcode-path         Xcode install path. Default is '/Developer'"
    print "--ios-min-version    The minimum iOS SDK version supported by targets, Default is '4.2', supported versions are 4.2, 4.3 and 5.0"

#
# Check arguments
#
verbose = 0
tag = "HEAD"
xcodeVersion = "42";
xcodePath = "/Developer"
iOSVersion = "5.0"
iOSMinSDKVersion = "4.2"

try:
    opts, args = getopt.getopt(sys.argv[1:], "hv", ["xcode-version=", "xcode-path=", "ios-version="])
except getopt.GetoptError:
    usage()
    sys.exit(1)

for o, a in opts:
    if o == "-h":
        usage()
        sys.exit(0)
    elif o == "-v":
        verbose = 1
    elif o == "--xcode-path":
        xcodePath = a;
    elif o == "--ios-min-version":
        iOSMinSDKVersion = a;

if iOSMinSDKVersion != "5.0" and iOSMinSDKVersion != "4.3" and iOSMinSDKVersion != "4.2":
    usage()
    sys.exit(1)

if len(args) > 1:
    usage()
    sys.exit(1)

if len(args) == 1:
    tag = args[0]

cwd = os.getcwd()
os.chdir(rootDir)

# Get IceTouch version.
config = open(os.path.join("cpp", "config", "Make.rules.objc"), "r")
version = re.search("VERSION[\s]*= ([0-9\.]*)", config.read()).group(1)
mmversion = re.search("([0-9]+\.[0-9b]+)[\.0-9]*", version).group(1)
versionMinor = re.search("([0-9\.]*).([0-9\.]*)", version).group(2)
versionMajor = re.search("([0-9\.]*).([0-9\.]*)", version).group(1)


xcodeVersionMajor = xcodeVersion[0:1]
xcodeVersionMinor = xcodeVersion[1:2]
xcodeVersionString = xcodeVersionMajor + "." + xcodeVersionMinor

installerProject = "IceTouch-" + version + "-xcode-" + xcodeVersionString +".pmdoc"
installerReadme = "installer-xcode-" + xcodeVersionString + "-readme.rtf"

volname = "IceTouch " + version + " Xcode " + xcodeVersionString
basePackageName = "IceTouch-" + version + "-Xcode-" + xcodeVersionString

# First check to see whether the disk image is already accidently mounted.
if not os.system("mount | grep \"%s (\" 2>&1 > /dev/null" % volname):
    print "\"/Volumes/%s\": already mounted. Unmount and restart." % volname
    sys.exit(1)

#
# Get the distDir
#
distDir = os.path.abspath(os.path.join(rootDir, "..", "disticetouch-" + tag.replace('/', '-')))
baseDir = os.path.abspath(os.path.join(rootDir, "..", "buildicetouch-" + tag.replace('/', '-'))) + "-xcode-" + xcodeVersion
if not os.path.exists(baseDir):
    os.mkdir(baseDir)

# Create a symlink for use with PackageMaker.
latestBuildDir = os.path.abspath(os.path.join(rootDir, "..", "buildicetouch-LATEST"))
try:
    os.remove(latestBuildDir)
except OSError:
    pass
os.system("ln -s %s %s" % (baseDir, latestBuildDir))

buildDir = os.path.join(baseDir, "build")

print "Building " + version + " distributions in " + baseDir

os.chdir(os.path.join(baseDir))

if not os.path.exists(buildDir):
    print "Extracting archive...",
    sys.stdout.flush()
    baseName = os.path.join(distDir, "IceTouch-" + version)
    os.system("tar xfz " + os.path.join(distDir, baseName + ".tar.gz"))
    os.rename("IceTouch-" + version, "build")
    # Change SUBDIRS in top-level Makefile.
    substitute(os.path.join("build", "objc", "Makefile"), [(r'^SUBDIRS([\s]*)=.*', r'SUBDIRS\1= src include')])
    print "ok"
else:
    print "Using existing build directory"
    s1 = os.stat(buildDir)
    s2 = os.stat(os.path.join(distDir, "IceTouch-" + version + ".tar.gz"))
    if s2.st_mtime > s1.st_mtime:
        print "WARNING: distribution is more recent than the existing build"

os.chdir(buildDir)

print "Building distributions...",
sys.stdout.flush()

os.system(("DEVELOPER_PATH=%s OPTIMIZE_SPEED=yes make" % xcodePath))
os.system(("DEVELOPER_PATH=%s OPTIMIZE_SPEED=yes COMPILE_FOR_COCOA=yes make" % xcodePath))
os.system(("DEVELOPER_PATH=%s OPTIMIZE_SIZE=yes IPHONE_TARGET_MIN_SDK_VERSION=%s COMPILE_FOR_IPHONE=yes make" % (xcodePath, iOSMinSDKVersion)))
os.system(("DEVELOPER_PATH=%s OPTIMIZE_SIZE=yes IPHONE_TARGET_MIN_SDK_VERSION=%s COMPILE_FOR_IPHONE_SIMULATOR=yes make" % (xcodePath, iOSMinSDKVersion)))

os.chdir(os.path.join(buildDir, "Xcode", "Slice2ObjcPlugin"))

os.system(("DEVELOPER_PATH=%s XCODE_VERSION=%s make" % (xcodePath, xcodeVersion)))

print "ok"

name = "IceTouch-" + version
installerDir = os.path.join(baseDir, "installer")
developerDir = os.path.join(baseDir, "Developer")
sdkDir = os.path.join(developerDir, "SDKs", name)
cppSdkDir = os.path.join(developerDir, "SDKs", "IceTouchCpp-" + version)
examplesDir = os.path.join(developerDir, "Examples", name)
docDir = os.path.join(developerDir, "Documentation", name)
optDir = os.path.join(baseDir, "opt", name)

os.system("rm -rfv " + developerDir)
os.system("rm -rfv " + installerDir)
os.system("rm -rfv " + os.path.join(baseDir, "slice2objcplugin.pbplugin"))
os.system("rm -rfv " + optDir)

print "Preparing installer...",
sys.stdout.flush()

if not os.path.exists(installerDir):
    os.mkdir(installerDir)
copy(os.path.join(rootDir, "distribution", "src", "mac", "IceTouch", installerReadme),
     os.path.join(installerDir, "readme.rtf"))
copy(os.path.join(rootDir, "distribution", "src", "mac", "IceTouch", "uninstall.sh"),
     os.path.join(installerDir, "uninstall.sh"))

os.makedirs(optDir)
os.chdir(buildDir)
os.system("prefix=%s create_runpath_symlink=no make install" % optDir)

os.makedirs(docDir)

move(os.path.join(optDir, "ICE_TOUCH_LICENSE"), os.path.join(docDir, "ICE_TOUCH_LICENSE"))
move(os.path.join(optDir, "LICENSE"), os.path.join(docDir, "LICENSE"))
for f in [ "CHANGES", "RELEASE_NOTES"]:
    copy(os.path.join(rootDir, "objc", f), os.path.join(docDir, f))
copy(os.path.join(rootDir, "distribution", "src", "mac", "IceTouch", "README"),
     os.path.join(docDir, "README"))

copy(os.path.join(buildDir, "objc", "SDK", "IceTouch-" + version), sdkDir)
copy(os.path.join(buildDir, "objc", "SDK", "IceTouchCpp-" + version), cppSdkDir)

copy(os.path.join(buildDir, "Xcode", "Slice2ObjcPlugin", "build", "Release", "slice2objcplugin.pbplugin"),
     os.path.join(baseDir, "slice2objcplugin.pbplugin"))

os.makedirs(os.path.join(developerDir, "Examples"))
os.chdir(os.path.join(developerDir, "Examples"))
os.system("tar xfz %s" % os.path.join(distDir, "IceTouch-" + version + "-demos.tar.gz"))
os.rename("IceTouch-" + version + "-demos", name)
os.system("chmod -R g+wX %s" % examplesDir)
os.remove(os.path.join(examplesDir, "ICE_TOUCH_LICENSE"))
os.remove(os.path.join(examplesDir, "ICE_LICENSE"))

# SDK symlinks
os.chdir(os.path.join(developerDir, "SDKs"));
os.system("ln -s IceTouch-%s IceTouch-%s" % (version, mmversion))
os.system("ln -s IceTouchCpp-%s IceTouchCpp-%s" % (version, mmversion))

os.chdir(baseDir)

# Fix iPhone demos iOS SDK and deployment version.

xcodeSDKRootExprs = [ (re.compile("SDKROOT = iphoneos.*;"), "SDKROOT = \"iphoneos%s\";" % iOSVersion) ]
xcodeIPhoneOSDeplyomentExprs = [ (re.compile("IPHONEOS_DEPLOYMENT_TARGET = .*;"), "IPHONEOS_DEPLOYMENT_TARGET = %s;" % iOSMinSDKVersion) ]

for root, dirnames, filesnames in os.walk(os.path.join(examplesDir, "iPhone")):
    for f in filesnames:
        if fnmatch.fnmatch(f, "project.pbxproj"):
            substitute(os.path.join(root, f), xcodeSDKRootExprs)
            substitute(os.path.join(root, f), xcodeIPhoneOSDeplyomentExprs)
print "ok"

print "Fix file permissions",
os.system("chown -R root:admin " + baseDir);
os.system("chmod -R g+w " + baseDir);
print "ok"

print "Creating installer...",
sys.stdout.flush()

pmdoc = os.path.join(rootDir, "distribution", "src", "mac", "IceTouch", installerProject)
os.system(xcodePath + "/usr/bin/packagemaker --doc " + pmdoc + " --out " + latestBuildDir + "/installer/" + basePackageName + ".pkg")

print "ok"

os.chdir(baseDir)

print "Building disk image... " + volname + " ",
sys.stdout.flush()

if os.path.exists("scratch.dmg.sparseimage"):
    os.remove("scratch.dmg.sparseimage")
os.system("hdiutil create scratch.dmg -volname \"%s\" -type SPARSE -fs HFS+" % volname)
os.system("hdid scratch.dmg.sparseimage")
os.system("ditto -rsrc installer \"/Volumes/%s\"" % volname)
os.system("hdiutil detach \"/Volumes/%s\"" % volname)
os.system("hdiutil convert  scratch.dmg.sparseimage -format UDZO -o %s.dmg -imagekey zlib-devel=9" %
          basePackageName)
os.remove("scratch.dmg.sparseimage")
print "ok"

#
# Write source distribution report in README file.
#
writeBinDistReport("IceTouch", version)

os.chdir(cwd)
