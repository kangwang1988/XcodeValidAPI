//
//  ViewController.m
//  XcodeValidAPIDemo
//
//  Created by KyleWong on 26/10/2016.
//  Copyright © 2016 KyleWong. All rights reserved.
//

#import "ViewController.h"
#import "NKHintView.h"
#import <objc/runtime.h>

@interface ViewController ()<UITableViewDataSource,UITableViewDelegate,NSSecureCoding>
@property (nonatomic,strong) UIButton *btn;
@property (nonatomic,strong) UIPanGestureRecognizer *pan;
@property (nonatomic,strong) UIBarButtonItem *barButtonItem;
@end

@implementation ViewController
- (void)viewDidLoad {
    [super viewDidLoad];
    [self.view setBackgroundColor:[UIColor blueColor]];
    [self setBtn:[UIButton new]];
    [self.btn addTarget:self action:@selector(addTargetBtnFunc) forControlEvents:UIControlEventTouchUpInside];
    [self setPan:[UIPanGestureRecognizer new]];
    [self.pan addTarget:self action:@selector(addTargetGestureFunc)];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onAppEnterBg:) name:UIApplicationDidEnterBackgroundNotification object:nil];
    class_getName([self class]);
    // Do any additional setup after loading the view, typically from a nib.
}

- (void)viewWillAppear:(BOOL)animated{
    [super viewWillAppear:animated];
    [self setBarButtonItem:[[UIBarButtonItem alloc] initWithImage:nil style:UIBarButtonItemStyleDone target:self action:@selector(onBarButtonItemAction:)]];
}

- (void)viewDidAppear:(BOOL)animated{
    [super viewDidAppear:animated];
    [NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(onTimerExpired:) userInfo:nil repeats:YES];
}

- (void)viewDidLayoutSubviews{
    [super viewDidLayoutSubviews];
    NSThread *thread = [[NSThread alloc] initWithTarget:self selector:@selector(threadFunc:) object:nil];
    NKHintView *hint = [NKHintView new];
}

- (void)viewDidUnload{
    [super viewDidUnload];
    [self performSelector:@selector(performSelFunc)];
    CADisplayLink *displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayLinkExpired:)];
    if([self.delegate respondsToSelector:@selector(didLoadOfViewController:)])
        [self.delegate didLoadOfViewController:self];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(cancelPreviousFunc) object:nil];
    [NSThread detachNewThreadSelector:@selector(threadDetachFunc:) toTarget:self withObject:nil];
}

- (void)performSelFunc{
    NSLog(@"");
    [self.view addObserver:self forKeyPath:@"frame" options:NSKeyValueObservingOptionNew context:nil];
}

- (void)addTargetBtnFunc{
    NSLog(@"");
}

- (void)addTargetGestureFunc{
    NSLog(@"");
}

- (void)onAppEnterBg:(NSNotification *)aNotif{
    NSLog(@"");
}

- (void)onBarButtonItemAction:(id)sender{
    NSLog(@"");
}

- (void)onTimerExpired:(NSTimer *)aTimer{
    NSLog(@"");
}

- (void)cancelPreviousFunc{
    NSLog(@"");
}

- (void)threadDetachFunc:(id)param{
    NSLog(@"");
}

- (void)threadFunc:(id)param{
    NSLog(@"");
}

- (void)displayLinkExpired:(CADisplayLink *)aDisplayLink{
    NSLog(@"");
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey,id> *)change context:(void *)context{
    
}

#pragma mark - UITableViewDataSource
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section{
    return 10;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath{
    return nil;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath{
    return 40;
}

#pragma mark - UITableViewDelegate
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath{

}

- (nullable NSArray<UITableViewRowAction *> *)tableView:(UITableView *)tableView editActionsForRowAtIndexPath:(NSIndexPath *)indexPath{
    [self.class supportsSecureCoding];
    return nil;
}

+ (BOOL)supportsSecureCoding{
    return NO;
}
@end
