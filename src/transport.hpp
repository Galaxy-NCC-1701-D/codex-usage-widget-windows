#pragma once
#include "model.hpp"
namespace uw::transport {
inline std::vector<std::string> overrides(bool https){if(!https)return {};return {"model_provider='usage_widget_https'","model_providers.usage_widget_https={name='OpenAI',base_url='https://chatgpt.com/backend-api/codex',wire_api='responses',requires_openai_auth=true,supports_websockets=false}"};}
inline bool verified(const Json&root){const auto&c=root.at("result").at("config");const auto&p=c.at("model_providers").at("usage_widget_https");return !root.has("error")&&c.at("model_provider").str=="usage_widget_https"&&p.at("supports_websockets").type==Json::Boolean&&p.at("supports_websockets").number==0&&p.at("requires_openai_auth").number==1&&p.at("wire_api").str=="responses"&&p.at("base_url").str=="https://chatgpt.com/backend-api/codex";}
inline std::string mismatch(const Json&root,bool https){if(root.has("error"))return "config/read failed (code "+std::to_string((int)root.at("error").at("code").number)+")";const auto&c=root.at("result").at("config");if(c.type!=Json::Object)return "config/read returned no configuration object";if(!https)return "";const auto&p=c.at("model_providers").at("usage_widget_https");if(c.at("model_provider").str!="usage_widget_https")return "HTTPS provider override was not applied";if(p.at("base_url").str!="https://chatgpt.com/backend-api/codex")return "HTTPS endpoint override was not applied";if(p.at("wire_api").str!="responses")return "HTTPS provider does not use Responses";if(p.at("requires_openai_auth").number!=1)return "HTTPS provider did not retain OpenAI authentication";if(p.at("supports_websockets").type!=Json::Boolean||p.at("supports_websockets").number)return "HTTPS provider did not confirm WebSockets disabled";return "";}
class Verification {
 bool https;enum State{Pending,Accepted,Rejected}state=Pending;int64_t deadline=0;std::string error;
 public:explicit Verification(bool secure):https(secure){}
 void begin(int64_t now){state=Pending;error.clear();deadline=now+30000;}
 bool observe(const Json&root){error=mismatch(root,https);state=error.empty()?Accepted:Rejected;return state==Accepted;}
 bool tick(int64_t now){if(state==Pending&&deadline&&now>deadline){state=Rejected;error="config/read timed out after 30 seconds";return true;}return false;}
 bool ready()const{return state==Accepted;}
 std::string reason()const{return state==Pending?"Connection configuration is still being checked":state==Accepted?"Connection configuration verified":error;}
};

}
