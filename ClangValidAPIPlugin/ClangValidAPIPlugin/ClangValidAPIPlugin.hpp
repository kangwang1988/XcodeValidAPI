/*
 *  ClangValidAPIPlugin.hpp
 *  ClangValidAPIPlugin
 *
 *  Created by KyleWong on 26/10/2016.
 *  Copyright © 2016 KyleWong. All rights reserved.
 *
 */

#ifndef ClangValidAPIPlugin_
#define ClangValidAPIPlugin_

#include<iostream>
#include<sstream>
#include<typeinfo>

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "ValidAPIUtil.hpp"
#pragma GCC visibility push(default)
using namespace clang;
using namespace std;
using namespace llvm;

extern string gSrcRootPath;

namespace ValidAPI
{
    class ValidAPIASTVisitor : public RecursiveASTVisitor<ValidAPIASTVisitor>
    {
    private:
        ASTContext *context;
        string objcClsInterface;
        string objcClsImpl;
        string objcProtocol;
        bool objcIsInstanceMethod;
        bool calleeIsInstanceMethod;
        string objcSelector;
        string objcMethodSrcCode;
        string objcMethodFilename;
        string objcMethodRange;
        vector<string> hierarchy;
    public:
        void setContext(ASTContext &context);
        string pureSelFromSelector(string selector);
        bool VisitDecl(Decl *decl);
        bool VisitStmt(Stmt *s);
        bool handleUnknownReceiverInterface(ObjCMessageExpr *objcExpr,string calleeSel);
        void handleNotificationMessageExpr(ObjCMessageExpr *objcExpr);
        void handlePerformSelectorMessageExpr(ObjCMessageExpr *objcExpr);
        void handleTargetActionMessageExpr(ObjCMessageExpr *objcExpr);
    };
    class ValidAPIASTConsumer : public ASTConsumer
    {
    private:
        ValidAPIASTVisitor visitor;
        void HandleTranslationUnit(ASTContext &context);
    };
    
    class ValidAPIASTAction : public PluginASTAction
    {
    public:
        unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler,llvm::StringRef InFile);
        bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string>& args);
    };
}
#pragma GCC visibility pop
#endif
