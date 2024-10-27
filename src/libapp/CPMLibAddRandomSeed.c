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

Err CPMLibAddRandomSeed(UInt16 refnum, UInt8 *seedDataP, UInt32 dataLen) {
  uint64_t iret;
  pumpkin_system_call_p(CPM_LIB, cpmLibTrapAddRandomSeed, 0, &iret, NULL, refnum, seedDataP, dataLen);
  return (Err)iret;
}
