
#ifndef UC_IMAGEPROCESSOR_H
#define UC_IMAGEPROCESSOR_H

#include "ImageTypes.h"
#include "ImageIO.h"
#include "ColorSpace.h"
#include "ImageFilters.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

//***************************************************************************
// @brief ImageProcessor::Resize에서 사용할 리샘플링 방식
// @details Nearest는 최근접 이웃 보간, Bilinear는 양선형 보간을 사용한다.
//***************************************************************************
enum class ResizeMethod
{
    Nearest,  // 최근접 이웃 보간(빠르지만 계단현상 발생)
    Bilinear, // 양선형 보간(부드럽지만 상대적으로 느림)
};

//***************************************************************************
// @brief ImageProcessor::Rotate에서 사용할 회전 각도
// @details 시계 방향(Clockwise) 90/180/270도 회전을 지원한다.
//***************************************************************************
enum class RotateAngle
{
    CW90,   // 시계 방향 90도 회전
    CW180,  // 시계 방향 180도 회전
    CW270,  // 시계 방향 270도(반시계 90도) 회전
};

//***************************************************************************
// @brief 사용자가 실제로 다루게 되는 최상위 이미지 처리 헬퍼 클래스
// @details 내부적으로 ImageIO(코덱 디스패치) + ColorSpace + ImageFilters를
//          조합하고, 메서드 체이닝(fluent interface)으로 여러 처리를
//          이어붙일 수 있게 한다.
//***************************************************************************
class ImageProcessor
{
public:
    //***************************************************************************
    // @brief 빈 이미지로 기본 생성
    //***************************************************************************
    ImageProcessor() = default;

    //***************************************************************************
    // @brief 이미 디코드된 ImageBuffer로부터 생성
    // @param image 소유권을 이전받을 원본 이미지
    //***************************************************************************
    explicit ImageProcessor(ImageBuffer image) : image_(std::move(image)) {}

    ImageProcessor& Load(const std::string& path);
    const ImageProcessor& Save(const std::string& path) const;
    const ImageProcessor& Save(const std::string& path, ImageFormat format) const;

    // Load/Save의 메모리 버퍼 버전 (파일을 거치지 않음)
    ImageProcessor& LoadFromMemory(const uint8_t* data, size_t size);
    std::vector<uint8_t> SaveToMemory(ImageFormat format) const;

    //***************************************************************************
    // @brief 내부 ImageBuffer에 대한 쓰기 가능한 참조를 반환
    // @return 내부 ImageBuffer 참조
    //***************************************************************************
    ImageBuffer& Buffer() { return image_; }

    //***************************************************************************
    // @brief 내부 ImageBuffer에 대한 읽기 전용 참조를 반환
    // @return 내부 ImageBuffer 참조
    //***************************************************************************
    const ImageBuffer& Buffer() const { return image_; }

    //***************************************************************************
    // @brief 현재 이미지의 가로 크기를 반환
    // @return 가로 크기(픽셀)
    //***************************************************************************
    uint32_t Width() const { return image_.Width(); }

    //***************************************************************************
    // @brief 현재 이미지의 세로 크기를 반환
    // @return 세로 크기(픽셀)
    //***************************************************************************
    uint32_t Height() const { return image_.Height(); }

    ImageProcessor& Resize(uint32_t newWidth, uint32_t newHeight, ResizeMethod method = ResizeMethod::Bilinear);
    ImageProcessor& Crop(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    ImageProcessor& Rotate(RotateAngle angle);
    ImageProcessor& FlipHorizontal();
    ImageProcessor& FlipVertical();

    ImageProcessor& ToGrayscale();
    ImageProcessor& ToSepia();
    ImageProcessor& Invert();

    ImageProcessor& GaussianBlur(int radius = 2, double sigma = 1.4);
    ImageProcessor& Sharpen();
    ImageProcessor& EdgeDetect();
    ImageProcessor& AdjustBrightnessContrast(int brightness, double contrast);

private:
    void BilinearSample(double srcX, double srcY, uint8_t* out) const;

    ImageBuffer image_; // 현재 처리 중인 이미지 버퍼(RGBA8)
};

#endif // ndef UC_IMAGEPROCESSOR_H