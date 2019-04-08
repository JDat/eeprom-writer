#ifndef _eeprom_pindefine_H_
#define _eeprom_pindefine_H_

#include <Arduino.h>
// IC addr1 0x24
// IC addr2 0x26
#define IC1         4
#define IC2         6

#define addrpin0    11
#define addrpin1    10
#define addrpin2    9
#define addrpin3    8
#define addrpin4    7
#define addrpin5    6
#define addrpin6    5
#define addrpin7    4

#define addrpin8    10
#define addrpin9    9
#define addrpin10   6
#define addrpin11   8
#define addrpin12   3
#define addrpin13   11
#define addrpin14   12
#define addrpin15   2

#define addrpin16   1
#define addrpin17   13
#define addrpin18   0

#define datapin0    12
#define datapin1    13
#define datapin2    14
#define datapin3    0
#define datapin4    1
#define datapin5    2
#define datapin6    3
#define datapin7    4

#define cepin       5
#define oepin       7
#define wepin       14



uint8_t addrArray[]=  {addrpin0,addrpin1,addrpin2,addrpin3,
                      addrpin4,addrpin5,addrpin6,addrpin7,
                      addrpin8,addrpin9,addrpin10,addrpin11,
                      addrpin12,addrpin13,addrpin14,addrpin15,
                      addrpin16,addrpin17,addrpin18};

uint8_t addrRoute[]=  {IC2,IC2,IC2,IC2,
                      IC2,IC2,IC2,IC2,
                      IC1,IC1,IC1,IC1,
                      IC2,IC1,IC1,IC2,
                      IC2,IC1,IC2};

uint8_t dataArray[]=  {datapin0,datapin1,datapin2,datapin3,
                      datapin4,datapin5,datapin6,datapin7};

uint8_t dataRoute[]=  {IC2,IC2,IC2,IC1,
                      IC1,IC1,IC1,IC1};

#endif
