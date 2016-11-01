//
//  SiriUIViewController.m
//  XcodeValidAPIDemo
//
//  Created by KyleWong on 29/10/2016.
//  Copyright © 2016 KyleWong. All rights reserved.
//

#import "SiriUIViewController.h"

@implementation SiriUIViewController
- (void)viewDidLoad{
    [super viewDidLoad];
    id<INIntentHandlerProviding> x;
    INIntent *i;
    [x handlerForIntent:i];
    [NSNotificationCenter defaultCenter];
}

+ (BOOL)supportsSecureCoding{
    return YES;
}
@end
