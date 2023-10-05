//
//  Instruction.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "Instruction.hpp"

#include <cassert>

using namespace InstructionSet::x86;

bool InstructionSet::x86::has_displacement(Operation operation) {
	switch(operation) {
		default: return false;

		case Operation::JO:			case Operation::JNO:
		case Operation::JB:			case Operation::JNB:
		case Operation::JZ:			case Operation::JNZ:
		case Operation::JBE:		case Operation::JNBE:
		case Operation::JS:			case Operation::JNS:
		case Operation::JP:			case Operation::JNP:
		case Operation::JL:			case Operation::JNL:
		case Operation::JLE:		case Operation::JNLE:
		case Operation::LOOPNE:		case Operation::LOOPE:
		case Operation::LOOP:		case Operation::JCXZ:
		case Operation::CALLrel:	case Operation::JMPrel:
			return true;
	}
}

int InstructionSet::x86::max_displayed_operands(Operation operation) {
	switch(operation) {
		default:	return 2;

		case Operation::INC:	case Operation::DEC:
		case Operation::POP:	case Operation::PUSH:
		case Operation::MUL:	case Operation::IMUL_1:
		case Operation::IDIV:	case Operation::DIV:
		case Operation::ESC:
		case Operation::AAM:	case Operation::AAD:
		case Operation::INT:
		case Operation::JMPabs:	case Operation::JMPfar:
		case Operation::CALLabs:	case Operation::CALLfar:
		case Operation::NEG:	case Operation::NOT:
		case Operation::RETnear:
		case Operation::RETfar:
			return 1;

		// Pedantically, these have an displacement rather than an operand.
		case Operation::JO:		case Operation::JNO:
		case Operation::JB:		case Operation::JNB:
		case Operation::JZ:		case Operation::JNZ:
		case Operation::JBE:	case Operation::JNBE:
		case Operation::JS:		case Operation::JNS:
		case Operation::JP:		case Operation::JNP:
		case Operation::JL:		case Operation::JNL:
		case Operation::JLE:	case Operation::JNLE:
		case Operation::LOOPNE:		case Operation::LOOPE:
		case Operation::LOOP:		case Operation::JCXZ:
		case Operation::CALLrel:	case Operation::JMPrel:
		// Genuine zero-operand instructions:
		case Operation::CMPS:	case Operation::LODS:
		case Operation::MOVS:	case Operation::SCAS:
		case Operation::STOS:
		case Operation::CLC:	case Operation::CLD:
		case Operation::CLI:
		case Operation::STC:	case Operation::STD:
		case Operation::STI:
		case Operation::CMC:
		case Operation::LAHF:	case Operation::SAHF:
		case Operation::AAA:	case Operation::AAS:
		case Operation::DAA:	case Operation::DAS:
		case Operation::CBW:	case Operation::CWD:
		case Operation::INTO:
		case Operation::PUSHF:	case Operation::POPF:
		case Operation::IRET:
		case Operation::NOP:
		case Operation::XLAT:
		case Operation::SALC:
		case Operation::Invalid:
			return 0;
	}
}

std::string InstructionSet::x86::to_string(Operation operation, DataSize size, Model model) {
	switch(operation) {
		case Operation::AAA:	return "aaa";
		case Operation::AAD:	return "aad";
		case Operation::AAM:	return "aam";
		case Operation::AAS:	return "aas";
		case Operation::DAA:	return "daa";
		case Operation::DAS:	return "das";

		case Operation::CBW:	return "cbw";
		case Operation::CWD:	return "cwd";
		case Operation::ESC:	return "esc";

		case Operation::HLT:	return "hlt";
		case Operation::WAIT:	return "wait";

		case Operation::ADC:	return "adc";
		case Operation::ADD:	return "add";
		case Operation::SBB:	return "sbb";
		case Operation::SUB:	return "sub";
		case Operation::MUL:	return "mul";
		case Operation::IMUL_1:	return "imul";
		case Operation::DIV:	return "div";
		case Operation::IDIV:	return "idiv";

		case Operation::INC:	return "inc";
		case Operation::DEC:	return "dec";

		case Operation::IN:		return "in";
		case Operation::OUT:	return "out";

		case Operation::JO:		return "jo";
		case Operation::JNO:	return "jno";
		case Operation::JB:		return "jb";
		case Operation::JNB:	return "jnb";
		case Operation::JZ:		return "jz";
		case Operation::JNZ:	return "jnz";
		case Operation::JBE:	return "jbe";
		case Operation::JNBE:	return "jnbe";
		case Operation::JS:		return "js";
		case Operation::JNS:	return "jns";
		case Operation::JP:		return "jp";
		case Operation::JNP:	return "jnp";
		case Operation::JL:		return "jl";
		case Operation::JNL:	return "jnl";
		case Operation::JLE:	return "jle";
		case Operation::JNLE:	return "jnle";

		case Operation::CALLabs:	return "call";
		case Operation::CALLrel:	return "call";
		case Operation::CALLfar:	return "callf";
		case Operation::IRET:		return "iret";
		case Operation::RETfar:		return "retf";
		case Operation::RETnear:	return "retn";
		case Operation::JMPabs:		return "jmp";
		case Operation::JMPrel:		return "jmp";
		case Operation::JMPfar:		return "jmpf";
		case Operation::JCXZ:		return "jcxz";
		case Operation::INT:		return "int";
		case Operation::INTO:		return "into";

		case Operation::LAHF:	return "lahf";
		case Operation::SAHF:	return "sahf";
		case Operation::LDS:	return "lds";
		case Operation::LES:	return "les";
		case Operation::LEA:	return "lea";

		case Operation::CMPS: {
			constexpr char sizes[][6] = { "cmpsb", "cmpsw", "cmpsd", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Operation::LODS: {
			constexpr char sizes[][6] = { "lodsb", "lodsw", "lodsd", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Operation::MOVS: {
			constexpr char sizes[][6] = { "movsb", "movsw", "movsd", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Operation::SCAS: {
			constexpr char sizes[][6] = { "scasb", "scasw", "scasd", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Operation::STOS: {
			constexpr char sizes[][6] = { "stosb", "stosw", "stosd", "?" };
			return sizes[static_cast<int>(size)];
		}

		case Operation::LOOP:	return "loop";
		case Operation::LOOPE:	return "loope";
		case Operation::LOOPNE:	return "loopne";

		case Operation::MOV:	return "mov";
		case Operation::NEG:	return "neg";
		case Operation::NOT:	return "not";
		case Operation::AND:	return "and";
		case Operation::OR:		return "or";
		case Operation::XOR:	return "xor";
		case Operation::NOP:	return "nop";
		case Operation::POP:	return "pop";
		case Operation::POPF:	return "popf";
		case Operation::PUSH:	return "push";
		case Operation::PUSHF:	return "pushf";
		case Operation::RCL:	return "rcl";
		case Operation::RCR:	return "rcr";
		case Operation::ROL:	return "rol";
		case Operation::ROR:	return "ror";
		case Operation::SAL:	return "sal";
		case Operation::SAR:	return "sar";
		case Operation::SHR:	return "shr";

		case Operation::CLC:	return "clc";
		case Operation::CLD:	return "cld";
		case Operation::CLI:	return "cli";
		case Operation::STC:	return "stc";
		case Operation::STD:	return "std";
		case Operation::STI:	return "sti";
		case Operation::CMC:	return "cmc";

		case Operation::CMP:	return "cmp";
		case Operation::TEST:	return "test";

		case Operation::XCHG:	return "xchg";
		case Operation::XLAT:	return "xlat";
		case Operation::SALC:	return "salc";

		case Operation::SETMO:
			if(model == Model::i8086) {
				return "setmo";
			} else {
				return  "enter";
			}

		case Operation::SETMOC:
			if(model == Model::i8086) {
				return "setmoc";
			} else {
				return "bound";
			}

		case Operation::Invalid:	return "invalid";

		default:
			assert(false);
			return "";
	}
}

bool InstructionSet::x86::mnemonic_implies_data_size(Operation operation) {
	switch(operation) {
		default:	return false;

		case Operation::CMPS:
		case Operation::LODS:
		case Operation::MOVS:
		case Operation::SCAS:
		case Operation::STOS:
		case Operation::JMPrel:
		case Operation::LEA:
			return true;
	}
}

std::string InstructionSet::x86::to_string(DataSize size) {
	constexpr char sizes[][6] = { "byte", "word", "dword", "?" };
	return sizes[static_cast<int>(size)];
}

std::string InstructionSet::x86::to_string(Source source, DataSize size) {
	switch(source) {
		case Source::eAX: {
			constexpr char sizes[][4] = { "al", "ax", "eax", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eCX: {
			constexpr char sizes[][4] = { "cl", "cx", "ecx", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eDX: {
			constexpr char sizes[][4] = { "dl", "dx", "edx", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eBX: {
			constexpr char sizes[][4] = { "bl", "bx", "ebx", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eSPorAH: {
			constexpr char sizes[][4] = { "ah", "sp", "esp", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eBPorCH: {
			constexpr char sizes[][4] = { "ch", "bp", "ebp", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eSIorDH: {
			constexpr char sizes[][4] = { "dh", "si", "esi", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eDIorBH: {
			constexpr char sizes[][4] = { "bh", "di", "edi", "?" };
			return sizes[static_cast<int>(size)];
		}

		case Source::ES:	return "es";
		case Source::CS:	return "cs";
		case Source::SS:	return "ss";
		case Source::DS:	return "ds";
		case Source::FS:	return "fd";
		case Source::GS:	return "gs";

		case Source::None:				return "0";
		case Source::DirectAddress:		return "DirectAccess";
		case Source::Immediate:			return "Immediate";
		case Source::Indirect:			return "Indirect";
		case Source::IndirectNoBase:	return "IndirectNoBase";

		default: return "???";
	}
}
