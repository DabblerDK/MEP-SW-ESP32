#include "NES-MEP-Tools.h"

String UnsignedLong2String(unsigned long Input, boolean HideZero)
{
  if (HideZero && (Input == 0)) {
    return ("");
  }
  return (String(Input));
}

void dumpByteArray(const byte *byteArray, const unsigned long arraySize)
{
  Serial.print("0x");
  for(unsigned long i = 0; i < arraySize; i++)
  {
    if (byteArray[i] < 0x10)
      Serial.print("0");
    Serial.print(byteArray[i], HEX);
  }
  Serial.println();
}

String ASCIIString2HexString(String ASCII)
{
  String HexString = "";
  char buffer[3];
  for(unsigned long i = 0;i<ASCII.length();i++)
  {
    sprintf(buffer,"%02X",ASCII.charAt(i));
    HexString += buffer;
  }
  return HexString;
}

// ###############################################################
// ## Source: https://forum.arduino.cc/index.php?topic=586895.0 ##
// ###############################################################
// convert a hex string such as "A489B1" into an array like [0xA4, 0x89, 0xB1].
byte nibble(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;

  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  return 0;  // Not a valid hexadecimal character
}

String bytesToHexString(byte *byteArray, unsigned long byteArrayLen, boolean SpaceDelimiter)
{
  String hexString = "";
  char Buffer[3];
  
  for(unsigned long i = 0; i < byteArrayLen; i++)
  {
    sprintf(Buffer,"%02x",byteArray[i]);
    if(SpaceDelimiter && (hexString.length() > 0))
      hexString.concat(" ");
    hexString.concat(Buffer);
  }
  return hexString;
}

void hexStringToBytes(String hexString, byte *byteArray, unsigned long *byteArrayLen)
{
  bool oddLength = 0;

  byte currentByte = 0;
  byte byteIndex = 0;

  oddLength = hexString.length() & 1;
  
  memset(byteArray, 0, hexString.length()/2);
  for(unsigned long charIndex = 0; charIndex < hexString.length(); charIndex++)
  {
    bool oddCharIndex = charIndex & 1;

    if (oddLength)
    {
      // If the length is odd
      if (oddCharIndex)
      {
        // odd characters go in high nibble
        currentByte = nibble(hexString.charAt(charIndex)) << 4;
      }
      else
      {
        // Even characters go into low nibble
        currentByte |= nibble(hexString.charAt(charIndex));
        byteArray[byteIndex++] = currentByte;
        currentByte = 0;
      }
    }
    else
    {
      // If the length is even
      if (!oddCharIndex)
      {
        // Odd characters go into the high nibble
        currentByte = nibble(hexString.charAt(charIndex)) << 4;
      }
      else
      {
        // Odd characters go into low nibble
        currentByte |= nibble(hexString.charAt(charIndex));
        byteArray[byteIndex++] = currentByte;
        currentByte = 0;
      }
    }
  }
  *byteArrayLen = byteIndex;
}

String ByteAsHexString(byte Value)
{
  char Buffer[3];
  
  sprintf(Buffer,"%02x",Value);
  return(Buffer);
}

String WordAsHexString(word Value, boolean LSB)
{
  char Buffer[5];
  if(LSB)
    sprintf(Buffer,"%02x%02x",Value % 256,Value / 256);
  else
    sprintf(Buffer,"%02x%02x",Value / 256,Value % 256);
  return(Buffer);
}

String UnsignedLongAsHexString(unsigned long Value)
{
  String Buffer = "";
  char Buffer2[3];

  for(byte i = 0;i <= 3;i++)
  {
    sprintf(Buffer2,"%02x",Value % 256);
    Value = Value / 256;
    Buffer = Buffer2 + Buffer;
  }
  return(Buffer);
}

String UnsignedLongAs3ByteHexString(unsigned long Value)
{
  return(UnsignedLongAsHexString(Value).substring(2));
}

String Byte2Binary(byte Value)
{
  char Output[11];
  sprintf(Output,"0b%c%c%c%c%c%c%c%c", (((Value & 0x80) > 0) ? '1' : '0'),
                                       (((Value & 0x40) > 0) ? '1' : '0'),
                                       (((Value & 0x20) > 0) ? '1' : '0'),
                                       (((Value & 0x10) > 0) ? '1' : '0'),
                                       (((Value & 0x08) > 0) ? '1' : '0'),
                                       (((Value & 0x04) > 0) ? '1' : '0'),
                                       (((Value & 0x02) > 0) ? '1' : '0'),
                                       (((Value & 0x01) > 0) ? '1' : '0'));
  return(String(Output));                                      
}

String Word2Binary(word Value)
{
  String Buffer;
  Buffer = Byte2Binary(Value % 256);
  Buffer.remove(0,2);
  return Byte2Binary(Value/256) + Buffer;
}

String DateTime2String(int year_,int month_,int day_,int hour_,int minute_,int second_,int DayOfWeek,boolean JsonFormat)
{
  const String dayStr[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
  char buffer[40];

  if(JsonFormat)
    sprintf(buffer,"20%02d-%02d-%02dT%02d:%02d:%02d.000Z",year_,month_,day_,hour_,minute_,second_);
  else
    sprintf(buffer,"20%02d-%02d-%02d (%s) @ %02d:%02d:%02d",year_,month_,day_,dayStr[DayOfWeek-1],hour_,minute_,second_);
    
  return String(buffer);
}

String ScheduledDay2String(byte Day)
{
  const String dayStr[8] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Disabled"};
  if(Day<31) {
    return String(Day);
  }
  if(Day<40) {
    return dayStr[Day-32];
  }
  return "Unknown value ("+String(Day)+")";
}
