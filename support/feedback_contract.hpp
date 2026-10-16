#pragma once
#include "core.hpp"
#include "support_content.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
namespace shaker {
inline const std::string feedbackEndpoint=supportContent()["feedback"]["endpoint"].get<std::string>();
inline std::string feedbackTrim(std::string s){
    auto space=[](unsigned char c){return std::isspace(c)!=0;};
    s.erase(s.begin(),std::find_if_not(s.begin(),s.end(),space));
    s.erase(std::find_if_not(s.rbegin(),s.rend(),space).base(),s.end());return s;
}
inline Json feedbackPayload(std::string name,std::string email,std::string message,const std::string& version,const std::string& appId="dcs2psvr2-shaker"){
    name=feedbackTrim(name);email=feedbackTrim(email);message=feedbackTrim(message);
    if(name.empty()||name.size()>100)throw std::runtime_error("Enter your name (up to 100 characters).");
    const auto at=email.find('@');const auto dot=email.find('.',at==std::string::npos?0:at+1);
    if(email.size()>254||at==std::string::npos||at==0||at+1>=email.size()||dot==std::string::npos||dot==at+1||dot+1>=email.size()||email.find('@',at+1)!=std::string::npos||
       std::any_of(email.begin(),email.end(),[](unsigned char c){return std::isspace(c)!=0;}))
        throw std::runtime_error("Enter a valid email address.");
    if(message.empty()||message.size()>4000)throw std::runtime_error("Enter a message (up to 4000 characters).");
    return Json{{"app",appId},{"version",version},{"name",name},{"email",email},{"message",message}};
}
inline bool feedbackAccepted(unsigned status,const std::string& response){
    const auto json=Json::parse(response,nullptr,false);
    return status==200&&response.size()<=1024&&json.is_object()&&json.size()==1&&json.contains("ok")&&json["ok"].is_boolean()&&json["ok"].get<bool>();
}
}
