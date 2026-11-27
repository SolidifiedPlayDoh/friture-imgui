#pragma once
// Global constants, matching upstream Friture.

namespace fr {

constexpr int kSampleRate = 48000;       // Hz, device audio is converted to this
constexpr int kFramesPerBuffer = 512;    // device period
constexpr int kNOctave = 9;              // octave bank depth
constexpr double kPi = 3.14159265358979323846;

}  // namespace fr
