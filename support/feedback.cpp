#include "feedback.hpp"
#include "platform.hpp"
#include <winhttp.h>
#include <stdexcept>
namespace shaker {
namespace {
struct HttpHandle {
    HINTERNET value;
    explicit HttpHandle(HINTERNET h):value(h){if(!h)throw std::runtime_error("Could not connect to feedback delivery.");}
    ~HttpHandle(){WinHttpCloseHandle(value);}
    HttpHandle(const HttpHandle&)=delete;
    operator HINTERNET()const{return value;}
};
void require(BOOL result){if(!result)throw std::runtime_error("Could not send feedback. Check your connection and try again.");}
void post(const Json& payload){
    HttpHandle session(WinHttpOpen(L"dcs2psvr2-shaker feedback",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    require(WinHttpSetTimeouts(session,3000,3000,5000,5000));
    const auto url=wide(feedbackEndpoint);URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);
    parts.dwHostNameLength=parts.dwUrlPathLength=DWORD(-1);
    require(WinHttpCrackUrl(url.c_str(),0,0,&parts));
    HttpHandle connection(WinHttpConnect(session,std::wstring(parts.lpszHostName,parts.dwHostNameLength).c_str(),INTERNET_DEFAULT_HTTPS_PORT,0));
    HttpHandle request(WinHttpOpenRequest(connection,L"POST",std::wstring(parts.lpszUrlPath,parts.dwUrlPathLength).c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    require(WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect)));
    DWORD auth=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    require(WinHttpSetOption(request,WINHTTP_OPTION_AUTOLOGON_POLICY,&auth,sizeof(auth)));
    const auto body=payload.dump();
    require(WinHttpSendRequest(request,L"Content-Type: application/json\r\n",DWORD(-1),const_cast<char*>(body.data()),DWORD(body.size()),DWORD(body.size()),0));
    require(WinHttpReceiveResponse(request,nullptr));
    DWORD status=0,size=sizeof(status);
    require(WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX));
    wchar_t contentType[256]{};DWORD typeBytes=sizeof(contentType);
    require(WinHttpQueryHeaders(request,WINHTTP_QUERY_CONTENT_TYPE,WINHTTP_HEADER_NAME_BY_INDEX,contentType,&typeBytes,WINHTTP_NO_HEADER_INDEX));
    auto media=utf8(contentType);media=media.substr(0,media.find(';'));
    std::transform(media.begin(),media.end(),media.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(feedbackTrim(media)!="application/json")throw std::runtime_error("Feedback delivery returned an invalid response.");
    const auto deadline=GetTickCount64()+15000;
    std::string response;char buffer[1024];DWORD received=0;
    do{
        if(GetTickCount64()>deadline)throw std::runtime_error("Feedback delivery timed out. Please try again.");
        require(WinHttpReadData(request,buffer,sizeof(buffer),&received));
        response.append(buffer,received);
        if(response.size()>1024)throw std::runtime_error("Feedback delivery returned an invalid response.");
    }while(received);
    if(!feedbackAccepted(status,response))throw std::runtime_error("Feedback was not accepted. Please try again.");
}
}
FeedbackStatus FeedbackClient::status()const{std::lock_guard lock(mutex_);return status_;}
void FeedbackClient::send(const Json& payload){
    if(status().state==FeedbackState::Sending)return;
    if(worker_.joinable())worker_.join();
    {std::lock_guard lock(mutex_);status_={FeedbackState::Sending,"Sending..."};}
    worker_=std::jthread([this,payload]{
        FeedbackStatus result;
        try{post(payload);result={FeedbackState::Sent,"Feedback sent. Thank you!"};}
        catch(const std::exception&){result={FeedbackState::Failed,supportContent()["feedback"]["failure"].get<std::string>()};}
        std::lock_guard lock(mutex_);status_=result;
    });
}
}
