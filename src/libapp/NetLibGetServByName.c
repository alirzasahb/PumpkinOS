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

NetServInfoPtr NetLibGetServByName(UInt16 libRefNum, const Char *servNameP, const Char *protoNameP, NetServInfoBufPtr bufP, Int32 timeout, Err *errP) {
  uint64_t iret;
  pumpkin_system_call_p(NET_LIB, netLibTrapGetServByName, 0, &iret, NULL, libRefNum, servNameP, protoNameP, bufP, timeout, errP);
  return (NetServInfoPtr)iret;
}
