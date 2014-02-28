/* Arduino WaveRP Library
 * Copyright (C) 2009 by William Greiman
 *
 * This file is part of the Arduino WaveRP Library
 *
 * This Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Arduino WaveRP Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef WaveStructs_h
#define WaveStructs_h
#include <inttypes.h>
uint8_t const WAVE_FORMAT_UNKNOWN = 0;
uint8_t const WAVE_FORMAT_PCM = 1;

typedef struct wave_riff {
  char id[4];     // "RIFF"
  uint32_t size;  // fileSize - 8
  char type[4];   // "WAVE"
} WaveRiff;

typedef struct wave_fmt {
  char id[4];               // "fmt "
  uint32_t size;            // must be 16
  uint16_t formatTag;       // must be WAVE_FORMAT_PCM
  uint16_t channels;        // 1 or 2
  uint32_t sampleRate;      // Samples per second
  uint32_t bytesPerSecond;  // sampleRate*channels*bitsPerSample/8
  uint16_t blockAlign;      // channels*bitsPerSample/8
  uint16_t bitsPerSample;
} WaveFmt;

// use for read, many files have size 18 with extraBytes = 0
typedef struct wav_fmt_extra {
  char id[4];               // "fmt "
  uint32_t size;            // must be 18
  uint16_t formatTag;       // must be WAVE_FORMAT_PCM
  uint16_t channels;        // 1 or 2
  uint32_t sampleRate;
  uint32_t bytesPerSecond;  // sampleRate*channels*bitsPerSample/8
  uint16_t blockAlign;      // channels*bitsPerSample/8
  uint16_t bitsPerSample;
  uint16_t extraBytes;      // must be zero
} WaveFmtExtra;

typedef struct wave_data {
  char id[4];             // "data"
  uint32_t size;
} WaveData;

typedef struct wave_header {
  WaveRiff riff;
  WaveFmt  fmt;
  WaveData data;
} WaveHeader;

typedef struct wave_header_extra {
  WaveRiff     riff;
  WaveFmtExtra fmt;
  WaveData     data;
} WaveHeaderExtra;

#endif  // WaveStructs_h
