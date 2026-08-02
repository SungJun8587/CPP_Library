// pch.h: 미리 컴파일된 헤더 파일입니다.
// 아래 나열된 파일은 한 번만 컴파일되었으며, 향후 빌드에 대한 빌드 성능을 향상합니다.
// 코드 컴파일 및 여러 코드 검색 기능을 포함하여 IntelliSense 성능에도 영향을 미칩니다.
// 그러나 여기에 나열된 파일은 빌드 간 업데이트되는 경우 모두 다시 컴파일됩니다.
// 여기에 자주 업데이트할 파일을 추가하지 마세요. 그러면 성능이 저하됩니다.

#ifndef PCH_H
#define PCH_H

// 여기에 미리 컴파일하려는 헤더 추가
#define WIN32_LEAN_AND_MEAN		// 자주 사용하지 않는 API의 일부를 제외하여 Win32 헤더 파일의 크기를 줄이기 위해 설정(빌드 시간 단축 목적)
//#define _HAS_STD_BYTE 0			// c++17 옵션을 활성화 시 std::byte 를 비활성 하는 옵션

#include <windows.h>
#include <atlbase.h>
#include <crtdbg.h>
#include <locale.h>
#include <wtypes.h>

#include <string>
#include <vector>

#include <iostream>
using namespace std;

#include <BaseDefine.h>
#include <BaseRedefineDataType.h>
#include <BaseMacro.h>

#include <Util/ConvertCharset.h>
#include <Util/EncodingConvert.h>
#include <Util/FileUtil.h>

#endif // ndef PCH_H