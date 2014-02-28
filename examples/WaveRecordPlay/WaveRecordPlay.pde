/**
 * Example Arduino record/play sketch for the Adafruit Wave Shield.
 * For best results use Wave Shield version 1.1 or later.
 *
 * The SD/SDHC card should be formatted with 32KB allocation units/clusters.
 * Cards with 2GB or less should be formatted FAT16 to minimize file
 * system overhead. If possible use SDFormatter from
 * www.sdcard.org/consumers/formatter/
 *
 * The user must supply a microphone preamp that delivers 0 - vref volts
 * of audio signal to the analog pin.  The preamp should deliver vref/2
 * volts for silence.
 *
 */
#include <SdFat.h>
#include <WaveRP.h>
#include <SdFatUtil.h>
#include <ctype.h>
// record rate - must be in the range 4000 to 44100 samples per second
// best to use standard values like 8000, 11025, 16000, 22050, 44100
#define RECORD_RATE 22050
//#define RECORD_RATE 44100
//
// max recorded file size.  Size should be a multiple of cluster size.
// the recorder creates and erases a contiguous file of this size.
// 100*1024*1024 bytes - about 100 MB or 150 minutes at 11025 samples/second
#define MAX_FILE_SIZE 104857600UL  // 100 MB
//#define MAX_FILE_SIZE 1048576000UL // 1 GB
//
// Analog pin connected to mic preamp
#define MIC_ANALOG_PIN 0
//
// Voltage Reference Selections for ADC
//#define ADC_REFERENCE ADC_REF_AREF  // use voltage on AREF pin
 #define ADC_REFERENCE ADC_REF_AVCC  // use 5V VCC
//
// print the ADC range while recording if > 0
// print adcMax,adcMin if > 1
#define DISPLAY_RECORD_LEVEL 1
//
// print file info - useful for debug
#define PRINT_FILE_INFO 0
//
// print bad wave file size and SD busy errors for debug
#define PRINT_DEBUG_INFO 1
//------------------------------------------------------------------------------
// global variables
Sd2Card card;           // SD/SDHC card with support for version 2.00 features
SdVolume vol;           // FAT16 or FAT32 volume
SdFile root;            // volume's root directory
SdFile file;            // current file
WaveRP wave;            // wave file recorder/player
int16_t lastTrack = -1; // Highest track number
uint8_t trackList[32];  // bit list of used tracks
//------------------------------------------------------------------------------
// print error message and halt
void error(char* str) {
  PgmPrint("error: ");
  Serial.println(str);
  if (card.errorCode()) {
    PgmPrint("sdError: ");
    Serial.println(card.errorCode(), HEX);
    PgmPrint("sdData: ");
    Serial.println(card.errorData(), HEX);
  }
  while(1);
}
//------------------------------------------------------------------------------
// print help message
void help(void) {
  PgmPrintln("a     play all WAV files in the root dir");
  PgmPrintln("c     clear - deletes all tracks");
  PgmPrintln("d     delete last track");
  PgmPrintln("<n>d  delete track number <n>");
  PgmPrintln("h     help");
  PgmPrintln("l     list track numbers");
  PgmPrintln("p     play last track");
  PgmPrintln("<n>p  play track number <n>");
  PgmPrintln("r     record new track as last track");
  PgmPrintln("<n>r  record over deleted track <n>");
  PgmPrintln("v     record new track voice activated");
  PgmPrintln("<n>v  record over deleted track voice activated");
}
//------------------------------------------------------------------------------
// clear all bits in track list
void listClear(void)  {
  memset(trackList, 0, sizeof(trackList));
}
//------------------------------------------------------------------------------
// return bit for track n
uint8_t listGet(uint8_t n) {
  return (trackList[n >> 3] >> (n & 7)) & 1;
}
//------------------------------------------------------------------------------
// print list of tracks in ten columns with each column four characters wide
void listPrint(void) {
  PgmPrintln("\nTrack list:");
  uint8_t n = 0;
  uint8_t nc = 0;
  do {
    if (!listGet(n)) continue;
    if (n < 100) Serial.print(' ');
    if (n < 10) Serial.print(' ');
    Serial.print(n, DEC);
    if (++nc == 10) {
      Serial.println();
      nc = 0;
    } else {
      Serial.print(' ');
    }
  } while (n++ != 255);
  if (nc) Serial.println();
}
//------------------------------------------------------------------------------
// set bit for track n
void listSet(uint8_t n) {
  trackList[n >> 3] |= 1 << (n & 7);
}
//------------------------------------------------------------------------------
// Nag user about power and SD card
void nag(void) {
  PgmPrintln("\nTo avoid USB noise use a DC adapter");
  PgmPrintln("or battery for Arduino power.\n");
  uint8_t bpc = vol.blocksPerCluster();
  PgmPrint("BlocksPerCluster: ");
  Serial.println(bpc, DEC);
  uint8_t align = vol.dataStartBlock() & 0X3F;
  PgmPrint("Data alignment: ");
  Serial.println(align, DEC);
  PgmPrint("sdCard size: ");
  Serial.print(card.cardSize()/2000UL);PgmPrintln(" MB");
  if (align || bpc < 64) {
    PgmPrintln("\nFor best results use a 2 GB or larger card.");
    PgmPrintln("Format the card with 64 blocksPerCluster and alignment = 0.");
    PgmPrintln("If possible use SDFormater from www.sdcard.org/consumers/formatter/");
  }
  if (!card.eraseSingleBlockEnable()) {
    PgmPrintln("\nCard is not erase capable and can't be used for recording!");
  }
}
//------------------------------------------------------------------------------
// check for pause resume
void pauseResume(void) {
  if (!Serial.available()) return;
  uint8_t c = Serial.read();
  while (Serial.read() >= 0) {}
  if (c == 's') {
    wave.stop();
  } else if (c == 'p') {
    wave.pause();
    while (wave.isPaused()) {
      PgmPrintln("\nPaused - type 's' to stop 'r' to resume");
      while (!Serial.available()) {}
      c = Serial.read();
      if (c == 's') wave.stop();
      if (c == 'r') wave.resume();
    }
  }
}
//------------------------------------------------------------------------------
// play all files in the root dir
void playAll(void) {
  dir_t dir;
  char name[13];
  uint8_t np = 0;
  root.rewind();
  while (root.readDir(&dir) == sizeof(dir)) {
    //only play wave files
    if (strncmp_P((char *)&dir.name[8], PSTR("WAV"), 3)) continue;
    // remember current dir position
    uint32_t pos = root.curPosition();
    // format file name
    SdFile::dirName(dir, name);
    if (!playBegin(name)) continue;
    PgmPrintln(", type 's' to skip file 'a' to abort");
    while (wave.isPlaying()) {
      if (Serial.available()) {
        uint8_t c = Serial.read();
        while (Serial.read() >= 0) {}
        if (c == 's' || c == 'a') {
          wave.stop();
          file.close();
          if (c == 'a') return;
        }
      }
    }
    file.close();
    // restore dir position
    root.seekSet(pos);
  }
}
//------------------------------------------------------------------------------
// start file playing
uint8_t playBegin(char* name) {
  if (!file.open(&root, name, O_READ)) {
    PgmPrint("Can't open: ");
    Serial.println(name);
    return false;
  }
  if (!wave.play(&file)) {
    PgmPrint("Can't play: ");
    Serial.println(name);
    file.close();
    return false;
  }
#if PRINT_FILE_INFO
  Serial.print(wave.bitsPerSample, DEC);
  PgmPrint("-bit, ");
  Serial.print(wave.sampleRate/1000);
  PgmPrintln(" kps");
#endif // PRINT_FILE_INFO
#if PRINT_DEBUG_INFO
  if (wave.sdEndPosition > file.fileSize()) {
    PgmPrint("play Size mismatch,");
    Serial.print(file.fileSize());
    Serial.print(',');
    Serial.println(wave.sdEndPosition);
  }
#endif // PRINT_DEBUG_INFO
  PgmPrint("Playing: ");
  Serial.print(name);
  return true;
}
//------------------------------------------------------------------------------
// play a file
void playFile(char* name) {

  if (!playBegin(name)) return;
  PgmPrintln(", type 's' to stop 'p' to pause");
  while (wave.isPlaying()) {
    pauseResume();
  }
  file.close();
#if PRINT_DEBUG_INFO
  if (wave.errors()) {
    PgmPrint("busyErrors: ");
    Serial.println(wave.errors(), DEC);
  }
#endif // PRINT_DEBUG_INFO
}
//-----------------------------------------------------------------------------
void recordManualControl(void) {
  PgmPrintln("Recording - type 's' to stop 'p' to pause");
  uint8_t nl = 0;
  while (wave.isRecording()) {
#if DISPLAY_RECORD_LEVEL > 0
    wave.adcClearRange();
    delay(500);
#if DISPLAY_RECORD_LEVEL > 1
    Serial.print(wave.adcGetMax(), DEC);
    Serial.print(',');
    Serial.println(wave.adcGetMin(), DEC);
#else // #if DISPLAY_RECORD_LEVEL > 1
    Serial.print(wave.adcGetRange(), DEC);
    if (++nl % 8) {
      Serial.print(' ');
    } else {
      Serial.println();
    }
#endif // DISPLAY_RECORD_LEVEL > 1
#endif // DISPLAY_RECORD_LEVEL > 0
    // check for pause/stop
    pauseResume();
  }
}
//-----------------------------------------------------------------------------
#define SAR_TIMEOUT 4
#define SAR_THRESHOLD 40
void recordSoundActivated(void) {
  uint32_t t;
  wave.pause();
  uint8_t n = 0;
  wave.adcClearRange();
  PgmPrintln("Recording - type 's' to stop");
  while (1) {
    if (wave.adcGetRange() >= SAR_THRESHOLD) {
      if (wave.isPaused()) {
        wave.resume();
        Serial.print('r');
        if (++n % 40 == 0) Serial.println();
      }
      t = millis();
      wave.adcClearRange();
    } else if (!wave.isPaused()) {
      if ((millis() - t) > 1000*SAR_TIMEOUT) {
        wave.pause();
        Serial.print('p');
        if (++n % 40 == 0) Serial.println();
      }
    }
    if (Serial.read() == 's') {
      wave.stop();
      return;
    }
  }
}
//------------------------------------------------------------------------------
// scan root directory for track list and recover partial tracks
void scanRoot(void) {
  dir_t dir;
  char name[13];
  listClear();
  root.rewind();
  lastTrack = -1;
  while (root.readDir(&dir) == sizeof(dir)) {
    // only accept TRACKnnn.WAV with nnn < 256
    if (strncmp_P((char *)dir.name, PSTR("TRACK"), 5)) continue;
    if (strncmp_P((char *)&dir.name[8], PSTR("WAV"), 3)) continue;
    int16_t n = 0;
    uint8_t i;
    for (i = 5; i < 8 ; i++) {
      char c = (char)dir.name[i];
      if (!isdigit(c)) break;
      n *= 10;
      n += c - '0';
    }
    // nnn must be three digits and less than 256
    if (i != 8 || n > 255) continue;
    if (n > lastTrack) lastTrack = n;
    // mark track found
    listSet(n);
    if (dir.fileSize != MAX_FILE_SIZE) continue;
    // try to recover untrimmed file
    uint32_t pos = root.curPosition();
    if (!trackName(n, name)
      || !file.open(&root, name, O_READ |O_WRITE)
      || !wave.trim(&file)) {
      if (!file.truncate(0)) {
        PgmPrint("Can't trim: ");
        Serial.println(name);
      }
    }
    file.close();
    root.seekSet(pos);
  }
}
//------------------------------------------------------------------------------
// delete all tracks on SD
void trackClear(void) {
  char name[13];
  while (Serial.read() >= 0) {}
  PgmPrintln("Type Y to delete all tracks!");
  while (!Serial.available()) {}
  if (Serial.read() != 'Y') {
    PgmPrintln("Delete all canceled!");
    return;
  }
  for (uint16_t i = 0; i < 256; i++) {
    if (!listGet(i)) continue;
    if (!trackName(i, name)) return;
    if (!SdFile::remove(&root, name)) {
      PgmPrint("Delete failed for: ");
      Serial.println(name);
      return;
    }
  }
  PgmPrintln("Deleted all tracks!");
}
//------------------------------------------------------------------------------
// delete a track
void trackDelete(int16_t track) {
  char name[13];
  if (!trackName(track, name)) return;
  while (Serial.read() >= 0) {}
  PgmPrint("Type y to delete: ");
  Serial.println(name);
  while (!Serial.available()) {}
  if (Serial.read() != 'y') {
    PgmPrintln("Delete canceled!");
    return;
  }
  if (SdFile::remove(&root, name)) {
    PgmPrintln("Deleted!");
  } else {
    PgmPrintln("Delete failed!");
  }
}
//------------------------------------------------------------------------------
// format a track name in 8.3 format
uint8_t trackName(int16_t number, char* name) {
  if (0 <= number && number <= 255) {
    strcpy_P(name, PSTR("TRACK000.WAV"));
    name[5] = '0' + number/100;
    name[6] = '0' + (number/10)%10;
    name[7] = '0' + number%10;
    return true;
  }
  PgmPrint("Invalid track number: ");
  Serial.println(number);
  return false;
}
//------------------------------------------------------------------------------
// play a track
void trackPlay(int16_t track) {
  char name[13];
  if (!trackName(track, name)) return;
  playFile(name);
}
//------------------------------------------------------------------------------
// record a track
void trackRecord(int16_t track, uint8_t mode) {
  char name[13];
  if (track < 0) track = lastTrack + 1;
  if (!trackName(track , name)) return;
  if (file.open(&root, name, O_READ)) {
    PgmPrint("Track already exists. Use '");
    Serial.print(track);
    Serial.print("d' to delete it.");
    file.close();
    return;
  }
  PgmPrint("Creating: ");
  Serial.println(name);
  if (!file.createContiguous(&root, name, MAX_FILE_SIZE)) {
    PgmPrintln("Create failed");
    return;
  }
  if(!wave.record(&file, RECORD_RATE, MIC_ANALOG_PIN, ADC_REFERENCE)) {
    PgmPrintln("Record failed");
    file.remove();
    return;
  }
  if (mode == 'v') {
    recordSoundActivated();
  } else {
    recordManualControl();
  }
  // trim unused space from file
  wave.trim(&file);
  file.close();
#if PRINT_DEBUG_INFO
  if (wave.errors() ){
    PgmPrint("busyErrors: ");
    Serial.println(wave.errors(), DEC);
  }
#endif // PRINT_DEBUG_INFO
}
//==============================================================================
// Standard Arduino setup() and loop() functions
//------------------------------------------------------------------------------
// setup Serial port and SD card
void setup(void) {
  Serial.begin(9600);
  delay(10);
  PgmPrint("\nFreeRam: ");
  Serial.println(FreeRam());
  if (!card.init()) error("card.init");
  if (!vol.init(&card)) error("vol.init");
  if (!root.openRoot(&vol)) error("openRoot");
  nag();  // nag user about power and SD card
}
//------------------------------------------------------------------------------
// loop to play and record files.
void loop() {
  // insure file is closed
  if (file.isOpen()) file.close();
  // scan root dir to build track list and set lastTrack
  scanRoot();
  while (Serial.read() >= 0) {}
  PgmPrintln("\ntype a command or h for help");
  int16_t track = -1;
  uint8_t c;
  while(track < 256){
    while (!Serial.available()) {}
    c = Serial.read();
    if (!isdigit(c)) break;
    track = (track < 0 ? 0 : 10 * track) + c - '0';
  }
  if (track < 0 && (c == 'd' || c == 'p')) {
    if (lastTrack < 0) {
      PgmPrintln("No tracks exist");
      return;
    }
    track = lastTrack;
  }
  Serial.println();
  if (c == 'a') playAll();
  else if (c == 'c') trackClear();
  else if (c == 'd') trackDelete(track);
  else if (c == 'h') help();
  else if (c == 'l') listPrint();
  else if (c == 'p') trackPlay(track);
  else if (c == 'r' || c == 'v') trackRecord(track, c);
  else PgmPrintln("? - type h for help");
}