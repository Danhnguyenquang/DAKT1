#ifndef ALL_FRAMES_H
#define ALL_FRAMES_H

#include <Arduino.h>

// Kích thước của khung hình rỗng (64x64 pixel)
#define FRAME_WIDTH  64
#define FRAME_HEIGHT 64

// Tổng số khung hình (để tối thiểu là 2 để có hiệu ứng chuyển đổi)
#define TOTAL_FRAMES 2

// Tạo mảng dữ liệu rỗng (số 0 tương đương với màu đen)
const uint8_t PROGMEM frame_0[512] = {0};
const uint8_t PROGMEM frame_1[512] = {0};

// Tạo mảng con trỏ chứa các khung hình theo chuẩn của code gốc
const uint8_t* const frames[TOTAL_FRAMES] PROGMEM = {
  frame_0,
  frame_1
};

#endif