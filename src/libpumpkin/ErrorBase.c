#include <PalmOS.h>

#include "sys.h"
#include "thread.h"
#include "pwindow.h"
#include "vfs.h"
#include "mem.h"
#include "pumpkin.h"
#include "debug.h"

#define PALMOS_MODULE "Error"

MemPtr *ErrExceptionList(void) {
  return pumpkin_get_exception();
}

void ErrThrow(Int32 err) {
  MemPtr *p;
  ErrExceptionType *e;

  if ((p = ErrExceptionList()) != NULL) {
    if ((e = (ErrExceptionType *)(*p)) != NULL) {
      *p = e->nextP;
      ErrLongJump(e->state, err);
    }
  }
}

void ErrDisplayFileLineMsg(const Char * const filename, UInt16 lineNo, const Char * const msg) {
  ErrDisplayFileLineMsgEx(filename, NULL, lineNo, msg, 0);
}

void ErrDisplayFileLineMsgEx(const Char * const filename, const Char * const function, UInt16 lineNo, const Char * const msg, int finish) {
  char buf[256];
  char *fn, *m;
  int i;

  fn = filename ? (char *)filename : "unknown";
  for (i = StrLen(fn)-1; i >= 0; i--) {
    if (fn[i] == '/') {
      fn = &fn[i+1];
      break;
    }
  }
  m = msg ? (char *)msg : "no message";
  if (function) {
    sys_snprintf(buf, sizeof(buf) - 1, "Fatal Alert %s, %s, Line:%d: %s", fn, function, lineNo, m);
  } else {
    sys_snprintf(buf, sizeof(buf) - 1, "Fatal Alert %s, Line:%d: %s", fn, lineNo, m);
  }
  SysFatalAlert(buf);

  pumpkin_fatal_error(finish);
}

/*
Int16 ErrSetJump(ErrJumpBuf buf) {
  return setjmp(buf);
}

void ErrLongJump(ErrJumpBuf buf, Int16 result) {
  longjmp(buf, result);
}
*/

UInt16 ErrAlertCustom(Err errCode, Char *errMsgP, Char *preMsgP, Char *postMsgP) {
  debug(DEBUG_ERROR, PALMOS_MODULE, "ErrAlertCustom not implemented");
  return 0;
}

