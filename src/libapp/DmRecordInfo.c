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

Err DmRecordInfo(DmOpenRef dbP, UInt16 index, UInt16 *attrP, UInt32 *uniqueIDP, LocalID *chunkIDP) {
  uint64_t iret;
  pumpkin_system_call_p(0, sysTrapDmRecordInfo, 0, &iret, NULL, dbP, index, attrP, uniqueIDP, chunkIDP);
  return (Err)iret;
}
