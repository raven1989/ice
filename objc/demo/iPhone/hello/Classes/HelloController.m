// **********************************************************************
//
// Copyright (c) 2003-2010 ZeroC, Inc. All rights reserved.
//
// This copy of Ice Touch is licensed to you under the terms described in the
// ICE_TOUCH_LICENSE file included in this distribution.
//
// **********************************************************************

#import <HelloController.h>
#import <Ice/Ice.h>
#import <Hello.h>

// Various delivery mode constants
// The simulator does not support SSL.
#if TARGET_IPHONE_SIMULATOR
#   define DeliveryModeTwoway  0
#   define DeliveryModeOneway 1
#   define DeliveryModeOnewayBatch  2
#   define DeliveryModeDatagram 3
#   define DeliveryModeDatagramBatch 4
// These are defined, but invalid.
#   define DeliveryModeTwowaySecure -1
#   define DeliveryModeOnewaySecure -2
#   define DeliveryModeOnewaySecureBatch -3
#   define DeliveryModeTwowayAccessory -4
#   define DeliveryModeOnewayAccessory -5
#   define DeliveryModeOnewayAccessoryBatch -6
#else
#   define DeliveryModeTwoway  0
#   define DeliveryModeTwowaySecure 1
#   define DeliveryModeOneway 2
#   define DeliveryModeOnewayBatch  3
#   define DeliveryModeOnewaySecure 4
#   define DeliveryModeOnewaySecureBatch 5
#   define DeliveryModeDatagram 6
#   define DeliveryModeDatagramBatch 7
#   define DeliveryModeTwowayAccessory 8
#   define DeliveryModeOnewayAccessory 9
#   define DeliveryModeOnewayAccessoryBatch 10
#endif

//
// Avoid warning for undocumented UISlider method
//
@interface UISlider(UndocumentedAPI)
-(void)setShowValue:(BOOL)val;
@end

@implementation HelloController

static NSString* hostnameKey = @"hostnameKey";

+(void)initialize
{
    NSDictionary* appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                 @"127.0.0.1", hostnameKey, nil];
	
    [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];
}

-(void)applicationWillTerminate
{
    [communicator destroy];
}

-(void)viewDidLoad
{
    ICEInitializationData* initData = [ICEInitializationData initializationData];
    initData.properties = [ICEUtil createProperties];

    // The simulator does not support SSL or the accessory transport.
#if !TARGET_IPHONE_SIMULATOR
    [initData.properties setProperty:@"IceSSL.CheckCertName" value:@"0"];
    [initData.properties setProperty:@"IceSSL.CertAuthFile" value:@"cacert.der"];
    [initData.properties setProperty:@"IceSSL.CertFile" value:@"c_rsa1024.pfx"];
    [initData.properties setProperty:@"IceSSL.Password" value:@"password"];
	
    // Configure the accessory transport.
    ICEConfigureAccessoryTransport(initData.properties);
#endif     

	// Dispatch AMI callbacks on the main thread
    initData.dispatcher = ^(id<ICEDispatcherCall> call, id<ICEConnection> con)
    {
        dispatch_sync(dispatch_get_main_queue(), ^ { [call run]; });
    };
	
    communicator = [[ICEUtil createCommunicator:initData] retain];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillTerminate) 
                                                 name:UIApplicationWillTerminateNotification
                                               object:nil]; 
    
    // When the user starts typing, show the clear button in the text field.
    hostnameTextField.clearButtonMode = UITextFieldViewModeWhileEditing;
    
    // Defaults for the UI elements.
    hostnameTextField.text = [[NSUserDefaults standardUserDefaults] stringForKey:hostnameKey];
    flushButton.enabled = NO;
    
    // This generates a compile time warning, but does actually work!
    [delaySlider setShowValue:YES];
    [timeoutSlider setShowValue:YES];
    
    statusLabel.text = @"Ready";
    
    showAlert = NO;
}

-(BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

-(void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning]; // Releases the view if it doesn't have a superview
    // Release anything that's not essential, such as cached data
}

-(void)dealloc
{
    [flushButton release];
    [hostnameTextField release];
    [statusLabel release];
    [timeoutSlider release];
    [delaySlider release];
    [activity release];
    [modePicker release];
    [communicator release];
    
    [super dealloc];
}

#pragma mark UIAlertViewDelegate

-(void)didPresentAlertView:(UIAlertView *)alertView
{
    showAlert = YES;
}

-(void)alertView:(UIAlertView*)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex
{
    showAlert = NO;
}

#pragma mark UITextFieldDelegate

-(BOOL)textFieldShouldReturn:(UITextField *)theTextField
{
    // If we've already showing an invalid hostname alert, then we ignore enter.
    if(showAlert)
    {
        return NO;
    }

    // Close the text field.
    [theTextField resignFirstResponder];
    return YES;
}

-(void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    // Dismiss the keyboard when the view outside the text field is touched.
    [hostnameTextField resignFirstResponder];

    [super touchesBegan:touches withEvent:event];
}

#pragma mark AMI Callbacks

-(void)exception:(ICEException*) ex
{
    [activity stopAnimating];       

    statusLabel.text = @"Ready";

    NSString* s = [NSString stringWithFormat:@"%@", ex];
    // open an alert with just an OK button
    UIAlertView *alert = [[[UIAlertView alloc] initWithTitle:@"Error"
                                                     message:s
                                                    delegate:self
                                           cancelButtonTitle:@"OK"
                                           otherButtonTitles:nil] autorelease];
    [alert show];
}

#pragma mark UI Element Callbacks

-(id<DemoHelloPrx>)createProxy
{
    // The simulator does not support SSL or accessories.
    int deliveryMode = [modePicker selectedRowInComponent:0];
#if TARGET_IPHONE_SIMULATOR
    NSString* s = [NSString stringWithFormat:@"hello:tcp -h %@ -p 10000:udp -h %@ -p 10000",
                   hostnameTextField.text, hostnameTextField.text];
#else
    NSString* s;
	if(deliveryMode == DeliveryModeTwowayAccessory || deliveryMode == DeliveryModeOnewayAccessory || deliveryMode == DeliveryModeOnewayAccessoryBatch)
	{
		s = @"hello:accessory -p com.zeroc.helloWorld";
	}
	else
	{
		s = [NSString stringWithFormat:@"hello:tcp -h %@ -p 10000:ssl -h %@ -p 10001:udp -h %@ -p 10000",
			 hostnameTextField.text, hostnameTextField.text, hostnameTextField.text];
	}
#endif     
    
    [[NSUserDefaults standardUserDefaults] setObject:hostnameTextField.text forKey:hostnameKey];

    ICEObjectPrx* prx = [communicator stringToProxy:s];
    
    switch(deliveryMode)
    {
        case DeliveryModeTwoway:
        case DeliveryModeTwowayAccessory:
            prx = [prx ice_twoway];
            break;
        case DeliveryModeTwowaySecure:
            prx = [[prx ice_twoway] ice_secure:YES];
            break;
        case DeliveryModeOneway:
        case DeliveryModeOnewayAccessory:
            prx = [prx ice_oneway];
            break;
        case DeliveryModeOnewayBatch:
        case DeliveryModeOnewayAccessoryBatch:
            prx = [prx ice_batchOneway];
            break;
        case DeliveryModeOnewaySecure:
            prx = [[prx ice_oneway] ice_secure:YES];
            break;
        case DeliveryModeOnewaySecureBatch:
            prx = [[prx ice_batchOneway] ice_secure:YES];
            break;
        case DeliveryModeDatagram:
            prx = [prx ice_datagram];
            break;
        case DeliveryModeDatagramBatch:
            prx = [prx ice_batchDatagram];
            break;
    }
    
    int timeout = (int)(timeoutSlider.value * 1000.0f); // Convert to ms.
    if(timeout != 0)
    {
        prx = [prx ice_timeout:timeout];
    }
    
    return [DemoHelloPrx uncheckedCast:prx];
}

-(void)sayHello:(id)sender
{
    @try
    {
        id<DemoHelloPrx> hello = [self createProxy];
        int delay = (int)(delaySlider.value * 1000.0f); // Convert to ms.

        int deliveryMode = [modePicker selectedRowInComponent:0];
        if(deliveryMode != DeliveryModeOnewayBatch &&
           deliveryMode != DeliveryModeOnewaySecureBatch &&
		   deliveryMode != DeliveryModeOnewayAccessoryBatch &&
           deliveryMode != DeliveryModeDatagramBatch)
        {
			__block BOOL response = NO;
			ICEAsyncResult* result = [hello begin_sayHello:delay 
												  response:^ {
													  response = YES;
													  [activity stopAnimating];
													  statusLabel.text = @"Ready";
												  }
												 exception:^(ICEException* ex) {
													 response = YES;
													 [self exception:ex];
												 }
													  sent:^(BOOL sentSynchronously) {
														  if(response)
														  {
															  return; // Response was received already.
														  }
														  
														  int deliveryMode = [modePicker selectedRowInComponent:0];
														  if(deliveryMode == DeliveryModeTwoway || deliveryMode == DeliveryModeTwowaySecure)
														  {
															  statusLabel.text = @"Waiting for response";
														  }
														  else if(!sentSynchronously)
														  {
															  statusLabel.text = @"Ready";
															  [activity stopAnimating];       
														  }
														  
													  }];
			if(![result sentSynchronously])
			{
				
				[activity startAnimating];
				statusLabel.text = @"Sending request";
			}
			else 
			{
				int deliveryMode = [modePicker selectedRowInComponent:0];
				if(deliveryMode != DeliveryModeTwoway && deliveryMode != DeliveryModeTwowaySecure)
				{
					statusLabel.text = @"Ready";
				}
			}
        }
        else
        {
            [hello sayHello:delay];
            flushButton.enabled = YES;
            statusLabel.text = @"Queued hello request";
        }
    }
    @catch(ICELocalException* ex)
    {
        [self exception:ex];
    }
}

-(void)shutdown:(id)sender
{
    @try
    {
        id<DemoHelloPrx> hello = [self createProxy];
        int deliveryMode = [modePicker selectedRowInComponent:0];
        if(deliveryMode != DeliveryModeOnewayBatch &&
           deliveryMode != DeliveryModeOnewaySecureBatch &&
           deliveryMode != DeliveryModeDatagramBatch)
        {
            [hello begin_shutdown:^ { [activity stopAnimating]; statusLabel.text = @"Ready"; }
                        exception:^(ICEException* ex) { [self exception:ex]; }];
            if(deliveryMode == DeliveryModeTwoway || deliveryMode == DeliveryModeTwowaySecure)
            {
                [activity startAnimating];
                statusLabel.text = @"Waiting for response";
            }
        }
        else
        {
            [hello shutdown];
            flushButton.enabled = YES;
            statusLabel.text = @"Queued shutdown request";
        }
    }
    @catch(ICELocalException* ex)
    {
        [self exception:ex];
    }
}

-(void)flushBatch:(id) sender
{
    @try
    {
        [communicator begin_flushBatchRequests:^(ICEException* ex) { [self exception:ex]; }];
    }
    @catch(ICELocalException* ex)
    {
		[self exception:ex];
    }
    flushButton.enabled = NO;
    statusLabel.text = @"Flushed batch requests";
}

#pragma mark UIPickerViewDataSource

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
    return 1;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component
{
#if TARGET_IPHONE_SIMULATOR
    return 4;
#else
    return 11;
#endif
}

#pragma mark UIPickerViewDelegate

- (NSString *)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component
{
    switch(row)
    {
        case DeliveryModeTwoway:
            return @"Twoway";
        case DeliveryModeTwowaySecure:
            return @"Twoway secure";
        case DeliveryModeTwowayAccessory:
            return @"Twoway (accessory)";
        case DeliveryModeOneway:
            return @"Oneway";
        case DeliveryModeOnewayBatch:
            return @"Oneway batch";
        case DeliveryModeOnewaySecure:
            return @"Oneway secure";
        case DeliveryModeOnewayAccessory:
            return @"Oneway (accessory)";
        case DeliveryModeOnewaySecureBatch:
            return @"Oneway secure batch";
        case DeliveryModeOnewayAccessoryBatch:
            return @"Oneway batch (accessory)";
        case DeliveryModeDatagram:
            return @"Datagram";
        case DeliveryModeDatagramBatch:
            return @"Datagram batch";
    }
    return @"UNKNOWN";
}

@end


