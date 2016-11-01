//
//  ValidAPIUtil.cpp
//  ClangValidAPIPlugin
//
//  Created by KyleWong on 11/10/2016.
//
//

#include "ValidAPIUtil.hpp"
#include <streambuf>
#include <algorithm>

string gSrcRootPath = "";
string kKeyInterfSelDictFilename = "filename";
string kKeyInterfSelDictSourceCode = "sourceCode";
string kKeyInterfSelDictRange = "range";
string kKeyInterfSelDictCallees = "callee";
string kKeyInterfDictProperties = "properties";
string kKeyInterfSelDictSuperClass = "superClass";
string kKeyInterfSelDictProtos = "protos";
string kKeyInterfSelDictInterfs = "interfs";
string kKeyInterfSelDictIsInSrcDir = "isInSrcDir";
string kKeyFuncEnumVarFuncs = "funcs";
string kKeyFuncEnumVarEnums = "enums";
string kKeyFuncEnumVarVars = "vars";

json clsMethodJson,clsInterfHierachy,protoInterfHierachy,clsMethodAddNotifsJson,notifPostedCallersJson,protoInterfCallJson,funcEnumVarJson,clsPropertyGSJson,protoPropertyGSJson;
string filename;
using namespace std;

void ValidAPIUtil::setFilename(string fname,bool forceSet){
    if(!fname.length())
        return;
    if(!forceSet && filename.length())
        return;
    filename = fname;
    string cs2Remove = "/\\";
    for (unsigned int i = 0; i < cs2Remove.length(); ++i)
        filename.erase (remove(filename.begin(), filename.end(), cs2Remove.at(i)), filename.end());
}

bool ValidAPIUtil::writeJsonToFile(json j, string filename){
    if(j.is_null()){
        return false;
    }
    ofstream ofs;
    ofs.open (filename,ofstream::out | ofstream::trunc);
    ofs<<j<<endl;
    ofs.close();
    return true;
}

void ValidAPIUtil::appendObjcClsMethodImpl(bool isInstanceMethod,string cls,string selector,string filename,unsigned rangeFrom,unsigned rangeTo,string sourcecode){
    stringstream ss;
    ss<<rangeFrom<<'-'<<rangeTo;
    string key = string(isInstanceMethod?"-":"+")+"["+cls+" "+selector+"]";
    clsMethodJson[key]={{kKeyInterfSelDictFilename,filename},{kKeyInterfSelDictSourceCode,sourcecode},{kKeyInterfSelDictRange,ss.str()}};
}

void ValidAPIUtil::appendObjcMethodImplCall(bool isInstanceMethod, string cls, string selector, bool calleeIsInstanceMethod, string calleeCls, string calleeSel){
    string key = string(isInstanceMethod?"-":"+")+"["+cls+" "+selector+"]";
    string value = string(calleeIsInstanceMethod?"-":"+")+"["+calleeCls+" "+calleeSel+"]";
    json clsMethodObj = json(clsMethodJson[key]);
    json callees = json(clsMethodObj[kKeyInterfSelDictCallees]);
    vector<string> oldCalleeVec;
    if(callees.is_array())
        oldCalleeVec = callees.get<vector<string>>();
    if(find(oldCalleeVec.begin(),oldCalleeVec.end(),value)==oldCalleeVec.end())
        callees.push_back(value);
    clsMethodObj[kKeyInterfSelDictCallees]=callees;
    clsMethodJson[key]=clsMethodObj;
}

void ValidAPIUtil::appendObjcCls(string cls,string supCls,vector<string> protoVec,bool isInSrcDir){
    stringstream ss;
    json clsJson = json(clsInterfHierachy[cls]);
    json protoJson = json(clsJson[kKeyInterfSelDictProtos]);
    vector<string> oldProtoVec;
    if(protoJson.is_array())
        oldProtoVec = protoJson.get<vector<string>>();
    for(vector<string>::iterator it = protoVec.begin();it!=protoVec.end();it++){
        string proto = *it;
        if(find(oldProtoVec.begin(), oldProtoVec.end(), proto) == oldProtoVec.end()) {
            protoJson.push_back(proto);
        }
    }
    clsInterfHierachy[cls]={{kKeyInterfSelDictSuperClass,supCls},{kKeyInterfSelDictProtos,protoJson},{kKeyInterfSelDictInterfs,clsJson[kKeyInterfSelDictInterfs]},{kKeyInterfSelDictIsInSrcDir,isInSrcDir?"1":"0"}};
}

void ValidAPIUtil::appendObjcClsInterf(string cls,bool isInstanceInterf,string selector){
    json interfHierachyObj = json(clsInterfHierachy[cls]);
    json interfsJson = json(interfHierachyObj[kKeyInterfSelDictInterfs]);
    string value = string(isInstanceInterf?"-":"+")+"["+cls+" "+selector+"]";
    vector<string> oldInterfsVec;
    if(interfsJson.is_array())
        oldInterfsVec = interfsJson.get<vector<string>>();
    if(find(oldInterfsVec.begin(),oldInterfsVec.end(),value)==oldInterfsVec.end())
        interfsJson.push_back(value);
    clsInterfHierachy[cls]={{kKeyInterfSelDictSuperClass,interfHierachyObj[kKeyInterfSelDictSuperClass]},{kKeyInterfSelDictProtos,interfHierachyObj[kKeyInterfSelDictProtos]},{kKeyInterfSelDictInterfs,interfsJson}};
}

void ValidAPIUtil::appendObjcClsProperty(string cls, bool isInstanceProperty, string getter, string setter, string property){
    json propertyJson = json(clsPropertyGSJson[cls]);
    string key = string(isInstanceProperty?"-":"+")+"["+cls+" "+getter+"]";
    if(getter.length() && propertyJson.find(key)==propertyJson.end())
        propertyJson[key]=property;
    key = string(isInstanceProperty?"-":"+")+"["+cls+" "+setter+"]";
    if(setter.length() && propertyJson.find(key)==propertyJson.end())
        propertyJson[key]=property;
    clsPropertyGSJson[cls]=propertyJson;
}

void ValidAPIUtil::appendObjcProto(string proto,vector<string> refProto,bool isInSrcDir){
    json protoJson = json(protoInterfHierachy[proto]);
    json protoRefProtosJson = json(protoJson[kKeyInterfSelDictProtos]);
    vector<string> oldProtoRefProtoVec;
    if(protoRefProtosJson.is_array())
        oldProtoRefProtoVec = protoRefProtosJson.get<vector<string>>();
    for(vector<string>::iterator it = refProto.begin();it!=refProto.end();it++){
        string value = *it;
        if(find(oldProtoRefProtoVec.begin(),oldProtoRefProtoVec.end(),value)==oldProtoRefProtoVec.end())
            protoRefProtosJson.push_back(*it);
    }
    protoInterfHierachy[proto]={{kKeyInterfSelDictInterfs,protoJson[kKeyInterfSelDictInterfs]},{kKeyInterfSelDictProtos,protoRefProtosJson},{kKeyInterfSelDictIsInSrcDir,isInSrcDir?"1":"0"}};
}

void ValidAPIUtil::appendObjcProtoInterf(string proto, bool isInstanceInterf, string selector){
    json protoHierachyObj = json(protoInterfHierachy[proto]);
    json interfs = json(protoHierachyObj[kKeyInterfSelDictInterfs]);
    string value = string(isInstanceInterf?"-":"+")+"["+proto+" "+selector+"]";
    vector<string> oldInterfsVec;
    if(interfs.is_array())
        oldInterfsVec = interfs.get<vector<string>>();
    if(find(oldInterfsVec.begin(),oldInterfsVec.end(),value)==oldInterfsVec.end())
        interfs.push_back(value);
    protoInterfHierachy[proto]={{kKeyInterfSelDictInterfs,interfs},{kKeyInterfSelDictProtos,protoHierachyObj[kKeyInterfSelDictProtos]},{kKeyInterfSelDictIsInSrcDir,protoHierachyObj[kKeyInterfSelDictIsInSrcDir]}};
}

void ValidAPIUtil::appendObjcProtoProperty(string proto, bool isInstanceProperty, string getter, string setter, string property){
    json propertyJson = json(protoPropertyGSJson[proto]);
    string key = string(isInstanceProperty?"-":"+")+"["+proto+" "+getter+"]";
    if(getter.length() && propertyJson.find(key)==propertyJson.end())
        propertyJson[key]=property;
    key = string(isInstanceProperty?"-":"+")+"["+proto+" "+setter+"]";
    if(setter.length() && propertyJson.find(key)==propertyJson.end())
        propertyJson[key]=property;
    protoPropertyGSJson[proto]=propertyJson;
}

void ValidAPIUtil::appendObjcAddNotificationCall(bool isInstanceMethod, string cls, string selector,bool calleeIsInstanceMethod, string calleeCls, string calleeSel, string notif){
    string key = string(calleeIsInstanceMethod?"-":"+")+"["+calleeCls+" "+calleeSel+"]";
    string callerValueItem = string(isInstanceMethod?"-":"+")+"["+cls+" "+selector+"]";
    json clsMethodNotifsJson = json(clsMethodAddNotifsJson[key]);
    json notifJson = json(clsMethodNotifsJson[notif]);
    vector<string> notifJsonVector;
    if(notifJson.is_array())
        notifJsonVector = notifJson.get<vector<string>>();
    if(find(notifJsonVector.begin(),notifJsonVector.end(),callerValueItem)==notifJsonVector.end())
        notifJson.push_back(callerValueItem);
    clsMethodNotifsJson[notif]=notifJson;
    clsMethodAddNotifsJson[key]=clsMethodNotifsJson;
}

void ValidAPIUtil::appendObjcPostNotificationCall(bool isInstanceMethod, string cls, string selector, string notif){
    string callerValueItem = string(isInstanceMethod?"-":"+")+"["+cls+" "+selector+"]";
    json notifJson = json(notifPostedCallersJson[notif]);
    vector<string> notifJsonVector;
    if(notifJson.is_array())
        notifJsonVector = notifJson.get<vector<string>>();
    if(find(notifJsonVector.begin(),notifJsonVector.end(),callerValueItem)==notifJsonVector.end())
        notifJson.push_back(callerValueItem);
    notifPostedCallersJson[notif]=notifJson;
}

void ValidAPIUtil::appendObjcProtoInterfCall(bool isInstanceMethod, string cls, string selector, string proto, string protoSel){
    string key = string("-")+"["+proto+" "+protoSel+"]";
    string callerValueItem = string(isInstanceMethod?"-":"+")+"["+cls+" "+selector+"]";
    json protoInterfJson = json(protoInterfCallJson[key]);
    vector<string> protoInterfVector;
    if(protoInterfJson.is_array())
        protoInterfVector = protoInterfJson.get<vector<string>>();
    if(find(protoInterfVector.begin(),protoInterfVector.end(),callerValueItem)==protoInterfVector.end())
        protoInterfJson.push_back(callerValueItem);
    protoInterfCallJson[key]=protoInterfJson;
}

void ValidAPIUtil::appendFunction(string function){
    json funcJson = json(funcEnumVarJson[kKeyFuncEnumVarFuncs]);
    vector<string> funcVec = funcJson.is_array()?funcJson.get<vector<string>>():vector<string>();
    if(find(funcVec.begin(),funcVec.end(),function)==funcVec.end())
        funcJson.push_back(function);
    funcEnumVarJson[kKeyFuncEnumVarFuncs]=funcJson;
}

void ValidAPIUtil::appendVar(string var){
    json varJson = json(funcEnumVarJson[kKeyFuncEnumVarVars]);
    vector<string> varVec = varJson.is_array()?varJson.get<vector<string>>():vector<string>();
    if(find(varVec.begin(),varVec.end(),var)==varVec.end())
        varJson.push_back(var);
    funcEnumVarJson[kKeyFuncEnumVarVars]=varJson;
}

void ValidAPIUtil::appendEnum(string em){
    json enumJson = json(funcEnumVarJson[kKeyFuncEnumVarEnums]);
    vector<string> enumVec = enumJson.is_array()?enumJson.get<vector<string>>():vector<string>();
    if(find(enumVec.begin(),enumVec.end(),em)==enumVec.end())
        enumJson.push_back(em);
    funcEnumVarJson[kKeyFuncEnumVarEnums]=enumJson;
}

void ValidAPIUtil::synchronize(){
    string fileprefix = string(filename);
    if(!fileprefix.length()){
        ofstream ofs;
        ofs.open (gSrcRootPath+"error.json",ofstream::out);
        ofs<<"Error:Filename not set."<<clsMethodJson<<endl;
        ofs.close();
        cout<<"[KWLM]:Nofilename"<<endl;
    }
    writeJsonToFile(clsMethodJson, gSrcRootPath+"/Analyzer/"+fileprefix+".clsMethod.jsonpart");
    writeJsonToFile(clsInterfHierachy, gSrcRootPath+"/Analyzer/"+fileprefix+".clsInterfHierachy.jsonpart");
    writeJsonToFile(protoInterfHierachy, gSrcRootPath+"/Analyzer/"+fileprefix+".protoInterfHierachy.jsonpart");
    writeJsonToFile(clsMethodAddNotifsJson,gSrcRootPath+"/Analyzer/"+fileprefix+".clsMethodAddNotifs.jsonpart");
    writeJsonToFile(notifPostedCallersJson, gSrcRootPath+"/Analyzer/"+fileprefix+".notifPostedCallers.jsonpart");
    writeJsonToFile(protoInterfCallJson, gSrcRootPath+"/Analyzer/"+fileprefix+".protoInterfCall.jsonpart");
    writeJsonToFile(funcEnumVarJson, gSrcRootPath+"/Analyzer/"+fileprefix+".funcEnumVar.jsonpart");
    writeJsonToFile(clsPropertyGSJson, gSrcRootPath+"/Analyzer/"+fileprefix+".clsPropertyGS.jsonpart");
    writeJsonToFile(protoPropertyGSJson, gSrcRootPath+"/Analyzer/"+fileprefix+".protoPropertyGS.jsonpart");
}
