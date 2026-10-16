#include "updates.hpp"
#include "version.hpp"
#include <iostream>
#include <stdexcept>
using namespace shaker;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("Check failed: ")+#x+" at line "+std::to_string(__LINE__));}while(0)
template<class F> void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}CHECK(rejected);}
Json fixture(const std::string& tag){
    const auto version=tag.starts_with('v')?tag.substr(1):tag;
    const auto name="dcs2psvr2-shaker-"+version+"-Setup.exe";
    return {{"tag_name",tag},{"draft",false},{"prerelease",true},{"assets",Json::array({
        {{"name",name},{"state","uploaded"},{"size",3},{"digest","sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
         {"browser_download_url",std::string(releasesUrl)+"/download/"+tag+"/"+name}}
    })}};
}
int main(int argc,char** argv){
    try{
        if(argc==3&&std::string(argv[1])=="--live"){
            const auto release=checkReleases();
            std::cout<<"Installed "<<appVersion<<"; GitHub "<<release.version<<"; comparison "<<compareVersions(release.version,appVersion)<<'\n';
            const auto path=downloadInstaller(release,wide(argv[2]));verifyInstaller(path,release);
            std::cout<<"PASS: public release lookup, HTTPS asset download and SHA-256 verified; installer was NOT launched\n";return 0;
        }
        CHECK(shouldShowChangelog("0.3.0-alpha.2","0.3.0-alpha.3",true));
        CHECK(!shouldShowChangelog("0.3.0-alpha.3","0.3.0-alpha.3",true));
        CHECK(!shouldShowChangelog("0.3.0-alpha.4","0.3.0-alpha.3",true));
        CHECK(shouldShowChangelog("","0.3.0-alpha.3",true));
        CHECK(!shouldShowChangelog("","0.3.0-alpha.3",false));
        CHECK(shouldShowChangelog("broken","0.3.0-alpha.3",true));
        CHECK(latestChangelog("# Changelog\n\n## Alpha 3\n- New\n\n## Alpha 2\n- Old")=="- New\n");
        CHECK(latestChangelog("").find("unavailable")!=std::string::npos);
        CHECK(compareVersions("v0.2.0-alpha","0.1.3")>0);
        CHECK(compareVersions("0.2.0-alpha","0.2.0-alpha")==0);
        CHECK(compareVersions("0.2.0-alpha.2","0.2.0-alpha.10")<0);
        CHECK(compareVersions("0.2.0-alpha","0.2.0-beta")<0);
        CHECK(compareVersions("0.2.0-rc.1","0.2.0")<0);
        CHECK(compareVersions("0.2.0+build.2","0.2.0+other.1")==0);
        CHECK(compareVersions("0.9.0","0.10.0")<0);
        CHECK(compareVersions("0.2.0-alpha.1","0.2.0-alpha.beta")<0);
        rejects([]{compareVersions("../../payload","0.2.0");});
        rejects([]{compareVersions("0.2.0-alpha.01","0.2.0");});
        rejects([]{compareVersions("0.02.0","0.2.0");});
        rejects([]{compareVersions("999999999999999999999999999999.2.0","0.2.0");});
        auto draft=fixture("v1.0.0");draft["draft"]=true;
        auto list=Json::array({fixture("v0.1.1"),draft,fixture("v0.2.0-alpha"),fixture("unrelated"),fixture("v0.1.0")});
        auto release=newestRelease(list);CHECK(release);CHECK(release->version=="0.2.0-alpha");CHECK(release->installable());
        list.push_back(fixture("v0.2.0"));CHECK(newestRelease(list)->version=="0.2.0");
        CHECK(!newestRelease(Json::array()));rejects([]{newestRelease(Json::object());});
        CHECK(allowedUpdateUrl(releasesApi));CHECK(allowedUpdateUrl(release->installerUrl));
        CHECK(allowedUpdateUrl("https://release-assets.githubusercontent.com/file?sig=abc",true));
        CHECK(!allowedUpdateUrl("https://release-assets.githubusercontent.com/file",false));
        CHECK(!allowedUpdateUrl("http://release-assets.githubusercontent.com/file",true));
        CHECK(!allowedUpdateUrl("https://release-assets.githubusercontent.com.evil.test/file",true));
        CHECK(!allowedUpdateUrl("https://release-assets.githubusercontent.com@evil.test/file",true));
        CHECK(!allowedUpdateUrl("https://github.com/another/repo/releases/download/a/file.exe"));
        auto bad=*release;bad.installerUrl="file:///C:/payload.exe";CHECK(!bad.installable());
        bad=*release;bad.installerName="../payload.exe";CHECK(!bad.installable());
        bad=*release;bad.digest="";CHECK(!bad.installable());
        bad=*release;bad.size=0;CHECK(!bad.installable());
        bad=*release;bad.size=101*1024*1024;CHECK(!bad.installable());
        auto noDigest=fixture("v0.3.0-alpha");noDigest["assets"][0]["digest"]=nullptr;
        list.push_back(noDigest);CHECK(newestRelease(list)->version=="0.3.0-alpha");CHECK(!newestRelease(list)->installable());
        // Keep advertising the newest release even if only manual download is available.
        auto noAsset=fixture("v0.4.0-alpha");noAsset["assets"]=Json::array();list.push_back(noAsset);
        CHECK(newestRelease(list)->version=="0.4.0-alpha");CHECK(!newestRelease(list)->installable());
        auto malformedAsset=fixture("v0.5.0-alpha");malformedAsset["assets"][0].erase("size");list.push_back(malformedAsset);
        CHECK(newestRelease(list)->version=="0.5.0-alpha");CHECK(!newestRelease(list)->installable());
        const auto folder=fs::temp_directory_path()/(L"dcs2psvr2-shaker-update-tests-"+std::to_wstring(GetCurrentProcessId()));
        const auto path=folder/L"fixture.bin";
        writeTextAtomic(path,"abc");verifyInstaller(path,*release);
        writeTextAtomic(path,"abd");rejects([&]{verifyInstaller(path,*release);});
        writeTextAtomic(path,"ab");rejects([&]{verifyInstaller(path,*release);});
        writeTextAtomic(path,"abcd");rejects([&]{verifyInstaller(path,*release);});
        std::stop_source stopped;stopped.request_stop();
        rejects([&]{downloadInstaller(*release,folder,stopped.get_token());});
        CHECK(!fs::exists(folder/L"installer.part"));CHECK(!fs::exists(folder/wide(release->installerName)));
        fs::remove(path);fs::remove(folder);
        std::cout<<"PASS: SemVer/Alpha ordering, release selection, allowed downloads, missing assets/checksums, corrupt/truncated installers and cancellation\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
