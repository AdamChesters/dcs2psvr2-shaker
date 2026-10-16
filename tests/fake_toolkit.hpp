#pragma once
#include <windows.h>
struct FakeMotorState {volatile LONG motor=0,calls=0,active=1,result=0;};
inline constexpr wchar_t fakeMapEnv[]=L"DCS2PSVR2_SHAKER_TEST_MAPPING";
