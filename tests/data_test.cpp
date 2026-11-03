#include "../src/chat_data.hpp"
#include <cassert>
#include <iostream>
using namespace uw;using namespace uw::data;
int main(){
 Conversation c;c.id="a-1";c.thread="codex-thread";c.title=u8"图片与文件 😀";c.model="test";c.effort="low";c.updated=1793664000;c.https=false;c.interrupted=true;
 c.messages={{true,u8"分析\n\"引号\"\\",{{u8"输入.png",u8"C:\\用户\\attachments\\a.png",true,""}}},{false,"# Result\n[Download](widget-output:out.csv)",{{"out.csv","C:\\outputs\\out.csv",false,""},{"image.png","C:\\attachments\\b.png",true,"https://example.invalid/a.png?q=1&b=2"}}}};
 auto raw=serialize(c);auto d=parse(raw);assert(serialize(d)==raw&&d.messages[1].attachments[1].url==c.messages[1].attachments[1].url);assert(d.messages[0].user&&!d.messages[1].user&&!d.https&&d.interrupted);assert(d.updated==1793664000);
 for(auto text:{R"({"result":{}})",R"({"params":{"rateLimitResetCredits":null}})",R"({"result":{"rateLimitResetCredits":{"availableCount":-1}}})",R"({"result":{"rateLimitResetCredits":{"credits":[{"status":"available"}]}}})"})assert(!credits(Parser(text).parse()).known);
 auto credit=credits(Parser(R"({"result":{"rateLimitResetCredits":{"availableCount":7,"credits":[{"status":"available","expiresAt":1800},{"status":"expired","expiresAt":10},{"status":"available","expiresAt":1500}]}}})").parse());assert(credit.known&&credit.count==7&&credit.expiry==1500);
 credit=credits(Parser(R"({"params":{"rateLimitResetCredits":{"availableCount":0,"credits":null}}})").parse());assert(credit.known&&credit.count==0&&!credit.expiry);
 for(auto s:{"reset","alreadyRedeemed","nothingToReset","noCredit"})assert(confirmedResetOutcome(s));for(auto s:{"unknown","","timeout","newOutcome"})assert(!confirmedResetOutcome(s));
 for(auto s:{"report.csv",u8"分析.md","hello world.txt"})assert(safeOutputName(s));for(auto s:{"",".","..","../escape.txt","..\\escape.txt","C:report.txt","report. ","report.txt\n","trailing.","bad\rname.txt"})assert(!safeOutputName(s));assert(!safeOutputName(std::string("hidden\0.txt",11)));assert(!safeOutputName(std::string(181,'a')));
 std::cout<<"PASS conversation/attachment Unicode roundtrip, restart flags, authoritative reset count/expiry, unknown reset preservation and output filename confinement\n";
}
