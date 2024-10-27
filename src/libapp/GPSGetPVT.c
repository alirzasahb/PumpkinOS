#include <PalmOS.h>
#include <PalmCompatibility.h>
#include <VFSMgr.h>
#include <ExpansionMgr.h>
#include <DLServer.h>
#include <SerialMgrOld.h>
#include <UDAMgr.h>
#include <PceNativeCall.h>
#include <FixedMath.h>
#include <CPMLib.h>
#include <GPSLib68K.h>
#include <GPDLib.h>
#include <PdiLib.h>
#include <BtLib.h>
#include <FSLib.h>
#include <SslLib.h>
#include <INetMgr.h>
#include <SlotDrvrLib.h>

Err GPSGetPVT(const UInt16 refNum, GPSPVTDataType *pvt) {
  uint64_t iret;
  pumpkin_system_call_p(GPS_LIB, gpsLibTrapGetPVT, 0, &iret, NULL, refNum, pvt);
  return (Err)iret;
}
