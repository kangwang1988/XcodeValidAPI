/*
 *  ClangValidAPIPlugin.cpp
 *  ClangValidAPIPlugin
 *
 *  Created by KyleWong on 26/10/2016.
 *  Copyright © 2016 KyleWong. All rights reserved.
 *
 */

#include <iostream>
#include "ClangValidAPIPlugin.hpp"
#include "ClangValidAPIPluginPriv.hpp"

namespace ValidAPI
{
    void ValidAPIASTVisitor::setContext(ASTContext &context)
    {
        this->context = &context;
    }
    string ValidAPIASTVisitor::pureSelFromSelector(string selector){
        string pureSel = string(selector);
        if(selector.find("@selector(")!=string::npos){
            pureSel = selector.substr(string("@selector(").length(),selector.length()-string("@selector(").length()-1);
        }
        return pureSel;
    }
    bool ValidAPIASTVisitor::VisitDecl(Decl *decl) {
        if(isa<ObjCInterfaceDecl>(decl) || isa<ObjCImplDecl>(decl) || isa<ObjCProtocolDecl>(decl)){
            objcClsInterface = string();
            objcClsImpl = string();
            objcProtocol=string();
            objcIsInstanceMethod = true;
            objcSelector = string();
            objcMethodSrcCode = string();
            objcMethodFilename = string();
            objcMethodRange = string();
            hierarchy=vector<string>();
        }
        if(isa<ObjCInterfaceDecl>(decl)){
            ObjCInterfaceDecl *interfDecl = (ObjCInterfaceDecl*)decl;
            objcClsInterface = interfDecl->getNameAsString();
            vector<string> protoVec;
            for(ObjCList<ObjCProtocolDecl>::iterator it = interfDecl->all_referenced_protocol_begin();it!=interfDecl->all_referenced_protocol_end();it++){
                protoVec.push_back((*it)->getNameAsString());
            }
            string filename = context->getSourceManager().getFilename(interfDecl->getSourceRange().getBegin()).str();
            ValidAPIUtil::appendObjcCls(objcClsInterface, (interfDecl->getSuperClass()?interfDecl->getSuperClass()->getNameAsString():""),protoVec,!filename.find(gSrcRootPath)?true:false);
        }
        if(isa<ObjCCategoryDecl>(decl)){
            ObjCCategoryDecl *categoryDecl = (ObjCCategoryDecl*)decl;
            objcClsInterface = categoryDecl->getClassInterface()->getNameAsString();
            vector<string> protoVec;
            for(ObjCList<ObjCProtocolDecl>::iterator it = categoryDecl->protocol_begin();it!=categoryDecl->protocol_end();it++){
                protoVec.push_back((*it)->getNameAsString());
            }
            string filename = context->getSourceManager().getFilename(categoryDecl->getSourceRange().getBegin()).str();
            ValidAPIUtil::appendObjcCls(objcClsInterface, (categoryDecl->getClassInterface()->getSuperClass()?categoryDecl->getClassInterface()->getSuperClass()->getNameAsString():""),protoVec,!filename.find(gSrcRootPath)?true:false);
        }
        if(isa<ObjCProtocolDecl>(decl)){
            ObjCProtocolDecl *protoDecl = (ObjCProtocolDecl *)decl;
            objcProtocol = protoDecl->getNameAsString();
            vector<string> refProtos;
            for(ObjCProtocolList::iterator it = protoDecl->protocol_begin();it!=protoDecl->protocol_end();it++){
                refProtos.push_back((*it)->getNameAsString());
            }
            string curFilename = context->getSourceManager().getFilename(protoDecl->getSourceRange().getBegin()).str();
            //当前是在定义处
            if(protoDecl->hasDefinition()
               && context->getSourceManager().getFilename(protoDecl->getDefinition()->getSourceRange().getBegin())==context->getSourceManager().getFilename(protoDecl->getSourceRange().getBegin())){
                ValidAPIUtil::appendObjcProto(objcProtocol, refProtos,!curFilename.find(gSrcRootPath)?true:false);
            }
        }
        if(isa<ObjCImplDecl>(decl)){
            ObjCImplDecl *interDecl = (ObjCImplDecl *)decl;
            objcClsImpl = interDecl->getNameAsString();
            ValidAPIUtil::setFilename(context->getSourceManager().getFilename(decl->getSourceRange().getBegin()).str(),true);
        }
        if(isa<ObjCPropertyDecl>(decl)){
            ObjCPropertyDecl *propertyDecl = (ObjCPropertyDecl *)decl;
            objcIsInstanceMethod = propertyDecl->isInstanceProperty();
            if(objcClsInterface.length()){
                ValidAPIUtil::appendObjcClsInterf(objcClsInterface, objcIsInstanceMethod,propertyDecl->getGetterName().getAsString());
                ValidAPIUtil::appendObjcClsInterf(objcClsInterface, objcIsInstanceMethod,propertyDecl->getSetterName().getAsString());
                ValidAPIUtil::appendObjcClsProperty(objcClsInterface, objcIsInstanceMethod, propertyDecl->getGetterName().getAsString(),propertyDecl->getSetterName().getAsString(),propertyDecl->getNameAsString());
            }
            else if(objcProtocol.length()){
                ValidAPIUtil::appendObjcProtoInterf(objcProtocol, objcIsInstanceMethod,propertyDecl->getGetterName().getAsString());
                ValidAPIUtil::appendObjcProtoInterf(objcProtocol, objcIsInstanceMethod,propertyDecl->getSetterName().getAsString());
                ValidAPIUtil::appendObjcProtoProperty(objcProtocol, objcIsInstanceMethod, propertyDecl->getGetterName().getAsString(),propertyDecl->getSetterName().getAsString(),propertyDecl->getNameAsString());
            }
        }
        if(isa<ObjCMethodDecl>(decl)){
            ObjCMethodDecl *methodDecl = (ObjCMethodDecl *)decl;
            objcIsInstanceMethod = methodDecl->isInstanceMethod();
            objcSelector = methodDecl->getSelector().getAsString();
            if(objcClsInterface.length()){
                ValidAPIUtil::appendObjcClsInterf(objcClsInterface, objcIsInstanceMethod,objcSelector);
            }
            else if(objcProtocol.length()){
                ValidAPIUtil::appendObjcProtoInterf(objcProtocol, objcIsInstanceMethod, objcSelector);
            }
            else if(objcClsImpl.length()){
                if(methodDecl->hasBody()){
                    Stmt *methodBody = methodDecl->getBody();
                    objcMethodSrcCode.assign(context->getSourceManager().getCharacterData(methodBody->getSourceRange().getBegin()),methodBody->getSourceRange().getEnd().getRawEncoding()-methodBody->getSourceRange().getBegin().getRawEncoding()+1);
                    objcMethodFilename = context->getSourceManager().getFilename(methodBody->getSourceRange().getBegin()).str();
                    if(objcMethodFilename.find(gSrcRootPath)!=string::npos){
                        objcMethodFilename = objcMethodFilename.substr(gSrcRootPath.length(),objcMethodFilename.length()-gSrcRootPath.length());
                        ostringstream stringStream;
                        stringStream<<methodBody->getSourceRange().getBegin().getRawEncoding()<<"-"<<methodBody->getSourceRange().getEnd().getRawEncoding();
                        objcMethodRange = stringStream.str();
                        ValidAPIUtil::appendObjcClsMethodImpl(objcIsInstanceMethod, objcClsImpl, objcSelector, objcMethodFilename, methodBody->getSourceRange().getBegin().getRawEncoding(),methodBody->getSourceRange().getEnd().getRawEncoding(), objcMethodSrcCode);
                    }
                }
            }
        }
        return true;
    }
    bool ValidAPIASTVisitor::VisitStmt(Stmt *s) {
        string curFilename = context->getSourceManager().getFilename(s->getSourceRange().getBegin()).str();
        ValidAPIUtil::setFilename(curFilename,false);
        if(isa<DeclRefExpr>(s)){
            DeclRefExpr *callExpr = (DeclRefExpr *)s;
            ValueDecl *decl = callExpr->getDecl();
            string declName = decl->getNameAsString();
            if(curFilename.find(gSrcRootPath)==string::npos || !declName.compare("self"))
                return true;
            if(isa<VarDecl>(decl)){
                ValidAPIUtil::appendVar(declName);
            }
            else if(isa<FunctionDecl>(decl)){
                ValidAPIUtil::appendFunction(declName);
            }
            else if(isa<EnumConstantDecl>(decl)){
                ValidAPIUtil::appendEnum(declName);
            }
        }
        else if(isa<ObjCMessageExpr>(s))
        {
            ObjCMessageExpr *objcExpr = (ObjCMessageExpr*)s;
            ObjCMessageExpr::ReceiverKind kind = objcExpr->getReceiverKind();
            calleeIsInstanceMethod = true;
            string calleeSel = objcExpr->getSelector().getAsString();
            string receiverType =objcExpr->getReceiverType().getAsString();
            size_t pos = receiverType.find(" ");
            LangOptions LangOpts;
            LangOpts.ObjC2 = true;
            PrintingPolicy Policy(LangOpts);
            if(pos!=string::npos){
                receiverType = receiverType.substr(0,pos);
            }
            switch (kind) {
                case ObjCMessageExpr::Class:
                case ObjCMessageExpr::SuperClass:
                    calleeIsInstanceMethod = false;
                    break;
                case ObjCMessageExpr::Instance:
                case ObjCMessageExpr::SuperInstance:
                    break;
                default:
                    break;
            }
            if(!objcExpr->getReceiverInterface() || (receiverType.find("<")!=string::npos && receiverType.find(">")!=string::npos)){
                this->handleUnknownReceiverInterface(objcExpr, calleeSel);
            }
            bool isCallOriginalMethod = true;
            //处理selector
            if(!calleeSel.compare("performSelectorOnMainThread:withObject:waitUntilDone:modes:") ||
               !calleeSel.compare("performSelectorOnMainThread:withObject:waitUntilDone:") ||
               !calleeSel.compare("performSelector:onThread:withObject:waitUntilDone:modes:") ||
               !calleeSel.compare("performSelector:onThread:withObject:waitUntilDone:") ||
               !calleeSel.compare("performSelectorInBackground:withObject:") ||
               !calleeSel.compare("performSelector:") ||
               !calleeSel.compare("performSelector:withObject:") ||
               !calleeSel.compare("performSelector:withObject:withObject:") ||
               !calleeSel.compare("performSelector:withObject:afterDelay:inModes:") ||
               !calleeSel.compare("performSelector:withObject:afterDelay:")){
                this->handlePerformSelectorMessageExpr(objcExpr);
            }
            //处理手势/按钮等target/action这种
            else if(!calleeSel.compare("addTarget:action:") ||
                    !calleeSel.compare("addTarget:action:forControlEvents:") ||
                    !calleeSel.compare("initWithTarget:action:")
                    ){
                this->handleTargetActionMessageExpr(objcExpr);
            }
            //通知
            else if(!calleeSel.compare("addObserver:selector:name:object:") ||
                    !calleeSel.compare("postNotification:") ||
                    !calleeSel.compare("postNotificationName:object:") ||
                    !calleeSel.compare("postNotificationName:object:userInfo:")){
                this->handleNotificationMessageExpr(objcExpr);
            }
            //处理UIBarButtonItem
            else if(!calleeSel.compare("initWithImage:style:target:action:") ||
                    !calleeSel.compare("initWithImage:landscapeImagePhone:style:target:action:") ||
                    !calleeSel.compare("initWithTitle:style:target:action:") ||
                    !calleeSel.compare("initWithBarButtonSystemItem:target:action:")){
                string sTarget,sSel;
                raw_string_ostream paramTarget(sTarget),paramSel(sSel);
                string paramTypeTarget,paramTypeSel;
                if(!calleeSel.compare("initWithBarButtonSystemItem:target:action:")){
                    objcExpr->getArg(1)->printPretty(paramTarget, 0, Policy);
                    objcExpr->getArg(2)->printPretty(paramSel, 0, Policy);
                    paramTypeTarget = objcExpr->getArg(1)->getType().getAsString(),
                    paramTypeSel = objcExpr->getArg(2)->getType().getAsString();
                }
                else if(!calleeSel.compare("initWithTitle:style:target:action:") || !calleeSel.compare("initWithImage:style:target:action:")){
                    objcExpr->getArg(2)->printPretty(paramTarget, 0, Policy);
                    objcExpr->getArg(3)->printPretty(paramSel, 0, Policy);
                    paramTypeTarget = objcExpr->getArg(2)->getType().getAsString(),
                    paramTypeSel = objcExpr->getArg(3)->getType().getAsString();
                }
                else if(!calleeSel.compare("initWithImage:landscapeImagePhone:style:target:action:")){
                    objcExpr->getArg(3)->printPretty(paramTarget, 0, Policy);
                    objcExpr->getArg(4)->printPretty(paramSel, 0, Policy);
                    paramTypeTarget = objcExpr->getArg(3)->getType().getAsString(),
                    paramTypeSel = objcExpr->getArg(4)->getType().getAsString();
                }
                string target = paramTarget.str(),sel = pureSelFromSelector(paramSel.str());
                if(!target.compare("self")){
                    ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,objcIsInstanceMethod, objcClsImpl,sel);
                }
            }
            //处理Timer
            else if(!calleeSel.compare("scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:") || !calleeSel.compare("timerWithTimeInterval:target:selector:userInfo:repeats:") || !calleeSel.compare("initWithFireDate:interval:target:selector:userInfo:repeats:") ||
                    !calleeSel.compare("scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:dispatchQueue:")){
                string sTarget,sSel;
                raw_string_ostream paramTarget(sTarget),paramSel(sSel);
                string paramTypeTarget,paramTypeSel;
                if(!calleeSel.compare("initWithFireDate:interval:target:selector:userInfo:repeats:")){
                    objcExpr->getArg(2)->printPretty(paramTarget, 0, Policy);
                    objcExpr->getArg(3)->printPretty(paramSel, 0, Policy);
                    paramTypeTarget = objcExpr->getArg(2)->getType().getAsString(),
                    paramTypeSel = objcExpr->getArg(3)->getType().getAsString();
                }
                else{
                    objcExpr->getArg(1)->printPretty(paramTarget, 0, Policy);
                    objcExpr->getArg(2)->printPretty(paramSel, 0, Policy);
                    paramTypeTarget = objcExpr->getArg(1)->getType().getAsString();
                    paramTypeSel = objcExpr->getArg(2)->getType().getAsString();
                }
                string target = paramTarget.str(),sel = pureSelFromSelector(paramSel.str());
                if(!target.compare("self")){
                    ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,objcIsInstanceMethod, objcClsImpl,sel);
                }
            }
            //NSObject cancelPreviousPerformRequestsWithTarget
            else if(!calleeSel.compare("cancelPreviousPerformRequestsWithTarget:selector:object:")){
                string sTarget,sSel;
                raw_string_ostream paramTarget(sTarget),paramSel(sSel);
                string paramTypeTarget,paramTypeSel;
                objcExpr->getArg(0)->printPretty(paramTarget, 0, Policy);
                objcExpr->getArg(1)->printPretty(paramSel, 0, Policy);
                paramTypeTarget = objcExpr->getArg(0)->getType().getAsString();
                paramTypeSel = objcExpr->getArg(1)->getType().getAsString();
                string target = paramTarget.str(),sel = pureSelFromSelector(paramSel.str());
                if(!target.compare("self")){
                    ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,objcIsInstanceMethod, objcClsImpl,sel);
                }
            }
            //处理NSThread
            else if(!calleeSel.compare("detachNewThreadSelector:toTarget:withObject:")){
                string sTarget,sSel;
                raw_string_ostream paramTarget(sTarget),paramSel(sSel);
                string paramTypeTarget,paramTypeSel;
                objcExpr->getArg(0)->printPretty(paramSel, 0, Policy);
                objcExpr->getArg(1)->printPretty(paramTarget, 0, Policy);
                paramTypeSel = objcExpr->getArg(0)->getType().getAsString();
                paramTypeTarget = objcExpr->getArg(1)->getType().getAsString();
                string target = paramTarget.str(),sel = pureSelFromSelector(paramSel.str());
                if(!target.compare("self")){
                    ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,objcIsInstanceMethod, objcClsImpl,sel);
                }
            }
            else if(!calleeSel.compare("initWithTarget:selector:object:")){
                string sTarget,sSel;
                raw_string_ostream paramTarget(sTarget),paramSel(sSel);
                string paramTypeTarget,paramTypeSel;
                objcExpr->getArg(0)->printPretty(paramTarget, 0, Policy);
                objcExpr->getArg(1)->printPretty(paramSel, 0, Policy);
                paramTypeTarget = objcExpr->getArg(0)->getType().getAsString();
                paramTypeSel = objcExpr->getArg(1)->getType().getAsString();
                string target = paramTarget.str(),sel = pureSelFromSelector(paramSel.str());
                if(!target.compare("self")){
                    ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,objcIsInstanceMethod, objcClsImpl,sel);
                }
            }
            //处理CADisplayLink
            else if(!calleeSel.compare("displayLinkWithTarget:selector:"))
            {
                string sTarget,sSel;
                raw_string_ostream paramTarget(sTarget),paramSel(sSel);
                string paramTypeTarget,paramTypeSel;
                objcExpr->getArg(0)->printPretty(paramTarget, 0, Policy);
                objcExpr->getArg(1)->printPretty(paramSel, 0, Policy);
                paramTypeTarget = objcExpr->getArg(0)->getType().getAsString();
                paramTypeSel = objcExpr->getArg(1)->getType().getAsString();
                string target = paramTarget.str(),sel = pureSelFromSelector(paramSel.str());
                if(!target.compare("self")){
                    ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,objcIsInstanceMethod, objcClsImpl,sel);
                }
            }
            //处理KVO
            else if(!calleeSel.compare("addObserver:forKeyPath:options:context:"))
            {
                string sTarget,sSel;
                raw_string_ostream paramTarget(sTarget);
                string paramTypeTarget,paramTypeSel;
                objcExpr->getArg(0)->printPretty(paramTarget, 0, Policy);
                paramTypeTarget = objcExpr->getArg(0)->getType().getAsString();
                string target = paramTarget.str();
                if(!target.compare("self")){
                    ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,objcIsInstanceMethod, objcClsImpl,"observeValueForKeyPath:ofObject:change:context:");
                }
            }
            //处理[XXXClass new]
            else if(!calleeSel.compare("new")){
                if(objcExpr->getReceiverInterface()){
                    string receiverInterface = objcExpr->getReceiverInterface()->getNameAsString();
                    ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,false, receiverInterface,"alloc");
                    ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,true, receiverInterface,"init");
                }
                isCallOriginalMethod = false;
            }
            if(objcMethodFilename.length() && isCallOriginalMethod){
                if(objcExpr->getReceiverInterface()){
                    string receiverInterface = objcExpr->getReceiverInterface()->getNameAsString();
                    ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector, calleeIsInstanceMethod, receiverInterface, calleeSel);
                }
                else{
                    this->handleUnknownReceiverInterface(objcExpr, calleeSel);
                }
            }
        }
        return true;
    }
    bool ValidAPIASTVisitor::handleUnknownReceiverInterface(ObjCMessageExpr *objcExpr,string calleeSel){
        //无明确类型，形如id<XXXDelegate>的调用
        string receiverType =objcExpr->getReceiverType().getAsString();
        size_t pos = receiverType.find(" ");
        if(pos!=string::npos){
            receiverType = receiverType.substr(0,pos);
        }
        LangOptions LangOpts;
        LangOpts.ObjC2 = true;
        PrintingPolicy Policy(LangOpts);
        string sExpr;
        raw_string_ostream paramExpr(sExpr);
        objcExpr->printPretty(paramExpr, 0, Policy);
        sExpr = paramExpr.str();
        remove_blank(sExpr);
        //优先处理delegate，以防self->_delegate这种。
        if(receiverType.find("<")!=string::npos && receiverType.find(">")!=string::npos){
            int pos1 = receiverType.find("<"),pos2=receiverType.rfind(">");
            string protocolStr = receiverType.substr(pos1+1,pos2-pos1-1);
            vector<string> protocols = split(protocolStr,',');
            for(vector<string>::iterator it = protocols.begin();it!=protocols.end();it++){
                string protocol = *it;
                trim(protocol);
                ValidAPIUtil::appendObjcProtoInterfCall(objcIsInstanceMethod, objcClsImpl, objcSelector, protocol,calleeSel);
            }
        }
        else if(sExpr.find("[[selfclass]")==0 || sExpr.find("[self.class")==0){
            ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,false, objcClsImpl,"class");
            ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,false, objcClsImpl,calleeSel);
        }
        else if(sExpr.find("[[selfalloc]")==0){
            ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,false, objcClsImpl,"alloc");
            ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,true, objcClsImpl,calleeSel);
        }
        else if(sExpr.find("[self")==0){
            ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector,objcIsInstanceMethod, objcClsImpl,calleeSel);
        }
        return true;
    }
    void ValidAPIASTVisitor::handleNotificationMessageExpr(ObjCMessageExpr *objcExpr){
        //Receiver明确是NSNotificationCenter
        LangOptions LangOpts;
        LangOpts.ObjC2 = true;
        PrintingPolicy Policy(LangOpts);
        //此处不处理:postNotification(运行时才知道)，addObserverForName:object:queue:usingBlock(无需处理)
        string notifSel = objcExpr->getSelector().getAsString();
        if(!notifSel.compare("addObserver:selector:name:object:")){
            string s0,s1,s2;
            raw_string_ostream param0(s0),param1(s1),param2(s2);
            objcExpr->getArg(0)->printPretty(param0, 0, Policy);
            objcExpr->getArg(1)->printPretty(param1, 0, Policy);
            objcExpr->getArg(2)->printPretty(param2, 0, Policy);
            string paramType0 = objcExpr->getArg(0)->getType().getAsString(),
            paramType1 = objcExpr->getArg(1)->getType().getAsString(),
            paramType2 = objcExpr->getArg(2)->getType().getAsString();
            string param1Sel = pureSelFromSelector(param1.str());
            string paramNotif = pureSelFromSelector(param2.str());
            if(paramNotif.find("@\"")!=string::npos){
                paramNotif = paramNotif.substr(string("@\"").length(),paramNotif.length()-string("@\"").length()-1);
            }
            if(!param0.str().compare("self") &&!paramType1.compare("SEL")){
                if(has_suffix(paramNotif, "Notification") && (!paramNotif.find("AV") || !paramNotif.find("UI") || !paramNotif.find("NS"))){
                    ValidAPIUtil::appendObjcAddNotificationCall(objcIsInstanceMethod, objcClsImpl, objcSelector,objcIsInstanceMethod,objcClsImpl,param1Sel,paramNotif);
                    ValidAPIUtil::appendObjcPostNotificationCall(true, kAppMainEntryClass, kAppMainEntrySelector,paramNotif);
                }
                else{
                    ValidAPIUtil::appendObjcAddNotificationCall(objcIsInstanceMethod, objcClsImpl, objcSelector,objcIsInstanceMethod,objcClsImpl,param1Sel,paramNotif);
                }
            }
        }
        else if(notifSel.find("postNotificationName:")==0){
            string s0;
            raw_string_ostream param0(s0);
            objcExpr->getArg(0)->printPretty(param0, 0, Policy);
            string paramType0 = objcExpr->getArg(0)->getType().getAsString();
            string paramNotif = pureSelFromSelector(param0.str());
            string notif = param0.str();
            if(notif.find("@\"")!=string::npos){
                notif = notif.substr(string("@\"").length(),notif.length()-string("@\"").length()-1);
            }
            if(has_suffix(paramNotif, "Notification") && (!paramNotif.find("AV") || !paramNotif.find("UI") || !paramNotif.find("NS"))){
                ValidAPIUtil::appendObjcPostNotificationCall(true, kAppMainEntryClass, kAppMainEntrySelector,notif);
            }
            else{
                ValidAPIUtil::appendObjcPostNotificationCall(objcIsInstanceMethod, objcClsImpl, objcSelector,notif);
            }
        }
    }
    void ValidAPIASTVisitor::handlePerformSelectorMessageExpr(ObjCMessageExpr *objcExpr){
        //需考虑ReceiverInterface
        LangOptions LangOpts;
        LangOpts.ObjC2 = true;
        PrintingPolicy Policy(LangOpts);
        string performSel = objcExpr->getSelector().getAsString();
        if(performSel.find("performSelector")==0){
            string s0;
            raw_string_ostream param0(s0);
            objcExpr->getArg(0)->printPretty(param0, 0, Policy);
            string param0Sel = pureSelFromSelector(param0.str());
            //支持可明确知道类型的调用，如[self perform]/[VCClass perform]
            if(objcExpr->getReceiverInterface()){
                string receiverInterface = objcExpr->getReceiverInterface()->getNameAsString();
                ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector, calleeIsInstanceMethod, receiverInterface, param0Sel);
            }
            else{
                this->handleUnknownReceiverInterface(objcExpr, param0Sel);
            }
        }
    }
    void ValidAPIASTVisitor::handleTargetActionMessageExpr(ObjCMessageExpr *objcExpr){
        //需考虑ReceiverInterface
        LangOptions LangOpts;
        LangOpts.ObjC2 = true;
        PrintingPolicy Policy(LangOpts);
        string sTarget,sSel;
        raw_string_ostream paramTarget(sTarget),paramSel(sSel);
        objcExpr->getArg(0)->printPretty(paramTarget, 0, Policy);
        objcExpr->getArg(1)->printPretty(paramSel, 0, Policy);
        string target = paramTarget.str();
        string sel = pureSelFromSelector(paramSel.str());
        if(!target.compare("self")){
            target = objcClsImpl;
            ValidAPIUtil::appendObjcMethodImplCall(objcIsInstanceMethod, objcClsImpl, objcSelector, objcIsInstanceMethod, objcClsImpl, sel);
        }
    }
    void ValidAPIASTConsumer::HandleTranslationUnit(ASTContext &context){
        visitor.setContext(context);
        visitor.TraverseDecl(context.getTranslationUnitDecl());
        ValidAPIUtil::synchronize();
    }
    unique_ptr<ASTConsumer> ValidAPIASTAction::CreateASTConsumer(CompilerInstance &Compiler,llvm::StringRef InFile)
    {
        return unique_ptr<ValidAPIASTConsumer>(new ValidAPIASTConsumer);
    }
    bool ValidAPIASTAction::ParseArgs(const CompilerInstance &CI, const std::vector<std::string>& args) {
        size_t cnt = args.size();
        if(cnt == 1){
            string relativePath = args.at(0);
            gSrcRootPath = absolutePathFromRelative(relativePath);
        }
        return true;
    }
}

static clang::FrontendPluginRegistry::Add<ValidAPI::ValidAPIASTAction>
X("ClangValidAPIPlugin", "ClangValidAPIPlugin");
