#include "NES-MEP-Tools.h"
#include "NES-MEP.h"

unsigned long SequenceNo = 0;
byte ProcedureSequenceNo = 5; // Must be 5-9 for MEP
boolean AlertSeqenceActive = false;

MeterConfigStruct MeterConfig;
ConsumptionDataStruct ConsumptionData;
MeterInfoStruct MeterInfo;

extern MEPQueueStruct MEPQueue[];

// ####################################################
// ## Source: "078-0372-01BD_MEP_Client_DC OSGP.pdf" ##
// ####################################################
/*
// This iterates over the key 1.5 times. This is intended to
// make it more difficult to break the key by examining the
// before and after plain text. Macro assumes length is even!
*/

/*****************************************************************
Function: Digest
Parameters:
          apduBytes -> bytes of <apduSize>
          key -> <keyLen> bytes of key value. Typically 128 bits.
          digestValueOut -> 8 bytes of key result. Zero before first 
          call.
          pState -> 2 bytes of state value. Zero to start.
Returns:  None
Purpose:  To compute the digest based on the authorization key.
Comments: This routine works segment by segment for long messages.
******************************************************************/

#define DIGEBT_KEY_ITERATIONS(len) (len==6 ? len : (len + len/2))

void Digest(const byte *apduBytes, unsigned long apduSize, const byte *key, byte *digestValueOut, DState *pState, byte bEnd, int keyLen)
{
  unsigned long idx = 0;
  byte m, n, v, keyByte, last;
  byte j = pState->j;
  byte i = pState->i;
  byte iter = DIGEBT_KEY_ITERATIONS(keyLen);

  do
  {
    /* for each byte of the key (repeating the process a second time for the first half of the key) */
    do
    {
      keyByte = key[i%keyLen] >> j;
      last = digestValueOut[(8-j) & 0x7];
      v = 7-j;
      
      if (apduSize != 0 || bEnd == 0)
      {
        /* for each bit of the key byte */
        do
        {
          /* zero pad the message to use up any left over key bits */
          if (apduSize)
          {
            /* use bytes is address order not reverse */
            m = apduBytes[idx++];
            apduSize--;
          }
          else
          {
            if (bEnd)
            { 
              /* if this is the end, finish the key bits */ 
              m = 0;
            }
            else
            { 
              /* not the end, so return leaving state */
              j = 7-v;
              goto done;
            }
          }
          n = ~(digestValueOut[v] + v);
          last = digestValueOut[v] = last + m + ((keyByte & 1) ?
                                                 ((n << 1) + (n >> 7)) :
                                                 -((n >> 1) + (n << 7)));
          keyByte >>= 1;
        } while (v--);
      }
      else
      {
        /* Faster version. This case assumes most processing is done with * bEnd == true and no APDU data */
        do
        {
          n = ~(digestValueOut[v] + v);
          last = digestValueOut[v] = last + ((keyByte & 1) ?
                                             ((n << 1) + (n >> 7)) :
                                             -((n >> 1) + (n << 7)));
          keyByte >>= 1;
        } while (v--); 
      }
      j = 0;
    } while (++i < iter);
    i = 0;
  } while (apduSize > 0);

done:
  /* return State */
  pState->j = j;
  pState->i = i;
}

String AddDigest(String request, String key)
{
  byte apduByteArray[MaxMEPRequestLength] = {0};
  unsigned long apduByteArrayLen = 0;
  byte keyByteArray[16] = {0};
  unsigned long keyByteArrayLen = 0;
  byte digestByteArray[8] = {0};
  DState pState;

  hexStringToBytes(request.substring(4), apduByteArray, &apduByteArrayLen);
  hexStringToBytes(ASCIIString2HexString(key).substring(0,32), keyByteArray, &keyByteArrayLen);
  memset(&pState,0,sizeof(pState));
  memset(digestByteArray, 0, 8);
  Digest(apduByteArray, apduByteArrayLen, keyByteArray, digestByteArray, &pState, true, keyByteArrayLen);
  return request+bytesToHexString(digestByteArray,8,false);
}

unsigned long GetPackageLength(byte *Package)
{
  return(Package[0]*256+Package[1]+2);
}

boolean PackageIsValid(byte *Package, unsigned long PackageLength)
{
  if(PackageLength < 2)
  {
    return(false);
  }
     
  if(GetPackageLength(Package) > PackageLength)
  {
    return(false);
  }
  
  return(true);
}

boolean DigestIsValidOnResponsePackage(byte *RequestPackage, unsigned long RequestPackageLength, byte *ResponsePackage, unsigned long ResponsePackageLength)
{
  if(!PackageIsValid(RequestPackage,RequestPackageLength))
  {
    return(false);
  }
  
  if(!PackageIsValid(ResponsePackage,ResponsePackageLength))
  {
    return(false);
  }
  // TO-DO: Validate response digest!!!
  Serial.println("TO-DO: Validate response digest!!!");
  return(true);
}

void IncreaseMEPQueueIndex(byte *Index)
{
  (*Index)++;
  if (*Index >= MaxMEPBuffer)
  {
    *Index = 0;
  }  
}

void queueRequest(String request,String key,MEPQueueStruct *MEPQueue,byte *MEPQueueNextIndex,NextActionEnum NextAction)
{
  request.replace(" ","");
  request = WordAsHexString(request.length()/2+4+8,false) + request + // Add sequence no. (4) and digest (8)
            UnsignedLongAsHexString(SequenceNo);
  SequenceNo++;
  request = AddDigest(request,key); 
  MEPQueue[*MEPQueueNextIndex].Millis = millis();
  hexStringToBytes(request, MEPQueue[*MEPQueueNextIndex].Request, &MEPQueue[*MEPQueueNextIndex].RequestLength);
  MEPQueue[*MEPQueueNextIndex].ReplyLength = 0;
  MEPQueue[*MEPQueueNextIndex].NextAction = NextAction;
  MEPQueue[*MEPQueueNextIndex].SendAttempts = 0;
  Serial.printf("Queued request at index %i\r\n",*MEPQueueNextIndex);
  IncreaseMEPQueueIndex(MEPQueueNextIndex);
}

void IncreaseProcedureSequenceNo(byte *SequenceNo)
{
  (*SequenceNo)++;
  if(*SequenceNo>9)
  {
    *SequenceNo = 5;
  }
}

void queueProcedureRequest(word procedureNo,String parameters,String key,MEPQueueStruct *MEPQueue,byte *MEPQueueNextIndex,NextActionEnum NextAction)
{
  String request;
  
  parameters.replace(" ","");
  request = "400007" +                                       // Full write to BT07 / "Procedure Initialization Table"
            WordAsHexString(parameters.length()/2+3,false) + // Add procedure no. (2) and procedure sequence no (1)
            WordAsHexString(procedureNo,true) +
            ByteAsHexString(ProcedureSequenceNo) +
            parameters;
  Serial.printf("Handling Procedure Request at index %i\r\n",*MEPQueueNextIndex);
  queueRequest(request,key,MEPQueue,MEPQueueNextIndex,NextAction);
  IncreaseProcedureSequenceNo(&ProcedureSequenceNo);
}

void queueResponseWithNoRequest(byte *Response,unsigned long ResponseLength,MEPQueueStruct *MEPQueue,byte *MEPQueueNextIndex)
{
  MEPQueue[*MEPQueueNextIndex].Millis = millis();
  MEPQueue[*MEPQueueNextIndex].RequestLength = 0;
  memcpy(MEPQueue[*MEPQueueNextIndex].Reply,Response,ResponseLength);
  MEPQueue[*MEPQueueNextIndex].ReplyLength = ResponseLength; 
  MEPQueue[*MEPQueueNextIndex].NextAction = None;
  MEPQueue[*MEPQueueNextIndex].SendAttempts = 0;
  Serial.printf("Queued response at index %i\r\n",*MEPQueueNextIndex);
  IncreaseMEPQueueIndex(MEPQueueNextIndex);
}

boolean Decode0x0001(unsigned long ReplyLength,byte Reply[],char *Manufacturer,char *Model,byte *MainFirmwareVersionNumber,byte *FirmwareRevisionNumber)
{
  if(ReplyLength <= 15) {
    return false;
  }
  memcpy(Manufacturer,Reply,4);
  Manufacturer[4] = 0;
  memcpy(Model,Reply+4,8);
  Model[8] = 0;
  *MainFirmwareVersionNumber = Reply[14];
  *FirmwareRevisionNumber = Reply[15];
  return true;
}

boolean Decode0x0015(unsigned long ReplyLength,byte Reply[],byte *BT21_0,byte *BT21_1,byte *BT21_2,byte *BT21_3,byte *BT21_4,byte *BT21_5,byte *BT21_6,byte *BT21_7,byte *BT21_8,byte *BT21_9)
{
  if(ReplyLength <= 9) {
    return false;
  }
  *BT21_0 = Reply[0];
  *BT21_1 = Reply[1];
  *BT21_2 = Reply[2];
  *BT21_3 = Reply[3];
  *BT21_4 = Reply[4];
  *BT21_5 = Reply[5];
  *BT21_6 = Reply[6];
  *BT21_7 = Reply[8];
  *BT21_8 = Reply[8];
  *BT21_9 = Reply[9];
  return true;
}

boolean Decode0x0017(unsigned long ReplyLength,byte Reply[],long *FwdActiveWhL1L2L3,long *RevActiveWhL1L2L3)
{
  byte A = ((MeterConfig.BT21_0 & 0b00000100) == 0b00000100) ? 1 : 0; // BT21_0_2
  byte B = A + 4 * MeterConfig.BT21_3;
  byte DmdRcd = MeterConfig.BT21_6 * 5 + 4 + 4 + MeterConfig.BT21_6 * 4; // 5 is STIME_DATE = 5 bytes. 4, 4 and 4 are size of NI_FMAT2 = 32 bit signed integer
  byte CoinRcd = MeterConfig.BT21_6 * 4; // 4 is size of NI_FMAT2 = 32 bit signed integer
  byte C = B + MeterConfig.BT21_4 + DmdRcd;
  byte D = C + MeterConfig.BT21_5 + CoinRcd;
  if(ReplyLength <= A+4+4) {
    return false;
  }
  // All these are type NI_FMAT1 = 32 bit signed integer
  memcpy(FwdActiveWhL1L2L3, Reply + A, 4);
  memcpy(RevActiveWhL1L2L3, Reply + A+4, 4);
  return true;
}

boolean Decode0x001C(unsigned long ReplyLength,byte Reply[],long *FwdActiveWL1L2L3,long *RevActiveWL1L2L3,long *ImportReactiveVArL1L2L3,long *ExportReactiveVArL1L2L3,
                     long *RMSCurrentmAL1,long *RMSCurrentmAL2,long *RMSCurrentmAL3,long *RMSVoltagetmVL1,long *RMSVoltagetmVL2,long *RMSVoltagetmVL3,
                     long *PowerFactorL1,long *FrequencymHz,long *VAL1L2L3,long *PowerFactorL2,long *PowerFactorL3,long *FwdActiveWL1,long *FwdActiveWL2,long *FwdActiveWL3,
                     long *RevActiveWL1,long *RevActiveWL2,long *RevActiveWL3,long *RMSVoltagemVL1Cont,long *RMSVoltagemVL2Cont,long *RMSVoltagemVL3Cont,
                     long *RMSVoltagemVL1Avg,long *RMSVoltagemVL2Avg,long *RMSVoltagemVL3Avg,long *AverageFwdActiveWL1L2L3,long *AverageRevActiveWL1L2L3,
                     long *AverageFwdActiveWL1,long *AverageFwdActiveWL2,long *AverageFwdActiveWL3,long *AverageRevActiveWL1,long *AverageRevActiveWL2,long *AverageRevActiveWL3)

{
  byte A = ((MeterConfig.BT21_0 & 0b01000000) == 0b01000000) ? 3 : 0; // BT21_0_6
  byte Offset = (A + 4) * MeterConfig.BT21_8;
  if(ReplyLength <= Offset + 54*4) {
    return false;
  }
  // All these are type NI_FMAT1 = 32 bit signed integer
  memcpy(FwdActiveWL1L2L3,        Reply + Offset +  0*4, 4);
  memcpy(RevActiveWL1L2L3,        Reply + Offset +  1*4, 4);
  memcpy(ImportReactiveVArL1L2L3, Reply + Offset +  2*4, 4);
  memcpy(ExportReactiveVArL1L2L3, Reply + Offset +  3*4, 4);
  memcpy(RMSCurrentmAL1,          Reply + Offset +  4*4, 4);
  memcpy(RMSCurrentmAL2,          Reply + Offset +  5*4, 4);
  memcpy(RMSCurrentmAL3,          Reply + Offset +  6*4, 4);
  memcpy(RMSVoltagetmVL1,         Reply + Offset +  7*4, 4);
  memcpy(RMSVoltagetmVL2,         Reply + Offset +  8*4, 4);
  memcpy(RMSVoltagetmVL3,         Reply + Offset +  9*4, 4);
  memcpy(PowerFactorL1,           Reply + Offset + 10*4, 4);  
  memcpy(FrequencymHz,            Reply + Offset + 11*4, 4);  
  memcpy(VAL1L2L3,                Reply + Offset + 12*4, 4);  
  memcpy(PowerFactorL2,           Reply + Offset + 13*4, 4);  
  memcpy(PowerFactorL3,           Reply + Offset + 14*4, 4);  
  memcpy(FwdActiveWL1,            Reply + Offset + 26*4, 4);  
  memcpy(FwdActiveWL2,            Reply + Offset + 27*4, 4);  
  memcpy(FwdActiveWL3,            Reply + Offset + 28*4, 4);
  memcpy(RevActiveWL1,            Reply + Offset + 29*4, 4);  
  memcpy(RevActiveWL2,            Reply + Offset + 30*4, 4);  
  memcpy(RevActiveWL3,            Reply + Offset + 31*4, 4);  
  memcpy(RMSVoltagemVL1Cont,      Reply + Offset + 41*4, 4);  
  memcpy(RMSVoltagemVL2Cont,      Reply + Offset + 42*4, 4);  
  memcpy(RMSVoltagemVL3Cont,      Reply + Offset + 43*4, 4);  
  memcpy(RMSVoltagemVL1Avg,       Reply + Offset + 44*4, 4);  
  memcpy(RMSVoltagemVL2Avg,       Reply + Offset + 45*4, 4);  
  memcpy(RMSVoltagemVL3Avg,       Reply + Offset + 46*4, 4);
  memcpy(AverageFwdActiveWL1L2L3, Reply + Offset + 47*4, 4);  
  memcpy(AverageRevActiveWL1L2L3, Reply + Offset + 48*4, 4);  
  memcpy(AverageFwdActiveWL1,     Reply + Offset + 49*4, 4);  
  memcpy(AverageFwdActiveWL2,     Reply + Offset + 50*4, 4);  
  memcpy(AverageFwdActiveWL3,     Reply + Offset + 51*4, 4);  
  memcpy(AverageRevActiveWL1,     Reply + Offset + 52*4, 4);  
  memcpy(AverageRevActiveWL2,     Reply + Offset + 53*4, 4);  
  memcpy(AverageRevActiveWL3,     Reply + Offset + 54*4, 4);  
  return true;
}

boolean Decode0x0034(unsigned long ReplyLength,byte Reply[],byte *year_,byte *month_,byte *day_,byte *hour_,byte *minute_,byte *second_,byte *DayOfWeek,boolean *DST,boolean *GMT,boolean *TZ,boolean *DSTApp)
{
  if(ReplyLength <= 6) {
    return false;
  }
  *year_     = Reply[0];
  *month_    = Reply[1];
  *day_      = Reply[2];
  *hour_     = Reply[3];
  *minute_   = Reply[4];
  *second_   = Reply[5];
  *DayOfWeek = (Reply[6] & 0b00000111);
  *DST       = (Reply[6] & 0b00001000);
  *GMT       = (Reply[6] & 0b00010000);
  *TZ        = (Reply[6] & 0b00100000);
  *DSTApp    = (Reply[6] & 0b01000000);
  return true;
}

boolean Decode0x0803(unsigned long ReplyLength,byte Reply[],char *UtilitySerialNumber,word *ImageCRC)
{
  if(ReplyLength <= 138) {
    return false;
  }
  memcpy(UtilitySerialNumber,Reply,30);
  UtilitySerialNumber[30] = 0;
  byte i = 29;
  while((i > 0) && (UtilitySerialNumber[i] == 0x20)) {
    UtilitySerialNumber[i] = 0;
    i--;
  }
  *ImageCRC = Reply[137]*256+Reply[138];
  return true;
}

boolean Decode0x080B(unsigned long ReplyLength,byte Reply[],byte *ET11_0,byte *ET11_1,byte *ET11_2,byte *ET11_3,byte *ET11_4,word *ET11_5,word *ET11_7,word *ET11_9,byte *ET11_11,
                     byte *ET11_12,byte *ET11_13,byte *ET11_18,byte *ET11_19,byte *ET11_20,byte *ET11_21,byte *ET11_22,byte *ET11_23,byte *ET11_24,byte *ET11_25,byte *ET11_26,byte *ET11_27,word *ET11_28,
                     word *ET11_30)
{
  if(ReplyLength <= 31) {
    return false;
  }
  *ET11_0 = Reply[0];
  *ET11_1 = Reply[1];
  *ET11_2 = Reply[2];
  *ET11_3 = Reply[3];
  *ET11_4 = Reply[4];
  *ET11_5 = Reply[5]*256 + Reply[6];
  *ET11_7 = Reply[7]*256 + Reply[8];
  *ET11_9 = Reply[9]*256 + Reply[10];
  *ET11_11 = Reply[11];
  *ET11_12 = Reply[12];
  *ET11_13 = Reply[13];
  *ET11_18 = Reply[18];
  *ET11_19 = Reply[19];
  *ET11_20 = Reply[20];
  *ET11_21 = Reply[21];
  *ET11_22 = Reply[22];
  *ET11_23 = Reply[23];
  *ET11_24 = Reply[24];
  *ET11_25 = Reply[25];
  *ET11_26 = Reply[26];
  *ET11_27 = Reply[27];
  *ET11_28 = Reply[28]*256 + Reply[29];
  *ET11_30 = Reply[30]*256 + Reply[31];
  return true;
}

boolean Decode0x080D(unsigned long ReplyLength,byte Reply[],ET13_MBusInfoStruct MBusInfo[],byte *ET13_MEP_0,byte *ET13_MEP_1,byte *ET13_MEP_2,byte *ET13_MEP_3,word *ET13_MEP_4,byte *ET13_MEP_6,
                     byte *ET13_MEP_7,byte *ET13_MEP_8,byte ET13_MEP_9[])
{
  word Offset;

  if(ReplyLength <= MeterConfig.ET11_0 * MeterConfig.ET11_1) {
    return false;
  }

  for(byte i = 0;i<4;i++)
  {
    Offset = i * MeterConfig.ET11_1;
    MBusInfo[i].ET13_MBus_0 = Reply[Offset];
    MBusInfo[i].ET13_MBus_1 = Reply[Offset + 1];
    MBusInfo[i].ET13_MBus_2 = Reply[Offset + 2];
    MBusInfo[i].ET13_MBus_3 = Reply[Offset + 3];
    MBusInfo[i].ET13_MBus_4 = Reply[Offset + 4]*256 + Reply[Offset + 5];
    MBusInfo[i].ET13_MBus_6 = Reply[Offset + 6];
    MBusInfo[i].ET13_MBus_7 = Reply[Offset + 7];
    memcpy(MBusInfo[i].ET13_MBus_8, Reply + Offset + 8, 20);  
    MBusInfo[i].ET13_MBus_28 = Reply[Offset + 28];
    MBusInfo[i].ET13_MBus_29 = Reply[Offset + 29];
    memcpy(MBusInfo[i].ET13_MBus_30, Reply + Offset + 30, 32); 
    MBusInfo[i].ET13_MBus_62 = Reply[Offset + 62]*256 + Reply[Offset + 63];
    MBusInfo[i].ET13_MBus_64 = Reply[Offset + 64]*256 + Reply[Offset + 65];
  }
  
  Offset = 4 * MeterConfig.ET11_1;
  *ET13_MEP_0 = Reply[Offset];
  *ET13_MEP_1 = Reply[Offset + 1];
  *ET13_MEP_2 = Reply[Offset + 2];
  *ET13_MEP_3 = Reply[Offset + 3];
  *ET13_MEP_4 = Reply[Offset + 4]*256 + Reply[Offset + 5];
  *ET13_MEP_6 = Reply[Offset + 6];
  *ET13_MEP_7 = Reply[Offset + 7];
  *ET13_MEP_8 = Reply[Offset + 8];
  memcpy(ET13_MEP_9, Reply + Offset + 9, MeterConfig.ET11_1 - 9);
  return true;
}

boolean Decode0x0832(unsigned long ReplyLength,byte Reply[],byte ET50_0[],byte *ET50_30,word ET50_31[],long ET50_43[],byte *ET50_67)
{
  if(ReplyLength <= 31) {
    return false;
  }
  memcpy(ET50_0, Reply, 30);
  *ET50_30 = Reply[30];
  memcpy(ET50_31, Reply + 31, 6*2); // 2 = word (array of 6)
  memcpy(ET50_43, Reply + 43, 6*4); // 4 = long (array of 6)
  *ET50_67 = Reply[67];
  return true;
}

boolean Decode0x0855(unsigned long ReplyLength,byte Reply[],String *Message)
{
  if(ReplyLength <= 3) {
    return false;
  }
  switch(Reply[3]) 
  {
    case 0x00: *Message = "0x00 Procedure completed";
               break;
    case 0x01: *Message = "0x01 Procedure accepted but not fully completed";
               break;
    case 0x02: *Message = "0x02 Invalid parameter for known procedure, procedure ignored";
               break;
    case 0x03: *Message = "0x03 Procedure conflicts with current device setup, procedure ignored";
               break;
    case 0x04: *Message = "0x04 Timing constraint, procedure ignored";
               break;
    case 0x05: *Message = "0x05 No authorization for requested procedure, procedure ignored";
               break;
    case 0x06: *Message = "0x06 Unrecognized procedure, procedure ignored";
               break;
    default:   *Message = "0x"+ByteAsHexString(Reply[3])+" Unknows Procedure Result Code (internal error)";
               break;
  }
  return true;
}

String BillingReadFreq2String(byte Frequency)
{
  const String frequencyStr[5] = {"Hourly","Daily","Weekly","Monthly","Never"};
  return frequencyStr[Frequency];
}

String BaudRate2String(byte Baud)
{
  const String baudRateStr[13] = {"9600 (MEP default)","300","600","1200","2400 (M-Bus default)","4800","9600","14400 (not guaranteed)","19200 (not guaranteed)",
                                  "28800 (not guaranteed)","38400 (not guaranteed)","57600 (not guaranteed)","115200 (not guaranteed)"};
  if(Baud >= 13) {
    return "Reserved for future use (" + String(Baud) + ")";
  }
  return baudRateStr[Baud];
}

String ReplyData2String(MEPQueueStruct *MEPQueue,byte MEPQueueReplyIndex, boolean UpdateDataStructures)
{
  if(MEPQueue[MEPQueueReplyIndex].RequestLength < 8)
    return "";

  if(MEPQueue[MEPQueueReplyIndex].Reply[2] != 0x00)
    return "";

  if((MEPQueue[MEPQueueReplyIndex].Request[2] == 0x30) ||
     ((MEPQueue[MEPQueueReplyIndex].Request[2] == 0x3f) && (MEPQueue[MEPQueueReplyIndex].Request[5] == 0x00) &&
      (MEPQueue[MEPQueueReplyIndex].Request[6] == 0x00) && (MEPQueue[MEPQueueReplyIndex].Request[7] == 0x00))) {
        
    switch(MEPQueue[MEPQueueReplyIndex].Request[3]*256 + MEPQueue[MEPQueueReplyIndex].Request[4])
    {
      case 0x0001: char Manufacturer[5]; // BT01: General Manufacturer Information
                   char Model[9];
                   byte MainFirmwareVersionNumber;
                   byte FirmwareRevisionNumber;
                   if(Decode0x0001(MEPQueue[MEPQueueReplyIndex].ReplyLength-5,MEPQueue[MEPQueueReplyIndex].Reply+5,Manufacturer,Model,&MainFirmwareVersionNumber,&FirmwareRevisionNumber)) {      
                     memcpy(MeterInfo.BT01_Manufacturer,Manufacturer,5);
                     memcpy(MeterInfo.BT01_Model,Model,9);
                     MeterInfo.BT01_MainFirmwareVersionNumber = MainFirmwareVersionNumber;
                     MeterInfo.BT01_FirmwareRevisionNumber = FirmwareRevisionNumber;
                     return "Manufacturer: " + String(Manufacturer) + "<br>" +
                            "Model: " + String(Model) + "<br>" +
                            "Version: " + String(MainFirmwareVersionNumber) + "." + String(FirmwareRevisionNumber);
                   }
                   break;
                   
      case 0x0015: if(Decode0x0015(MEPQueue[MEPQueueReplyIndex].ReplyLength-5,MEPQueue[MEPQueueReplyIndex].Reply+5,&MeterConfig.BT21_0,&MeterConfig.BT21_1,&MeterConfig.BT21_2,&MeterConfig.BT21_3,
                                   &MeterConfig.BT21_4,&MeterConfig.BT21_5,&MeterConfig.BT21_6,&MeterConfig.BT21_7,&MeterConfig.BT21_8,&MeterConfig.BT21_9)) {
                     return "Bool. flags 0: " + Byte2Binary(MeterConfig.BT21_0) + "<br>" +
                            "Bool. flags 1: " + Byte2Binary(MeterConfig.BT21_1) + "<br>" +
                            "No. of Self-reads: " + String(MeterConfig.BT21_2) + "<br>" +
                            "No. of Summations: " + String(MeterConfig.BT21_3) + "<br>" +
                            "No. of Demands: " + String(MeterConfig.BT21_4) + "<br>" +
                            "No. of Coincident Values: " + String(MeterConfig.BT21_5) + "<br>" +
                            "No. of Occurrences: " + String(MeterConfig.BT21_6) + "<br>" +
                            "No. of Tiers: " + String(MeterConfig.BT21_7) + "<br>" +
                            "No. of Present Demands: " + String(MeterConfig.BT21_8) + "<br>" +
                            "No. of Present Values: " + String(MeterConfig.BT21_9);
                   } 
                   break;
                   
      case 0x0017: long FwdActiveWhL1L2L3;
                   long RevActiveWhL1L2L3;
                   if(Decode0x0017(MEPQueue[MEPQueueReplyIndex].ReplyLength-5,MEPQueue[MEPQueueReplyIndex].Reply+5,&FwdActiveWhL1L2L3,&RevActiveWhL1L2L3)) {
                     Serial.printf("Decoded FwdActiveWhL1L2L3=%lu and RevActiveWhL1L2L3=%lu\r\n",FwdActiveWhL1L2L3,RevActiveWhL1L2L3);
                     if(UpdateDataStructures) {
                       ConsumptionData.BT23_Fwd_Act_Wh = FwdActiveWhL1L2L3;
                       ConsumptionData.BT23_Rev_Act_Wh = RevActiveWhL1L2L3;
                       Serial.printf("Storing Fwd_Act_Wh=%lu and Rev_Act_Wh=%lu\r\n",ConsumptionData.BT23_Fwd_Act_Wh,ConsumptionData.BT23_Rev_Act_Wh);
                     }
                     return "Fwd Active Wh L1L2L3: " + String(FwdActiveWhL1L2L3) + "<br>" +
                            "Rev Active Wh L1L2L3: " + String(RevActiveWhL1L2L3);
                   }
                   break;
                   
      case 0x001C: long FwdActiveWL1L2L3;
                   long RevActiveWL1L2L3;
                   long ImportReactiveVArL1L2L3;
                   long ExportReactiveVArL1L2L3;
                   long RMSCurrentmAL1;
                   long RMSCurrentmAL2;
                   long RMSCurrentmAL3;
                   long RMSVoltagemVL1;
                   long RMSVoltagemVL2;
                   long RMSVoltagemVL3;
                   long PowerFactorL1;
                   long FrequencymHz;
                   long VAL1L2L3;
                   long PowerFactorL2;
                   long PowerFactorL3;
                   long FwdActiveWL1;
                   long FwdActiveWL2;
                   long FwdActiveWL3;
                   long RevActiveWL1;
                   long RevActiveWL2;
                   long RevActiveWL3;                  
                   long RMSVoltagemVL1Cont;
                   long RMSVoltagemVL2Cont;
                   long RMSVoltagemVL3Cont;
                   long RMSVoltagemVL1Avg;
                   long RMSVoltagemVL2Avg;
                   long RMSVoltagemVL3Avg;
                   long AverageFwdActiveWL1L2L3;
                   long AverageRevActiveWL1L2L3;
                   long AverageFwdActiveWL1;
                   long AverageFwdActiveWL2;
                   long AverageFwdActiveWL3;
                   long AverageRevActiveWL1;
                   long AverageRevActiveWL2;
                   long AverageRevActiveWL3;                  
                   if(Decode0x001C(MEPQueue[MEPQueueReplyIndex].ReplyLength-5,MEPQueue[MEPQueueReplyIndex].Reply+5,&FwdActiveWL1L2L3,&RevActiveWL1L2L3,&ImportReactiveVArL1L2L3,&ExportReactiveVArL1L2L3,
                                   &RMSCurrentmAL1,&RMSCurrentmAL2,&RMSCurrentmAL3,&RMSVoltagemVL1,&RMSVoltagemVL2,&RMSVoltagemVL3,
                                   &PowerFactorL1,&FrequencymHz,&VAL1L2L3,&PowerFactorL2,&PowerFactorL3,
                                   &FwdActiveWL1,&FwdActiveWL2,&FwdActiveWL3,&RevActiveWL1,&RevActiveWL2,&RevActiveWL3,                     
                                   &RMSVoltagemVL1Cont,&RMSVoltagemVL2Cont,&RMSVoltagemVL3Cont,&RMSVoltagemVL1Avg,&RMSVoltagemVL2Avg,&RMSVoltagemVL3Avg,
                                   &AverageFwdActiveWL1L2L3,&AverageRevActiveWL1L2L3,&AverageFwdActiveWL1,&AverageFwdActiveWL2,&AverageFwdActiveWL3,&AverageRevActiveWL1,&AverageRevActiveWL2,&AverageRevActiveWL3)) {
                     if(UpdateDataStructures) {
                       ConsumptionData.BT28_Fwd_W = FwdActiveWL1L2L3;
                       ConsumptionData.BT28_Rev_W = RevActiveWL1L2L3;
                       ConsumptionData.BT28_Freq_mHz = FrequencymHz;
                       ConsumptionData.BT28_RMS_mA_L1 = RMSCurrentmAL1;
                       ConsumptionData.BT28_RMS_mA_L2 = RMSCurrentmAL2;
                       ConsumptionData.BT28_RMS_mA_L3 = RMSCurrentmAL3;
                       ConsumptionData.BT28_RMS_mV_L1 = RMSVoltagemVL1;
                       ConsumptionData.BT28_RMS_mV_L2 = RMSVoltagemVL2;
                       ConsumptionData.BT28_RMS_mV_L3 = RMSVoltagemVL3;
                       ConsumptionData.BT28_Fwd_W_L1 = FwdActiveWL1;
                       ConsumptionData.BT28_Fwd_W_L2 = FwdActiveWL2;
                       ConsumptionData.BT28_Fwd_W_L3 = FwdActiveWL3;
                       ConsumptionData.BT28_Rev_W_L1 = RevActiveWL1;
                       ConsumptionData.BT28_Rev_W_L2 = RevActiveWL2;
                       ConsumptionData.BT28_Rev_W_L3 = RevActiveWL3;
                       ConsumptionData.BT28_Fwd_Avg_W = AverageFwdActiveWL1L2L3;
                       ConsumptionData.BT28_Rev_Avg_W = AverageRevActiveWL1L2L3;
                       ConsumptionData.BT28_Fwd_Avg_W_L1 = AverageFwdActiveWL1;
                       ConsumptionData.BT28_Fwd_Avg_W_L2 = AverageFwdActiveWL2;
                       ConsumptionData.BT28_Fwd_Avg_W_L3 = AverageFwdActiveWL3;
                       ConsumptionData.BT28_Rev_Avg_W_L1 = AverageRevActiveWL1;
                       ConsumptionData.BT28_Rev_Avg_W_L2 = AverageRevActiveWL2;
                       ConsumptionData.BT28_Rev_Avg_W_L3 = AverageRevActiveWL3;
                     }  
                     return "Fwd Active W L1L2L3: " + String(FwdActiveWL1L2L3) + "<br>" +
                            "Rev Active W L1L2L3: " + String(RevActiveWL1L2L3) + "<br>" +
                            "Import Reactive VAr L1L2L3: " + String(ImportReactiveVArL1L2L3) + "<br>" +
                            "Export Reactive VAr L1L2L3: " + String(ExportReactiveVArL1L2L3) + "<br>" +
                            "RMS Current (mA) L1: " + String(RMSCurrentmAL1) + "<br>" +
                            "RMS Current (mA) L2: " + String(RMSCurrentmAL2) + "<br>" +
                            "RMS Current (mA) L3: " + String(RMSCurrentmAL3) + "<br>" +
                            "RMS Voltage (mV) L1: " + String(RMSVoltagemVL1) + "<br>" +
                            "RMS Voltage (mV) L2: " + String(RMSVoltagemVL2) + "<br>" +
                            "RMS Voltage (mV) L3: " + String(RMSVoltagemVL3) + "<br>" +
                            "Frequency (mHz): " + String(FrequencymHz) + "<br>" +
                            "VA L1L2L3: " + String(VAL1L2L3) + "<br>" +
                            "Power Factor L1 (1/1000): " + String(PowerFactorL1) + "<br>" +
                            "Power Factor L2 (1/1000): " + String(PowerFactorL2) + "<br>" +
                            "Power Factor L3 (1/1000): " + String(PowerFactorL3) + "<br>" +
                            "Fwd Active W L1: " + String(FwdActiveWL1) + "<br>" +
                            "Fwd Active W L2: " + String(FwdActiveWL2) + "<br>" +
                            "Fwd Active W L3: " + String(FwdActiveWL3) + "<br>" + 
                            "Rev Active W L1: " + String(RevActiveWL1) + "<br>" +
                            "Rev Active W L2: " + String(RevActiveWL2) + "<br>" +
                            "Rev Active W L3: " + String(RevActiveWL3) + "<br>" +
                            "RMS Voltage (mV) L1 - Continuous: " + String(RMSVoltagemVL1Cont) + "<br>" +
                            "RMS Voltage (mV) L2 - Continuous: " + String(RMSVoltagemVL2Cont) + "<br>" + 
                            "RMS Voltage (mV) L3 - Continuous: " + String(RMSVoltagemVL3Cont) + "<br>" + 
                            "RMS Voltage (mV) L1 - Average: " + String(RMSVoltagemVL1Avg) + "<br>" + 
                            "RMS Voltage (mV) L2 - Average: " + String(RMSVoltagemVL2Avg) + "<br>" + 
                            "RMS Voltage (mV) L3 - Average: " + String(RMSVoltagemVL3Avg) + "<br>" + 
                            "Average Fwd Active W L1L2L3: " + String(AverageFwdActiveWL1L2L3) + "<br>" + 
                            "Average Rev Active W L1L2L3: " + String(AverageRevActiveWL1L2L3) + "<br>" + 
                            "Average Fwd Active W L1: " + String(AverageFwdActiveWL1) + "<br>" + 
                            "Average Fwd Active W L2: " + String(AverageFwdActiveWL2) + "<br>" + 
                            "Average Fwd Active W L3: " + String(AverageFwdActiveWL3) + "<br>" + 
                            "Average Rev Active W L1: " + String(AverageRevActiveWL1) + "<br>" + 
                            "Average Rev Active W L2: " + String(AverageRevActiveWL2) + "<br>" + 
                            "Average Rev Active W L3: " + String(AverageRevActiveWL3);
                   }
                   break;
                          
      case 0x0034: byte year_;
                   byte month_;
                   byte day_;
                   byte hour_;
                   byte minute_;
                   byte second_;
                   byte DayOfWeek;
                   boolean DST;
                   boolean GMT;
                   boolean TZ;
                   boolean DSTApp;
                   struct tm tm;
                   struct tm tm2;
                   struct timeval tv;

                   if(Decode0x0034(MEPQueue[MEPQueueReplyIndex].ReplyLength-5,MEPQueue[MEPQueueReplyIndex].Reply+5,&year_,&month_,&day_,&hour_,&minute_,&second_,&DayOfWeek,&DST,&GMT,&TZ,&DSTApp)) {
                     if(UpdateDataStructures) {
                       setenv("TZ", "UTC", 1);
                       tzset();
                       configTime(0,0,"");
                       tm.tm_year = int(year_)+100;
                       tm.tm_mon = int(month_)-1;
                       tm.tm_mday = int(day_);
                       tm.tm_hour = int(hour_);
                       tm.tm_min = int(minute_);
                       tm.tm_sec = int(second_);
                       tv.tv_sec = mktime(&tm);
                       settimeofday(&tv, NULL);

                       // For some reason the ESP32 is off when set - maybe an ESP32 bug. Do some delta adjustments to fix it
                       getLocalTime(&tm2); 
                       tv.tv_sec = mktime(&tm) - mktime(&tm2) + mktime(&tm);
                       settimeofday(&tv, NULL);                      
                     }
                     return "Date and Time: " + DateTime2String(year_,month_,day_,hour_,minute_,second_,DayOfWeek+1,false) + "<br>" +
                            "Status of Daylight Saving Time: " + String(DST) + "<br>" +
                            "Is Greenwich Mean Time (GMT): " + String(GMT) + "<br>" +
                            "Time Zone Applied: " + String(TZ) + "<br>" +
                            "Daylight Saving Time Applied: " + String(DSTApp);
                   }
                   break;
                   
      case 0x0803: char UtilitySerialNumber[31]; // ET03: Utility Information
                   word ImageCRC;
                   if(Decode0x0803(MEPQueue[MEPQueueReplyIndex].ReplyLength-5,MEPQueue[MEPQueueReplyIndex].Reply+5,UtilitySerialNumber,&ImageCRC)) {
                     memcpy(MeterInfo.ET03_UtilitySerialNumber,UtilitySerialNumber,31);
                     return "Utility Serial Number: "+String(UtilitySerialNumber) + "<br>" +
                            "Image CRC: "+String(ImageCRC);
                   }
                   break;
      
      case 0x080B: if(Decode0x080B(MEPQueue[MEPQueueReplyIndex].ReplyLength-5,MEPQueue[MEPQueueReplyIndex].Reply+5,&MeterConfig.ET11_0,&MeterConfig.ET11_1,&MeterConfig.ET11_2,&MeterConfig.ET11_3,&MeterConfig.ET11_4,
                                   &MeterConfig.ET11_5,&MeterConfig.ET11_7,&MeterConfig.ET11_9,&MeterConfig.ET11_11,&MeterConfig.ET11_12,&MeterConfig.ET11_13,&MeterConfig.ET11_18,&MeterConfig.ET11_19,&MeterConfig.ET11_20,
                                   &MeterConfig.ET11_21,&MeterConfig.ET11_22,&MeterConfig.ET11_23,&MeterConfig.ET11_24,&MeterConfig.ET11_25,&MeterConfig.ET11_26,&MeterConfig.ET11_27,&MeterConfig.ET11_28,&MeterConfig.ET11_30)) {
                     return "No. of Devices: " + String(MeterConfig.ET11_0) + "<br>" +
                            "Config Entry Size: " + String(MeterConfig.ET11_1) + "<br>" +
                            "Status Entry Size: " + String(MeterConfig.ET11_2) + "<br>" +
                            "On-demand Request Queue Size: " + String(MeterConfig.ET11_3) + "<br>" +
                            "On-demand Request Entry Size: " + String(MeterConfig.ET11_4) + "<br>" +
                            "Data Entry Size: " + String(MeterConfig.ET11_5) + "<br>" +
                            "Transaction Request Length: " + String(MeterConfig.ET11_7) + "<br>" +
                            "Transaction Response Length: " + String(MeterConfig.ET11_9) + "<br>" +
                            "On-demand Write Entry Size: " + String(MeterConfig.ET11_11) + "<br>" +
                            "Phase Measurement Data Size: " + String(MeterConfig.ET11_12) + "<br>" +
                            "Config Entry 2 Size: " + String(MeterConfig.ET11_13) + "<br>" +
                            "No. of Group IDs: " + String(MeterConfig.ET11_18) + "<br>" +
                            "No. of Harmonics: " + String(MeterConfig.ET11_19) + "<br>" +
                            "M-Bus Multicast Message Length: " + String(MeterConfig.ET11_20) + "<br>" +
                            "ET22 Alarm Size: " + String(MeterConfig.ET11_21) + "<br>" +
                            "MEP Data Sources: " + String(MeterConfig.ET11_22) + "<br>" +
                            "ET48 Entry Count: " + String(MeterConfig.ET11_23) + "<br>" +
                            "Max Critical Event Bitmaps: " + String(MeterConfig.ET11_24) + "<br>" +
                            "ET57 Entry Count: " + String(MeterConfig.ET11_25) + "<br>" +
                            "Time-based Relay Switches: " + String(MeterConfig.ET11_26) + "<br>" +
                            "MEP Data Alert Sources: " + String(MeterConfig.ET11_27) + "<br>" +
                            "RAM-only Transaction Request Length: " + String(MeterConfig.ET11_28) + "<br>" +
                            "RAM-only Transaction Response Length: " + String(MeterConfig.ET11_30);
                   }
                   break;
      case 0x080D:  {
        String Message;
        if(Decode0x080D(MEPQueue[MEPQueueReplyIndex].ReplyLength-5,MEPQueue[MEPQueueReplyIndex].Reply+5,MeterConfig.MBusInfo,&MeterConfig.ET13_MEP_0,&MeterConfig.ET13_MEP_1,&MeterConfig.ET13_MEP_2,
                        &MeterConfig.ET13_MEP_3,&MeterConfig.ET13_MEP_4,&MeterConfig.ET13_MEP_6,&MeterConfig.ET13_MEP_7,&MeterConfig.ET13_MEP_8,MeterConfig.ET13_MEP_9)) {
          for(byte i=0;i<4;i++) {
            Message = Message + "M-Bus" + String(i+1) + "-Scheduled Billing Read - Day: " + ScheduledDay2String(MeterConfig.MBusInfo[i].ET13_MBus_0) + "<br>" +
                                "M-Bus" + String(i+1) + "-Scheduled Billing Read - Hour: " + String(MeterConfig.MBusInfo[i].ET13_MBus_1) + "<br>" +
                                "M-Bus" + String(i+1) + "-Scheduled Billing Read - Minute: " + String(MeterConfig.MBusInfo[i].ET13_MBus_2) + "<br>" +
                                "M-Bus" + String(i+1) + "-Scheduled Billing Read - Frequency: " + BillingReadFreq2String(MeterConfig.MBusInfo[i].ET13_MBus_3) + "<br>" +
                                "M-Bus" + String(i+1) + "-Scheduled Status Read (M-Bus) - Frequency: " + String(MeterConfig.MBusInfo[i].ET13_MBus_4) + "<br>" +
                                "M-Bus" + String(i+1) + "-Baud Rate: " + BaudRate2String(MeterConfig.MBusInfo[i].ET13_MBus_6) + "<br>" +
                                "M-Bus" + String(i+1) + "-Device Alarm Bitmask: " + Byte2Binary(MeterConfig.MBusInfo[i].ET13_MBus_7) + "<br>" +
                                "M-Bus" + String(i+1) + "-Customer ID: " + String(MeterConfig.MBusInfo[i].ET13_MBus_8) + "<br>" +
                                "M-Bus" + String(i+1) + "-App Reset Parameter: " + String(MeterConfig.MBusInfo[i].ET13_MBus_28) + "<br>" +
                                "M-Bus" + String(i+1) + "-Security Key: " + bytesToHexString(MeterConfig.MBusInfo[i].ET13_MBus_30,MeterConfig.MBusInfo[i].ET13_MBus_29,true) + "<br>" +
                                "M-Bus" + String(i+1) + "-Ta: " + String(MeterConfig.MBusInfo[i].ET13_MBus_62) + "<br>" +
                                "M-Bus" + String(i+1) + "-To: " + String(MeterConfig.MBusInfo[i].ET13_MBus_64) + "<br>";
          }
          return Message +
                 "MEP-Scheduled Billing Read - Day: " + ScheduledDay2String(MeterConfig.ET13_MEP_0) + "<br>" +
                 "MEP-Scheduled Billing Read - Hour: " + String(MeterConfig.ET13_MEP_1) + "<br>" +
                 "MEP-Scheduled Billing Read - Minute: " + String(MeterConfig.ET13_MEP_2) + "<br>" +
                 "MEP-Scheduled Billing Read - Frequency: " + BillingReadFreq2String(MeterConfig.ET13_MEP_3) + "<br>" +
                 "MEP-Scheduled Status Read (M-Bus) - Frequency: " + String(MeterConfig.ET13_MEP_4) + "<br>" +
                 "MEP-Baud Rate: " + BaudRate2String(MeterConfig.ET13_MEP_6) + "<br>" +
                 "MEP-Device Alarm Bitmask: " + Byte2Binary(MeterConfig.ET13_MEP_7) + "<br>" +
                 "MEP-Alert flags: " + Byte2Binary(MeterConfig.ET13_MEP_8) + "<br>" +
                 "MEP-Non-urgent Downlink Data: " + bytesToHexString(MeterConfig.ET13_MEP_9,57,true);
        }
        break;
      }
      
      case 0x0832: {
        if(Decode0x0832(MEPQueue[MEPQueueReplyIndex].ReplyLength-5,MEPQueue[MEPQueueReplyIndex].Reply+5,MeterConfig.ET50_0,&MeterConfig.ET50_30,MeterConfig.ET50_31,MeterConfig.ET50_43,&MeterConfig.ET50_67)) {
                     return "Identification String (MEP ID): " + bytesToHexString(MeterConfig.ET50_0,30,true) + "<br>" +
                            "MEP Flags: " + Byte2Binary(MeterConfig.ET50_30) + "<br>" +
                            "Data Sources Icon Display Control: " + Word2Binary(MeterConfig.ET50_31[0]) + ", " + Word2Binary(MeterConfig.ET50_31[1]) + ", " + Word2Binary(MeterConfig.ET50_31[2]) + ", " + 
                                                                    Word2Binary(MeterConfig.ET50_31[3]) + ", " + Word2Binary(MeterConfig.ET50_31[4]) + ", " + Word2Binary(MeterConfig.ET50_31[5]) + "<br>" +
                            "Data for Display/Load Profile Sources: " + String(MeterConfig.ET50_43[0]) + ", " + String(MeterConfig.ET50_43[1]) + ", " + String(MeterConfig.ET50_43[2]) + ", " + 
                                                                        String(MeterConfig.ET50_43[3]) + ", " + String(MeterConfig.ET50_43[4]) + ", " + String(MeterConfig.ET50_43[5]) + "<br>" +
                            "MEP Icon Display Control: " + Byte2Binary(MeterConfig.ET50_67);
        }
        break;
      }
      
      case 0x0008: // same as 0x0855
      case 0x083B: // same as 0x0855
      case 0x0855: {
        String Message;
        if(Decode0x0855(MEPQueue[MEPQueueReplyIndex].ReplyLength-5,MEPQueue[MEPQueueReplyIndex].Reply+5,&Message)) {
          return Message;
        }
        break;
      }
    }
  }
  return "";
}

String ReplyPackageToMessage(MEPQueueStruct *MEPQueue,byte MEPQueueReplyIndex)
{
  String Message = "";
    
  if(MEPQueue[MEPQueueReplyIndex].ReplyLength == 0)
  {
    return("");
  }

  if((MEPQueue[MEPQueueReplyIndex].RequestLength == MEPQueue[MEPQueueReplyIndex].ReplyLength) &&
     (memcmp(MEPQueue[MEPQueueReplyIndex].Request, MEPQueue[MEPQueueReplyIndex].Reply, MEPQueue[MEPQueueReplyIndex].RequestLength) == 0)) {
    return "Request looped back to me (are you debugging?)";
  }

  if((MEPQueue[MEPQueueReplyIndex].ReplyLength == 2) && (MEPQueue[MEPQueueReplyIndex].Reply[0] == 0x00) && (MEPQueue[MEPQueueReplyIndex].Reply[1] == 0x00)) {
    return "0x0000 Alert Sequence from meter";
  }

  Message = Message + "Request: ";
  switch(MEPQueue[MEPQueueReplyIndex].Request[2]) {
    case 0x30: Message = "0x30 Full Table Read";
               break;
    case 0x3F: Message = "0x3F Partial Table Read";
               break;
    case 0x40: Message = "0x40 Table Write";
               break;
    case 0x4F: Message = "0x4F Partial Table Write";
               break;
    default:   Message = "<span style='color:red'>0x"+ByteAsHexString(MEPQueue[MEPQueueReplyIndex].Reply[2])+" Unknown Command (internal error)</span>";
               break;
  }
  Message = Message + " ";

  Message = Message + "<b>";
  switch(MEPQueue[MEPQueueReplyIndex].Request[3]*256 + MEPQueue[MEPQueueReplyIndex].Request[4]) {
    case 0x0001: Message = Message + "0x0001 BT01 General Manufacturer Information";
                 break;
    case 0x0015: Message = Message + "0x0015 BT21 Actual Register";
                 break;
    case 0x0017: Message = Message + "0x0017 BT23 Current Register Data";
                 break;
    case 0x001C: Message = Message + "0x001C BT28 Present Register Data";
                 break;
    case 0x0034: Message = Message + "0x0034 BT52 UTC Clock";
                 break;
    case 0x0803: Message = Message + "0x0803 ET03 Utility Information";
                 break;
    case 0x080B: Message = Message + "0x080B ET11 MFG Dimension Table";
                 break;
    case 0x080D: Message = Message + "0x080D ET13 MEP/M-Bus Device Configuration";
                 break;
    case 0x0832: Message = Message + "0x0832 ET50 MEP Inbound Data Space";
                 break;
    default:     Message = Message + "<span style='color:red'>0x"+ByteAsHexString(MEPQueue[MEPQueueReplyIndex].Reply[3])+ByteAsHexString(MEPQueue[MEPQueueReplyIndex].Reply[4])+" Unknown Table (internal error)</span>";
                 break;
  }
  Message = Message + "</b><br>";

  if(MEPQueue[MEPQueueReplyIndex].ReplyLength >= 3) {
    Message = Message + "Meter response: <span style=";
    switch(MEPQueue[MEPQueueReplyIndex].Reply[2])
    {
      case 0x00: Message = Message + "'color:green'>0x00 Successful response";
                 break;
      case 0x01: Message = Message + "'color:red'>0x01 Unrecognized request";
                 break;
      case 0x02: Message = Message + "'color:red'>0x02 Unsupported request";
                 break;
      case 0x03: Message = Message + "'color:red'>0x03 No permission for specified request";
                 break;
      case 0x04: Message = Message + "'color:red'>0x04 Requested operation not possible";
                 break;
      case 0x05: Message = Message + "'color:red'>0x05 Table operation not possible";
                 break;
      case 0x0B: Message = Message + "'color:red'>0x0B Authentication failure";
                 break;
      case 0x0C: Message = Message + "'color:goldenrod'>0x0C Invalid sequence number (this is normal for 1st request)";
                 break;
      default:   Message = Message + "'color:red'>0x"+ByteAsHexString(MEPQueue[MEPQueueReplyIndex].Reply[2])+" Unknown Response Code (internal error)";
                 break;
    }
    Message = Message + "</span>";
    
    if(AlertSeqenceActive) {
      Message = Message + " <span style='color:red'><b>[ALERT]</b></span>";
    }

    return Message + "<br>" + ReplyData2String(MEPQueue,MEPQueueReplyIndex,false);
  }

  return "";
}

void HandleInvalidSequenceNumber(String key,MEPQueueStruct *MEPQueue,byte MEPQueueReplyIndex, byte *MEPQueueNextIndex)
{
  String Request;

  // Handle 0x04 Invalid Procedure Sequence No.
  if((MEPQueue[MEPQueueReplyIndex].Request[2] == 0x40) && (MEPQueue[MEPQueueReplyIndex].Request[3] == 0x00) && (MEPQueue[MEPQueueReplyIndex].Request[4] == 0x07) && // Full write to table BT07 = Procedure call
     (MEPQueue[MEPQueueReplyIndex].Reply[2] == 0x04)) // 0x04: Invalid Procedure Sequence No. - try next one
  {
    Request = bytesToHexString(MEPQueue[MEPQueueReplyIndex].Request+2, MEPQueue[MEPQueueReplyIndex].RequestLength-8-4-2, false); // 8=digest, 4=sequence no, 2=length
    Request = Request.substring(0,14) + ByteAsHexString(ProcedureSequenceNo) + Request.substring(16);
    queueRequest(Request,key,MEPQueue,MEPQueueNextIndex,MEPQueue[MEPQueueReplyIndex].NextAction);
    IncreaseProcedureSequenceNo(&ProcedureSequenceNo);
    return;
  }

  // Handle 0x0C Invalid Sequence No.
  if(MEPQueue[MEPQueueReplyIndex].Reply[2] == 0x0C) // 0x0C: Correct sequence number to be used in subsequent request messages.
  {
    Request = bytesToHexString(MEPQueue[MEPQueueReplyIndex].Request+2, MEPQueue[MEPQueueReplyIndex].RequestLength-8-4-2, false); // 8=digest, 4=sequence no, 2=length
    SequenceNo = MEPQueue[MEPQueueReplyIndex].Reply[3] * 256 * 256 * 256 +
                 MEPQueue[MEPQueueReplyIndex].Reply[4] * 256 * 256 +
                 MEPQueue[MEPQueueReplyIndex].Reply[5] * 256 +
                 MEPQueue[MEPQueueReplyIndex].Reply[6];
    queueRequest(Request,key,MEPQueue,MEPQueueNextIndex,MEPQueue[MEPQueueReplyIndex].NextAction);
  }
}

void HandleAlertSequence(byte *Response, unsigned long *ResponseLength,String key,MEPQueueStruct *MEPQueue,byte *MEPQueueNextIndex)
{
  while((*ResponseLength >= 2) && (Response[0] == 0x00) && (Response[1] == 0x00)) // 0x0000: Alert Sequence
  {
    Serial.printf("Alert sequence (0x0000) received...\r\n");
    queueResponseWithNoRequest(Response,2,MEPQueue,MEPQueueNextIndex);
    if(!AlertSeqenceActive)
    {
      queueRequest("3f080b0000010001",key,MEPQueue,MEPQueueNextIndex,AlertET11); // Ask ET11/"MFG Dimension Table" for "size in bytes of a MEP configuration table entry for M-Bus ET13." 
      AlertSeqenceActive = true;
    }

    (*ResponseLength) -= 2;
    memcpy(Response,Response+2,*ResponseLength);
  }
}

void HandleNextAction(String key,MEPQueueStruct *MEPQueue,byte MEPQueueReplyIndex, byte *MEPQueueNextIndex)
{
  if(MEPQueue[MEPQueueReplyIndex].Reply[2] != 0x00) // 0x00: Successful response
  {
    return;
  }
  
  switch(MEPQueue[MEPQueueReplyIndex].NextAction)
  {
    case AlertET11: queueRequest("3f080d" + UnsignedLongAs3ByteHexString(4*MEPQueue[MEPQueueReplyIndex].Reply[5] + 8) + "0001",
                                 key,MEPQueue,MEPQueueNextIndex,AlertET13); // Ask ET13/"MEP/M-Bus Device Configuration" table for MEP "Alert flags" 
                    break;
                    
    case AlertET13: queueRequest("3f083200001e0001",key,MEPQueue,MEPQueueNextIndex,AlertET50); // Ask ET50/"MEP Inbound Data Space" table for the "ET59 Response" flag 
                    break;

    case AlertET50: if((MEPQueue[MEPQueueReplyIndex].Reply[5] & 2) == 0)
                      queueProcedureRequest(0x0827,"04010600010203040506",key,MEPQueue,MEPQueueNextIndex,AlertEP39BT08); // Call EP39/"Post MEP Data": 04=Scheduled read data + alarms, 01=Success, 0600=6 byte data length
                    else
                      queueProcedureRequest(0x0827,"04010600010203040506",key,MEPQueue,MEPQueueNextIndex,AlertEP39ET58); // Call EP39/"Post MEP Data": 04=Scheduled read data + alarms, 01=Success, 0600=6 byte data length
                    break;

     case AlertEP39BT08: queueRequest("3000080200",key,MEPQueue,MEPQueueNextIndex,AlertClear); // Ask BT08/"MEP Procedure Response" table BT08
                         break;
      
     case AlertEP39ET58: queueRequest("30083a0200",key,MEPQueue,MEPQueueNextIndex,AlertClear); // Ask ET58/"MEP Procedure Response" table ET58
                         break;
                         
     case AlertClear: AlertSeqenceActive = false;
                      break;
  }
}

void MEPEnable(boolean State)
{
  Serial.printf("Setting ENABLE pin %i\r\n",State);
  pinMode(METER_ENABLE_PIN, OUTPUT);
  if(State)
  {
    digitalWrite(METER_ENABLE_PIN,LOW);
  }
  else
  {
    digitalWrite(METER_ENABLE_PIN,HIGH);
  }  
}

void RS3232Enable(boolean State)
{
  Serial.printf("Setting RS3232 pin %i\r\n",State);
  pinMode(RS3232_ENABLE_PIN, OUTPUT);
  if(State)
  {
    digitalWrite(RS3232_ENABLE_PIN,HIGH);
  }
  else
  {
    digitalWrite(RS3232_ENABLE_PIN,LOW);
  }  
}


String MaxMEPReplyLengthAsHex()
{
  return WordAsHexString(MaxMEPReplyLength,false);
}
