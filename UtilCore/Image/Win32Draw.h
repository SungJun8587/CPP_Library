
//***************************************************************************
// Win32Draw.h : interface for the Win32Draw class.
//
//***************************************************************************

#ifndef UC_WIN32DRAW_H
#define UC_WIN32DRAW_H

#include "ImageTypes.h"
#include <windows.h>

//***************************************************************************
// @brief ImageBuffer(RGBA8)를 Win32 HDC에 StretchDIBits로 blit하는
//        정적 유틸리티 클래스
// @details GDI의 32bpp BI_RGB DIB는 픽셀을 B,G,R,X 바이트 순서로 요구하므로
//          내부적으로 RGBA -> BGRA 변환 버퍼를 만든 뒤 top-down(음수 높이)
//          DIB로 StretchDIBits를 호출한다. CxImage::Draw2(hdc, x, y, w, h)와
//          동일한 사용 방식(지정 위치에 지정 크기로 스트레치 출력)을 제공한다.
//***************************************************************************
class Win32Draw 
{
public:
    static void DrawToDC(HDC hdc, const ImageBuffer& img, int x, int y, int destW, int destH);
    static void DrawToDC(HDC hdc, const ImageBuffer& img, int x, int y);
};

#endif // UC_WIN32DRAW_H
