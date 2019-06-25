#define ARDUINO_TYPE MEGA

/* ----------------------------------------------------------------------------
WARNING: The Arduino Leonardo, Arduino Esplora and the Arduino Micro all use the same chip (ATmega32U4).
They will all have the Leonardo bootloader burnt onto them. This means that if you have a
Micro or Esplora it will be identified as a Leonardo in the Tools -> Serial Port menu.
This is because the PID (Product ID) in the USB firmware will be 0x0036 (Leonardo).
This only applies during the uploading process. You should still select the correct board in the
Tools -> Boards menu.
------------------------------------------------------------------------------ */

#define VERSION "1.38"

// make true to use ICSP programming
#define ICSP_PROGRAMMING true


const int ENTER_PROGRAMMING_ATTEMPTS = 50;

/*

 Copyright 2012 Nick Gammon.


 PERMISSION TO DISTRIBUTE

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.


 LIMITATION OF LIABILITY

 The software is provided "as is", without warranty of any kind, express or implied,
 including but not limited to the warranties of merchantability, fitness for a particular
 purpose and noninfringement. In no event shall the authors or copyright holders be liable
 for any claim, damages or other liability, whether in an action of contract,
 tort or otherwise, arising from, out of or in connection with the software
 or the use or other dealings in the software.

*/

#include <SPI.h>
#include <avr/pgmspace.h>


const unsigned long BAUD_RATE = 115200;

const byte CLOCKOUT = 9;


#ifdef ARDUINO_PINOCCIO
  const byte RESET = SS;  // --> goes to reset on the target board
#else
  const byte RESET = 10;  // --> goes to reset on the target board
#endif

#if ARDUINO < 100
  const byte SCK = 13;    // SPI clock
#endif

#include "Signatures.h"
#include "General_Stuff.h"


// structure to hold signature and other relevant data about each bootloader
typedef struct {
   byte sig [3];                // chip signature
   unsigned long loaderStart;   // start address of bootloader (bytes)
   const byte * bootloader;     // address of bootloader hex data
   unsigned int loaderLength;   // length of bootloader hex data (bytes)
   byte lowFuse, highFuse, extFuse, lockByte;  // what to set the fuses, lock bits to.
} bootloaderType;


// hex bootloader data

#if ARDUINO_TYPE == UNO
  #include "bootloaders-uno.h"
#elif ARDUINO_TYPE == MEGA
  #include "bootloaders-mega.h"
#endif
// see Atmega328 datasheet page 298
const bootloaderType bootloaders [] PROGMEM =
  {
// Only known bootloaders are in this array.
// If we support it at all, it will have a start address.
// If not compiled into this particular version the bootloader address will be zero.

  // USE_AT90USB82
  { { 0x1E, 0x93, 0x82 },
        0x1000,      // start address
        at90usb82_bin,// loader image
        sizeof at90usb82_bin,
        0xEF,         // fuse low byte: external clock, m
        0xD9,         // fuse high byte: SPI enable, NOT boot into bootloader, 4096 byte bootloader
        0xF4,         // fuse extended byte: brown-out detection at 2.6V
        0xCF },       // lock bits
        
  // USE_AT90USB162
  { { 0x1E, 0x94, 0x82 },
        0x3000,      // start address
        at90usb162_bin,// loader image
        sizeof at90usb162_bin,
        0xEF,         // fuse low byte: external clock, m
        0xD9,         // fuse high byte: SPI enable, NOT boot into bootloader, 4096 byte bootloader
        0xF4,         // fuse extended byte: brown-out detection at 2.6V
        0xCF },       // lock bits
  // ATmega8U2
  { { 0x1E, 0x93, 0x89 },
        0x1000,      // start address
        atmega8u2_bin,// loader image
        sizeof atmega8u2_bin,
        0xEF,         // fuse low byte: external clock, m
        0xD9,         // fuse high byte: SPI enable, NOT boot into bootloader, 4096 byte bootloader
        0xF4,         // fuse extended byte: brown-out detection at 2.6V
        0xCF },       // lock bits
  // ATmega16U2
  { { 0x1E, 0x94, 0x89 },
        0x3000,      // start address
        atmega16u2_bin,// loader image
        sizeof atmega16u2_bin,
        0xEF,         // fuse low byte: external clock, m
        0xD9,         // fuse high byte: SPI enable, NOT boot into bootloader, 4096 byte bootloader
        0xF4,         // fuse extended byte: brown-out detection at 2.6V
        0xCF },       // lock bits

  // ATmega32U2
  { { 0x1E, 0x95, 0x8A },
        0x7000,      // start address
        atmega32u2_bin,// loader image
        sizeof atmega32u2_bin,
        0xFF,         // fuse low byte: external clock, max start-up time
        0xD8,         // fuse high byte: SPI enable, boot into bootloader, 1280 byte bootloader
        0xCB,         // fuse extended byte: brown-out detection at 2.6V
        0x2F },       // lock bits: SPM is not allowed to write to the Boot Loader section.
  
  };  // end of bootloaders



void getFuseBytes ()
  {
  Serial.print (F("LFuse = "));
  showHex (readFuse (lowFuse), true);
  Serial.print (F("HFuse = "));
  showHex (readFuse (highFuse), true);
  Serial.print (F("EFuse = "));
  showHex (readFuse (extFuse), true);
  Serial.print (F("Lock byte = "));
  showHex (readFuse (lockByte), true);
  Serial.print (F("Clock calibration = "));
  showHex (readFuse (calibrationByte), true);
  }  // end of getFuseBytes

bootloaderType currentBootloader;


// burn the bootloader to the target device
void writeBootloader ()
  {
  bool foundBootloader = false;

  for (unsigned int j = 0; j < NUMITEMS (bootloaders); j++)
    {

    memcpy_P (&currentBootloader, &bootloaders [j], sizeof currentBootloader);

    if (memcmp (currentSignature.sig, currentBootloader.sig, sizeof currentSignature.sig) == 0)
      {
      foundBootloader = true;
      break;
      }  // end of signature found
    }  // end of for each signature

  if (!foundBootloader)
    {
    Serial.println (F("No bootloader support for this device."));
    return;
    }

  // if in the table, but with zero length, we need to enable a #define to use it.
  if (currentBootloader.loaderLength == 0)
    {
    Serial.println (F("Bootloader for this device is disabled, edit " __FILE__ " to enable it."));
    return;
    }

  unsigned int i;

  byte lFuse = readFuse (lowFuse);

  byte newlFuse = currentBootloader.lowFuse;
  byte newhFuse = currentBootloader.highFuse;
  byte newextFuse = currentBootloader.extFuse;
  byte newlockByte = currentBootloader.lockByte;


  unsigned long addr = currentBootloader.loaderStart;
  unsigned int  len = currentBootloader.loaderLength;
  unsigned long pagesize = currentSignature.pageSize;
  unsigned long pagemask = ~(pagesize - 1);
  const byte * bootloader = currentBootloader.bootloader;


  byte subcommand = 'U';
  Serial.print (F("Bootloader address = 0x"));
  Serial.println (addr, HEX);
  Serial.print (F("Bootloader length = "));
  Serial.print (len);
  Serial.println (F(" bytes."));


  unsigned long oldPage = addr & pagemask;

  Serial.println (F("Type 'Q' to quit, 'V' to verify, or 'G' to program the chip with the bootloader ..."));

    // Automatically fix up fuse to run faster, then write to device
    if (lFuse != newlFuse)
      {
      if ((lFuse & 0x80) == 0)
        Serial.println (F("Clearing 'Divide clock by 8' fuse bit."));

      Serial.println (F("Fixing low fuse setting ..."));
      writeFuse (newlFuse, lowFuse);
      delay (1000);
      stopProgramming ();  // latch fuse
      if (!startProgramming ())
        return;
      delay (1000);
      }

    Serial.println (F("Erasing chip ..."));
    eraseMemory ();
    Serial.println (F("Writing bootloader ..."));
    for (i = 0; i < len; i += 2)
      {
      unsigned long thisPage = (addr + i) & pagemask;
      // page changed? commit old one
      if (thisPage != oldPage)
        {
        commitPage (oldPage, true);
        oldPage = thisPage;
        }
      writeFlash (addr + i, pgm_read_byte(bootloader + i));
      writeFlash (addr + i + 1, pgm_read_byte(bootloader + i + 1));
      }  // end while doing each word

    // commit final page
    commitPage (oldPage, true);
    Serial.println (F("Written."));

  Serial.println (F("Verifying ..."));

  // count errors
  unsigned int errors = 0;
  // check each byte
  for (i = 0; i < len; i++)
    {
    byte found = readFlash (addr + i);
    byte expected = pgm_read_byte(bootloader + i);
    if (found != expected)
      {
      if (errors <= 100)
        {
        Serial.print (F("Verification error at address "));
        Serial.print (addr + i, HEX);
        Serial.print (F(". Got: "));
        showHex (found);
        Serial.print (F(" Expected: "));
        showHex (expected, true);
        }  // end of haven't shown 100 errors yet
      errors++;
      }  // end if error
    }  // end of for

  if (errors == 0)
    Serial.println (F("No errors found."));
  else
    {
    Serial.print (errors, DEC);
    Serial.println (F(" verification error(s)."));
    if (errors > 100)
      Serial.println (F("First 100 shown."));
    return;  // don't change fuses if errors
    }  // end if

    Serial.println (F("Writing fuses ..."));

    writeFuse (newlFuse, lowFuse);
    writeFuse (newhFuse, highFuse);
    writeFuse (newextFuse, extFuse);
    writeFuse (newlockByte, lockByte);

    // confirm them
    getFuseBytes ();

  Serial.println (F("Done."));

  } // end of writeBootloader

void getSignature ()
  {
  foundSig = -1;

  byte sig [3];
  Serial.print (F("Signature = "));
  readSignature (sig);
  for (byte i = 0; i < 3; i++)
    showHex (sig [i]);

  Serial.println ();

  for (unsigned int j = 0; j < NUMITEMS (signatures); j++)
    {

    memcpy_P (&currentSignature, &signatures [j], sizeof currentSignature);

    if (memcmp (sig, currentSignature.sig, sizeof sig) == 0)
      {
      foundSig = j;
      Serial.print (F("Processor = "));
      Serial.println (currentSignature.desc);
      Serial.print (F("Flash memory size = "));
      Serial.print (currentSignature.flashSize, DEC);
      Serial.println (F(" bytes."));
      if (currentSignature.timedWrites)
        Serial.println (F("Writes are timed, not polled."));
      return;
      }  // end of signature found
    }  // end of for each signature

  Serial.println (F("Unrecogized signature."));
  }  // end of getSignature

void setup ()
  {
  Serial.begin (BAUD_RATE);
  while (!Serial) ;  // for Leonardo, Micro etc.
  Serial.println ();
  Serial.println (F("Atmega chip programmer."));
  Serial.println (F("Written by Nick Gammon."));
  Serial.println (F("Version " VERSION));
  Serial.println (F("Compiled on " __DATE__ " at " __TIME__ " with Arduino IDE " xstr(ARDUINO) "."));

  initPins ();

 }  // end of setup

void loop ()
  {

  if (startProgramming ())
    {
    getSignature ();
    getFuseBytes ();

    // if we found a signature try to write a bootloader
    if (foundSig != -1)
      writeBootloader ();
    stopProgramming ();
    }   // end of if entered programming mode OK


  Serial.println (F("Type 'C' when ready to continue with another chip ..."));
  while (toupper (Serial.read ()) != 'C')
    {}

  }  // end of loop
