// **********************************************************************
//
// Copyright (c) 2003-2012 ZeroC, Inc. All rights reserved.
//
// This copy of Ice Touch is licensed to you under the terms described in the
// ICE_TOUCH_LICENSE file included in this distribution.
//
// **********************************************************************

#import <UIKit/UIKit.h>

@class TestViewController;

@interface AppDelegate : NSObject <UIApplicationDelegate>
{
@private
    UIWindow *window;
    IBOutlet UINavigationController *navigationController;
    NSArray* tests;
    int currentTest;
    BOOL ssl;
    BOOL loop;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) IBOutlet UINavigationController* navigationController;
@property (nonatomic, readonly) NSArray* tests;
@property (nonatomic) int currentTest;
@property (nonatomic) BOOL ssl;
@property (nonatomic) BOOL loop;

-(void)testCompleted:(BOOL)success;

@end

