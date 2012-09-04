//===-- MipsMCInstLower.cpp - Convert Mips MachineInstr to MCInst ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains code to lower Mips MachineInstrs to their corresponding
// MCInst records.
//
//===----------------------------------------------------------------------===//

#include "MipsMCInstLower.h"
#include "MipsAsmPrinter.h"
#include "MipsInstrInfo.h"
#include "MCTargetDesc/MipsBaseInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Target/Mangler.h"

using namespace llvm;

MipsMCInstLower::MipsMCInstLower(MipsAsmPrinter &asmprinter)
  : AsmPrinter(asmprinter) {}

void MipsMCInstLower::Initialize(Mangler *M, MCContext *C) {
  Mang = M;
  Ctx = C;
}

MCOperand MipsMCInstLower::LowerSymbolOperand(const MachineOperand &MO,
                                              MachineOperandType MOTy,
                                              unsigned Offset) const {
  MCSymbolRefExpr::VariantKind Kind;
  const MCSymbol *Symbol;

  switch(MO.getTargetFlags()) {
  default:                   llvm_unreachable("Invalid target flag!");
  case MipsII::MO_NO_FLAG:   Kind = MCSymbolRefExpr::VK_None; break;
  case MipsII::MO_GPREL:     Kind = MCSymbolRefExpr::VK_Mips_GPREL; break;
  case MipsII::MO_GOT_CALL:  Kind = MCSymbolRefExpr::VK_Mips_GOT_CALL; break;
  case MipsII::MO_GOT16:     Kind = MCSymbolRefExpr::VK_Mips_GOT16; break;
  case MipsII::MO_GOT:       Kind = MCSymbolRefExpr::VK_Mips_GOT; break;
  case MipsII::MO_ABS_HI:    Kind = MCSymbolRefExpr::VK_Mips_ABS_HI; break;
  case MipsII::MO_ABS_LO:    Kind = MCSymbolRefExpr::VK_Mips_ABS_LO; break;
  case MipsII::MO_TLSGD:     Kind = MCSymbolRefExpr::VK_Mips_TLSGD; break;
  case MipsII::MO_TLSLDM:    Kind = MCSymbolRefExpr::VK_Mips_TLSLDM; break;
  case MipsII::MO_DTPREL_HI: Kind = MCSymbolRefExpr::VK_Mips_DTPREL_HI; break;
  case MipsII::MO_DTPREL_LO: Kind = MCSymbolRefExpr::VK_Mips_DTPREL_LO; break;
  case MipsII::MO_GOTTPREL:  Kind = MCSymbolRefExpr::VK_Mips_GOTTPREL; break;
  case MipsII::MO_TPREL_HI:  Kind = MCSymbolRefExpr::VK_Mips_TPREL_HI; break;
  case MipsII::MO_TPREL_LO:  Kind = MCSymbolRefExpr::VK_Mips_TPREL_LO; break;
  case MipsII::MO_GPOFF_HI:  Kind = MCSymbolRefExpr::VK_Mips_GPOFF_HI; break;
  case MipsII::MO_GPOFF_LO:  Kind = MCSymbolRefExpr::VK_Mips_GPOFF_LO; break;
  case MipsII::MO_GOT_DISP:  Kind = MCSymbolRefExpr::VK_Mips_GOT_DISP; break;
  case MipsII::MO_GOT_PAGE:  Kind = MCSymbolRefExpr::VK_Mips_GOT_PAGE; break;
  case MipsII::MO_GOT_OFST:  Kind = MCSymbolRefExpr::VK_Mips_GOT_OFST; break;
  case MipsII::MO_HIGHER:    Kind = MCSymbolRefExpr::VK_Mips_HIGHER; break;
  case MipsII::MO_HIGHEST:   Kind = MCSymbolRefExpr::VK_Mips_HIGHEST; break;
  }

  switch (MOTy) {
  case MachineOperand::MO_MachineBasicBlock:
    Symbol = MO.getMBB()->getSymbol();
    break;

  case MachineOperand::MO_GlobalAddress:
    Symbol = Mang->getSymbol(MO.getGlobal());
    Offset += MO.getOffset();
    break;

  case MachineOperand::MO_BlockAddress:
    Symbol = AsmPrinter.GetBlockAddressSymbol(MO.getBlockAddress());
    Offset += MO.getOffset();
    break;

  case MachineOperand::MO_ExternalSymbol:
    Symbol = AsmPrinter.GetExternalSymbolSymbol(MO.getSymbolName());
    Offset += MO.getOffset();
    break;

  case MachineOperand::MO_JumpTableIndex:
    Symbol = AsmPrinter.GetJTISymbol(MO.getIndex());
    break;

  case MachineOperand::MO_ConstantPoolIndex:
    Symbol = AsmPrinter.GetCPISymbol(MO.getIndex());
    Offset += MO.getOffset();
    break;

  default:
    llvm_unreachable("<unknown operand type>");
  }

  const MCSymbolRefExpr *MCSym = MCSymbolRefExpr::Create(Symbol, Kind, *Ctx);

  if (!Offset)
    return MCOperand::CreateExpr(MCSym);

  // Assume offset is never negative.
  assert(Offset > 0);

  const MCConstantExpr *OffsetExpr =  MCConstantExpr::Create(Offset, *Ctx);
  const MCBinaryExpr *Add = MCBinaryExpr::CreateAdd(MCSym, OffsetExpr, *Ctx);
  return MCOperand::CreateExpr(Add);
}

/*
static void CreateMCInst(MCInst& Inst, unsigned Opc, const MCOperand &Opnd0,
                         const MCOperand &Opnd1,
                         const MCOperand &Opnd2 = MCOperand()) {
  Inst.setOpcode(Opc);
  Inst.addOperand(Opnd0);
  Inst.addOperand(Opnd1);
  if (Opnd2.isValid())
    Inst.addOperand(Opnd2);
}
*/

MCOperand MipsMCInstLower::LowerOperand(const MachineOperand &MO,
                                        unsigned offset) const {
  MachineOperandType MOTy = MO.getType();

  switch (MOTy) {
  default: llvm_unreachable("unknown operand type");
  case MachineOperand::MO_Register:
    // Ignore all implicit register operands.
    if (MO.isImplicit()) break;
    return MCOperand::CreateReg(MO.getReg());
  case MachineOperand::MO_Immediate:
    return MCOperand::CreateImm(MO.getImm() + offset);
  case MachineOperand::MO_MachineBasicBlock:
  case MachineOperand::MO_GlobalAddress:
  case MachineOperand::MO_ExternalSymbol:
  case MachineOperand::MO_JumpTableIndex:
  case MachineOperand::MO_ConstantPoolIndex:
  case MachineOperand::MO_BlockAddress:
    return LowerSymbolOperand(MO, MOTy, offset);
  case MachineOperand::MO_RegisterMask:
    break;
 }

  return MCOperand();
}

void MipsMCInstLower::Lower(const MachineInstr *MI, MCInst &OutMI) const {
  OutMI.setOpcode(MI->getOpcode());

  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
    const MachineOperand &MO = MI->getOperand(i);
    MCOperand MCOp = LowerOperand(MO);

    if (MCOp.isValid())
      OutMI.addOperand(MCOp);
  }
}

// If the D<shift> instruction has a shift amount that is greater
// than 31 (checked in calling routine), lower it to a D<shift>32 instruction
void MipsMCInstLower::LowerLargeShift(const MachineInstr *MI,
                                      MCInst& Inst,
                                      int64_t Shift) {
  // rt
  Inst.addOperand(LowerOperand(MI->getOperand(0)));
  // rd
  Inst.addOperand(LowerOperand(MI->getOperand(1)));
  // saminus32
  Inst.addOperand(MCOperand::CreateImm(Shift));

  switch (MI->getOpcode()) {
  default:
    // Calling function is not synchronized
    llvm_unreachable("Unexpected shift instruction");
    break;
  case Mips::DSLL:
    Inst.setOpcode(Mips::DSLL32);
    break;
  case Mips::DSRL:
    Inst.setOpcode(Mips::DSRL32);
    break;
  case Mips::DSRA:
    Inst.setOpcode(Mips::DSRA32);
    break;
  }
}

// Pick a DEXT instruction variant based on the pos and size operands
void MipsMCInstLower::LowerDEXT(const MachineInstr *MI,  MCInst& Inst) {

  assert(MI->getNumOperands() == 4 && "Invalid no. of machine operands for DEXT!");
  assert(MI->getOperand(2).isImm());
  int64_t pos = MI->getOperand(2).getImm();
  assert(MI->getOperand(3).isImm());
  int64_t size = MI->getOperand(3).getImm();

  // rt
  Inst.addOperand(LowerOperand(MI->getOperand(0)));
  // rs
  Inst.addOperand(LowerOperand(MI->getOperand(1)));

  // DEXT
  if ((pos < 32) && (size <= 32)) {
    Inst.addOperand(MCOperand::CreateImm(pos));
    Inst.addOperand(MCOperand::CreateImm(size));
    Inst.setOpcode(Mips::DEXT);
  }
  // DEXTU
  else if ((pos < 64) && (size <= 32)) {
    Inst.addOperand(MCOperand::CreateImm(pos - 32));
    Inst.addOperand(MCOperand::CreateImm(size));
    Inst.setOpcode(Mips::DEXTU);
  }
  // DEXTM
  else {
    Inst.addOperand(MCOperand::CreateImm(pos));
    Inst.addOperand(MCOperand::CreateImm(size - 32));
    Inst.setOpcode(Mips::DEXTM);
  }
  return;
}