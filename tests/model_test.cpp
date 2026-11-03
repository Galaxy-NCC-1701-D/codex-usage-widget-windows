#include "../src/model.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace uw;
auto q=extract(Parser(R"({"result":{"rateLimits":{"primary":{"usedPercent":28,"windowDurationMins":300,"resetsAt":1800000000},"secondary":{"usedPercent":54,"windowDurationMins":10080}}}})").parse());assert(q.five.available&&q.five.remaining==72&&q.week.remaining==46);
q=extract(Parser(R"({"params":{"rateLimits":{"primary":{"usedPercent":20,"windowDurationMins":10080},"secondary":null}}})").parse());assert(!q.five.available&&q.week.remaining==80);
q=extract(Parser(R"({"rateLimits":{"primary":{"usedPercent":101,"windowDurationMins":300}},"rateLimitsByLimitId":{"other":{"primary":{"usedPercent":5,"windowDurationMins":300}}}})").parse());assert(!q.five.available);
q=extract(Parser(R"({"rateLimitsByLimitId":{"codex":{"secondary":{"usedPercent":0,"windowDurationMins":300}}}})").parse());assert(q.five.remaining==100);
assert(Parser(R"({"x":"a\"b\u4e2d\ud83d\ude00"})").parse().at("x").str.size()>4);
for(auto s:{"{","[1,]","01","{\"a\":NaN}","1e","\"\\uD800\""}){bool bad=false;try{Parser(s).parse();}catch(...){bad=true;}assert(bad);}
std::cout<<"All quota/parser cases passed\n";}
