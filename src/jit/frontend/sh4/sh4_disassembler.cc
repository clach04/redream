#include "core/assert.h"
#include "core/memory.h"
#include "core/string.h"
#include "jit/frontend/sh4/sh4_disassembler.h"

using namespace re;
using namespace re::jit::frontend::sh4;

struct InstrType {
  Op op;
  const char *desc;
  const char *sig;
  int cycles;
  int flags;
  uint16_t opcode_mask;
  uint16_t imm_mask, imm_shift;
  uint16_t disp_mask, disp_shift;
  uint16_t rm_mask, rm_shift;
  uint16_t rn_mask, rn_shift;
};

static InstrType s_instrs[NUM_OPCODES] = {
    {OP_INVALID, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
#define SH4_INSTR(name, desc, sig, cycles, flags)                     \
  { OP_##name, desc, #sig, cycles, flags, 0, 0, 0, 0, 0, 0, 0, 0, 0 } \
  ,
#include "jit/frontend/sh4/sh4_instr.inc"
#undef SH4_INSTR
};
static InstrType *s_instr_lookup[UINT16_MAX] = {};

static void GetArgMask(const char *instr_code, char c, uint16_t *mask,
                       uint16_t *shift) {
  size_t len = strlen(instr_code);
  if (mask) *mask = 0;
  if (shift) *shift = 0;
  for (size_t i = 0; i < len; i++) {
    if ((!c && instr_code[i] == '1') || (c && instr_code[i] == c)) {
      if (mask) *mask |= (1 << (len - i - 1));
      if (shift) *shift = static_cast<uint16_t>(len - i - 1);
    }
  }
}

static void InitInstrTables() {
  static bool initialized = false;

  if (initialized) {
    return;
  }

  initialized = true;

  // finalize type information by extracting argument encoding information
  // from signatures
  for (int i = 1 /* skip OP_INVALID */; i < NUM_OPCODES; i++) {
    InstrType *type = &s_instrs[i];

    GetArgMask(type->sig, 'i', &type->imm_mask, &type->imm_shift);
    GetArgMask(type->sig, 'd', &type->disp_mask, &type->disp_shift);
    GetArgMask(type->sig, 'm', &type->rm_mask, &type->rm_shift);
    GetArgMask(type->sig, 'n', &type->rn_mask, &type->rn_shift);
    GetArgMask(type->sig, 0, &type->opcode_mask, NULL);
  }

  // initialize lookup table
  for (int w = 0; w < 0x10000; w += 0x1000) {
    for (int x = 0; x < 0x1000; x += 0x100) {
      for (int y = 0; y < 0x100; y += 0x10) {
        for (int z = 0; z < 0x10; z++) {
          uint16_t value = w + x + y + z;

          for (int i = 1 /* skip OP_INVALID */; i < NUM_OPCODES; i++) {
            InstrType *type = &s_instrs[i];
            uint16_t arg_mask = type->imm_mask | type->disp_mask |
                                type->rm_mask | type->rn_mask;

            if ((value & ~arg_mask) == type->opcode_mask) {
              s_instr_lookup[value] = type;
              break;
            }
          }
        }
      }
    }
  }
}

bool SH4Disassembler::Disasm(Instr *i) {
  InitInstrTables();

  InstrType *type = s_instr_lookup[i->opcode];

  if (!type) {
    i->op = OP_INVALID;
    return false;
  }

  i->op = type->op;
  i->cycles = type->cycles;
  i->flags = type->flags;
  i->Rm = (i->opcode & type->rm_mask) >> type->rm_shift;
  i->Rn = (i->opcode & type->rn_mask) >> type->rn_shift;
  i->disp = (i->opcode & type->disp_mask) >> type->disp_shift;
  i->imm = (i->opcode & type->imm_mask) >> type->imm_shift;

  return true;
}

void SH4Disassembler::Format(const Instr &i, char *buffer, size_t buffer_size) {
  if (i.op == OP_INVALID) {
    snprintf(buffer, buffer_size, "%08x  .word 0x%04x", i.addr, i.opcode);
    return;
  }

  char value[128];
  size_t value_len;
  uint32_t movsize;
  uint32_t pcmask;

  // copy initial formatted description
  snprintf(buffer, buffer_size, "%08x  %s", i.addr, s_instrs[i.op].desc);

  // used by mov operators with displacements
  if (strnstr(buffer, ".b", buffer_size)) {
    movsize = 1;
    pcmask = 0xffffffff;
  } else if (strnstr(buffer, ".w", buffer_size)) {
    movsize = 2;
    pcmask = 0xffffffff;
  } else if (strnstr(buffer, ".l", buffer_size)) {
    movsize = 4;
    pcmask = 0xfffffffc;
  } else {
    movsize = 0;
    pcmask = 0;
  }

  // (disp:4,rn)
  value_len = snprintf(value, sizeof(value), "(0x%x,rn)", i.disp * movsize);
  CHECK_EQ(strnrep(buffer, buffer_size, "(disp:4,rn)", 11, value, value_len),
           0);

  // (disp:4,rm)
  value_len = snprintf(value, sizeof(value), "(0x%x,rm)", i.disp * movsize);
  CHECK_EQ(strnrep(buffer, buffer_size, "(disp:4,rm)", 11, value, value_len),
           0);

  // (disp:8,gbr)
  value_len = snprintf(value, sizeof(value), "(0x%x,gbr)", i.disp * movsize);
  CHECK_EQ(strnrep(buffer, buffer_size, "(disp:8,gbr)", 12, value, value_len),
           0);

  // (disp:8,pc)
  value_len = snprintf(value, sizeof(value), "(0x%08x)",
                       (i.disp * movsize) + (i.addr & pcmask) + 4);
  CHECK_EQ(strnrep(buffer, buffer_size, "(disp:8,pc)", 11, value, value_len),
           0);

  // disp:8
  value_len = snprintf(value, sizeof(value), "0x%08x",
                       ((int8_t)i.disp * 2) + i.addr + 4);
  CHECK_EQ(strnrep(buffer, buffer_size, "disp:8", 6, value, value_len), 0);

  // disp:12
  value_len =
      snprintf(value, sizeof(value), "0x%08x",
               ((((int32_t)(i.disp & 0xfff) << 20) >> 20) * 2) + i.addr + 4);
  CHECK_EQ(strnrep(buffer, buffer_size, "disp:12", 7, value, value_len), 0);

  // drm
  value_len = snprintf(value, sizeof(value), "dr%d", i.Rm);
  CHECK_EQ(strnrep(buffer, buffer_size, "drm", 3, value, value_len), 0);

  // drn
  value_len = snprintf(value, sizeof(value), "dr%d", i.Rn);
  CHECK_EQ(strnrep(buffer, buffer_size, "drn", 3, value, value_len), 0);

  // frm
  value_len = snprintf(value, sizeof(value), "fr%d", i.Rm);
  CHECK_EQ(strnrep(buffer, buffer_size, "frm", 3, value, value_len), 0);

  // frn
  value_len = snprintf(value, sizeof(value), "fr%d", i.Rn);
  CHECK_EQ(strnrep(buffer, buffer_size, "frn", 3, value, value_len), 0);

  // fvm
  value_len = snprintf(value, sizeof(value), "fv%d", i.Rm);
  CHECK_EQ(strnrep(buffer, buffer_size, "fvm", 3, value, value_len), 0);

  // fvn
  value_len = snprintf(value, sizeof(value), "fv%d", i.Rn);
  CHECK_EQ(strnrep(buffer, buffer_size, "fvn", 3, value, value_len), 0);

  // rm
  value_len = snprintf(value, sizeof(value), "r%d", i.Rm);
  CHECK_EQ(strnrep(buffer, buffer_size, "rm", 2, value, value_len), 0);

  // rn
  value_len = snprintf(value, sizeof(value), "r%d", i.Rn);
  CHECK_EQ(strnrep(buffer, buffer_size, "rn", 2, value, value_len), 0);

  // #imm8
  value_len = snprintf(value, sizeof(value), "0x%02x", i.imm);
  CHECK_EQ(strnrep(buffer, buffer_size, "#imm8", 5, value, value_len), 0);
}
