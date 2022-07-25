#pragma once
#include <Arduino.h>

String UnsignedLong2String(unsigned long Input, boolean HideZero);
void dumpByteArray(const byte *byteArray, const unsigned long arraySize);
void hexStringToBytes(String hexString, byte *byteArray, unsigned long *byteArrayLen);
String ASCIIString2HexString(String ASCII);
String bytesToHexString(byte *byteArray, unsigned long byteArrayLen, boolean SpaceDelimiter);
String ByteAsHexString(byte Value);
String WordAsHexString(word Value, boolean LSB);
String UnsignedLongAsHexString(unsigned long Value);
String UnsignedLongAs3ByteHexString(unsigned long Value);
String Byte2Binary(byte Value);
String Word2Binary(word Value);
String DateTime2String(int year_,int month_,int day_,int hour_,int minute_,int second_,int DayOfWeek,boolean JsonFormat);
String ScheduledDay2String(byte Day);
