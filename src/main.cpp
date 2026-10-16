// Win32 + Direct3D 11 host for Dear ImGui (MIT). See THIRD_PARTY_NOTICES.md.
#include "app.hpp"
#include "haptics.hpp"
#include "integration.hpp"
#include "resource.h"
#include "logo.hpp"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <shellapi.h>
#include <memory>
#include <vector>
#include <algorithm>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace shaker {
namespace {
ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;IDXGISwapChain* swapchain=nullptr;ID3D11RenderTargetView* target=nullptr;
ID3D11ShaderResourceView* logoTexture=nullptr;
std::vector<std::pair<std::string,ID3D11ShaderResourceView*>> socialTextures;
App* activeApp=nullptr;bool visible=false;bool hotkeyRegistered=false;
constexpr UINT trayMessage=WM_APP+1,showMessage=WM_APP+2;
constexpr wchar_t windowClass[]=L"Dcs2Psvr2ShakerWindow";
void cleanTarget(){if(target){target->Release();target=nullptr;}}
void makeTarget(){ID3D11Texture2D* buffer=nullptr;if(SUCCEEDED(swapchain->GetBuffer(0,IID_PPV_ARGS(&buffer)))){device->CreateRenderTargetView(buffer,nullptr,&target);buffer->Release();}}
void cleanDevice(){if(logoTexture){logoTexture->Release();logoTexture=nullptr;}for(auto& [icon,view]:socialTextures)view->Release();socialTextures.clear();cleanTarget();if(swapchain){swapchain->Release();swapchain=nullptr;}if(context){context->Release();context=nullptr;}if(device){device->Release();device=nullptr;}}
bool makeDevice(HWND window){
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=window;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL level;D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};
    HRESULT result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,2,D3D11_SDK_VERSION,&desc,&swapchain,&device,&level,&context);
    if(FAILED(result))result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,levels,2,D3D11_SDK_VERSION,&desc,&swapchain,&device,&level,&context);
    if(FAILED(result))return false;makeTarget();logoTexture=createLogoTexture(device);socialTextures=createSocialTextures(device);return target!=nullptr;
}
void show(HWND window){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);visible=true;}
LRESULT WINAPI windowProc(HWND window,UINT message,WPARAM w,LPARAM l){
    if(ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(window,message,w,l))return true;
    switch(message){
    case WM_SIZE:
        if(device && w!=SIZE_MINIMIZED){cleanTarget();swapchain->ResizeBuffers(0,LOWORD(l),HIWORD(l),DXGI_FORMAT_UNKNOWN,0);makeTarget();}return 0;
    case WM_GETMINMAXINFO:{auto p=reinterpret_cast<MINMAXINFO*>(l);p->ptMinTrackSize={1020,720};return 0;}
    case WM_SYSCOMMAND:if((w&0xfff0)==SC_KEYMENU)return 0;break;
    case WM_CLOSE:ShowWindow(window,SW_HIDE);visible=false;return 0;
    case WM_HOTKEY:if(activeApp)activeApp->emergencyStop();return 0;
    case WM_QUERYENDSESSION:if(activeApp)activeApp->emergencyStop();return TRUE;
    case WM_ENDSESSION:if(w)PostQuitMessage(0);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    case showMessage:show(window);return 0;
    case trayMessage:
        if(LOWORD(l)==WM_LBUTTONUP||LOWORD(l)==WM_LBUTTONDBLCLK)show(window);
        if(LOWORD(l)==WM_RBUTTONUP){POINT pos;GetCursorPos(&pos);auto menu=CreatePopupMenu();
            AppendMenuW(menu,MF_STRING,1,L"Open dcs2psvr2-shaker");AppendMenuW(menu,MF_STRING,2,L"Stop and mute output");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,3,L"Exit");
            SetForegroundWindow(window);int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,pos.x,pos.y,0,window,nullptr);DestroyMenu(menu);
            if(result==1)show(window);else if(result==2&&activeApp)activeApp->emergencyStop();else if(result==3)PostQuitMessage(0);
        }return 0;
    }return DefWindowProcW(window,message,w,l);
}
void style(){
    ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();
    s.WindowPadding={28,22};s.FramePadding={12,7};s.ItemSpacing={12,10};s.FrameRounding=6;s.GrabRounding=5;
    s.WindowRounding=0;s.ChildRounding=10;s.ScrollbarRounding=8;s.ChildBorderSize=1;s.TabRounding=6;
    auto& c=s.Colors;c[ImGuiCol_WindowBg]={.025f,.029f,.034f,1};c[ImGuiCol_ChildBg]={0,0,0,0};c[ImGuiCol_Text]={.92f,.93f,.94f,1};
    c[ImGuiCol_TextDisabled]={.55f,.59f,.62f,1};c[ImGuiCol_Border]={.15f,.20f,.22f,1};c[ImGuiCol_Separator]=c[ImGuiCol_Border];
    c[ImGuiCol_FrameBg]={.08f,.10f,.12f,1};c[ImGuiCol_FrameBgHovered]={.12f,.19f,.22f,1};c[ImGuiCol_FrameBgActive]={.13f,.25f,.29f,1};
    c[ImGuiCol_Button]={.09f,.15f,.18f,1};c[ImGuiCol_ButtonHovered]={.14f,.28f,.33f,1};c[ImGuiCol_ButtonActive]={.18f,.36f,.42f,1};
    c[ImGuiCol_Header]={.10f,.19f,.23f,1};c[ImGuiCol_HeaderHovered]={.13f,.25f,.29f,1};c[ImGuiCol_HeaderActive]={.15f,.30f,.35f,1};
    c[ImGuiCol_CheckMark]={.35f,.73f,.84f,1};c[ImGuiCol_SliderGrab]={.29f,.65f,.75f,1};c[ImGuiCol_SliderGrabActive]={.44f,.82f,.91f,1};
    c[ImGuiCol_PlotHistogram]=c[ImGuiCol_CheckMark];c[ImGuiCol_PlotLines]=c[ImGuiCol_CheckMark];
}
}
int runWindow(HINSTANCE instance){
    auto singleton=CreateMutexW(nullptr,FALSE,L"Local\\dcs2psvr2-shaker_App_v1");
    if(singleton && GetLastError()==ERROR_ALREADY_EXISTS){auto other=FindWindowW(windowClass,nullptr);if(other)PostMessageW(other,showMessage,0,0);CloseHandle(singleton);return 0;}
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc{sizeof(wc),CS_CLASSDC,windowProc,0,0,instance,nullptr,LoadCursor(nullptr,IDC_ARROW),nullptr,nullptr,windowClass,nullptr};
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(IDI_APP_ICON),IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
    if(!wc.hIcon)wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    if(!wc.hIconSm)wc.hIconSm=wc.hIcon;
    RegisterClassExW(&wc);
    auto window=CreateWindowW(windowClass,L"dcs2psvr2-shaker by Adam Chesters",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1200,840,nullptr,nullptr,instance,nullptr);
    if(!window){if(singleton)CloseHandle(singleton);return 1;}
    NOTIFYICONDATAW tray{sizeof(tray)};tray.hWnd=window;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=trayMessage;tray.hIcon=wc.hIconSm;
    wcscpy_s(tray.szTip,L"dcs2psvr2-shaker — DCS headset haptics");Shell_NotifyIconW(NIM_ADD,&tray);
    hotkeyRegistered=RegisterHotKey(window,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_SPACE)!=0;
    int result=0;
    try{
        App app;activeApp=&app;show(window);
        bool done=false,renderer=false;
        while(!done){
            MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);if(msg.message==WM_QUIT)done=true;}
            if(done)break;
            if(!visible || IsIconic(window)){
                if(renderer){ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();cleanDevice();renderer=false;}
                MsgWaitForMultipleObjects(0,nullptr,FALSE,250,QS_ALLINPUT);continue;
            }
            if(!renderer){
                if(!makeDevice(window))throw std::runtime_error("Could not initialize the display");
                IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
                io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf",18.f);
                if(fs::exists(L"C:/Windows/Fonts/georgiab.ttf"))io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/georgiab.ttf",34.f);
                style();ImGui_ImplWin32_Init(window);ImGui_ImplDX11_Init(device,context);renderer=true;
            }
            ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();app.render(logoTexture,[](const std::string& icon)->void*{for(const auto& [name,view]:socialTextures)if(name==icon)return view;return nullptr;});
            if(!hotkeyRegistered){ImGui::SetNextWindowPos({400,16},ImGuiCond_Once);ImGui::Begin("Shortcut unavailable",nullptr,ImGuiWindowFlags_AlwaysAutoResize);ImGui::TextUnformatted("Ctrl+Alt+Space is in use. The STOP button and tray menu remain available.");ImGui::End();}
            ImGui::Render();const float clear[]={.025f,.029f,.034f,1};context->OMSetRenderTargets(1,&target,nullptr);context->ClearRenderTargetView(target,clear);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            HRESULT hr=swapchain->Present(1,0);if(hr==DXGI_STATUS_OCCLUDED)Sleep(50);
            else if(FAILED(hr)){app.emergencyStop();ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();cleanDevice();renderer=false;Sleep(100);}
        }
        activeApp=nullptr;if(renderer){ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();}cleanDevice();
    }catch(const std::exception& e){activeApp=nullptr;cleanDevice();MessageBoxW(window,wide(e.what()).c_str(),L"dcs2psvr2-shaker",MB_ICONERROR);result=1;}
    if(hotkeyRegistered)UnregisterHotKey(window,1);Shell_NotifyIconW(NIM_DELETE,&tray);DestroyWindow(window);UnregisterClassW(windowClass,instance);if(singleton)CloseHandle(singleton);return result;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    int argc=0;wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc);std::vector<std::wstring> args;
    for(int i=1;i<argc;++i)args.emplace_back(argv[i]);LocalFree(argv);
    try{
        if(args.size()==4 && args[0]==L"--haptics-host")return shaker::runHapticsHost(args[1],args[2],std::stoul(args[3]));
        if(args.size()==3 && args[0]==L"--probe")return shaker::runProbe(std::clamp(std::stoi(args[1]),0,25),args[2]);
        if(args.size()==2 && args[0]==L"--install-export"){shaker::installIntegration(args[1],shaker::appDirectory());return 0;}
        if(args.size()==2 && args[0]==L"--remove-export"){shaker::removeIntegration(args[1]);return 0;}
        if(args.size()==1 && args[0]==L"--uninstall-integration"){
            auto p=shaker::dataDirectory()/L"settings.json";
            if(shaker::fs::exists(p))for(const auto& profile:shaker::Settings::fromJson(shaker::Json::parse(shaker::readText(p))).dcsProfiles)shaker::removeIntegration(shaker::wide(profile));
            for(const auto& profile:shaker::dcsProfiles())if(shaker::integrationInstalled(profile))shaker::removeIntegration(profile);
            return 0;
        }
        return shaker::runWindow(instance);
    }catch(const std::exception& e){MessageBoxW(nullptr,shaker::wide(e.what()).c_str(),L"dcs2psvr2-shaker",MB_ICONERROR);return 2;}
}
