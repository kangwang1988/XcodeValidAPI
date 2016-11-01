//
//  ViewController.h
//  XcodeValidAPIDemo
//
//  Created by KyleWong on 26/10/2016.
//  Copyright © 2016 KyleWong. All rights reserved.
//

#import <UIKit/UIKit.h>

@class ViewController;
@protocol  ViewControllerDelegate <NSObject>
- (void)didLoadOfViewController:(ViewController *)vc;
@end

@interface ViewController : UIViewController
@property (nonatomic,weak) id<ViewControllerDelegate> delegate;
@end
