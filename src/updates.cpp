#include "updates.hpp"
#include "version.hpp"
#include <winhttp.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <regex>
#include <stdexcept>

namespace shaker {
namespace {
constexpr uint64_t maxInstallerBytes=100*1024*1024;
struct Version {std::array<uint64_t,3> numbers{};std::vector<std::string> prerelease;};
bool numeric(const std::string& s){return !s.empty()&&s.find_first_not_of("0123456789")==std::string::npos;}
Version parseVersion(const std::string& text){
    static const std::regex pattern(R"(^v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$)");
    std::smatch m;
    if(text.size()>128||!std::regex_match(text,m,pattern))throw std::runtime_error("Invalid release version");
    Version v;
    for(size_t i=0;i<3;++i)v.numbers[i]=std::stoull(m[i+1].str());
    const auto pre=m[4].str();size_t start=0;
    while(start<pre.size()){
        const auto end=pre.find('.',start);auto part=pre.substr(start,end==std::string::npos?end:end-start);
        if(numeric(part)&&part.size()>1&&part[0]=='0')throw std::runtime_error("Invalid prerelease version");
        v.prerelease.push_back(part);if(end==std::string::npos)break;start=end+1;
    }
    return v;
}
bool validDigest(const std::string& digest){
    return digest.size()==64 && digest.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos;
}
std::string lower(std::string s){for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
struct Internet {
    HINTERNET h;
    explicit Internet(HINTERNET value):h(value){if(!h)throw std::runtime_error("Windows HTTP error "+std::to_string(GetLastError()));}
    ~Internet(){WinHttpCloseHandle(h);}
    Internet(const Internet&)=delete;
    Internet& operator=(const Internet&)=delete;
    operator HINTERNET()const{return h;}
};
void httpOk(BOOL result){if(!result)throw std::runtime_error("Windows HTTP error "+std::to_string(GetLastError()));}
void checkpoint(std::stop_token stop,uint64_t deadline){
    if(stop.stop_requested())throw std::runtime_error("Update operation cancelled");
    if(GetTickCount64()>deadline)throw std::runtime_error("GitHub request timed out. Try again later.");
}
// Redirects are followed explicitly so HTTPS and the GitHub asset-host allowlist
// apply at every hop. No tokens, profile data, or simulator telemetry are sent.
void get(const std::string& initial,uint64_t limit,std::stop_token stop,
         const std::function<void(const char*,size_t)>& consume){
    const auto deadline=GetTickCount64()+(initial==releasesApi?15000:180000);
    Internet session(WinHttpOpen(wide(std::string("dcs2psvr2-shaker/")+appVersion).c_str(),WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    httpOk(WinHttpSetTimeouts(session,3000,3000,5000,5000));
    std::string url=initial;
    for(int redirect=0;redirect<=5;++redirect){
        checkpoint(stop,deadline);
        if(!allowedUpdateUrl(url,redirect>0))throw std::runtime_error("Unexpected release download address");
        auto wurl=wide(url);URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);
        parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=DWORD(-1);
        httpOk(WinHttpCrackUrl(wurl.c_str(),0,0,&parts));
        const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
        std::wstring path(parts.lpszUrlPath,parts.dwUrlPathLength);
        if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
        Internet connection(WinHttpConnect(session,host.c_str(),INTERNET_DEFAULT_HTTPS_PORT,0));
        Internet request(WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
        DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        httpOk(WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy)));
        DWORD auth=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
        httpOk(WinHttpSetOption(request,WINHTTP_OPTION_AUTOLOGON_POLICY,&auth,sizeof(auth)));
        DWORD disabled=WINHTTP_DISABLE_COOKIES;
        httpOk(WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled)));
        const wchar_t* headers=initial==releasesApi?L"Accept: application/vnd.github+json\r\n":L"Accept: application/octet-stream\r\n";
        httpOk(WinHttpSendRequest(request,headers,DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0));
        checkpoint(stop,deadline);httpOk(WinHttpReceiveResponse(request,nullptr));
        DWORD status=0,size=sizeof(status);
        httpOk(WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX));
        if(status==301||status==302||status==303||status==307||status==308){
            // The releases API is fixed; only installer assets may redirect.
            if(initial==releasesApi)throw std::runtime_error("Unexpected GitHub API redirect");
            std::array<wchar_t,8192> location{};size=DWORD(location.size()*sizeof(wchar_t));
            httpOk(WinHttpQueryHeaders(request,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location.data(),&size,WINHTTP_NO_HEADER_INDEX));
            url=utf8(location.data());continue;
        }
        if(status!=200){
            if(status==403||status==429)throw std::runtime_error("GitHub access is unavailable or rate limited. Try again later.");
            throw std::runtime_error("GitHub returned HTTP "+std::to_string(status));
        }
        uint64_t received=0;std::array<char,65536> buffer{};
        while(true){
            checkpoint(stop,deadline);DWORD bytes=0;
            httpOk(WinHttpReadData(request,buffer.data(),DWORD(buffer.size()),&bytes));
            if(!bytes)break;received+=bytes;
            if(received>limit)throw std::runtime_error("Release download exceeded its expected size");
            consume(buffer.data(),bytes);
        }
        checkpoint(stop,deadline);return;
    }
    throw std::runtime_error("Too many release download redirects");
}
}

int compareVersions(const std::string& left,const std::string& right){
    const auto a=parseVersion(left),b=parseVersion(right);
    if(a.numbers!=b.numbers)return a.numbers>b.numbers?1:-1;
    if(a.prerelease.empty()!=b.prerelease.empty())return a.prerelease.empty()?1:-1;
    for(size_t i=0;i<std::min(a.prerelease.size(),b.prerelease.size());++i){
        const auto& x=a.prerelease[i];const auto& y=b.prerelease[i];if(x==y)continue;
        const bool xn=numeric(x),yn=numeric(y);
        if(xn!=yn)return xn?-1:1;
        if(xn&&x.size()!=y.size())return x.size()>y.size()?1:-1;
        return x>y?1:-1;
    }
    return a.prerelease.size()==b.prerelease.size()?0:a.prerelease.size()>b.prerelease.size()?1:-1;
}
bool shouldShowChangelog(const std::string& lastShown,const std::string& current,bool existingSettings){
    if(lastShown.empty())return existingSettings; // Migrate installations predating this marker.
    try{return compareVersions(current,lastShown)>0;}catch(...){return existingSettings;}
}
std::string latestChangelog(const std::string& markdown){
    const auto heading=markdown.find("## ");
    if(heading==std::string::npos)return "Release notes are unavailable. Open Releases for details.";
    const auto body=markdown.find('\n',heading);
    if(body==std::string::npos)return "Release notes are unavailable. Open Releases for details.";
    const auto next=markdown.find("\n## ",body);
    return markdown.substr(body+1,next==std::string::npos?next:next-body-1);
}
bool allowedUpdateUrl(const std::string& url,bool assetRedirect){
    if(url==releasesApi)return !assetRedirect;
    if(url.size()>8192||url.find_first_of("\r\n\t \\#")!=std::string::npos)return false;
    // Initial asset names/tags are checked more strictly against their release.
    if(url.starts_with(std::string(releasesUrl)+"/download/"))return true;
    return assetRedirect&&(url.starts_with("https://release-assets.githubusercontent.com/")||
                           url.starts_with("https://objects.githubusercontent.com/"));
}
bool Release::installable()const{
    if(size==0||size>maxInstallerBytes||!validDigest(digest))return false;
    try{parseVersion(tag);}catch(...){return false;}
    const auto fileVersion=tag.starts_with('v')?tag.substr(1):tag;
    // '+' needs URL encoding; such releases remain available through the web page.
    if(fileVersion.find('+')!=std::string::npos)return false;
    return installerName=="dcs2psvr2-shaker-"+fileVersion+"-Setup.exe" &&
        installerUrl==std::string(releasesUrl)+"/download/"+tag+"/"+installerName;
}
std::optional<Release> newestRelease(const Json& list){
    if(!list.is_array())throw std::runtime_error("GitHub did not return a releases list");
    std::optional<Release> newest;
    for(const auto& item:list){
        try{
            if(item.value("draft",false))continue;
            const auto tag=item.at("tag_name").get<std::string>();parseVersion(tag);
            if(newest&&compareVersions(tag,newest->tag)<=0)continue;
            Release r;r.tag=tag;r.version=tag.starts_with('v')?tag.substr(1):tag;
            r.page=std::string(releasesUrl)+"/tag/"+tag;
            const auto expected="dcs2psvr2-shaker-"+r.version+"-Setup.exe";
            if(item.contains("assets")&&item["assets"].is_array())for(const auto& asset:item["assets"]){
                try{
                if(asset.value("name","")!=expected||asset.value("state","")!="uploaded")continue;
                r.installerName=expected;r.installerUrl=asset.value("browser_download_url","");
                const auto& size=asset.at("size");
                if(size.is_number_unsigned())r.size=size.get<uint64_t>();
                else if(size.is_number_integer()&&size.get<int64_t>()>0)r.size=uint64_t(size.get<int64_t>());
                if(asset.contains("digest")&&asset["digest"].is_string()){
                    const auto digest=asset["digest"].get<std::string>();if(digest.starts_with("sha256:"))r.digest=lower(digest.substr(7));
                }
                }catch(const std::exception&){/* Still advertise this version for manual download. */}
            }
            newest=r;
        }catch(const std::exception&){/* Ignore unrelated or malformed releases. */}
    }
    return newest;
}
Release checkReleases(std::stop_token stop){
    std::string body;get(releasesApi,4*1024*1024,stop,[&](const char* data,size_t bytes){body.append(data,bytes);});
    const auto release=newestRelease(Json::parse(body));
    if(!release)throw std::runtime_error("No published version could be checked on GitHub");
    return *release;
}
void verifyInstaller(const fs::path& path,const Release& release){
    if(!release.installable())throw std::runtime_error("No verified Windows installer is available for this release. Open Releases instead.");
    if(fs::file_size(path)!=release.size)throw std::runtime_error("The installer download is incomplete. Please retry.");
    if(lower(sha256(path))!=lower(release.digest))throw std::runtime_error("The installer checksum does not match GitHub. Please retry.");
}
fs::path downloadInstaller(const Release& release,const fs::path& directory,std::stop_token stop,DownloadProgress progress){
    if(!release.installable())throw std::runtime_error("No verified Windows installer is available for this release. Open Releases instead.");
    fs::create_directories(directory);
    const auto partial=directory/L"installer.part",destination=directory/wide(release.installerName);
    try{
        std::ofstream file(partial,std::ios::binary|std::ios::trunc);if(!file)throw std::runtime_error("Cannot save the installer download");
        uint64_t received=0;
        get(release.installerUrl,release.size,stop,[&](const char* data,size_t bytes){
            file.write(data,std::streamsize(bytes));if(!file)throw std::runtime_error("Cannot write the installer download");
            received+=bytes;if(progress)progress(received);
        });
        file.close();if(!file)throw std::runtime_error("Cannot finish the installer download");
        verifyInstaller(partial,release);
        if(stop.stop_requested())throw std::runtime_error("Update operation cancelled");
        if(!MoveFileExW(partial.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot prepare the installer");
        return destination;
    }catch(...){std::error_code ec;fs::remove(partial,ec);throw;}
}
bool launchInstaller(const fs::path& path,const fs::path& destination){
    const auto args=L"/DIR=\""+destination.wstring()+L"\"";
    SHELLEXECUTEINFOW info{sizeof(info)};info.fMask=SEE_MASK_NOCLOSEPROCESS;info.lpVerb=L"open";
    info.lpFile=path.c_str();info.lpParameters=args.c_str();info.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&info))return false;if(info.hProcess)CloseHandle(info.hProcess);return true;
}
UpdateClient::UpdateClient(){check();}
UpdateClient::~UpdateClient(){cancel();if(worker_.joinable())worker_.join();}
UpdateStatus UpdateClient::status()const{std::lock_guard lock(mutex_);return status_;}
void UpdateClient::fail(const std::string& message){std::lock_guard lock(mutex_);status_.state=UpdateState::Failed;status_.message=message;}
void UpdateClient::cancel(){worker_.request_stop();}
void UpdateClient::check(){
    {std::lock_guard lock(mutex_);
        if(worker_.joinable()&&(status_.state==UpdateState::Checking||status_.state==UpdateState::Downloading))return;
        status_=UpdateStatus{};
    }
    worker_=std::jthread([this](std::stop_token stop){try{
        auto r=checkReleases(stop);const auto state=compareVersions(r.version,appVersion)>0?UpdateState::Available:UpdateState::Current;
        std::lock_guard lock(mutex_);status_.state=state;status_.release=std::move(r);
    }catch(const std::exception& e){fail(e.what());}});
}
void UpdateClient::download(){
    Release release;
    {std::lock_guard lock(mutex_);
        if(status_.state!=UpdateState::Available&&status_.state!=UpdateState::Failed)return;
        if(!status_.release||!status_.release->installable()||compareVersions(status_.release->version,appVersion)<=0)return;
        release=*status_.release;status_.state=UpdateState::Downloading;status_.downloaded=0;status_.message.clear();
    }
    worker_=std::jthread([this,release](std::stop_token stop){try{
        const auto folder=dataDirectory()/L"updates"/(std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
        auto path=downloadInstaller(release,folder,stop,[this](uint64_t bytes){std::lock_guard lock(mutex_);status_.downloaded=bytes;});
        std::lock_guard lock(mutex_);status_.installer=std::move(path);status_.state=UpdateState::Ready;
    }catch(const std::exception& e){fail(e.what());}});
}
}
