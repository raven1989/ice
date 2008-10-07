// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_TOUCH_LICENSE file included in this distribution.
//
// **********************************************************************

#import <Ice/Config.h>

@protocol ICEObjectFactory <NSObject>
-(ICEObject*) create:(NSString*)sliceId;
-(void) destroy;
@end

