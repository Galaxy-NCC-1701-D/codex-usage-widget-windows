#pragma once
#include "chat_protocol.hpp"
namespace uw::data {
struct Attachment {std::string name,path;bool image=false;std::string url="";};
struct Message {bool user=false;std::string text;std::vector<Attachment>attachments;};
struct Conversation {std::string id,thread,title,model,effort;int64_t updated=0;bool https=true,interrupted=false;std::vector<Message>messages;};
inline std::string serialize(const Conversation&c){using chat::quote;std::string out="{\"id\":"+quote(c.id)+",\"thread\":"+quote(c.thread)+",\"title\":"+quote(c.title)+",\"model\":"+quote(c.model)+",\"effort\":"+quote(c.effort)+",\"updated\":"+std::to_string(c.updated)+",\"https\":"+(c.https?"true":"false")+",\"interrupted\":"+(c.interrupted?"true":"false")+",\"messages\":[";for(auto&m:c.messages){if(out.back()!='[')out+=',';out+="{\"user\":"+std::string(m.user?"true":"false")+",\"text\":"+quote(m.text)+",\"attachments\":[";for(auto&a:m.attachments){if(out.back()!='[')out+=',';out+="{\"name\":"+quote(a.name)+",\"path\":"+quote(a.path)+",\"image\":"+(a.image?"true":"false")+",\"url\":"+quote(a.url)+"}";}out+="]}";}return out+"]}";}
inline Conversation parse(const std::string&s){auto j=Parser(s).parse();Conversation c;c.id=j.at("id").str;c.thread=j.at("thread").str;c.title=j.at("title").str;c.model=j.at("model").str;c.effort=j.at("effort").str;c.updated=(int64_t)j.at("updated").number;c.https=j.at("https").number!=0;c.interrupted=j.at("interrupted").number!=0;for(auto&m:j.at("messages").arr){Message v;v.user=m.at("user").number!=0;v.text=m.at("text").str;for(auto&a:m.at("attachments").arr)v.attachments.push_back({a.at("name").str,a.at("path").str,a.at("image").number!=0,a.at("url").str});c.messages.push_back(v);}return c;}
inline bool confirmedResetOutcome(const std::string&s){return s=="reset"||s=="alreadyRedeemed"||s=="nothingToReset"||s=="noCredit";}
inline bool safeOutputName(const std::string&s){return !s.empty()&&s.size()<=180&&s!="."&&s!=".."&&s.find_first_of("/\\:\r\n") == std::string::npos&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32;})&&s.back()!='.'&&s.back()!=' ';}
struct ResetCredit {
    std::int64_t granted = 0;
    std::int64_t expires = 0;
    bool noExpiration = false;
};

struct Credits {
    bool known = false;
    int count = 0;
    bool detailsKnown = false;
    std::vector<ResetCredit> rows;
};

inline std::int64_t creditDate(const Json& value) {
    // Bound conversion to calendar years Windows can format.
    if (value.type != Json::Number || value.number <= 0 ||
        value.number > 253402300799.0 || std::floor(value.number) != value.number) return 0;
    return static_cast<std::int64_t>(value.number);
}

inline Credits credits(const Json& root) {
    const auto& payload = root.has("result") ? root.at("result") : root.at("params");
    const auto& summary = payload.at("rateLimitResetCredits");
    const auto& count = summary.at("availableCount");
    Credits result;
    if (count.type != Json::Number || count.number < 0 || count.number > 2147483647.0 ||
        std::floor(count.number) != count.number) return result;

    result.known = true;
    result.count = static_cast<int>(count.number);
    const auto& details = summary.at("credits");
    result.detailsKnown = details.type == Json::Array;
    for (const auto& item : details.arr) {
        if (item.at("status").str != "available") continue;
        result.rows.push_back({
            creditDate(item.at("grantedAt")),
            creditDate(item.at("expiresAt")),
            item.has("expiresAt") && item.at("expiresAt").type == Json::Null
        });
    }
    std::stable_sort(result.rows.begin(), result.rows.end(), [](const auto& a, const auto& b) {
        if (!a.expires) return false;
        return !b.expires || a.expires < b.expires;
    });
    if (result.rows.size() > static_cast<std::size_t>(result.count)) result.rows.resize(result.count);
    return result;
}
}
