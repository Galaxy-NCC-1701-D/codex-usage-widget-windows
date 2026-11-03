// Standalone protocol fixture; never contacts OpenAI and never logs message content.
#include "../src/chat_protocol.hpp"
#include <iostream>
int main(){std::string line;bool ready=false;int turn=0,credits=2;std::set<std::string>redeemed;while(std::getline(std::cin,line)){try{auto j=uw::Parser(line).parse();auto method=j.at("method").str;int id=(int)j.at("id").number;auto reply=[&](const std::string&result){std::cout<<"{\"id\":"<<id<<",\"result\":"<<result<<"}"<<std::endl;};
 if(method=="initialize")reply(R"({"userAgent":"mock"})");else if(method=="initialized")ready=true;
 else if(method=="config/read"&&ready)reply(R"({"config":{"model_provider":"usage_widget_https","model_providers":{"usage_widget_https":{"name":"OpenAI","base_url":"https://chatgpt.com/backend-api/codex","wire_api":"responses","requires_openai_auth":true,"supports_websockets":false}}}})");
 else if(method=="account/read"&&ready)reply(R"({"account":{"type":"chatgpt","email":"mock@example.invalid","planType":"plus"}})");
 else if(method=="model/list"&&ready)reply(R"({"data":[{"model":"fixture-model","displayName":"Fixture model","isDefault":true,"defaultReasoningEffort":"medium","supportedReasoningEfforts":[{"reasoningEffort":"low"},{"reasoningEffort":"medium"}]}],"nextCursor":null})");
 else if(method=="account/rateLimits/read"&&ready)reply(std::string("{\"rateLimitResetCredits\":{\"availableCount\":")+std::to_string(credits)+",\"credits\":null},"+R"("rateLimits":{"primary":{"usedPercent":28,"windowDurationMins":300,"resetsAt":1999999999},"secondary":{"usedPercent":54,"windowDurationMins":10080,"resetsAt":1999999999}}})");
 else if((method=="thread/start"||method=="thread/resume")&&ready)reply(R"({"thread":{"id":"mock-thread"}})");
 else if(method=="turn/start"&&ready){auto tid="mock-turn-"+std::to_string(++turn);reply("{\"turn\":{\"id\":"+uw::chat::quote(tid)+",\"status\":\"inProgress\"}}");auto prefix="{\"method\":\"item/agentMessage/delta\",\"params\":{\"threadId\":\"mock-thread\",\"turnId\":"+uw::chat::quote(tid)+",\"itemId\":\"mock-item\",\"delta\":";for(auto text:{u8"Mock reply: 你好！\n",u8"Follow-up context uses the same thread.\n",u8"This fixture makes no network request."})std::cout<<prefix<<uw::chat::quote(text)<<"}}"<<std::endl;std::cout<<"{\"method\":\"turn/completed\",\"params\":{\"threadId\":\"mock-thread\",\"turn\":{\"id\":"<<uw::chat::quote(tid)<<",\"status\":\"completed\"}}}"<<std::endl;}
 else if(method=="thread/delete"&&ready)reply("{}");
 else if(method=="account/rateLimitResetCredit/consume"&&ready){auto key=j.at("params").at("idempotencyKey").str;auto outcome=redeemed.count(key)?"alreadyRedeemed":credits>0?"reset":"noCredit";if(outcome==std::string("reset")){credits--;redeemed.insert(key);}reply("{\"outcome\":"+uw::chat::quote(outcome)+"}");}
 else if(method=="turn/interrupt")reply("{}");
 }catch(...){return 2;}}}
