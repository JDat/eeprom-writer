/*
// EEPROM Programmer with I2C I/O expanders
//
// 
// Base and core Written by K Adcock.
// http://danceswithferrets.org/geekblog/?page_id=903
//
// I2C modifications by JDat
// Support for 32 pin (up to 512k * 8 ) Flash
//
// This software presents a 115200-8N1 serial port.
//
// R[hex address]                         - reads 16 bytes of data from the EEPROM
// W[hex address]:[data in two-char hex]  - writes up to 16 bytes of data to the EEPROM
// P                                      - set write-protection bit (Atmels only, AFAIK)
// U                                      - clear write-protection bit (ditto)
// V                                      - prints the version string
// E                                      - Erase all chip contents to 0xFF
//
// Any data read from the EEPROM will have a CRC checksum appended to it (separated by a comma).
// If a string of data is sent with an optional checksum, then this will be checked
// before anything is written.
//

*/
//#include <avr/pgmspace.h>
//#include <Wire.h>
#include "pindefine.h"
#include "Adafruit_MCP23017.h"

const char hex[] =
{
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

const char version_string[] = {"EEPROM Version=0.02"};

static const int kPin_WaitingForInput  = LED_BUILTIN;

byte g_cmd[80]; // strings received from the controller will go in here
static const int kMaxBufferSize = 16;
byte buffer[kMaxBufferSize];

uint16_t ic1out=0;
uint16_t ic2out=0;

Adafruit_MCP23017 mcp1;
Adafruit_MCP23017 mcp2;

void setAddrOut(){
  for (uint8_t i=0;i<=18;i++){
    if (addrRoute[i]==IC1){
      mcp1.pinMode(addrArray[i],OUTPUT);
      
    } else if (addrRoute[i]==IC2){
      mcp2.pinMode(addrArray[i],OUTPUT);
    }
  }
}

void setup()
{
  Serial.begin(115200);

  //Wire.setClock(400000);
  Wire.setClock(1000000);
  //Wire.setClock(3400000);

  //Wire.begin();
  mcp1.begin(IC1);
  mcp2.begin(IC2);

  
  pinMode(kPin_WaitingForInput, OUTPUT); digitalWrite(kPin_WaitingForInput, HIGH);

  // address lines are ALWAYS outputs
  setAddrOut();
  
  // control lines are ALWAYS outputs
  //pinMode(kPin_nCE, OUTPUT); digitalWrite(kPin_nCE, LOW); // might as well keep the chip enabled ALL the time
  mcp1.pinMode(cepin,OUTPUT); bitClear(ic1out,cepin);
  mcp1.writeGPIOAB(ic1out);
  //pinMode(kPin_nOE, OUTPUT); digitalWrite(kPin_nOE, HIGH);
  mcp1.pinMode(oepin,OUTPUT); bitSet(ic1out,oepin);
  mcp1.writeGPIOAB(ic1out);

  //pinMode(kPin_nWE, OUTPUT); digitalWrite(kPin_nWE, HIGH); // not writing
  mcp1.pinMode(wepin,OUTPUT); bitSet(ic1out,wepin);
  mcp1.writeGPIOAB(ic1out);

  SetDataLinesAsInputs();
  SetAddress(0);

  //Serial.println("chip Erase start");
  //void EraseChip();
  //delay(300);
  //Serial.println("Erase complete");
//  while (-1){
//    
//  }
}

void loop()
{
    digitalWrite(kPin_WaitingForInput, HIGH);
    ReadString();
    digitalWrite(kPin_WaitingForInput, LOW);

    switch (g_cmd[0])
    {
      case 'V': Serial.println(version_string); break;
      case 'P': SetSDPState(true); break;
      case 'U': SetSDPState(false); break;
      case 'R': ReadEEPROM(); break;
      case 'W': WriteEEPROM(); break;
      case 'E': eraseChip(); break;
      case 0: break; // empty string. Don't mind ignoring this.
      default: Serial.println("ERR Unrecognised command"); break;
    }
}

void ReadEEPROM() // R<address>  - read kMaxBufferSize bytes from EEPROM, beginning at <address> (in hex)
{
  if (g_cmd[1] == 0)
  {
    Serial.println("ERR");
    return;
  }

  // decode ASCII representation of address (in hex) into an actual value
  long addr = 0;
  int x = 1;
  while (x < 6 && g_cmd[x] != 0)
  {
    addr = addr << 4;
    addr |= HexToVal(g_cmd[x++]);
  }

  //Serial.println("Read addr: " + String(addr,HEX) );
  //digitalWrite(kPin_nWE, HIGH); // disables write
  bitSet(ic1out,wepin); mcp1.writeGPIOAB(ic1out);

  SetDataLinesAsInputs();
  //digitalWrite(kPin_nOE, LOW); // makes the EEPROM output the byte
  bitClear(ic1out,oepin); mcp1.writeGPIOAB(ic1out);

  ReadEEPROMIntoBuffer(addr, kMaxBufferSize);

  // now print the results, starting with the address as hex ...
  Serial.print(hex[ (addr & 0xF0000) >> 16 ]);
  Serial.print(hex[ (addr & 0x0F000) >> 12 ]);
  Serial.print(hex[ (addr & 0x00F00) >> 8  ]);
  Serial.print(hex[ (addr & 0x000F0) >> 4  ]);
  Serial.print(hex[ (addr & 0x0000F)       ]);
  Serial.print(":");
  PrintBuffer(kMaxBufferSize);

  Serial.println("OK");

  //digitalWrite(kPin_nOE, HIGH); // stops the EEPROM outputting the byte
  bitSet(ic1out,oepin); mcp1.writeGPIOAB(ic1out);
}

void WriteEEPROM() // W<four byte hex address>:<data in hex, two characters per byte, max of 16 bytes per line>
{
  if (g_cmd[1] == 0)
  {
    Serial.println("ERR");
    return;
  }

  long addr = 0;
  int x = 1;
  while (g_cmd[x] != ':' && g_cmd[x] != 0)
  {
    addr = addr << 4;
    addr |= HexToVal(g_cmd[x]);
    ++x;
  }

  // g_cmd[x] should now be a :
  if (g_cmd[x] != ':')
  {
    Serial.println("ERR");
    return;
  }

  x++; // now points to beginning of data
  uint8_t iBufferUsed = 0;
  while (g_cmd[x] && g_cmd[x+1] && iBufferUsed < kMaxBufferSize && g_cmd[x] != ',')
  {
    uint8_t c = (HexToVal(g_cmd[x]) << 4) | HexToVal(g_cmd[x+1]);
    buffer[iBufferUsed++] = c;
    x += 2;
  }

  // if we're pointing to a comma, then the optional checksum has been provided!
  if (g_cmd[x] == ',' && g_cmd[x+1] && g_cmd[x+2])
  {
    byte checksum = (HexToVal(g_cmd[x+1]) << 4) | HexToVal(g_cmd[x+2]);

    byte our_checksum = CalcBufferChecksum(iBufferUsed);

    if (our_checksum != checksum)
    {
      // checksum fail!
      iBufferUsed = -1;
      Serial.print("ERR ");
      Serial.print(checksum, HEX);
      Serial.print(" ");
      Serial.print(our_checksum, HEX);
      Serial.println("");
      return;
    }
  }

  // buffer should now contains some data
  if (iBufferUsed > 0)
  {
    WriteBufferToEEPROM(addr, iBufferUsed);
  }

  if (iBufferUsed > -1)
  {
    Serial.println("OK");
  }
}

void eraseChip()
{
  //digitalWrite(kPin_LED_Red, HIGH);

  //digitalWrite(kPin_nWE, HIGH); // disables write
  bitSet(ic1out,wepin); mcp1.writeGPIOAB(ic1out);
  //digitalWrite(kPin_nOE, LOW); // makes the EEPROM output the byte
  bitClear(ic1out,oepin); mcp1.writeGPIOAB(ic1out);
  //SetDataLinesAsInputs();

  //byte bytezero = ReadByteFrom(0);

  //digitalWrite(kPin_nOE, HIGH); // stop EEPROM from outputting byte
  bitSet(ic1out,oepin); mcp1.writeGPIOAB(ic1out);
  //digitalWrite(kPin_nCE, HIGH);
  bitClear(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
  
  SetDataLinesAsOutputs();

    WriteByteTo(0x5555, 0xAA);
    WriteByteTo(0x2AAA, 0x55);
    WriteByteTo(0x5555, 0x80);
    WriteByteTo(0x5555, 0xAA);
    WriteByteTo(0x2AAA, 0x55);
    WriteByteTo(0x5555, 0x10);

  //WriteByteTo(0x0000, bytezero); // this "dummy" write is required so that the EEPROM will flush its buffer of commands.

  //digitalWrite(kPin_nCE, LOW); // return to on by default for the rest of the code
  //bitClear(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
  
  bitSet(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
  delay(300);
  Serial.println("OK");
}

// Important note: the EEPROM needs to have data written to it immediately after sending the "unprotect" command, so that the buffer is flushed.
// So we read byte 0 from the EEPROM first, then use that as the dummy write afterwards.
// It wouldn't matter if this facility was used immediately before writing an EEPROM anyway ... but it DOES matter if you use this option
// in isolation (unprotecting the EEPROM but not changing it).

void SetSDPState(bool bWriteProtect)
{
  //digitalWrite(kPin_LED_Red, HIGH);

  //digitalWrite(kPin_nWE, HIGH); // disables write
  bitSet(ic1out,wepin); mcp1.writeGPIOAB(ic1out);
  //digitalWrite(kPin_nOE, LOW); // makes the EEPROM output the byte
  bitClear(ic1out,oepin); mcp1.writeGPIOAB(ic1out);
  SetDataLinesAsInputs();

  byte bytezero = ReadByteFrom(0);

  //digitalWrite(kPin_nOE, HIGH); // stop EEPROM from outputting byte
  bitSet(ic1out,oepin); mcp1.writeGPIOAB(ic1out);
  //digitalWrite(kPin_nCE, HIGH);
  bitSet(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
  
  SetDataLinesAsOutputs();

  // This is for the AT28C256
  if (bWriteProtect)
  {
    WriteByteTo(0x5555, 0xAA);
    WriteByteTo(0x2AAA, 0x55);
    WriteByteTo(0x5555, 0xA0);
  }
  else
  {

    WriteByteTo(0x5555, 0xAA);
    WriteByteTo(0x2AAA, 0x55);
    WriteByteTo(0x5555, 0x80);
    WriteByteTo(0x5555, 0xAA);
    WriteByteTo(0x2AAA, 0x55);
    WriteByteTo(0x5555, 0x20);

    //WriteByteTo(0x5555, 0xAA);
    //WriteByteTo(0x2AAA, 0x55);
    //WriteByteTo(0x5555, 0xA0);

    //WriteByteTo(0x5555, 0xAA);

    //WriteByteTo(0x2AAA, 0x55);

    //WriteByteTo(0x5555, 0x20);

  }

  //WriteByteTo(0x0000, bytezero); // this "dummy" write is required so that the EEPROM will flush its buffer of commands.

  //digitalWrite(kPin_nCE, LOW); // return to on by default for the rest of the code
  //bitClear(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
  
  //bitSet(ic1out,cepin); mcp1.writeGPIOAB(ic1out);

  //Serial.print("OK SDP ");
  //if (bWriteProtect)
  //{
    //Serial.println("enabled");
  //}
  //else
  //{
    //Serial.println("disabled");
  //}
}

// ----------------------------------------------------------------------------------------

void ReadEEPROMIntoBuffer(long addr, int size)
{
  //digitalWrite(kPin_nWE, HIGH);
  bitSet(ic1out,wepin); mcp1.writeGPIOAB(ic1out);
  SetDataLinesAsInputs();
  //digitalWrite(kPin_nOE, LOW);
  bitClear(ic1out,oepin); mcp1.writeGPIOAB(ic1out);

  for (int x = 0; x < size; ++x)
  {
    buffer[x] = ReadByteFrom(addr + x);
  }

  //digitalWrite(kPin_nOE, HIGH);
  bitSet(ic1out,oepin); mcp1.writeGPIOAB(ic1out);
}

void WriteBufferToEEPROM(long addr, int size)
{
  //bitSet(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
  
  //digitalWrite(kPin_nOE, HIGH); // stop EEPROM from outputting byte
  bitSet(ic1out,oepin); mcp1.writeGPIOAB(ic1out);
  //digitalWrite(kPin_nWE, HIGH); // disables write
  bitSet(ic1out,wepin); mcp1.writeGPIOAB(ic1out);
  
  SetDataLinesAsOutputs();

  bitClear(ic1out,cepin); mcp1.writeGPIOAB(ic1out);      
  
  for (uint8_t x = 0; x < size; ++x)
  {
    //SetSDPState(true);
    WriteByteTo(0x5555, 0xAA);
    WriteByteTo(0x2AAA, 0x55);
    WriteByteTo(0x5555, 0xA0);
    WriteByteTo(addr + x, buffer[x]);
    for (uint8_t i=0;i<=2;i++){
      //WriteByteTo(addr + x, buffer[x]);
      bitClear(ic1out,wepin); mcp1.writeGPIOAB(ic1out);
//      bitClear(ic1out,cepin); mcp1.writeGPIOAB(ic1out);      
      bitSet(ic1out,wepin); mcp1.writeGPIOAB(ic1out);
//      bitSet(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
      
    }
  }
  bitSet(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
}
//W00000:112233445566778899AABBCCDDEEFF00
// ----------------------------------------------------------------------------------------

// this function assumes that data lines have already been set as INPUTS, and that
// nOE is set LOW.
byte ReadByteFrom(long addr)
{
  SetAddress(addr);
  //digitalWrite(kPin_nCE, LOW);
  bitClear(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
  byte b = ReadData();
  //digitalWrite(kPin_nCE, HIGH);
  bitSet(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
  return b;
}

// this function assumes that data lines have already been set as OUTPUTS, and that
// nOE is set HIGH.
void WriteByteTo(long addr, byte b)
{
  SetAddress(addr);
  SetData(b);

  //digitalWrite(kPin_nCE, LOW);
  //bitClear(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
  //digitalWrite(kPin_nWE, LOW); // enable write
  bitClear(ic1out,wepin); mcp1.writeGPIOAB(ic1out);

  //digitalWrite(kPin_nWE, HIGH); // disable write
  bitSet(ic1out,wepin); mcp1.writeGPIOAB(ic1out);  
  //digitalWrite(kPin_nCE, HIGH);
  //bitSet(ic1out,cepin); mcp1.writeGPIOAB(ic1out);
}

// ----------------------------------------------------------------------------------------

void SetDataLinesAsInputs()
{
  for (uint8_t i=0;i<=7;i++){
    if (dataRoute[i]==IC1){
      mcp1.pinMode(dataArray[i],INPUT);
    } else if (dataRoute[i]==IC2){
      mcp2.pinMode(dataArray[i],INPUT);
    }
  }
}

void SetDataLinesAsOutputs()
{
  for (uint8_t i=0;i<=7;i++){
    if (dataRoute[i]==IC1){
      mcp1.pinMode(dataArray[i],OUTPUT);
    } else if (dataRoute[i]==IC2){
      mcp2.pinMode(dataArray[i],OUTPUT);
    }
  }
}

void SetAddress(long a)
{
  for (uint8_t i=0;i<=18;i++){
    if (addrRoute[i]==IC1){
      if ( a & long(round(pow(2,i))) ){
        bitSet(ic1out,addrArray[i]);
      } else {
        bitClear(ic1out,addrArray[i]);
      }
    } else if (addrRoute[i]==IC2){
      if ( a & long(round(pow(2,i))) ) {
        bitSet(ic2out,addrArray[i]);
      } else {
        bitClear(ic2out,addrArray[i]);
      }
    }
  }

  mcp1.writeGPIOAB(ic1out);
  mcp2.writeGPIOAB(ic2out);
}

// this function assumes that data lines have already been set as OUTPUTS.
void SetData(byte b)
{
  for (uint8_t i=0;i<=7;i++){
    if (dataRoute[i]==IC1){
      if ( b & (1 << i) ){
        bitSet(ic1out,dataArray[i]);
      } else {
        bitClear(ic1out,dataArray[i]);
      }
    } else if (dataRoute[i]==IC2){
      if ( b & (1 << i) ) {
        bitSet(ic2out,dataArray[i]);
      } else {
        bitClear(ic2out,dataArray[i]);
      }
    }
  }
  
  mcp1.writeGPIOAB(ic1out);
  mcp2.writeGPIOAB(ic2out);
}

// this function assumes that data lines have already been set as INPUTS.
byte ReadData()
{
  byte b = 0;

  for (uint8_t i=0;i<=7;i++) {
    if (dataRoute[i]==IC1){
      if ( mcp1.digitalRead(dataArray[i]) ) {
        bitSet(b,i);
      }
    } else if (dataRoute[i]==IC2){
      if ( mcp2.digitalRead(dataArray[i]) ) {
        bitSet(b,i);
      }      
    }
  }
  return(b);
}

// ----------------------------------------------------------------------------------------

void PrintBuffer(int size)
{
  uint8_t chk = 0;

  for (uint8_t x = 0; x < size; ++x)
  {
    Serial.print(hex[ (buffer[x] & 0xF0) >> 4 ]);
    Serial.print(hex[ (buffer[x] & 0x0F)      ]);

    chk = chk ^ buffer[x];
  }

  Serial.print(",");
  Serial.print(hex[ (chk & 0xF0) >> 4 ]);
  Serial.print(hex[ (chk & 0x0F)      ]);
  Serial.println("");
}

void ReadString()
{
  int i = 0;
  byte c;

  g_cmd[0] = 0;
  do
  {
    if (Serial.available())
    {
      c = Serial.read();
      if (c > 31)
      {
        g_cmd[i++] = c;
        g_cmd[i] = 0;
      }
    }
  }
  while (c != 10);
}

uint8_t CalcBufferChecksum(uint8_t size)
{
  uint8_t chk = 0;

  for (uint8_t x = 0; x < size; ++x)
  {
    chk = chk ^  buffer[x];
  }

  return(chk);
}

// converts one character of a HEX value into its absolute value (nibble)
byte HexToVal(byte b)
{
  if (b >= '0' && b <= '9') return(b - '0');
  if (b >= 'A' && b <= 'F') return((b - 'A') + 10);
  if (b >= 'a' && b <= 'f') return((b - 'a') + 10);
  return(0);
}

