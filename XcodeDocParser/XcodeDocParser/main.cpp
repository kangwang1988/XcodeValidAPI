//
//  main.cpp
//  XcodeDocParser
//
//  Created by KyleWong on 27/10/2016.
//  Copyright © 2016 KyleWong. All rights reserved.
//

#include <iostream>
#include <sqlite3.h>
#include <fstream>
#include "json.hpp"
#include "lzfse.h"
using namespace std;
using namespace nlohmann;
json clsJson,protoJson,clsInterfJson,clsPropertyJson,protoInterfJson,protoPropertyJson,constJson,funcJson;

int kSqlRecordStep = 100;
sqlite3 *mapDb,*cacheDb;
int mapRecordCnt,cacheRecordCnt;
int mapRC,cacheRC;
char *mapErrMsg = 0, *cacheErrMsg = 0;
string dbDir;
json constantVerJson,clsMethodVerJson,protoInterVerJson;

typedef enum{
    ClsMethodTokenMethodType,
    ClsMethodTokenClass,
    ClsMethodTokenSel
}ClsMethodTokenType;

static string joinedClsMethod(bool isInstanceMethod,string cls,string sel){
    return string(isInstanceMethod?"-":"+")+"["+cls+" "+sel+"]";
}

static vector<string> split(const string &s, char delim) {
    vector<string> elems;
    stringstream ss;
    ss.str(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

static inline bool has_suffix(const std::string &str, const std::string &suffix)
{
    return str.size() >= suffix.size() &&
    str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static inline string &ltrim(std::string &s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(),
                               not1(ptr_fun<int, int>(isspace))));
    return s;
}

// trim from end
static inline string &rtrim(string &s) {
    s.erase(find_if(s.rbegin(), s.rend(),
                    not1(ptr_fun<int, int>(isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline string &trim(string &s) {
    return ltrim(rtrim(s));
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

// Same as realloc(x,s), except x is freed when realloc fails
static inline void *lzfse_reallocf(void *x, size_t s) {
    void *y = realloc(x, s);
    if (y == 0) {
        free(x);
        return 0;
    }
    return y;
}

size_t uncompress(uint8_t *inData,size_t inSize,uint8_t **outData){
    size_t out_allocated = (4 * inSize);
    size_t out_size = 0;
    size_t aux_allocated = lzfse_decode_scratch_size();
    void *aux = aux_allocated ? malloc(aux_allocated) : 0;
    if (aux_allocated != 0 && aux == 0) {
        perror("malloc");
        exit(1);
    }
    uint8_t *out = (uint8_t *)malloc(out_allocated);
    if (out == 0) {
        perror("malloc");
        exit(1);
    }
    while (1) {
        out_size = lzfse_decode_buffer(out, out_allocated, inData, inSize, aux);
        // If output buffer was too small, grow and retry.
        if (out_size == 0 || (out_size == out_allocated)) {
            out_allocated <<= 1;
            out = (uint8_t *)lzfse_reallocf(out, out_allocated);
            if (out == 0) {
                perror("malloc");
                exit(1);
            }
            continue;
        }
        break;
    }
    free(aux);
    *outData = out;
    return out_size;
}

json readJsonFromFile(const string& file){
    ifstream ifs(file);
    string str((istreambuf_iterator<char>(ifs)),
               istreambuf_iterator<char>());
    return json::parse(str);
}

static void parseClsMethodExpr(string expr,string &lang,string &interfType,string &interf,string &selType,string &sel){
    vector<string> splitedVec = split(expr, '(');
    for(vector<string>::iterator it = splitedVec.begin();it!=splitedVec.end();it++){
        string token = *it;
        int pos = token.find(")");
        string tokenPref,tokenContent;
        if(pos!=string::npos){
            tokenPref = token.substr(0,pos);
            tokenContent = token.substr(pos+1,token.length()-pos-1);
        }
        else{
            pos = token.find(":");
            tokenPref = token.substr(0,pos);
            tokenContent = token.substr(pos+1,token.length()-pos-1);
        }
        if(!tokenPref.compare("c"))
            lang.assign(tokenContent);
        if(!tokenPref.compare("cs") || !tokenPref.compare("pl")){
            interfType.assign(tokenPref);
            interf.assign(tokenContent);
        }
        if(!tokenPref.compare("im") || !tokenPref.compare("cpy") || !tokenPref.compare("py") || !tokenPref.compare("cm")){
            selType.assign(tokenPref);
            sel.assign(tokenContent);
        }
    }
}

bool writeJsonToFile(json j, string filename){
    if(j.is_null()){
        return false;
    }
    ofstream ofs;
    ofs.open (filename,ofstream::out | ofstream::trunc);
    ofs<<j<<endl;
    ofs.close();
    return true;
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

static int cacheDbCallback(void *data, int argc, char **argv, char **pColName){
    string reference_path = *((string *)data);
    string relaPath = string(reference_path);
    replace(relaPath.begin(),relaPath.end(),'/','@');
    
    if(argc == 3){
        size_t compressed_size = atol(argv[1]);
        uint8_t *response_data = (uint8_t *)(argv[2]?argv[2]:NULL);
        uint8_t *outData=NULL;
        size_t uncompressed_size = uncompress(response_data, compressed_size, &outData);
        std::string s( reinterpret_cast< char const* >(outData),uncompressed_size) ;
        json j=json::parse(s);
        json kJson = j["k"],tJson = j["t"],yJson=j["y"],lJson=j["l"],uJson = j["u"];
        string ktype = kJson.is_string()?kJson.get<string>():"";
        string fromVer,toVer,deVer,lang,interfType,interf,selType,sel,constName,func,structName,property,macro,unionName;
        json tmpJson;
        string tmpStr;
        int pos;
        //只处理Objective-C
        if(!(lJson.is_string() && !lJson.get<string>().compare("occ")))
            return 0;
        //From-to Version
        for(json::iterator it = yJson.begin();it!=yJson.end();it++){
            tmpJson = it.value();
            json pJson = tmpJson["p"];
            tmpStr = pJson.is_string()?pJson.get<string>():"";
            if(!tmpStr.compare("ios")){
                fromVer = tmpJson["ir"].get<string>();
                toVer = tmpJson["cr"].get<string>();
                deVer = tmpJson["de"].is_string()?tmpJson["de"].get<string>():"";
                break;
            }
        }
        if(!fromVer.length() || !toVer.length()){
            return 0;
        }
        //Class
        if(!ktype.compare("cl")){
            tmpStr = uJson.is_string()?uJson.get<string>():"";
            parseClsMethodExpr(tmpStr, lang, interfType, interf, selType, sel);
            clsJson[interf]=fromVer+"@"+toVer+"@"+deVer;
        }
        //Protocol
        else if(!ktype.compare("intf")){
            tmpStr = uJson.is_string()?uJson.get<string>():"";
            parseClsMethodExpr(tmpStr, lang, interfType, interf, selType, sel);
            protoJson[interf]=fromVer+"@"+toVer+"@"+deVer;
        }
        //Interface Property
        else if(!ktype.compare("intfp")){
            tmpStr = uJson.is_string()?uJson.get<string>():"";
            parseClsMethodExpr(tmpStr, lang, interfType, interf, selType, sel);
            clsPropertyJson[joinedClsMethod(false, interf, sel)]=fromVer+"@"+toVer+"@"+deVer;
        }
        //Instance Method
        else if(!ktype.compare("instm") || !ktype.compare("instp")){
            tmpStr = uJson.is_string()?uJson.get<string>():"";
            parseClsMethodExpr(tmpStr, lang, interfType, interf, selType, sel);
            if(!ktype.compare("instm"))
                clsInterfJson[joinedClsMethod(true, interf, sel)] = fromVer+"@"+toVer+"@"+deVer;
            else{
                clsPropertyJson[joinedClsMethod(true,interf,sel)] = fromVer+"@"+toVer+"@"+deVer;
            }
        }
        //Class Method
        else if(!ktype.compare("cldata") || !ktype.compare("clm")){
            tmpStr = uJson.is_string()?uJson.get<string>():"";
            parseClsMethodExpr(tmpStr, lang, interfType, interf, selType, sel);
            if(!ktype.compare("clm")){
                clsInterfJson[joinedClsMethod(false,interf,sel)]=fromVer+"@"+toVer+"@"+deVer;
            }
            else{
                clsPropertyJson[joinedClsMethod(false,interf,sel)] = fromVer+"@"+toVer+"@"+deVer;
            }
        }
        //Interface Method/Data
        else if(!ktype.compare("intfm") || !ktype.compare("intfdata") || !ktype.compare("intfcm")){
            tmpStr = uJson.is_string()?uJson.get<string>():"";
            parseClsMethodExpr(tmpStr, lang, interfType, interf, selType, sel);
            if(!ktype.compare("intfdata")){
                protoPropertyJson[joinedClsMethod(true,interf,sel)]=fromVer+"@"+toVer+"@"+deVer;
            }
            else{
                protoInterfJson[joinedClsMethod(true,interf,sel)] = fromVer+"@"+toVer+"@"+deVer;
            }
        }
        //Func
        else if(!ktype.compare("func")){
            tmpStr = uJson.is_string()?uJson.get<string>():"";
            pos = tmpStr.rfind("@");
            if(pos!=string::npos)
                func = tmpStr.substr(pos+1,tmpStr.length()-pos-1);
            funcJson[func] = fromVer+"@"+toVer+"@"+deVer;
        }
        //Struct/Union
        else if(!ktype.compare("struct") || !ktype.compare("union")){
            tmpStr = uJson.is_string()?uJson.get<string>():"";
            pos = tmpStr.rfind("@");
            if(pos!=string::npos)
                structName = tmpStr.substr(pos+1,tmpStr.length()-pos-1);
        }
        //Struct Property
        else if(!ktype.compare("structp") || !ktype.compare("unionp")){
            tmpStr = uJson.is_string()?uJson.get<string>():"";
            pos = tmpStr.rfind("@");
            if(pos!=string::npos)
                property = tmpStr.substr(pos+1,tmpStr.length()-pos-1);
        }
        //Constant
        else if(!ktype.compare("data") || !ktype.compare("enum") || !ktype.compare("tdef") || !ktype.compare("tag") || !ktype.compare("econst")){
            tmpStr = uJson.is_string()?uJson.get<string>():"";
            pos = tmpStr.rfind("@");
            if(pos!=string::npos)
                constName = tmpStr.substr(pos+1,tmpStr.length()-pos-1);
            constJson[constName]=fromVer+"@"+toVer+"@"+deVer;
        }
        //Macro
        else if(!ktype.compare("macro")){
            tmpStr = uJson.is_string()?uJson.get<string>():"";
            pos = tmpStr.rfind("@");
            if(pos!=string::npos)
                macro = tmpStr.substr(pos+1,tmpStr.length()-pos-1);
        }
        else{
            if(reference_path.find("/")!=string::npos){
            }
        }
    }
    return 0;
}

static int mapDbCallback(void *data, int argc, char **argv, char **pColName){
    int *ptr = (int *)data;
    *ptr=*ptr+1;
    if(argc == 2){
        string request_key = string(argv[0]?argv[0]:"");
        string reference_path = string(argv[1]?argv[1]:"");
        char sqlBuf[1024]={0};
        sprintf(sqlBuf,"SELECT uncompressed_size,length(response_data),response_data from response where request_key='%s' ",request_key.c_str());
        /* Execute SQL statement */
        cacheRC = sqlite3_exec(cacheDb, sqlBuf, cacheDbCallback, (void*)&reference_path, &cacheErrMsg);
        if( cacheRC != SQLITE_OK ){
            cout<<"SQL error:"<<cacheErrMsg<<endl;
            sqlite3_free(cacheErrMsg);
        }
    }
    return 0;
}

int main(int argc, const char * argv[]) {
    if(argc!=2){
        cout<<"Usage:XcodeDocParser parent-directory-to-xcode-doc.db"<<endl;
        return -1;
    }
    dbDir = string(argv[1]);
    string mapDbFilename = dbDir+"/Analyzer/map.db";
    string cacheDbFilename = dbDir+"/Analyzer/cache.db";
    mapRC = sqlite3_open(mapDbFilename.c_str(), &mapDb);
    cacheRC = sqlite3_open(cacheDbFilename.c_str(), &cacheDb);
    if(mapRC){
        cout<<"Can't open database: "<<sqlite3_errmsg(mapDb)<<endl;
        return 1;
    }
    if(cacheRC){
        cout<<"Can't open database: "<<sqlite3_errmsg(cacheDb)<<endl;
        return 2;
    }
    /* Create SQL statement */
    int cursor = 0;
    while (true) {
        mapRecordCnt = 0;
        char sqlBuf[1024]={0};
        sprintf(sqlBuf,"SELECT request_key,reference_path from map where source_language=1 limit %d,%d",cursor,kSqlRecordStep);
        /* Execute SQL statement */
        mapRC = sqlite3_exec(mapDb, sqlBuf, mapDbCallback, (void*)&mapRecordCnt, &mapErrMsg);
        if(mapRC != SQLITE_OK ){
            cout<<"SQL error: "<<mapErrMsg<<endl;
            sqlite3_free(mapErrMsg);
            break;
        }
        else if(mapRecordCnt==0){
            break;
        }
        cursor+=mapRecordCnt;
    };
    sqlite3_close(mapDb);
    sqlite3_close(cacheDb);
    string keyCmp = "10.0@10.0@";
    for(json::iterator it = clsPropertyJson.begin();it!=clsPropertyJson.end();it++){
        string key = it.key();
        string value = it.value();
        string cls = getClsMethodToken(key,ClsMethodTokenClass);
        string clsVerAbbr = clsJson[cls].is_string()?clsJson[cls].get<string>():keyCmp;
        if(!value.compare(keyCmp) && clsVerAbbr.compare(keyCmp)){
            clsPropertyJson[key]=clsVerAbbr;
        }
    }
    for(json::iterator it = protoPropertyJson.begin();it!=protoPropertyJson.end();it++){
        string key = it.key();
        string value = it.value();
        string proto = getClsMethodToken(key,ClsMethodTokenClass);
        string protoVerAbbr = protoJson[proto].is_string()?protoJson[proto].get<string>():keyCmp;
        if(!value.compare(keyCmp) && protoVerAbbr.compare(keyCmp)){
            protoPropertyJson[key]=protoVerAbbr;
        }
    }
    writeJsonToFile(clsJson, dbDir+"/Analyzer/VerCls.json");
    writeJsonToFile(protoJson, dbDir+"/Analyzer/VerProto.json");
    writeJsonToFile(clsInterfJson, dbDir+"/Analyzer/VerClsInterf.json");
    writeJsonToFile(clsPropertyJson, dbDir+"/Analyzer/VerClsProperty.json");
    writeJsonToFile(protoInterfJson, dbDir+"/Analyzer/VerProtoInterf.json");
    writeJsonToFile(protoPropertyJson, dbDir+"/Analyzer/VerProtoProperty.json");
    writeJsonToFile(constJson, dbDir+"/Analyzer/VerConst.json");
    writeJsonToFile(funcJson, dbDir+"/Analyzer/VerFunc.json");
    return 0;
}
