#include "feedback_contract.hpp"
#include <iostream>
using namespace shaker;
void expect(bool value){if(!value)throw std::runtime_error("Feedback contract check failed");}
int main(){
    const auto body=feedbackPayload(" Adam ","adam@example.test "," An idea ","0.1.0-alpha");
    expect(body.size()==5&&body["app"]=="dcs2psvr2-shaker"&&body["version"]=="0.1.0-alpha"&&body["message"]=="An idea");
    for(const auto& fields:std::vector<std::array<std::string,3>>{{"","a@b.test","idea"},{std::string(101,'a'),"a@b.test","idea"},{"Adam","bad","idea"},{"Adam","a@@b.test","idea"},{"Adam","a@b.test"," "},{"Adam","a@b.test",std::string(4001,'a')}}){
        bool rejected=false;try{feedbackPayload(fields[0],fields[1],fields[2],"test");}catch(...){rejected=true;}expect(rejected);
    }
    expect(feedbackAccepted(200,"{\"ok\":true}"));
    for(const auto& response:{"{}","{\"ok\":false}","{\"ok\":\"true\"}","{\"ok\":true,\"extra\":1}","bad"})expect(!feedbackAccepted(200,response));
    expect(!feedbackAccepted(200,std::string("{\"ok\":true}")+std::string(1024,' ')));
    expect(!feedbackAccepted(500,"{\"ok\":true}"));
    std::cout<<"Feedback metadata, validation and explicit relay acceptance verified.\n";
}
