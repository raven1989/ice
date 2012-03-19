// **********************************************************************
//
// Copyright (c) 2003-2011 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

namespace IceInternal
{
#if !SILVERLIGHT
    using System.Diagnostics;

    public sealed class Time
    {
        static Time()
        {
            _stopwatch.Start();
        }

        public static long currentMonotonicTimeMillis()
        {
            return _stopwatch.ElapsedMilliseconds;
        }

        private static Stopwatch _stopwatch = new Stopwatch();
    }
#else
    public sealed class Time
    {
        static Time()
        {
            _begin = System.DateTime.Now.Ticks;
        }

        public static long currentMonotonicTimeMillis()
        {
            return (System.DateTime.Now.Ticks - _begin) / 10000;
        }

        private static long _begin;
    }
#endif
}
