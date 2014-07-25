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
#include <avr/interrupt.h>
#if ARDUINO < 100
#include <WProgram.h>
#else  // ARDUINO
#include <Arduino.h>
#endif  // ARDUINO
#include <WaveRP.h>
#include <WaveStructs.h>
#include <SdFatUtil.h>
#include <mcpDac.h>
//#include <mg2HW.h>



//extern mg2HW hw;
// ISR static variables

uint16_t const BUF_LENGTH = 512;  // must be 512, the SD block size

// This is one of two 512 byte buffers.  The other is the SD cache.
// No file access is allowed while recording or playing a file.
// WaveRP::trim() uses this as the SD block buffer
static uint8_t buf[BUF_LENGTH];
// ADC/DAC buffer
static uint8_t *rpBuffer;
static uint16_t rpIndex;
static uint16_t rpByteCount;
// SDbuffer
static uint8_t *sdBuffer;
static uint16_t sdByteCount;
//------------------------------------------------------------------------------
// static ISR variables shared with WaveRP class
volatile uint8_t WaveRP::adcMax;
volatile uint8_t WaveRP::adcMin;
volatile uint8_t WaveRP::bitsPerSample;
volatile uint8_t WaveRP::busyError;
volatile uint8_t WaveRP::Channels;
volatile uint8_t WaveRP::rpPause = false;
volatile uint8_t WaveRP::rpState = RP_STATE_STOPPED;
volatile uint32_t WaveRP::sampleRate;
Sd2Card *WaveRP::sdCard;
volatile SdBaseFile *WaveRP::sdFile;
volatile uint8_t WaveRP::sdStatus = SD_STATUS_END_OF_DATA;
uint32_t WaveRP::sdStartBlock;
uint32_t WaveRP::sdEndBlock;
volatile uint32_t WaveRP::sdEndPosition;
volatile uint32_t WaveRP::sdCurPosition;
volatile uint16_t WaveRP::_crush;
volatile bool WaveRP::audioThru;
#if DVOLUME
volatile uint8_t  WaveRP::volMult = 0;
volatile uint16_t WaveRP::volOffset = 0;
#endif  // DVOLUME
//------------------------------------------------------------------------------
// stop record or play - to be called with interrupts disabled
static void isrStop(void) {
  ADCSRA &= ~(1 << ADIE);  // disable ADC interrupt
  TIMSK1 &= ~_BV(OCIE1B);  // disable DAC timer interrupt
 // mcpDacSend(0);
  WaveRP::rpState = RP_STATE_STOPPED;
}
//------------------------------------------------------------------------------
// timer interrupt for DAC

/*
uint16_t WaveRP::getData(){
	return data;
}
*/
/*
uint16_t WaveRP::getNextSampe(){
//rpBuffer[];
}
*/
/*
bool fade;
int fadeStep;
uint16_t fadeFrom;
unsigned char fadeCount;
#define FADE_LENGTH 32
*/
uint8_t sample;

uint8_t count=0;
uint8_t divider=128;
//boolean divide=false;

ISR(TIMER1_COMPB_vect) {

  if (WaveRP::rpState != RP_STATE_PLAYING) {
    TIMSK1 &= ~_BV(OCIE1B);  // disable DAC timer interrupt
   // mcpDacSend(0); //novinka
    return;
  }
  // pause has not been tested
  if (WaveRP::rpPause) return;
  if (rpIndex >= rpByteCount) {
    if (WaveRP::sdStatus == SD_STATUS_BUFFER_READY) {
      // swap buffers
      uint8_t *tmp = rpBuffer;
      rpBuffer = sdBuffer;
      sdBuffer = tmp;
      rpIndex = 0;
      rpByteCount = sdByteCount;
      WaveRP::sdStatus = SD_STATUS_BUFFER_BUSY;
      // cause interrupt to start SD read
      TIMSK1 |= _BV(OCIE1A);
    } else if (WaveRP::sdStatus == SD_STATUS_BUFFER_BUSY) {
      if (WaveRP::busyError != 0XFF) WaveRP::busyError++;
      return;
    } else if (WaveRP::sdStatus == SD_STATUS_END_OF_DATA){
   		 WaveRP::rpPause = true;
   		 
   		return;
    } else {
    //	WaveRP::rpPause = true;
      isrStop();
      return;
    }
  }
  uint16_t data;
  
  count++;
  if(count>=divider) count=0;//, hw.updateDisplay();//,divide=true;
  //else divide=false;
  
  if (WaveRP::bitsPerSample == 8) {
    // 8-bit is unsigned
    data = rpBuffer[rpIndex] << 4;
    
   // if(divide) 
    rpIndex++;
  } else {
    // 16-bit is signed - keep high 12 bits for DAC
    data = ((rpBuffer[rpIndex + 1] ^ 0X80) << 4) | (rpBuffer[rpIndex] >> 4);
  //if(divide) 
  rpIndex += 2;
  }
  data=data|WaveRP::_crush;
#if DVOLUME // fast lennart
  if (WaveRP::volMult) {
    uint32_t tmp = WaveRP::volMult * (uint32_t)data;
    data = tmp >> 8;
    data += WaveRP::volOffset;
  }
#endif  // DVOLUME
  
//data=(data*volume)>>16
//data=data+(sample<<4);
mcpDacSend(data);
/*
  if(!fade) fadeFrom=data,mcpDacSend(data);
  else{
 	 if(++fadeCount>FADE_LENGTH){
 		 fadeCount=0, fade=false;
 	 }
 	 else{
 	 	//fadeStep=(int)(data-fadeFrom);
 	 	//fadeFrom=fadeFrom+((fadeCount*fadeStep)/FADE_LENGTH);
 	 	//fadeFrom=(fadeFrom+fadeFrom+data)/3;
 	 	//if((abs(fadeFrom-data))<50) fade=false;
 	 	//
 	 	//if(fadeStep<50) fade=false,mcpDacSend(data);
 	 	//else{i
 	 	
 	 	//if(fadeStep<0) 
 	 	//else fadeFrom=fadeFrom-((fadeCount*abs(fadeStep))/FADE_LENGTH);
 	 	//(FADE_LENGTH-fadeCount);
 	 	//data=(data*(fadeCount<<10))>>16;
  	 	mcpDacSend(data);
  	 	//}
  	 }
  }
 */ 
  
}

//------------------------------------------------------------------------------
// ADC done interrupt

ISR(ADC_vect) {
  // read data
  sample = ADCH;
//  unsigned char sampleL=ADCL;
  // clear timer campare B flag
  TIFR1 &= _BV(OCF1B);
  if (WaveRP::rpState != RP_STATE_RECORDING) {
    // disable ADC interrupt
 //   ADCSRA &= ~(1 << ADIE);
   // return;
    if (sample > WaveRP::adcMax) WaveRP::adcMax = sample;
 	if (sample < WaveRP::adcMin) WaveRP::adcMin = sample;
    if(WaveRP::audioThru) mcpDacSend(sample<<4);
  }
  else{
  // set min and max before pause
  if (sample > WaveRP::adcMax) WaveRP::adcMax = sample;
  if (sample < WaveRP::adcMin) WaveRP::adcMin = sample;
  // pause has not been tested
  if (WaveRP::rpPause) return;
  if (rpIndex >= BUF_LENGTH) {
    if (WaveRP::sdStatus == SD_STATUS_BUFFER_READY) {
     // swap buffers
      uint8_t *tmp = sdBuffer;
      sdBuffer = rpBuffer;
      rpBuffer = tmp;
      WaveRP::sdStatus= SD_STATUS_BUFFER_BUSY;
      rpIndex = 0;
      // cause interrupt to start SD write
      TIMSK1 |= _BV(OCIE1A);
    } else if (WaveRP::sdStatus == SD_STATUS_BUFFER_BUSY) {
      if (WaveRP::busyError != 0XFF) WaveRP::busyError++;
      return;
    }
    else {
      isrStop();
      return;
    }
  }
  rpBuffer[rpIndex++] = sample;
  	//rpBuffer[rpIndex++] = sampleL; //16bit test
	//rpBuffer[rpIndex++] = sample;



  if(WaveRP::audioThru) mcpDacSend(sample<<4);
  }
}
boolean readBlock(){
sdByteCount = BUF_LENGTH;
    uint32_t dataRemaining = WaveRP::sdEndPosition - WaveRP::sdCurPosition;
    if (sdByteCount > dataRemaining) {
      sdByteCount = dataRemaining;
      if (sdByteCount == 0) {
        WaveRP::sdStatus = SD_STATUS_END_OF_DATA;
        return false;
      }
    }
    
uint32_t block = WaveRP::sdStartBlock + WaveRP::sdCurPosition/512;
    WaveRP::sdCurPosition += sdByteCount;
    if (!WaveRP::sdCard->readBlock(block, sdBuffer)) {
      WaveRP::sdStatus = SD_STATUS_IO_ERROR;
      return false;
    }
    return true;
}    
    
//------------------------------------------------------------------------------
// Interrupt for SD read/write
ISR(TIMER1_COMPA_vect) {
  // disable interrupt
  TIMSK1 &= ~_BV(OCIE1A);
  if (WaveRP::sdStatus != SD_STATUS_BUFFER_BUSY) return;
  sei();
//  Sd2Card *card = SdVolume::sdCard();
  if (WaveRP::rpState == RP_STATE_RECORDING) {
    // write 512 byte data block to SD
    if (!WaveRP::sdCard->writeData(sdBuffer)) {
      WaveRP::sdStatus = SD_STATUS_IO_ERROR;
      return;
    }
    WaveRP::sdCurPosition += 512;
    // check for end of preallocated file
    if (WaveRP::sdCurPosition >= WaveRP::sdEndPosition) {
      WaveRP::sdStatus = SD_STATUS_END_OF_DATA;
      // terminate SD multiple block write
      WaveRP::sdCard->writeStop();
      return;
    }
  } else if (WaveRP::rpState == RP_STATE_PLAYING) {
  /*
    sdByteCount = BUF_LENGTH;
    uint32_t dataRemaining = WaveRP::sdEndPosition - WaveRP::sdCurPosition;
    if (sdByteCount > dataRemaining) {
      sdByteCount = dataRemaining;
      if (sdByteCount == 0) {
        WaveRP::sdStatus = SD_STATUS_END_OF_DATA;
        return;
      }
    }
    
    uint32_t block = WaveRP::sdStartBlock + WaveRP::sdCurPosition/512;
    WaveRP::sdCurPosition += sdByteCount;
    if (!WaveRP::sdCard->readBlock(block, sdBuffer)) {
      WaveRP::sdStatus = SD_STATUS_IO_ERROR;
      return;
    }
    */
    if(!readBlock()) return;
  }
  cli();
  WaveRP::sdStatus = SD_STATUS_BUFFER_READY;
}
//==============================================================================
// static helper functions
//------------------------------------------------------------------------------
// start ADC - should test results of prescaler algorithm.  Does it help?
// The idea is to allow max time for ADC to minimize noise.
void WaveRP::adcInit(uint16_t rate, uint8_t pin, uint8_t ref) {
  // mcpDacInit();
  // Set ADC reference
  // Left adjust ADC result to allow easy 8 bit reading
  // Set low three bits of analog pin number
  //  ADMUX = (1 << REFS0) | (1 << ADLAR) | (ADC_PIN & 7);
  ADMUX = (1 << ADLAR);
  ADMUX = (ref & ADC_REF_MASK) |(1 << ADLAR) | (pin & 7);
  // trigger on timer/counter 1 compare match B
  ADCSRB = (1 << ADTS2) | (1 << ADTS0);
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  // the MUX5 bit of ADCSRB selects whether we're reading from channels
  // 0 to 7 (MUX5 low) or 8 to 15 (MUX5 high).
  if (pin > 7) ADCSRB |= (1 << MUX5);
#endif  // defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  // Each sample requires 13 ADC clock cycles
  // prescaler must be smaller than preMax
 // uint16_t preMax = F_CPU/(rate * 13UL);
 uint8_t adps;  // prescaler bits for ADCSRA
  /*
  if (preMax >  100) {
    // use prescaler of 64
    adps = (1 << ADPS2) | (1 << ADPS1);
  } else if (preMax > 50) {
    // use prescaler of 32
    adps = (1 << ADPS2) | (1 << ADPS0);
  } else if (preMax > 25) {
    // use prescaler of 16
    adps = (1 << ADPS2);
  } else {
    // use prescaler of 8
    adps = (1 << ADPS1) | (1 << ADPS0);
  }
  */
 // adps = (1 << ADPS2) | (1 << ADPS1)| (1 << ADPS0); //hack
//   adps = (1 << ADPS2) | (1 << ADPS1);
   adps = (1 << ADPS2) | (1 << ADPS0);
  // Enable ADC, Auto trigger mode, Enable ADC Interrupt, Start A2D Conversions
  ADCSRA = (1 << ADEN) | (1 << ADATE) | (1 << ADIE) | (1 << ADSC) | adps;
  /// here
}
//------------------------------------------------------------------------------
// setup 16 bit timer1
static void setRate(uint16_t rate) {
  // no pwm
  TCCR1A = 0;
  // no clock div, CTC mode
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
  // set TOP for timer reset
  ICR1 = F_CPU/(uint32_t)rate;
  // compare for SD interrupt
  OCR1A =  10;
  // compare for DAC interrupt
  OCR1B = 1;
}
//------------------------------------------------------------------------------
// do a binary search to find recorded part of a wave file
// assumes file was pre-erased using SD erase command
static uint8_t waveSize(SdBaseFile* file, uint32_t* size) {
  Sd2Card* card = file->volume()->sdCard();
  uint32_t hi;
  uint32_t lo;
  uint16_t n;
  // get block range for file
  if (!file->contiguousRange(&lo, &hi)) return false;
  // remember first
  uint32_t first = lo;
  // find last block of recorded data
  while (lo < hi) {
    // need to add one to round up mid - matches way interval is divided below
    uint32_t mid = (hi + lo + 1)/2;
    // read block
    if (!WaveRP::sdCard->readBlock(mid, buf)) {
      return false;
    }
    // look for data - SD erase may result in all zeros or all ones
    for (n = 0; n < 512; n++) {
      if (0 < buf[n] && buf[n] < 0XFF) break;
    }
    if (n == 512) {
      // no data search lower half of interval
      hi = mid - 1;
    } else {
      // search upper half of interval keeping found data in interval
      lo = mid;
    }
  }
  // read last block with data to get count for last block
  if (!card->readBlock(lo, buf)) return false;
  for (n = 512; n > 0; n--) {
    if (0 < buf[n-1] && buf[n-1] < 0XFF) break;
  }
  *size = 512*(lo - first) + n;
  return true;
}
//==============================================================================
// WaveRP member functions
//------------------------------------------------------------------------------
/** play a wave file
 *
 * \param[in] file File to play.
 *
 * \return true for success or false for failure.
 */
bool WaveRP::play(SdBaseFile* file) {
  sdCard = file->volume()->sdCard();
  if (!file->contiguousRange(&sdStartBlock, &sdEndBlock)) {
   // PgmPrintln("File is not contiguous");
    return false;
  }
  // read header into buffer
  WaveHeaderExtra *header = reinterpret_cast<WaveHeaderExtra *>(buf);
  // compiler will optimize these out - makes program more readable
  WaveRiff *riff = &header->riff;
  WaveFmtExtra *fmt = &header->fmt;
  WaveData *data = &header->data;
  file->rewind();

  // file must start with RIFF chunck
  if (file->read(riff, 12) != 12
      || strncmp_P(riff->id, PSTR("RIFF"), 4)
      || strncmp_P(riff->type, PSTR("WAVE"), 4)) {
        return false;
  }
  // fmt chunck must be next
  if (file->read(fmt, 8) != 8
     || strncmp_P(fmt->id, PSTR("fmt "), 4)) {
        return false;
  }
  uint16_t size = fmt->size;
  if (size == 16 || size == 18) {
    if (file->read(&fmt->formatTag, size) != (int16_t)size) {
      return false;
    }
  } else {
    // don't read fmt chunk - must not be compressed so cause an error
    fmt->formatTag = WAVE_FORMAT_UNKNOWN;
  }
  if (fmt->formatTag != WAVE_FORMAT_PCM) {
   // PgmPrintln("Compression not supported");
    return false;
  }
  Channels = fmt->channels;
  if (Channels > 2) {
  //  PgmPrintln("Not mono/stereo!");
    return false;
  }
  sampleRate = fmt->sampleRate;
  bitsPerSample = fmt->bitsPerSample;
  if (bitsPerSample > 16) {
 //   PgmPrintln("More than 16 bits per sample!");
    return false;
  }
  uint8_t tooFast = sampleRate > 44100;  // flag
  if (sampleRate > 22050) {
    // ie 44khz
    if (Channels > 1) tooFast = 1;
  }
  if (tooFast) {
  //  PgmPrintln("Sample rate too high!");
    return false;
  }
  // find the data chunck
  while (1) {
    // read chunk ID
    if (file->read(data, 8) != 8) return false;
    if (!strncmp_P(data->id, PSTR("data"), 4)) {
      sdEndPosition = file->curPosition() + data->size;
      break;
    }
    // if not "data" then skip it!
    if (!file->seekCur(data->size)) return false;
  }
  sdCurPosition = file->curPosition();

  rpBuffer = buf;
  // fill the buffers so SD blocks line up with buffer boundaries.
  rpByteCount = BUF_LENGTH - file->curPosition() % BUF_LENGTH;
  uint32_t maxCount = sdEndPosition - sdCurPosition;
  if (rpByteCount > maxCount) rpByteCount = maxCount;
  if (file->read(rpBuffer, rpByteCount) != rpByteCount) return false;
  sdCurPosition += rpByteCount;
  sdBuffer = reinterpret_cast<uint8_t*>(file->volume()->cacheClear());
  if (sdCurPosition < sdEndPosition) {
    sdByteCount = BUF_LENGTH;
    maxCount = sdEndPosition - sdCurPosition;
    if (maxCount < sdByteCount) sdByteCount = maxCount;
    uint32_t block = sdStartBlock + sdCurPosition / 512;
    if (!sdCard->readBlock(block, sdBuffer)) return false;
    sdStatus = SD_STATUS_BUFFER_READY;
    sdCurPosition += sdByteCount;
  } else {
    sdByteCount = 0;
    sdStatus = SD_STATUS_END_OF_DATA;
  }
  busyError = 0;
  sdFile = file;
  // clear pasue
  rpPause = false;
  // set ADC/DAC state
  rpState = RP_STATE_PLAYING;
  // setup timer
  setRate(sampleRate*Channels);
  // initialize DAC pins
  mcpDacInit();
  // enable timer interrupt for DAC
  TIMSK1 |= _BV(OCIE1B);
  return true;
}
//------------------------------------------------------------------------------
/** record to a preallocated contiguous file
 *
 * \param[in] file File to use for recording
 * \param[in] rate Record rate in samples/second.
 * \param[in] pin Analog pin connected to microphone amp.
 * \param[in] ref Type of analog reference to use.
 *
 * \return true for success or false for failure.
 */
bool WaveRP::record(SdBaseFile* file, uint16_t rate, uint8_t pin, uint8_t ref) {
  sdCard = file->volume()->sdCard();
  if (rate < 4000 || 44100 < rate) {
  //  PgmPrintln("rate not in range 4000 to 44100");
    return false;
  }
  if (pin > ADC_MAX_PIN || ref != (ref & ADC_REF_MASK)) return false;
  sdFile = file;
//  Sd2Card *card = SdVolume::sdCard();
  // get raw location of file's data blocks
  if (!file->contiguousRange(&sdStartBlock, &sdEndBlock)) return false;
  // tell SD card to erase all of the file's data
  if (!sdCard->erase(sdStartBlock, sdEndBlock)) return false;
  // setup timer
  setRate(rate);
  sdBuffer = buf;
  sdStatus = SD_STATUS_BUFFER_READY;
  // use SdVolume's cache as second buffer to save RAM
  // can't access files while recording
  if (!(rpBuffer = reinterpret_cast<uint8_t*>(file->volume()->cacheClear()))) {
    return false;
  }
  // fill in wave file header
  WaveHeader *header = reinterpret_cast<WaveHeader *>(rpBuffer);
  // RIFF chunck
  strncpy_P(header->riff.id, PSTR("RIFF"), 4);
  header->riff.size = file->fileSize() - 8;
  strncpy_P(header->riff.type, PSTR("WAVE"), 4);
  // fmt chunck
  strncpy_P(header->fmt.id, PSTR("fmt "), 4);
  header->fmt.size = sizeof(WaveFmt) - 8;
  header->fmt.formatTag = WAVE_FORMAT_PCM;
  header->fmt.channels = 1;
  header->fmt.sampleRate = rate;
  header->fmt.bytesPerSecond = rate;
  header->fmt.blockAlign = 1;
  header->fmt.bitsPerSample = 8; //16bit rec hack
  // data chunck
  strncpy_P(header->data.id, PSTR("data"), 4);
  header->data.size = file->fileSize() - sizeof(WaveHeader);
  // location for first sample
  rpIndex = sizeof(WaveHeader);
  // zero rest of block
  for (uint16_t i = rpIndex; i < BUF_LENGTH; i++) rpBuffer[i] = 0;
  // write first block to SD in case no data is recorded
  if (!sdCard->writeBlock(sdStartBlock, rpBuffer)) return false;
  // begin position for ISR write handler
  sdCurPosition = rpIndex;
  // end position for ISR write handler
  sdEndPosition = file->fileSize();

  busyError = 0;
  // tell the SD card where we plan to write
  if (!sdCard->writeStart(sdStartBlock, sdEndBlock - sdStartBlock + 1)) {
    return false;
  }
  // clear pasue
  rpPause = false;
  // set ADC/DAC state
  rpState = RP_STATE_RECORDING;
  // start the ADC
  adcInit(rate, pin, ref);
  return true;
}
//------------------------------------------------------------------------------
#if DVOLUME || defined(__DOXYGEN__)
static uint8_t dbToMult[] PROGMEM = {
  0, 228, 203, 181, 162, 144, 128, 114, 102, 91, 81, 72, 64, 57, 51, 46, 41,
  36, 32, 29, 26, 23, 20, 18, 16, 14, 13, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
/** Set volume factor
 *  \param[in] db amount to decrease volume.  Max is 37
 */
void WaveRP::setVolume(uint8_t db) {
  volMult = db > 37 ? 1 : pgm_read_byte(&dbToMult[db]);
  volOffset = 2048 - 8 * volMult;
}
#endif  // DVOLUME
//------------------------------------------------------------------------------
/** stop player or recorder */
void WaveRP::stop(void) {
  cli();
  uint8_t tmp = rpState;
  isrStop();
  sei();
  // clear pasue
  rpPause = false;
  if (tmp == RP_STATE_RECORDING) {
    sdCard->writeStop();
  }
}
//------------------------------------------------------------------------------
/** trim unused space from contiguous wave file - must have been pre-erased
 *
 * \param[in] file File to trim.
 *
 * \return true for success else false.
 */
bool WaveRP::trim(SdBaseFile* file) {
  uint32_t fsize;
  // if error getting size of recording
  if (!waveSize(file, &fsize)) return false;
  // if too short to have header
  if (fsize < sizeof(WaveHeader)) return false;
  if (!file->seekSet(0)) return false;
  // read header
  if (file->read(buf, sizeof(WaveHeader)) != sizeof(WaveHeader)) return false;
  WaveHeader *header = reinterpret_cast<WaveHeader *>(buf);
  // check for correct size header
  if (strncmp_P(header->data.id, PSTR("data"), 4)) return false;
  // update size fields
  header->riff.size = fsize - 8;
  header->data.size = fsize - sizeof(WaveHeader);
  if (!file->seekSet(0)) return false;
  // write header back
  if (file->write(buf, sizeof(WaveHeader)) != sizeof(WaveHeader)) return false;
  // truncate file
  return file->truncate(fsize);
}
//==============================================================================
// untested functions - need to decide if pause/resume works for recorder
//------------------------------------------------------------------------------
/** seek - only allow if player and paused.
 *  Not debugged - do not use
 *
 * \param[in] pos New file position.
 */
void WaveRP::seek(uint32_t pos) {
//fadeFrom=data;
//fadeCount=0;
//fade=true;

  pos -= pos % BUF_LENGTH;
  // don't play metadata
  if (pos <= BUF_LENGTH) pos = BUF_LENGTH;
  if (pos > sdEndPosition) pos = sdEndPosition; 
  if (isPlaying() && isPaused()){
 	sdCurPosition = pos;
    //if(pos == sdEndPosition) return;
   //	readBlock(); ////novinka
   }
   
  
  
}
//------------------------------------------------------------------------------
/** Set sample rate - only allow for player.
 *  Not debugged - do not use
 *
 * \param[in] samplerate New rate in samples/sec
 */
void WaveRP::setSampleRate(uint32_t samplerate) { // try for recording
//if(samplerate<7500) divider=3, samplerate=samplerate*3;
/*
if(samplerate<13000) divider=2,samplerate*=2;//samplerate*2; //novinka
else divider=1;
*/
  if (isPlaying()) {
  
  //s  while (TCNT1 != 0); // GRANNYFUCKER !!!
    ICR1 = F_CPU / samplerate;
    
  }
}
