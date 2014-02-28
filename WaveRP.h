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
#ifndef WaveRP_h
#define WaveRP_h
#include <SdFat.h>
 /**
  * Use Software volume control if nonzero.  Uses multiply and shift to
  * decrease volume by 1 dB per step. See DAC ISR in WaveHC.cpp.
  * Decreases MAX_CLOCK_RATE to 22050.
  */
#define DVOLUME 1
// Voltage Reference Selections for ADC
/** AREF, Internal Vref turned off */
uint8_t const ADC_REF_AREF = 0;
/** AVCC with external capacitor at AREF pin */
uint8_t const ADC_REF_AVCC = 1 << REFS0;
/** Internal 1.1V Voltage Reference with external capacitor at AREF pin */
uint8_t const ADC_REF_INTERNAL = (1 << REFS0) | (1 << REFS1);
/** Mask to test for valid reference selection */
uint8_t const ADC_REF_MASK = (1 << REFS0) | (1 << REFS1);

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
/** max analog pin number for mega */
uint8_t const ADC_MAX_PIN = 15;
#else  // defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
/** max analog pin number for 328 Arduino */
uint8_t const ADC_MAX_PIN = 7;
#endif  // defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)

// Values for rpState
uint8_t const RP_STATE_STOPPED   = 0;
uint8_t const RP_STATE_PLAYING   = 1;
uint8_t const RP_STATE_RECORDING = 2;

// values for sdStatus
uint8_t const SD_STATUS_BUFFER_BUSY  = 0;
uint8_t const SD_STATUS_BUFFER_READY = 1;
uint8_t const SD_STATUS_END_OF_DATA  = 2;
uint8_t const SD_STATUS_IO_ERROR     = 3;

//------------------------------------------------------------------------------
/**
 * \class WaveRP
 * \brief Record and play Wave files on SD and SDHC cards.
 */
class WaveRP {
 public:
  // static variable shared with ISR
  /** Minimum sample value since last adcClearRange() call */
  volatile static uint8_t adcMin;
  /** Maximum sample value since last adcClearRange() call */
  volatile static uint8_t adcMax;
  /** number of SD busy error in ISR for ADC or DAC for current file */
  volatile static uint8_t busyError;
  /** Number of bits per sample for file being played.  Not set for recorder.*/
  volatile static uint8_t bitsPerSample;
  /** Number of channels, one or two, for file being played.
   *  Not used by recorder. */
  volatile static uint8_t Channels;
  /** recorder or player pause control - has not been tested */
  volatile static uint8_t rpPause;
  /** state of ADC or DAC ISR */
  volatile static uint8_t rpState;
  /** Sample rate for file being played.  Not set recorder. */
  volatile static uint32_t sampleRate;
  /** SD card for this file */
  static Sd2Card *sdCard;
  /** Status of SD ISR routine. */
  volatile static uint8_t sdStatus;
  /** SdFile for current record or play file */
  volatile static SdBaseFile *sdFile;
  /** address of first block of the file */
  static uint32_t sdStartBlock;
  /** address of last block of the file */
  static uint32_t sdEndBlock;
  /** File position for end of play data or max record position. */
  volatile static uint32_t sdEndPosition;
  /** current position for ISR raw read/write of SD */
  volatile static uint32_t sdCurPosition;
  volatile static uint16_t _crush;
  volatile static bool audioThru;
  
  #if DVOLUME  || defined(__DOXYGEN__)
  /** multiplier for volume control */
  volatile static uint8_t volMult;
  /** offset for volume control */
  volatile static uint16_t volOffset;
  void setVolume(uint8_t db);
 
  #endif  // DVOLUME
  //----------------------------------------------------------------------------
  /** Clears min and max sample values. */
  void adcClearRange(void) {
    adcMin = 0XFF;
    adcMax = 0;
  }
  /** \return The range of sample values since the call to adcClearRange. */
  uint8_t adcGetRange(void) {return adcMax < adcMin ? 0 : adcMax - adcMin;}
  /** \return ADC max value */
  uint8_t adcGetMax(void) {return adcMax;}
  /** \return ADC min value */
  uint8_t adcGetMin(void) {return adcMin;}
  /** \return number of SD busy errors in ISRs for the ADC or DAC. */
  uint8_t errors(void) {return busyError;}
  /** \return The pause status. */
  bool isPaused(void) {return rpPause;}
  /** \return true if the player is active. */
  bool isPlaying(void) {return rpState == RP_STATE_PLAYING;}
  /** \return true if the recorder is active. */
  bool isRecording(void) {return rpState == RP_STATE_RECORDING;}
  /** Pause recorder or player. */
  void pause(void) {rpPause = true;}
  uint16_t getData();
  bool play(SdBaseFile* file);
  bool record(SdBaseFile* file, uint16_t rate, uint8_t pin, uint8_t ref);
  /** Resume recorder or player. */
  void resume(void) {rpPause = false;}
  void stop(void);
  bool trim(SdBaseFile* file);
  void adcInit(uint16_t rate, uint8_t pin, uint8_t ref);
  //----------------------------------------------------------------------------
  // untested functions
  void seek(uint32_t pos); 
  uint32_t getCurPosition(void){ return sdCurPosition; };
  void setSampleRate(uint32_t samplerate); 
 
  void setCrush(uint16_t _CRUSH){ _crush=_CRUSH<<2;};
  void setAudioThru(bool _THRU){ audioThru=_THRU;};
};
#endif  // WaveRP_h
