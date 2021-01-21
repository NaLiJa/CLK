//
//  Executor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/1/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Executor.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

using namespace InstructionSet::M50740;

Executor::Executor() {
	// Cut down the list of all generated performers to those the processor actually uses, and install that
	// for future referencing by action_for.
	Decoder decoder;
	for(size_t c = 0; c < 256; c++) {
		const auto instruction = decoder.instrucion_for_opcode(uint8_t(c));

		// Treat invalid as NOP, because I've got to do _something_.
		if(instruction.operation == Operation::Invalid) {
			performers_[c] = performer_lookup_.performer(Operation::NOP, instruction.addressing_mode);
		} else {
			performers_[c] = performer_lookup_.performer(instruction.operation, instruction.addressing_mode);
		}
	}
}

void Executor::set_rom(const std::vector<uint8_t> &rom) {
	// Copy into place, and reset.
	const auto length = std::min(size_t(0x1000), rom.size());
	memcpy(&memory_[0x2000 - length], rom.data(), length);
	reset();

	// TEMPORARY: just to test initial wiring.
	for(int c = 0; c < 130; c++) {
		run_to_branch();
	}
}

void Executor::reset() {
	// Just jump to the reset vector.
	set_program_counter(uint16_t(memory_[0x1ffe] | (memory_[0x1fff] << 8)));
}

uint8_t Executor::read(uint16_t address) {
	address &= 0x1fff;
	switch(address) {
		default: return memory_[address];

		// TODO: external IO ports.

		// "Port R"; sixteen four-bit ports
		case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd7:
		case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
			printf("TODO: Port R\n");
		return 0xff;

		// Ports P0–P3.
		case 0xe0: case 0xe1:
		case 0xe2: case 0xe3:
		case 0xe4: case 0xe5:
		case 0xe8: case 0xe9:
			printf("TODO: Ports P0–P3\n");
		return 0xff;

		// Timers.
		case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
			printf("TODO: Timers\n");
		return 0xff;
	}
}

void Executor::write(uint16_t address, uint8_t value) {
	address &= 0x1fff;
	if(address < 0x60) {
		memory_[address] = value;
		return;
	}

	// TODO: all external IO ports.
}

void Executor::push(uint8_t value) {
	write(s_, value);
	--s_;
}

uint8_t Executor::pull() {
	++s_;
	return read(s_);
}

void Executor::set_flags(uint8_t flags) {
	negative_result_ = flags;
	overflow_result_ = uint8_t(flags << 1);
	index_mode_ = flags & 0x20;
	decimal_mode_ = flags & 0x08;
	interrupt_disable_ = flags & 0x04;
	zero_result_ = !(flags & 0x02);
	carry_flag_ = flags & 0x01;
}

uint8_t Executor::flags() {
	return
		(negative_result_ & 0x80) |
		((overflow_result_ & 0x80) >> 1) |
		(index_mode_ ? 0x20 : 0x00) |
		(decimal_mode_ ? 0x08 : 0x00) |
		interrupt_disable_ |
		(zero_result_ ? 0x00 : 0x02) |
		carry_flag_;
}

template <Operation operation, AddressingMode addressing_mode> void Executor::perform() {
	// Deal with all modes that don't access memory up here;
	// those that access memory will go through a slightly longer
	// sequence below that wraps the address and checks whether
	// a write is valid [if required].

	int address;
#define next8()		memory_[(program_counter_ + 1) & 0x1fff]
#define next16()	(memory_[(program_counter_ + 1) & 0x1fff] | (memory_[(program_counter_ + 2) & 0x1fff] << 8))

	printf("%04x\t%02x\t%d %d\t[x:%02x s:%02x]\n", program_counter_ & 0x1fff, memory_[program_counter_ & 0x1fff], int(operation), int(addressing_mode), x_, s_);

	// Underlying assumption below: the instruction stream will never
	// overlap with IO ports.
	switch(addressing_mode) {

		// Addressing modes with no further memory access.

			case AddressingMode::Implied:
				perform<operation>(nullptr);
				++program_counter_;
			return;

			case AddressingMode::Accumulator:
				perform<operation>(&a_);
				++program_counter_;
			return;

			case AddressingMode::Immediate:
				perform<operation>(&next8());
				program_counter_ += 2;
			return;

		// Special-purpose addressing modes.

			case AddressingMode::Relative:
				address = program_counter_ + 1 + size(addressing_mode) + int8_t(next8());
			break;

			case AddressingMode::SpecialPage:	address = 0x1f00 | next8();			break;

			case AddressingMode::ImmediateZeroPage:
				// LDM only...
				write(memory_[(program_counter_+2)&0x1fff], memory_[(program_counter_+1)&0x1fff]);
				program_counter_ += 1 + size(addressing_mode);
			return;

			/* TODO:

					AccumulatorRelative
					ZeroPageRelative

					... which are BBC/BBS-exclusive.
			*/

		// Addressing modes with a memory access.

			case AddressingMode::Absolute:		address = next16();					break;
			case AddressingMode::AbsoluteX:		address = next16() + x_;			break;
			case AddressingMode::AbsoluteY:		address = next16() + y_;			break;
			case AddressingMode::ZeroPage:		address = next8();					break;
			case AddressingMode::ZeroPageX:		address = (next8() + x_) & 0xff;	break;
			case AddressingMode::ZeroPageY:		address = (next8() + x_) & 0xff;	break;

			case AddressingMode::ZeroPageIndirect:
				address = next8();
				address = memory_[address] | (memory_[(address + 1) & 0xff] << 8);
			break;

			case AddressingMode::XIndirect:
				address = (next8() + x_) & 0xff;
				address = memory_[address] | (memory_[(address + 1)&0xff] << 8);
			break;

			case AddressingMode::IndirectY:
				address = (memory_[next8()] | (memory_[(next8()+1)&0xff] << 8)) + y_;
			break;

			case AddressingMode::AbsoluteIndirect:
				address = next16();
				address = memory_[address] | (memory_[(address + 1) & 0x1fff] << 8);
			break;

			default:
				assert(false);
	}

#undef next16
#undef next8
	program_counter_ += 1 + size(addressing_mode);

	// Check for a branch; those don't go through the memory accesses below.
	switch(operation) {
		case Operation::BRA: case Operation::JMP:
			set_program_counter(uint16_t(address));
		return;

		case Operation::JSR: {
			const auto return_address = program_counter_ - 1;
			push(uint8_t(return_address >> 8));
			push(uint8_t(return_address & 0xff));
			set_program_counter(uint16_t(address));
		} return;

		case Operation::BPL:	if(!(negative_result_&0x80))	set_program_counter(uint16_t(address));	return;
		case Operation::BMI:	if(negative_result_&0x80)		set_program_counter(uint16_t(address));	return;
		case Operation::BEQ:	if(!zero_result_)				set_program_counter(uint16_t(address));	return;
		case Operation::BNE:	if(zero_result_)				set_program_counter(uint16_t(address));	return;
		case Operation::BCS:	if(carry_flag_)					set_program_counter(uint16_t(address));	return;
		case Operation::BCC:	if(!carry_flag_)				set_program_counter(uint16_t(address));	return;
		case Operation::BVS:	if(overflow_result_ & 0x80)		set_program_counter(uint16_t(address));	return;
		case Operation::BVC:	if(!(overflow_result_ & 0x80))	set_program_counter(uint16_t(address));	return;

		/* TODO: BBC, BBS. */

		default: break;
	}


	assert(access_type(operation) != AccessType::None);

	if constexpr(access_type(operation) == AccessType::Read) {
		uint8_t source = read(uint16_t(address));
		perform<operation>(&source);
		return;
	}

	uint8_t value;
	if constexpr(access_type(operation) == AccessType::ReadModifyWrite) {
		value = read(uint16_t(address));
	} else {
		value = 0xff;
	}
	perform<operation>(&value);
	write(uint16_t(address), value);
}

template <Operation operation> void Executor::perform(uint8_t *operand [[maybe_unused]]) {
#define set_nz(a)	negative_result_ = zero_result_ = (a)
	switch(operation) {
		case Operation::LDA:	set_nz(a_ = *operand);	break;
		case Operation::LDX:	set_nz(x_ = *operand);	break;
		case Operation::LDY:	set_nz(y_ = *operand);	break;

		case Operation::STA:	*operand = a_;	break;
		case Operation::STX:	*operand = x_;	break;
		case Operation::STY:	*operand = y_;	break;

		case Operation::TXA:	set_nz(a_ = x_);	break;
		case Operation::TYA:	set_nz(a_ = y_);	break;
		case Operation::TXS:	s_ = x_;			break;
		case Operation::TAX:	set_nz(x_ = a_);	break;
		case Operation::TAY:	set_nz(y_ = a_);	break;
		case Operation::TSX:	set_nz(x_ = s_);	break;

		case Operation::SEB0:	case Operation::SEB1:	case Operation::SEB2:	case Operation::SEB3:
		case Operation::SEB4:	case Operation::SEB5:	case Operation::SEB6:	case Operation::SEB7:
			*operand |= 1 << (int(operation) - int(Operation::SEB0));
		break;
		case Operation::CLB0:	case Operation::CLB1:	case Operation::CLB2:	case Operation::CLB3:
		case Operation::CLB4:	case Operation::CLB5:	case Operation::CLB6:	case Operation::CLB7:
			*operand &= ~(1 << (int(operation) - int(Operation::CLB0)));
		break;

		case Operation::CLI:	interrupt_disable_ = 0x00;		break;
		case Operation::SEI:	interrupt_disable_ = 0x04;		break;
		case Operation::CLT:	index_mode_ = false;			break;
		case Operation::SET:	index_mode_ = true;				break;
		case Operation::CLD:	decimal_mode_ = false;			break;
		case Operation::SED:	decimal_mode_ = true;			break;
		case Operation::CLC:	carry_flag_ = 0;				break;
		case Operation::SEC:	carry_flag_ = 1;				break;
		case Operation::CLV:	overflow_result_ = 0;			break;

		case Operation::DEX:	--x_; set_nz(x_);				break;
		case Operation::INX:	++x_; set_nz(x_);				break;
		case Operation::DEY:	--y_; set_nz(y_);				break;
		case Operation::INY:	++y_; set_nz(y_);				break;
		case Operation::DEC:	--*operand; set_nz(*operand);	break;
		case Operation::INC:	++*operand; set_nz(*operand);	break;

		case Operation::RTS: {
			uint16_t target = pull();
			target |= pull() << 8;
			set_program_counter(target+1);
			--program_counter_;				// To undo the unavoidable increment
											// after exiting from here.
		} break;

		case Operation::RTI: {
			set_flags(pull());
			uint16_t target = pull();
			target |= pull() << 8;
			set_program_counter(target);
			--program_counter_;				// To undo the unavoidable increment
											// after exiting from here.
		} break;

		case Operation::ORA:	set_nz(a_ |= *operand);	break;
		case Operation::AND:	set_nz(a_ &= *operand);	break;
		case Operation::EOR:	set_nz(a_ ^= *operand);	break;

		// TODO:
		//
		//	BRK, FST, SLW, NOP, PHA, PHP, PLA, PLP, STP,
		//	ADC, SBC, BIT, CMP, CPX, CPY, ASL, LSR, COM, ROL, ROR, RRF

		/*
			Operations affected by the index mode flag: ADC, AND, CMP, EOR, LDA, ORA, and SBC.
		*/

		/*
			Already removed from the instruction stream:

				* all branches and jumps;
				* LDM.
		*/

		default:
			printf("Unimplemented operation: %d\n", int(operation));
			assert(false);
	}
#undef set_nz
}

void Executor::set_program_counter(uint16_t address) {
	printf("--- %04x ---\n", (address & 0x1fff) - 0x1000);
	program_counter_ = address;
	CachingExecutor::set_program_counter(address);
}
