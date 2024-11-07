#include <PalmOS.h>
#include <VFSMgr.h>

#include "thread.h"
#include "bytes.h"
#ifdef ARMEMU
#include "armemu.h"
#endif
#include "m68k/m68k.h"
#include "m68k/m68kcpu.h"
#include "emupalmosinc.h"
#include "emupalmos.h"
#include "heap.h"
#include "tos.h"
#include "gemdos.h"
#include "gemdos_proto.h"
#include "bios_proto.h"
#include "xbios_proto.h"
#include "debug.h"

#define headerSize       28
#define lowMemSize     2048
#define basePageSize    256
#define stackSize      4096

#define maxPath 256

//  uint8_t ph_branch[2];  branch to start of program (0x601a)
//  uint8_t ph_tlen[4];    .text length
//  uint8_t ph_dlen[4];    .data length
//  uint8_t ph_blen[4];    .bss length
//  uint8_t ph_slen[4];    length of symbol table
//  uint8_t ph_magic[4];
//  uint8_t ph_flags[4];   Atari special flags
//  uint8_t ph_abs[2];     has to be 0, otherwise no relocation takes place

// 00000000  60 1a 00 00 0f 28 00 00  00 28 00 00 00 44 00 00  |`....(...(...D..|
// 00000010  00 00 00 00 00 00 00 00  00 07 00 00 60 08 56 42  |............`.VB|
// 00000020  43 43 20 30 2e 39 2c 6f  00 04 49 f9 00 00 8f 28  |CC 0.9,o..I....(|

// base page format:
// 0x0000: base address of TPA
// 0x0004: end address of TPA + 1
// 0x0008: base address of text
// 0x000C: length of text
// 0x0010: base address of data
// 0x0014: length of data
// 0x0018: base address of bss
// 0x001C: length of bss
// 0x0020: DTA address pointer
// 0x0024: parent's base page pointer
// 0x0028: reserved
// 0x002C: pointer to env string
// 0x0080: command line image


// 0x00000000 - 0x009FFFFF: RAM
// 0x00A00000 – 0x00DEFFFF: RAM
// 0x00DF0000 – 0x00DFFFFF: RAM
// 0x00E00000 – 0x00EFFFFF: ROM
// 0x00F00000 - 0x00F0003F: IDE controller
// 0x00F00040 – 0x00F9FFFF: unassigned
// 0x00FA0000 – 0x00FBFFFF: cartridge ROM
// 0x00FC0000 – 0x00FEFFFF: OS ROM
// 0x00FF0000 – 0x00FF7FFF: unassigned
// 0x00FF8000 - 0x00FFF9FF: I/O
// 0x00FFFA00 - 0x00FFFA3F: I/O 68091 (peripheral)
// 0x00FFFA40 - 0x00FFFA7F: I/O 68881 (math)
// 0x00FFFA80 - 0x00FFFBFF: I/O 68901 (peripheral)
// 0x00FFFC00 - 0x00FFFC1F: I/O 6850 (keyboard, MIDI)
// 0x00FFFC20 - 0x00FFFC3F: I/O RTC
// 0x00FFFC40 – 0x00FFFFFF: unassigned
// 0x01000000 – 0x01FFFFFF: RAM (expansion)
// 0x02000000 – 0xFDFFFFFF: reserved
// 0xFE000000 – 0xFEFEFFFF: VME
// 0xFEFF0000 – 0xFEFFFFFF: VME
// 0xFF000000 – 0xFFFFFFFF: shadow of 0x00000000 – 0x00FFFFFF

static uint32_t tos_read_byte(uint8_t *memory, uint32_t memorySize, uint32_t address) {
  uint32_t value = 0;

  if ((address & 0xFF000000) == 0xFF000000) {
    address &= 0x00FFFFFF;
  }
  
  if (address >= 0x00FF8000 && address < 0x01000000) {
    debug(DEBUG_INFO, "TOS", "read from I/O register 0x%08X", address);

    switch (address) {
      case 0x00FFFA09: // MFP-ST Interrupt Enable Register B
        value = 0x40; // keyboard/MIDI
        break;
      case 0x00FFFC00: // Keyboard ACIA Control
        // bit 0: receiver full
        // bit 1: transmitter empty
        value = 3;
        break;
      case 0x00FFFC02: // keyboard ACIA data
        value = 0;
        break;
      case 0x00FFFC04: // MIDI ACIA Control
        value = 0;
        break;
      case 0x00FFFC06: // MIDI ACIA data
        value = 0;
        break;
      default:
        value = 0;
        break;
    }
  } else if (address < memorySize) {
    value = memory[address];
  } else {
    //debug(DEBUG_ERROR, "TOS", "read from unmapped address 0x%08X", address);
  }

  return value;
}

static void tos_write_byte(uint8_t *memory, uint32_t memorySize, uint32_t address, uint8_t value) {
  if ((address & 0xFF000000) == 0xFF000000) {
    address &= 0x00FFFFFF;
  }

  if (address >= 0x00FF8000 && address < 0x01000000) {
    debug(DEBUG_INFO, "TOS", "write 0x%02X to I/O register 0x%08X", value, address);
  } else if (address < memorySize) {
    memory[address] = value;
  } else {
    //debug(DEBUG_ERROR, "TOS", "write 0x%02X to unmapped address 0x%08X", value, address);
  }
}

static uint8_t read_byte(uint32_t address) {
  emu_state_t *state = m68k_get_emu_state();
  tos_data_t *data = (tos_data_t *)state->extra;

  return tos_read_byte(data->memory, data->memorySize, address);
}

static uint16_t read_word(uint32_t address) {
  emu_state_t *state = m68k_get_emu_state();
  tos_data_t *data = (tos_data_t *)state->extra;
  uint16_t w = 0;

  if ((address & 1) == 0) {
    w = (tos_read_byte(data->memory, data->memorySize, address) << 8) | tos_read_byte(data->memory, data->memorySize, address + 1);
  } else {
    debug(DEBUG_ERROR, "TOS", "read_word unaligned address 0x%08X", address);
  }

  return w;
}

static uint32_t read_long(uint32_t address) {
  emu_state_t *state = m68k_get_emu_state();
  tos_data_t *data = (tos_data_t *)state->extra;
  uint32_t l = 0;

  if ((address & 1) == 0) {
    l = (tos_read_byte(data->memory, data->memorySize, address    ) << 24) | (tos_read_byte(data->memory, data->memorySize, address + 1) << 16) |
        (tos_read_byte(data->memory, data->memorySize, address + 2) <<  8) |  tos_read_byte(data->memory, data->memorySize, address + 3);
  } else {
    debug(DEBUG_ERROR, "TOS", "read_long unaligned address 0x%08X", address);
  }

  return l;
}

static void write_byte(uint32_t address, uint8_t value) {
  emu_state_t *state = m68k_get_emu_state();
  tos_data_t *data = (tos_data_t *)state->extra;

  tos_write_byte(data->memory, data->memorySize, address, value);
}

static void write_word(uint32_t address, uint16_t value) {
  emu_state_t *state = m68k_get_emu_state();
  tos_data_t *data = (tos_data_t *)state->extra;

  if ((address & 1) == 0) {
    tos_write_byte(data->memory, data->memorySize, address    , value >> 8);
    tos_write_byte(data->memory, data->memorySize, address + 1, value & 0xFF);
  } else {
    debug(DEBUG_ERROR, "TOS", "write_word unaligned address 0x%08X", address);
  }
}

static void write_long(uint32_t address, uint32_t value) {
  emu_state_t *state = m68k_get_emu_state();
  tos_data_t *data = (tos_data_t *)state->extra;

  if ((address & 1) == 0) {
    tos_write_byte(data->memory, data->memorySize, address    ,  value >> 24);
    tos_write_byte(data->memory, data->memorySize, address + 1, (value >> 16) & 0xFF);
    tos_write_byte(data->memory, data->memorySize, address + 2, (value >>  8) & 0xFF);
    tos_write_byte(data->memory, data->memorySize, address + 3,  value        & 0xFF);
  } else {
    debug(DEBUG_ERROR, "TOS", "write_word unaligned address 0x%08X", address);
  }
}

static int cpu_instr_callback(unsigned int pc) {
  emu_state_t *state = m68k_get_emu_state();
  tos_data_t *data = (tos_data_t *)state->extra;
  uint32_t instr_size, d[8], a[8];
  char buf[128], buf2[128];
  int i;

  if (data->debug_m68k) {
    instr_size = m68k_disassemble(buf, pc, M68K_CPU_TYPE_68000);
    m68k_make_hex(buf2, pc, instr_size);
    for (i = 0; i < 8; i++) {
      d[i] = m68k_get_reg(NULL, M68K_REG_D0 + i);
      a[i] = m68k_get_reg(NULL, M68K_REG_A0 + i);
    }
    debug(DEBUG_TRACE, "M68K", "A0=0x%08X,A1=0x%08X,A2=0x%08X,A3=0x%08X,A4=0x%08X,A5=0x%08X,A6=0x%08X,A7=0x%08X",
      a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    debug(DEBUG_TRACE, "M68K", "%08X: %-20s: %s (%d,%d,%d,%d,%d,%d,%d,%d)",
      pc, buf2, buf, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
  }

  return 0;
}

static int tos_main_memory(uint8_t *tos, uint32_t tosSize, int drive, char *dir, int argc, char *argv[]) {
  MemHandle hMemory;
  uint8_t *memory, *reloc, *kbdvbase, *physbase, *lineaVars;
  uint16_t jump, aflags;
  uint32_t offset, textSize, dataSize, bssSize, symSize, relocSize, reserved, pflags, memorySize, heapSize;
  uint32_t pc, a7, basePageStart, stackStart, textStart, dataStart, bssStart, heapStart, *relocBase, value, rem;
  m68ki_cpu_core main_cpu;
  emu_state_t *state, *oldState;
  tos_data_t data;
  int i, j, k, r = -1;

  if (tos && tosSize > headerSize && dir) {
    offset = get2b(&jump, tos, 0);

    if (jump == 0x601a) {
      offset += get4b(&textSize, tos, offset);
      offset += get4b(&dataSize, tos, offset);
      offset += get4b(&bssSize, tos, offset);
      offset += get4b(&symSize, tos, offset);
      offset += get4b(&reserved, tos, offset);
      offset += get4b(&pflags, tos, offset);
      offset += get2b(&aflags, tos, offset);
      relocSize = tosSize - (headerSize + textSize + dataSize + symSize);

      debug(DEBUG_INFO, "TOS", "header:");
      debug_bytes(DEBUG_INFO, "TOS", tos, headerSize);
      debug(DEBUG_INFO, "TOS", "tos   size %d (0x%04X)", tosSize, tosSize);
      debug(DEBUG_INFO, "TOS", "text  size %d (0x%04X)", textSize, textSize);
      debug(DEBUG_INFO, "TOS", "data  size %d (0x%04X)", dataSize, dataSize);
      debug(DEBUG_INFO, "TOS", "bss   size %d (0x%04X)", bssSize, bssSize);
      debug(DEBUG_INFO, "TOS", "sym   size %d (0x%04X)", symSize, symSize);
      debug(DEBUG_INFO, "TOS", "reloc size %d (0x%04X)", relocSize, relocSize);
      debug(DEBUG_INFO, "TOS", "pflags 0x%08X", pflags);
      debug(DEBUG_INFO, "TOS", "aflags 0x%04X", aflags);

      oldState = emupalmos_install();
      state = pumpkin_get_local_storage(emu_key);
      state->extra = &data;

      rem = (textSize + dataSize + bssSize) & 0x0F;
      if (rem != 0) {
        bssSize += 16 - rem;
      }

      memorySize = 0x100000; // 1M
      hMemory = MemHandleNew(memorySize + 16);
      memory = MemHandleLock(hMemory);
      MemSet(memory, memorySize, 0);

      rem = ((uint64_t)(memory)) & 0x0F;
      if (rem != 0) {
        memory += 16 - rem;
      }

      MemMove(memory + lowMemSize + basePageSize, &tos[offset], textSize);
      if (dataSize > 0) {
        MemMove(memory + lowMemSize + basePageSize + textSize, &tos[offset + textSize], dataSize);
      }

      heapSize = memorySize - (lowMemSize + basePageSize + textSize + dataSize + bssSize + stackSize);
      basePageStart = lowMemSize;
      textStart     = lowMemSize + basePageSize;
      dataStart     = lowMemSize + basePageSize + textSize;
      bssStart      = lowMemSize + basePageSize + textSize + dataSize;
      heapStart     = lowMemSize + basePageSize + textSize + dataSize + bssSize;
      stackStart    = lowMemSize + basePageSize + textSize + dataSize + bssSize + heapSize;


      MemSet(&data, sizeof(tos_data_t), 0);
      data.debug_m68k = debug_getsyslevel("M68K") == DEBUG_TRACE;
      data.memory = memory;
      data.memorySize = memorySize;
      data.basePageStart = basePageStart;
      data.heapStart = heapStart;
      data.heapSize = heapSize;

      write_long(0x0000042E, data.memorySize); // phystop: physical top of ST compatible RAM

      write_long(basePageStart + 0x0000, basePageStart);
      write_long(basePageStart + 0x0004, memorySize);
      write_long(basePageStart + 0x0008, textStart);
      write_long(basePageStart + 0x000C, textSize);
      write_long(basePageStart + 0x0010, dataStart);
      write_long(basePageStart + 0x0014, dataSize);
      write_long(basePageStart + 0x0018, bssStart);
      write_long(basePageStart + 0x001C, bssSize);
      write_long(basePageStart + 0x0020, basePageStart + 0x0080);
      write_long(basePageStart + 0x0024, basePageStart);
      write_long(basePageStart + 0x0028, 0);
      write_long(basePageStart + 0x002C, basePageStart + 0x0028); // pointer to env string -> reserved

      for (i = 1, k = 0; i < argc && k < 128; i++) {
        if (i > 1) {
          write_byte(basePageStart + 0x0081 + k, ' ');
          k++;
        }
        for (j = 0; argv[i][j] && k < 128; j++, k++) {
          write_byte(basePageStart + 0x0081 + k, argv[i][j]);
        }
      }
      write_byte(basePageStart + 0x0080, k);

      debug(DEBUG_INFO, "TOS", "basepg: 0x%08X", basePageStart);
      debug_bytes_offset(DEBUG_INFO, "TOS", memory + basePageStart, basePageSize, basePageStart);

      debug(DEBUG_INFO, "TOS", "text  : 0x%08X", textStart);
      debug_bytes_offset(DEBUG_INFO, "TOS", memory + textStart, textSize, textStart);

      if (dataSize > 0) {
        debug(DEBUG_INFO, "TOS", "data  : 0x%08X", dataStart);
        debug_bytes_offset(DEBUG_INFO, "TOS", memory + dataStart, dataSize, dataStart);
      }

      if (bssSize > 0) {
        debug(DEBUG_INFO, "TOS", "bss   : 0x%08X", bssStart);
      }

      debug(DEBUG_INFO, "TOS", "heap  : 0x%08X", heapStart);
      debug(DEBUG_INFO, "TOS", "stack : 0x%08X", stackStart);

      if (relocSize > 0) {
        relocBase = (uint32_t *)(tos + headerSize + textSize + dataSize + symSize);
        get4b(&offset, (uint8_t *)relocBase, 0);
        if (offset) {
          debug(DEBUG_INFO, "TOS", "reloc:");
          debug_bytes(DEBUG_INFO, "TOS", (uint8_t *)relocBase, relocSize);

          value = read_long(textStart + offset);
          debug(DEBUG_INFO, "TOS", "reloc offset 0x00 0x%08X 0x%08X -> 0x%08X", offset, value, value + textStart);
          value += textStart;
          write_long(textStart + offset, value);

          for (reloc = ((uint8_t *)relocBase) + 4; *reloc; reloc++) {
            if (*reloc == 1) {
              offset += 254;
              continue;
            }
            offset += *reloc;

            value = read_long(textStart + offset);
            debug(DEBUG_INFO, "TOS", "reloc offset 0x%02X 0x%08X 0x%08X -> 0x%08X", *reloc, offset, value, value + textStart);
            value += textStart;
            write_long(textStart + offset, value);
          }
        }
      }

      data.heap = heap_init(&memory[heapStart], heapSize, NULL);

      kbdvbase = heap_alloc(data.heap, XBIOS_KBDVBASE_SIZE);
      data.kbdvbase = kbdvbase - memory;

      physbase = heap_alloc(data.heap, screenSize); // screen buffer (640*400/8 = 32000)
      data.physbase = physbase - memory;
      debug(DEBUG_INFO, "TOS", "screen: 0x%08X", data.physbase);

      lineaVars = heap_alloc(data.heap, lineaSize);
      data.lineaVars = lineaVars - memory;
      debug(DEBUG_INFO, "TOS", "linea : 0x%08X", data.lineaVars);

      data.a = VFSAddVolume("/app_tos/a/");
      data.b = VFSAddVolume("/app_tos/b/");
      data.c = VFSAddVolume("/app_tos/c/");
      Dsetdrv(drive);
      Dsetpath(dir);
      debug(DEBUG_INFO, "TOS", "using drive '%c' directory \"%s\"", drive + 'A', dir);

      pc = textStart;
      a7 = stackStart + stackSize - 16;
      state->stackStart = stackStart;

      write_long(a7 + 4, basePageStart);
      write_long(a7, 0); // return address

      MemSet(&main_cpu, sizeof(m68ki_cpu_core), 0);
      main_cpu.tos = 1;
      m68k_set_context(&main_cpu);
      m68k_init();
      m68k_set_cpu_type(M68K_CPU_TYPE_68000);
      m68k_pulse_reset();
      m68k_set_reg(M68K_REG_PC, pc);
      m68k_set_reg(M68K_REG_SP, a7);
      m68k_set_instr_hook_callback(cpu_instr_callback);

      emupalmos_memory_hooks(
        read_byte, read_word, read_long,
        write_byte, write_word, write_long
      );
      FntSetFont(mono8x16Font);

      if (data.debug_m68k) {
        debug(DEBUG_INFO, "TOS", "text disassembly begin");
        m68k_disassemble_range(textStart, dataStart, M68K_CPU_TYPE_68000);
        debug(DEBUG_INFO, "TOS", "text disassembly end");
      }

      for (; !emupalmos_finished() && !thread_must_end();) {
        if (m68k_get_reg(NULL, M68K_REG_PC) == 0) break;
        m68k_execute(&state->m68k_state, 100000);
      }
      r = 0;

      emupalmos_deinstall(oldState);
      MemHandleFree(hMemory);
    } else {
      debug(DEBUG_ERROR, "TOS", "0x601a signature not found");
      debug_bytes(DEBUG_INFO, "TOS", tos, headerSize);
    }
  } else {
    debug(DEBUG_ERROR, "TOS", "invalid TOS pointer (%p) or size (%d)", tos, tosSize);
  }

  return r;
}

int tos_main_vfs(char *path, int argc, char *argv[]) {
  char buf[maxPath], vfsd[16];
  UInt16 volRefNum;
  UInt32 tosSize, numBytesRead;
  FileRef fileRef;
  uint8_t *tos;
  int drive, last, len, i, r = -1;

  if (path && path[0]) {
    if (path[1] == ':') {
      drive = TxtUpperChar(path[0]);
      switch (drive) {
        case 'A':
        case 'B':
        case 'C':
          drive -= 'A';
          StrNCopy(buf, &path[2], sizeof(buf) - 1);
          break;
        default:
          debug(DEBUG_ERROR, "TOS", "invalid drive '%c' on path \"%s\"", drive, path);
          return -1;
      }
    } else {
      drive = 0;
      StrNCopy(buf, path, sizeof(buf) - 1);
    }

    for (i = 0, last = -1; buf[i]; i++) {
      if (buf[i] == '\\') last = i;
    }

    len = StrLen(buf);
    if (len > 0 && (last == -1 || (last >= 0 && last < len - 1))) {
      if (last >= 0) buf[last] = 0;
      StrNPrintF(vfsd, sizeof(vfsd) - 1, "/app_tos/%c/", drive + 'a');
      volRefNum = VFSAddVolume(vfsd);
      if (VFSFileOpen(volRefNum, &buf[last + 1], vfsModeRead, &fileRef) == errNone) {
        if (VFSFileSize(fileRef, &tosSize) == errNone) {
          if ((tos = MemPtrNew(tosSize)) != NULL) {
            if (VFSFileRead(fileRef, tosSize, tos, &numBytesRead) == errNone && numBytesRead == tosSize) {
              r = tos_main_memory(tos, tosSize, drive, buf[0] ? buf : "\\", argc, argv);
            } else {
              debug(DEBUG_ERROR, "TOS", "could not read file \"%s\"", buf);
            }
            MemPtrFree(tos);
          }
        } else {
          debug(DEBUG_ERROR, "TOS", "could not obtain file size for \"%s\"", buf);
        }
        VFSFileClose(fileRef);
      } else {
        debug(DEBUG_ERROR, "TOS", "could not open \"%s\"", buf);
      }
    } else {
      debug(DEBUG_ERROR, "TOS", "invalid path \"%s\"", path);
    }
  }

  return r;
}

int tos_main_resource(DmResType type, DmResID id, int argc, char *argv[]) {
  MemHandle hTos;
  uint8_t *tos;
  int r = -1;

  if ((hTos = DmGet1Resource(type, id)) != NULL) {
    if ((tos = MemHandleLock(hTos)) != NULL) {
      r = tos_main_memory(tos, MemHandleSize(hTos), 0, "\\", argc, argv);
      MemHandleUnlock(hTos);
    }
    DmReleaseResource(hTos);
  } else {
    debug(DEBUG_ERROR, "TOS", "resource not found");
  }

  return r;
}

static uint32_t tos_gemdos_systrap(void) {
  emu_state_t *state = m68k_get_emu_state();
  tos_data_t *data = (tos_data_t *)state->extra;
  uint8_t *memory = data->memory;
  uint32_t sp, opcode, r = 0;
  uint16_t idx;

  sp = m68k_get_reg(NULL, M68K_REG_SP);
  idx = 0;
  opcode = ARG16;

  switch (opcode) {
#include "gemdos_case.c"
    case 72: { // void *Malloc(int32_t number)
        int32_t number = ARG32;
        uint32_t addr = 0;
        if (number == -1) {
          // get size of the largest available memory block
          addr = 128*1024; // XXX
        } else if (number > 0) {
          uint8_t *p = heap_alloc(data->heap, number);
          addr = p ? (p - memory) : 0;
        }
        m68k_set_reg(M68K_REG_D0, addr);
        debug(DEBUG_INFO, "TOS", "GEMDOS Malloc(%d): 0x%08X", number, addr);
      }
      break;
    case 260: { // int32_t Fcntl(int16_t fh, int32_t arg, int16_t cmd)
        int16_t fh = ARG16;
        uint32_t arg = ARG32;
        int32_t cmd = ARG16;
        int32_t res = -1;
        switch (cmd) {
          case 0x5406: // TIOCGPGRP: returns via the parameter arg a pointer to the process group ID of the terminal
            write_long(arg, 1);
            res = 0;
            break;
        }
        m68k_set_reg(M68K_REG_D0, res);
        debug(DEBUG_INFO, "TOS", "GEMDOS Fcntl(%d, %d, 0x%04X): %d", fh, arg, cmd, res);
      }
      break;
    default:
      debug(DEBUG_ERROR, "TOS", "unmapped GEMDOS opcode %d", opcode);
      emupalmos_finish(1);
      break;
  }

  return r;
}

static uint32_t tos_bios_systrap(void) {
  emu_state_t *state = m68k_get_emu_state();
  tos_data_t *data = (tos_data_t *)state->extra;
  uint8_t *memory = data->memory;
  uint32_t sp, opcode, r = 0;
  uint16_t idx;

  sp = m68k_get_reg(NULL, M68K_REG_SP);
  idx = 0;
  opcode = ARG16;

  switch (opcode) {
#include "bios_case.c"
    default:
      debug(DEBUG_ERROR, "TOS", "unmapped BIOS opcode %d", opcode);
      emupalmos_finish(1);
      break;
  }

  return r;
}

static uint32_t tos_xbios_systrap(void) {
  emu_state_t *state = m68k_get_emu_state();
  tos_data_t *data = (tos_data_t *)state->extra;
  uint8_t *memory = data->memory;
  uint32_t sp, opcode, r = 0;
  uint16_t idx;

  sp = m68k_get_reg(NULL, M68K_REG_SP);
  idx = 0;
  opcode = ARG16;

  switch (opcode) {
#include "xbios_case.c"
    default:
      debug(DEBUG_ERROR, "TOS", "unmapped XBIOS opcode %d", opcode);
      emupalmos_finish(1);
      break;
  }

  return r;
}

static uint32_t tos_gem_systrap(void) {
  uint32_t d0, addr, r = 0;
  int16_t i, selector;
  vdi_pb_t vdi_pb;
  aes_pb_t aes_pb;

  d0 = m68k_get_reg(NULL, M68K_REG_D0);
  selector = d0 & 0xFFFF;

  if (selector == -2) {
    // app is checking if GDOS is installed
    // leaving d0 as is means GDOS is not installed
    debug(DEBUG_INFO, "TOS", "GDOS check");

  } else if (selector == 115) {
    // app is calling a VDI opcode
    addr = m68k_get_reg(NULL, M68K_REG_D1);
    vdi_pb.pcontrol = read_long(addr + 0);
    vdi_pb.pintin   = read_long(addr + 4);
    vdi_pb.pptsin   = read_long(addr + 8);
    vdi_pb.pintout  = read_long(addr + 12);
    vdi_pb.pptsout  = read_long(addr + 16);

    vdi_pb.opcode      = read_word(vdi_pb.pcontrol + 0);
    vdi_pb.ptsin_len   = read_word(vdi_pb.pcontrol + 2);
    vdi_pb.intin_len   = read_word(vdi_pb.pcontrol + 6);
    vdi_pb.sub_opcode  = read_word(vdi_pb.pcontrol + 10);
    vdi_pb.workstation = read_word(vdi_pb.pcontrol + 12);

    debug(DEBUG_INFO, "TOS", "VDI opcode %d.%d", vdi_pb.opcode, vdi_pb.sub_opcode);
    vdi_call(&vdi_pb);

    write_word(vdi_pb.pcontrol + 4, vdi_pb.ptsout_len);
    write_word(vdi_pb.pcontrol + 8, vdi_pb.intout_len);

  } else if (selector == 200) {
    // app is calling an AES opcode
    addr = m68k_get_reg(NULL, M68K_REG_D1);
    aes_pb.pcontrol = read_long(addr + 0);
    aes_pb.pglobal  = read_long(addr + 4);
    aes_pb.pintin   = read_long(addr + 8);
    aes_pb.pintout  = read_long(addr + 12);
    aes_pb.padrin   = read_long(addr + 16);
    aes_pb.padrout  = read_long(addr + 20);

    aes_pb.opcode     = read_word(aes_pb.pcontrol + 0);
    aes_pb.intin_len  = read_word(aes_pb.pcontrol + 2);
    aes_pb.adrin_len  = read_word(aes_pb.pcontrol + 6);

    for (i = 0; i < AES_GLOBAL_LEN; i++) {
      aes_pb.global[i] = read_word(aes_pb.pglobal + i*2);
    }

    for (i = 0; i < aes_pb.intin_len && i < AES_INTIN_LEN; i++) {
      aes_pb.intin[i] = read_word(aes_pb.pintin + i*2);
    }

    for (i = 0; i < aes_pb.adrin_len && i < AES_ADRIN_LEN; i++) {
      aes_pb.adrin[i] = read_long(aes_pb.padrin + i*4);
    }

    if (aes_call(&aes_pb) == -1) {
      emupalmos_finish(1);
    }

    write_word(aes_pb.pcontrol + 4, aes_pb.intout_len);
    write_word(aes_pb.pcontrol + 8, aes_pb.adrout_len);

    for (i = 0; i < aes_pb.adrout_len && i < AES_INTOUT_LEN; i++) {
      write_long(aes_pb.padrout + i*4, aes_pb.adrout[i]);
    }

    for (i = 0; i < aes_pb.intout_len && i < AES_ADROUT_LEN; i++) {
      write_word(aes_pb.pintout + i*2, aes_pb.intout[i]);
    }

    for (i = 0; i < AES_GLOBAL_LEN; i++) {
      write_word(aes_pb.pglobal + i*2, aes_pb.global[i]);
    }

  } else {
    debug(DEBUG_ERROR, "TOS", "invalid GEM selector %d", selector);
  }

  return r;
}

static uint32_t tos_linea(uint16_t opcode) {
  emu_state_t *state = m68k_get_emu_state();
  tos_data_t *data = (tos_data_t *)state->extra;

  debug(DEBUG_TRACE, "TOS", "LINEA opcode %d", opcode);

  switch (opcode) {
    case  0: // Initialization
      // output:
      // d0 = pointer to the base address of Line A interface variables
      // a0 = pointer to the base address of Line A interface variables
      // a1 = pointer to the array of pointers to the three system font headers
      // a2 = pointer to array of pointers to the fifteen Line A routines
      m68k_set_reg(M68K_REG_D0, data->lineaVars);
      m68k_set_reg(M68K_REG_A0, data->lineaVars);
      break;
    case  1: // Put pixel
      // input:
      // INTIN[0] = pixel value
      // PTSIN[0] = x coordinate
      // PTSIN[1] = y coordinate
      break;
    case  2: // Get pixel
      // input:
      // PTSIN[0] = x coordinate
      // PTSIN[1] = y coordinate
      // output: d0 = pixel value
      m68k_set_reg(M68K_REG_D0, 0);
      break;
    case  3:
      break;
    case  4:
      break;
    case  5:
      break;
    case  6:
      break;
    case  7:
      break;
    case  8:
      break;
    case  9: // Show mouse
      break;
    case 10: // Hide mouse
      break;
    case 11:
      break;
    case 12:
      break;
    case 13:
      break;
    case 14:
      break;
    case 15:
      break;
  }

  return 0;
}

uint32_t tos_systrap(uint16_t type) {
  uint32_t r = 0;

  switch (type) {
    case 1:
      r = tos_gemdos_systrap();
      break;
    case 2:
      r = tos_gem_systrap();
      break;
    case 13:
      r = tos_bios_systrap();
      break;
    case 14:
      r = tos_xbios_systrap();
      break;
    default:
      if (type >= 0xA000 && type <= 0xA00F) {
        r = tos_linea(type & 0x0FFF);
      } else {
        debug(DEBUG_ERROR, "TOS", "invalid syscall trap #%d", type);
        emupalmos_finish(1);
      }
      break;
  }

  return r;
}
