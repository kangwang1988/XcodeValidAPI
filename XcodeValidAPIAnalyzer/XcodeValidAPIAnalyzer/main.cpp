#include <iostream>
#include <string>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <fstream>
#include <iterator>
#include "json.hpp"
#include "ValidAPIUtil.hpp"

using namespace std;
using namespace nlohmann;

static string kKeyInterfSelDictFilename = "filename";
static string kKeyInterfSelDictSourceCode = "sourceCode";
static string kKeyInterfSelDictRange = "range";
static string kKeyInterfSelDictCallees = "callee";
static string kKeyInterfSelDictSuperClass = "superClass";
static string kKeyInterfSelDictProtos = "protos";
static string kKeyInterfSelDictInterfs = "interfs";
static string kKeyInterfSelDictIsInSrcDir = "isInSrcDir";
static string kKeyInterfSelDictNotifCallers = "notifCallers";
static string kKeyFuncEnumVarFuncs = "funcs";
static string kKeyFuncEnumVarEnums = "enums";
static string kKeyFuncEnumVarVars = "vars";
static string kKeywordFoundOK = "#FoundOK#";
string deployMinVer,curTarget,deployMaxVer;

json verClsJson,verClsInterfJson,verClsPropertyJson,verConstJson,verFuncJson,verProtoJson,verProtoInterfJson,verProtoPropertyJson;

typedef enum{
    ClsMethodTokenMethodType,
    ClsMethodTokenClass,
    ClsMethodTokenSel
}ClsMethodTokenType;

#pragma mark - Util
string joinedClsMethod(bool isInstanceMethod,string cls,string sel){
    return string(isInstanceMethod?"-":"+")+"["+cls+" "+sel+"]";
}

string getClsMethodToken(string clsMethod,ClsMethodTokenType tokenType){
    clsMethod = trim(clsMethod);
    switch (tokenType) {
        case ClsMethodTokenMethodType:
        {
            size_t pos = clsMethod.find("[");
            string str = clsMethod.substr(0,pos);
            if(pos != string::npos)
                return trim(str);
            break;
        }
        case ClsMethodTokenClass:
        {
            size_t pos = clsMethod.find("[");
            if(pos != string::npos){
                string str = clsMethod.substr(pos+1,clsMethod.length()-pos+1);
                pos = str.find(" ");
                if(pos != string::npos){
                    str = str.substr(0,pos);
                    return trim(str);
                }
            }
            break;
        }
        case ClsMethodTokenSel:
        {
            size_t pos = clsMethod.rfind(" ");
            if(pos != string::npos){
                string str = clsMethod.substr(pos+1,clsMethod.length()-pos-2);
                return trim(str);
            }
            break;
        }
        default:
            break;
    }
    return "";
}

string exec(const char* cmd) {
    char buffer[128];
    string result = "";
    shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL)
            result += buffer;
    }
    return result;
}

vector<string> jsonPartFiles(string folderDir,string suffix){
    stringstream ss;
    ss<<"find "<<folderDir<<" -type f -name \"*."<<suffix<<"\"";
    string findOpt = exec(ss.str().c_str());
    return split(findOpt,'\n');
}

int writeJsonToFile(json j,const string & file){
    fstream fs;
    string clsMethodJsonFile = file;
    fs.open(clsMethodJsonFile,ofstream::out | ofstream::trunc);
    if(!j.is_null())
        fs<<j;
    fs.close();
    return 0;
}

json readJsonFromFile(const string& file){
    ifstream ifs(file);
    string str((istreambuf_iterator<char>(ifs)),
               istreambuf_iterator<char>());
    return json::parse(str);
}

static string validateTokenVersion(string msgPref,string tokenVersionAbbr){
    if(!tokenVersionAbbr.length()){
        return "";
    }
    string msg;
    string fromVer(""),curVer(""),depreVer("");
    vector<string> verVec = split(tokenVersionAbbr, '@');
    fromVer = verVec.at(0);
    curVer = verVec.at(1);
    if(verVec.size()==3)
        depreVer = verVec.at(2);
    if(fromVer.length() && atof(fromVer.c_str())>atof(deployMinVer.c_str()))
        msg+=" start from "+fromVer+" ";
    if(depreVer.length() && atof(depreVer.c_str())<=atof(deployMaxVer.c_str()))
        msg+=" deprecate from "+depreVer+" ";
    if(msg.length())
        msg = msgPref+" "+msg;
    else
        msg = kKeywordFoundOK;
    return msg;
}

template<class T>
vector<T> mergeVector(vector<T> vec1,vector<T> vec2){
    vector<T> mergedVec;
    mergedVec.reserve(vec1.size() + vec2.size() );
    mergedVec.insert( mergedVec.end(), vec1.begin(), vec1.end() );
    mergedVec.insert( mergedVec.end(), vec2.begin(), vec2.end() );
    sort( mergedVec.begin(), mergedVec.end() );
    mergedVec.erase(unique( mergedVec.begin(), mergedVec.end() ), mergedVec.end() );
    return mergedVec;
}

json joinJsonPartFiles(vector<string> splittedElems,json &dupClsMethodJson){
    json allJson;
    for(vector<string>::iterator it=splittedElems.begin();it!=splittedElems.end();++it){
        json jpj = readJsonFromFile(*it);
        if(jpj.is_null())
            continue;
        for (json::iterator it = jpj.begin(); it != jpj.end(); ++it) {
            string key = it.key();
            json valueNew = it.value();
            if(allJson.find(key)==allJson.end()){
                allJson[key]=valueNew;
                continue;
            }
            json valueOld = json(allJson[key]);
            if(valueOld.is_null() || valueNew.is_null()){
                allJson[key]=(valueOld.is_null()?valueNew:valueOld);
                continue;
            }
            if(valueOld.type()!=valueNew.type()){
                continue;
            }
            if(valueOld.is_string() && valueNew.is_string()){
                continue;
            }
            if(valueOld.is_array() && valueNew.is_array()){
                vector<string> vecAB = mergeVector(valueOld.get<vector<string>>(), valueNew.get<vector<string>>());
                valueNew = json();
                for(vector<string>::iterator it = vecAB.begin();it!=vecAB.end();it++)
                    valueNew.push_back(*it);
                allJson[key]=valueNew;
                continue;
            }
            for(json::iterator itOld = valueOld.begin();itOld!=valueOld.end();itOld++){
                string subOldKey = itOld.key();
                json subOldValue = itOld.value();
                if(valueNew.find(subOldKey)==valueNew.end()){
                    valueNew[subOldKey]=subOldValue;
                    continue;
                }
                json subNewValue = valueNew[subOldKey];
                if(subOldValue.is_null() || subNewValue.is_null()){
                    valueNew[subOldKey]=(subOldValue.is_null()?subNewValue:subOldValue);
                    continue;
                }
                if(subOldValue.type()!=subNewValue.type()){
                    continue;
                }
                if(subOldValue.is_array() && subNewValue.is_array()){
                    vector<string> vecAB = mergeVector(subOldValue.get<vector<string>>(), subNewValue.get<vector<string>>());
                    subNewValue = json();
                    for(vector<string>::iterator it = vecAB.begin();it!=vecAB.end();it++)
                        subNewValue.push_back(*it);
                    valueNew[subOldKey]=subNewValue;
                    continue;
                }
                if(subOldValue.is_string() && subNewValue.is_string()){
                    if(!subOldValue.get<string>().compare(subNewValue.get<string>())){
                    }
                    else if(!subOldValue.get<string>().length() || !subNewValue.get<string>().length()){
                        if(!subNewValue.get<string>().length()){
                            subNewValue=subOldValue;
                            valueNew[subOldKey]=subNewValue;
                        }
                    }
                    else if(!subOldKey.compare(kKeyInterfSelDictFilename) ||
                            !subOldKey.compare(kKeyInterfSelDictRange) ||
                            !subOldKey.compare(kKeyInterfSelDictSourceCode)){
                        json cmJson =json(dupClsMethodJson[key]);
                        json fnJson = json(cmJson[subOldKey]);
                        vector<string> fnVec;
                        if(fnJson.is_array())
                            fnVec = fnJson.get<vector<string>>();
                        if(find(fnVec.begin(),fnVec.end(),subOldValue)==fnVec.end())
                            fnJson.push_back(subOldValue);
                        if(find(fnVec.begin(),fnVec.end(),subNewValue)==fnVec.end())
                            fnJson.push_back(subNewValue);
                        cmJson[subOldKey] = fnJson;
                        dupClsMethodJson[key]=cmJson;
                    }
                    else{
                    }
                    continue;
                }
            }
            allJson[key]=valueNew;
        }
    }
    return allJson;
}

#pragma mark - Version Validation
//继承关系搜索类
string searchClsVersionByClass(string byCls,json &clsInterfHierachyJson,json &verClsJson){
    if(!byCls.length()){
        return "";
    }
    json verAbbrJson = verClsJson[byCls];
    string verAbbr = verAbbrJson.is_string()?verAbbrJson.get<string>():"";
    if(verAbbr.length()){
        string msg = validateTokenVersion(string("Class:")+byCls,verAbbr);
        if(msg.length())
            return msg;
    }
    json supClassJson = json(json(clsInterfHierachyJson[byCls])[kKeyInterfSelDictSuperClass]);
    string supClass = (supClassJson.is_string()?supClassJson.get<string>():"");
    if(!byCls.compare("NSObject"))
        return "";
    return searchClsVersionByClass(supClass,clsInterfHierachyJson,verClsJson);
}

string searchClsMethodByClassWithSpecificProtocol(string clsMethod,string specificProtocol,json &clsInterfHierachyJson,json &protoInterfHierachyJson,json &clsPropertyGSJson,json &protoPropertyGSJson){
    string methodType,property,verAbbr,msg,sel;
    methodType = getClsMethodToken(clsMethod, ClsMethodTokenMethodType);
    sel  = getClsMethodToken(clsMethod, ClsMethodTokenSel);
    string protocolClsMethod = joinedClsMethod(!methodType.compare("-"), specificProtocol,sel);
    json tmpJson,verAbbrJson;
    //按照Selector处理
    verAbbrJson = verProtoInterfJson[protocolClsMethod];
    verAbbr = verAbbrJson.is_string()?verAbbrJson.get<string>():"";
    msg = validateTokenVersion(string("Protocol: ")+specificProtocol+" Selector: "+sel,verAbbr);
    if(msg.length())
        return msg;
    //按照property处理
    tmpJson = protoPropertyGSJson[specificProtocol];
    if(!tmpJson.is_null()){
        tmpJson = tmpJson[protocolClsMethod];
        property = tmpJson.is_string()?tmpJson.get<string>():"";
        verAbbrJson = verProtoPropertyJson[protocolClsMethod];
        verAbbr = verAbbrJson.is_string()?verAbbrJson.get<string>():"";
        msg = validateTokenVersion(string("Protocol: ")+specificProtocol+" Property: "+property,verAbbr);
        if(msg.length())
            return msg;
    }
    //处理引用的protocol
    json protoProtos = json(json(protoInterfHierachyJson[specificProtocol])[kKeyInterfSelDictProtos]);
    for(json::iterator it = protoProtos.begin();it!=protoProtos.end();it++){
        string msg = searchClsMethodByClassWithSpecificProtocol(clsMethod,*it, clsInterfHierachyJson, protoInterfHierachyJson, clsPropertyGSJson, protoPropertyGSJson);
        if(msg.length())
            return msg;
    }
    if(!specificProtocol.compare("NSObject"))
        return "";
    return "";
}

string searchClsMethodByClassWithProtocols(string clsMethod,string byCls,json &clsInterfHierachyJson,json &protoInterfHierachyJson,json &clsPropertyGSJson,json &protoPropertyGSJson){
    string methodType = getClsMethodToken(clsMethod,ClsMethodTokenMethodType);
    string sel = getClsMethodToken(clsMethod,ClsMethodTokenSel);
    if(!byCls.length() || !sel.length()){
        return "";
    }
    string msg,byClsMethod,property,verAbbr;
    json tmpJson,verAbbrJson;
    //按照Selector先处理
    byClsMethod = joinedClsMethod(!methodType.compare("-"), byCls,sel);
    tmpJson = verClsInterfJson[byClsMethod];
    msg = validateTokenVersion(string("Class: ")+byCls+" Sel: "+sel,tmpJson.is_string()?tmpJson.get<string>():"");
    if(msg.length())
        return msg;
    //按照property处理
    tmpJson= clsPropertyGSJson[byCls];
    if(!tmpJson.is_null()){
        tmpJson = tmpJson[byClsMethod];
        property = tmpJson.is_string()?tmpJson.get<string>():"";
        verAbbrJson = verClsPropertyJson[byClsMethod];
        verAbbr = verAbbrJson.is_string()?verAbbrJson.get<string>():"";
        msg = validateTokenVersion(string("Class: ")+byCls+"Property: "+property,verAbbr);
        if(msg.length())
            return msg;
    }
    json clsProtocols = json(json(clsInterfHierachyJson[byCls])[kKeyInterfSelDictProtos]);
    for(json::iterator protoIt = clsProtocols.begin();protoIt!=clsProtocols.end();protoIt++){
        string specificProtocol = *protoIt;
        string msg = searchClsMethodByClassWithSpecificProtocol(clsMethod, specificProtocol, clsInterfHierachyJson, protoInterfHierachyJson, clsPropertyGSJson, protoPropertyGSJson);
        if(msg.length())
            return msg;
    }
    if(!byCls.compare("NSObject"))
        return "";
    json supClassJson = json(json(clsInterfHierachyJson[byCls])[kKeyInterfSelDictSuperClass]);
    string supClass = (supClassJson.is_string()?supClassJson.get<string>():"");
    return searchClsMethodByClassWithProtocols(joinedClsMethod(!methodType.compare("-"),supClass, sel),supClass,clsInterfHierachyJson,protoInterfHierachyJson,clsPropertyGSJson,protoPropertyGSJson);
}

string searchProtoMethodByProtoWithProtocols(string protoMethod,string byProto,json &protoInterfHierachyJson,json &protoPropertyGSJson){
    string methodType = getClsMethodToken(protoMethod,ClsMethodTokenMethodType);
    string sel = getClsMethodToken(protoMethod,ClsMethodTokenSel);
    if(!protoMethod.length() || !sel.length()){
        return "";
    }
    string msg,byProtoMethod,property,verAbbr;
    json tmpJson,verAbbrJson;
    //按照Selector先处理
    byProtoMethod = joinedClsMethod(!methodType.compare("-"), byProto,sel);
    tmpJson = verProtoInterfJson[byProtoMethod];
    msg = validateTokenVersion(string("Protocol: ")+byProto+" Sel: "+sel,tmpJson.is_string()?tmpJson.get<string>():"");
    if(msg.length())
        return msg;
    //按照property处理
    tmpJson= protoPropertyGSJson[byProto];
    if(!tmpJson.is_null()){
        tmpJson = tmpJson[byProtoMethod];
        property = tmpJson.is_string()?tmpJson.get<string>():"";
        verAbbrJson = verProtoPropertyJson[byProtoMethod];
        verAbbr = verAbbrJson.is_string()?verAbbrJson.get<string>():"";
        msg = validateTokenVersion(string("Protocol: ")+byProto+"Property: "+property,verAbbr);
        if(msg.length())
            return msg;
    }
    json clsProtocols = json(json(protoInterfHierachyJson[byProto])[kKeyInterfSelDictProtos]);
    for(json::iterator protoIt = clsProtocols.begin();protoIt!=clsProtocols.end();protoIt++){
        string specificProtocol = *protoIt;
        string msg = searchProtoMethodByProtoWithProtocols(protoMethod, specificProtocol, protoInterfHierachyJson, protoPropertyGSJson);
        if(msg.length())
            return msg;
    }
    return "";
}

#pragma mark - Main
int main(int argc,char *argv[]){
    if(argc!=4){
        cout<<"Usage:"<<"postanalyze your-path-of-jsonparts-for-analyzing deploy-target-system-version latest-system-version"<<endl;
        return -1;
    }
    string folder(argv[1]);
    deployMinVer = argv[2],deployMaxVer = argv[3];
    json dupClsMethodJson;
    json clsMethodJson =  joinJsonPartFiles(jsonPartFiles(folder,"clsMethod.jsonpart"),dupClsMethodJson);
    writeJsonToFile(clsMethodJson,folder+"/clsMethod.json");
    
    json dupClsInterfHierachyJson;
    json clsInterfHierachyJson =  joinJsonPartFiles(jsonPartFiles(folder,"clsInterfHierachy.jsonpart"),dupClsInterfHierachyJson);
    writeJsonToFile(clsInterfHierachyJson,folder+"/clsInterfHierachy.json");
    
    json dupProtoInterfHierachyJson;
    json protoInterfHierachyJson = joinJsonPartFiles(jsonPartFiles(folder,"protoInterfHierachy.jsonpart"),dupProtoInterfHierachyJson);
    writeJsonToFile(protoInterfHierachyJson,folder+"/protoInterfHierachy.json");
    
    json dupClsMethodAddNotifsJson;
    json clsMethodAddNotifsJson= joinJsonPartFiles(jsonPartFiles(folder,"clsMethodAddNotifs.jsonpart"),dupClsMethodAddNotifsJson);
    writeJsonToFile(clsMethodAddNotifsJson,folder+"/clsMethodAddNotifs.json");
    
    json dupNotifPostedCallersJson;
    json notifPostedCallersJson = joinJsonPartFiles(jsonPartFiles(folder,"notifPostedCallers.jsonpart"),dupNotifPostedCallersJson);
    writeJsonToFile(notifPostedCallersJson,folder+"/notifPostedCallers.json");
    
    json dupProtoInterfCallJson;
    json protoInterfCallJson =   joinJsonPartFiles(jsonPartFiles(folder,"protoInterfCall.jsonpart"),dupProtoInterfCallJson);
    writeJsonToFile(protoInterfCallJson,folder+"/protoInterfCall.json");
    
    json dupClsPropertyGSJson;
    json clsPropertyGSJson = joinJsonPartFiles(jsonPartFiles(folder,"clsPropertyGS.jsonpart"),dupClsPropertyGSJson);
    writeJsonToFile(clsPropertyGSJson,folder+"/clsPropertyGS.json");
    
    json dupProtoPropertyGSJson;
    json protoPropertyGSJson = joinJsonPartFiles(jsonPartFiles(folder,"protoPropertyGS.jsonpart"),dupProtoPropertyGSJson);
    writeJsonToFile(protoPropertyGSJson,folder+"/protoPropertyGS.json");
    
    json dupFuncEnumVarJson;
    json funcEnumVarJson = joinJsonPartFiles(jsonPartFiles(folder,"funcEnumVar.jsonpart"),dupFuncEnumVarJson);
    writeJsonToFile(funcEnumVarJson,folder+"/funcEnumVar.json");
    
    verClsJson = readJsonFromFile(folder+"/VerCls.json");
    verClsInterfJson = readJsonFromFile(folder+"/VerClsInterf.json");
    verClsPropertyJson = readJsonFromFile(folder+"/VerClsProperty.json");
    verConstJson = readJsonFromFile(folder+"/VerConst.json");
    verFuncJson = readJsonFromFile(folder+"/VerFunc.json");
    verProtoJson = readJsonFromFile(folder+"/VerProto.json");
    verProtoInterfJson = readJsonFromFile(folder+"/VerProtoInterf.json");
    verProtoPropertyJson = readJsonFromFile(folder+"/VerProtoProperty.json");
    
    vector<string> tmpVec;
    string msg;
    //处理Class
    for(json::iterator it = clsInterfHierachyJson.begin();it!=clsInterfHierachyJson.end();it++){
        json clsIsInSrcDirJson = it.value()[kKeyInterfSelDictIsInSrcDir];
        string clsIsInSrcDirStr = clsIsInSrcDirJson.is_string()?clsIsInSrcDirJson.get<string>():"";
        //只处理自己写的类
        if(!clsIsInSrcDirStr.compare("1") && find(tmpVec.begin(),tmpVec.end(),it.key())==tmpVec.end()){
            tmpVec.push_back(it.key());
        }
    }
    json allClsJson(tmpVec),warnClsJson;
    for(json::iterator it = allClsJson.begin();it!=allClsJson.end();it++){
        string cls = it.value();
        msg = searchClsVersionByClass(cls, clsInterfHierachyJson, verClsJson);
        if(msg.length() &&msg.compare(kKeywordFoundOK)){
            warnClsJson[cls]=msg;
        }
    }
    writeJsonToFile(warnClsJson,folder+"/warnCls.json");
    
    //处理函数引用
    tmpVec.clear();
    json warnFuncJson;
    json funcJson = funcEnumVarJson[kKeyFuncEnumVarFuncs];
    for(json::iterator it = funcJson.begin();it!=funcJson.end();it++){
        string funcName = it.value();
        json funcVerJson = verFuncJson[funcName];
        string funcVer = funcVerJson.is_string()?funcVerJson.get<string>():"";
        msg = validateTokenVersion(string("Function:")+funcName, funcVer);
        if(msg.length() &&msg.compare(kKeywordFoundOK) && warnFuncJson.find(funcName)==warnFuncJson.end()){
            warnFuncJson[funcName]=msg;
        }
    }
    writeJsonToFile(warnFuncJson,folder+"/warnFuncs.json");
    
    //处理枚举
    tmpVec.clear();
    json warnEnumJson;
    json enumJson = funcEnumVarJson[kKeyFuncEnumVarEnums];
    for(json::iterator it = enumJson.begin();it!=enumJson.end();it++){
        string enumName = it.value();
        json enumVerJson = verConstJson[enumName];
        string enumVer = enumVerJson.is_string()?enumVerJson.get<string>():"";
        msg = validateTokenVersion(string("Enum:")+enumName, enumVer);
        if(msg.length() &&msg.compare(kKeywordFoundOK) && warnEnumJson.find(enumName)==warnEnumJson.end()){
            warnEnumJson[enumName]=msg;
        }
    }
    writeJsonToFile(warnEnumJson,folder+"/warnEnum.json");
    
    //处理变量
    json warnVarJson;
    tmpVec.clear();
    json varJson = funcEnumVarJson[kKeyFuncEnumVarVars];
    for(json::iterator it = varJson.begin();it!=varJson.end();it++){
        string varName = it.value();
        json varVerJson = verConstJson[varName];
        string enumVer = varVerJson.is_string()?varVerJson.get<string>():"";
        msg = validateTokenVersion(string("Var:")+varName, enumVer);
        if(msg.length() &&msg.compare(kKeywordFoundOK) && warnVarJson.find(varName)==warnVarJson.end()){
            warnVarJson[varName]=msg;
        }
    }
    writeJsonToFile(warnVarJson,folder+"/warnVar.json");
    
    //处理Cls-Selector使用
    json warnClsSelJson;
    vector<string> clsMethodVec;
    for(json::iterator it = clsMethodJson.begin();it!=clsMethodJson.end();it++){
        string clsMethod = it.key();
        json clsSelInfo = it.value();
        clsMethodVec.push_back(clsMethod);
        json callees = json(json(clsMethodJson[clsMethod])[kKeyInterfSelDictCallees]);
        vector<string> tmpVec = callees.is_array()?callees.get<vector<string>>():vector<string>();
        clsMethodVec.insert(clsMethodVec.end(),tmpVec.begin(),tmpVec.end());
    }
    sort( clsMethodVec.begin(), clsMethodVec.end() );
    clsMethodVec.erase( unique( clsMethodVec.begin(), clsMethodVec.end() ), clsMethodVec.end());
    for(vector<string>::iterator it = clsMethodVec.begin();it!=clsMethodVec.end();it++){
        string clsMethod = *it;
        msg = searchClsMethodByClassWithProtocols(clsMethod,getClsMethodToken(clsMethod, ClsMethodTokenClass), clsInterfHierachyJson, protoInterfCallJson, clsPropertyGSJson, protoPropertyGSJson);
        if(msg.length() &&msg.compare(kKeywordFoundOK) && warnClsSelJson.find(clsMethod)==warnClsSelJson.end()){
            warnClsSelJson[clsMethod]=msg;
        }
    }
    writeJsonToFile(warnClsSelJson,folder+"/warnClsSel.json");
    
    //处理id<Protocol>-Selector使用
    vector<string> protoMethodVec;
    json warnProtoSelJson;
    for(json::iterator it = protoInterfCallJson.begin();it!=protoInterfCallJson.end();it++){
        string protoMethod = it.key();
        protoMethodVec.push_back(protoMethod);
    }
    sort( protoMethodVec.begin(), protoMethodVec.end() );
    protoMethodVec.erase( unique( protoMethodVec.begin(), protoMethodVec.end() ), protoMethodVec.end());
    for(vector<string>::iterator it = protoMethodVec.begin();it!=protoMethodVec.end();it++){
        string protoMethod = *it;
        msg = searchProtoMethodByProtoWithProtocols(protoMethod, getClsMethodToken(protoMethod, ClsMethodTokenClass), protoInterfHierachyJson, protoPropertyGSJson);
        if(msg.length() &&msg.compare(kKeywordFoundOK) && warnProtoSelJson.find(protoMethod)==warnProtoSelJson.end()){
            warnProtoSelJson[protoMethod]=msg;
        }
    }
    writeJsonToFile(warnProtoSelJson,folder+"/warnProtoSel.json");
    return 0;
}
