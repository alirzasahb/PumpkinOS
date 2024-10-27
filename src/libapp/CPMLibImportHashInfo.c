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

Err CPMLibImportHashInfo(UInt16 refnum, UInt8 encoding, UInt8 *importDataP, UInt32 dataLen, APHashInfoType *hashInfoP) {
  uint64_t iret;
  pumpkin_system_call_p(CPM_LIB, cpmLibTrapImportHashInfo, 0, &iret, NULL, refnum, encoding, importDataP, dataLen, hashInfoP);
  return (Err)iret;
}
