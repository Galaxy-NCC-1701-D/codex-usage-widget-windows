#pragma once
#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cctype>
namespace uw {
struct Json {
 enum Type { Null, Number, String, Object, Array, Boolean } type=Null;
 double number=0; std::string str; std::map<std::string,Json> obj; std::vector<Json> arr;
 const Json& at(const std::string& k) const {static const Json empty; auto i=obj.find(k);return i==obj.end()?empty:i->second;}
 bool has(const std::string& k)const{return obj.count(k)!=0;}
};
class Parser {
 const std::string&s;size_t p=0;int depth=0;
 void ws(){while(p<s.size()&&(s[p]==' '||s[p]=='\r'||s[p]=='\n'||s[p]=='\t'))++p;}
 char take(){if(p>=s.size())throw std::runtime_error("Unexpected end");return s[p++];}
 void expect(char c){if(take()!=c)throw std::runtime_error("Invalid JSON");}
 void utf8(std::string&o,unsigned c){if(c<128)o+=char(c);else if(c<2048){o+=char(192|(c>>6));o+=char(128|(c&63));}else if(c<65536){o+=char(224|(c>>12));o+=char(128|((c>>6)&63));o+=char(128|(c&63));}else{o+=char(240|(c>>18));o+=char(128|((c>>12)&63));o+=char(128|((c>>6)&63));o+=char(128|(c&63));}}
 unsigned hex(){unsigned v=0;for(int i=0;i<4;i++){char c=take();v<<=4;if(c>='0'&&c<='9')v+=c-'0';else if(c>='a'&&c<='f')v+=c-'a'+10;else if(c>='A'&&c<='F')v+=c-'A'+10;else throw std::runtime_error("Invalid unicode");}return v;}
 std::string string(){expect('"');std::string o;while(true){char c=take();if(c=='"')return o;if((unsigned char)c<32)throw std::runtime_error("Invalid string");if(c!='\\'){o+=c;continue;}c=take();switch(c){case '"':case '\\':case '/':o+=c;break;case 'b':o+='\b';break;case 'f':o+='\f';break;case 'n':o+='\n';break;case 'r':o+='\r';break;case 't':o+='\t';break;case 'u':{unsigned u=hex();if(u>=0xd800&&u<=0xdbff){expect('\\');expect('u');unsigned lo=hex();if(lo<0xdc00||lo>0xdfff)throw std::runtime_error("Invalid surrogate");u=0x10000+((u-0xd800)<<10)+lo-0xdc00;}else if(u>=0xdc00&&u<=0xdfff)throw std::runtime_error("Invalid surrogate");utf8(o,u);break;}default:throw std::runtime_error("Invalid escape");}}}
 Json value(){ws();if(++depth>64)throw std::runtime_error("Too deep");Json j;char c=p<s.size()?s[p]:0;if(c=='{'){j.type=Json::Object;++p;ws();if(p<s.size()&&s[p]=='}')++p;else while(true){ws();auto k=string();ws();expect(':');j.obj[k]=value();ws();char n=take();if(n=='}')break;if(n!=',')throw std::runtime_error("Invalid object");}}
 else if(c=='['){j.type=Json::Array;++p;ws();if(p<s.size()&&s[p]==']')++p;else while(true){j.arr.push_back(value());ws();char n=take();if(n==']')break;if(n!=',')throw std::runtime_error("Invalid array");}}
 else if(c=='"'){j.type=Json::String;j.str=string();}
 else if(c=='n'||c=='t'||c=='f'){std::string lit=c=='n'?"null":c=='t'?"true":"false";for(char x:lit)expect(x);if(c!='n'){j.type=Json::Boolean;j.number=c=='t';}}
 else {size_t start=p;if(c=='-')++p;if(p>=s.size())throw std::runtime_error("Bad number");if(s[p]=='0')++p;else{if(s[p]<'1'||s[p]>'9')throw std::runtime_error("Bad number");while(p<s.size()&&isdigit((unsigned char)s[p]))++p;}if(p<s.size()&&s[p]=='.'){++p;size_t b=p;while(p<s.size()&&isdigit((unsigned char)s[p]))++p;if(b==p)throw std::runtime_error("Bad fraction");}if(p<s.size()&&(s[p]=='e'||s[p]=='E')){++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;size_t b=p;while(p<s.size()&&isdigit((unsigned char)s[p]))++p;if(b==p)throw std::runtime_error("Bad exponent");}j.type=Json::Number;j.number=std::stod(s.substr(start,p-start));if(!std::isfinite(j.number))throw std::runtime_error("Nonfinite");}--depth;return j;}
 public:Parser(const std::string&text):s(text){} Json parse(){auto j=value();ws();if(p!=s.size())throw std::runtime_error("Trailing input");return j;}
};
struct Window {bool available=false;double remaining=0;int64_t resets=0;};
struct Quota {Window five,week;};
inline Quota extract(const Json& root){
 const Json*data=&root;if(root.has("result"))data=&root.at("result");else if(root.has("params"))data=&root.at("params");
 const Json*bucket=&data->at("rateLimits");const auto&all=data->at("rateLimitsByLimitId");if(all.has("codex"))bucket=&all.at("codex");
 // Never merge unrelated meter buckets or guess a missing window's duration.
 Quota q;for(auto key:{"primary","secondary"}){const auto&w=bucket->at(key);const auto&u=w.at("usedPercent");const auto&m=w.at("windowDurationMins");if(u.type!=Json::Number||m.type!=Json::Number||u.number<0||u.number>100)continue;Window v;v.available=true;v.remaining=100-u.number;auto&r=w.at("resetsAt");if(r.type==Json::Number&&r.number>0&&r.number<9e15)v.resets=(int64_t)r.number;if(m.number==300)q.five=v;else if(m.number==10080)q.week=v;}return q;
}
}
