//
//  Stack.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Stack_hpp
#define Stack_hpp

#include "../AccessType.hpp"

#include <type_traits>

namespace InstructionSet::x86::Primitive {

// The below takes a reference in order properly to handle PUSH SP,
// which should place the value of SP after the push onto the stack.
template <typename IntT, bool preauthorised, typename ContextT>
void push(
	IntT &value,
	ContextT &context
) {
	context.registers.sp_ -= sizeof(IntT);
	if constexpr (preauthorised) {
		context.memory.template preauthorised_write<IntT>(Source::SS, context.registers.sp_, value);
	} else {
		context.memory.template access<IntT, AccessType::Write>(
			Source::SS,
			context.registers.sp_) = value;
	}
	context.memory.template write_back<IntT>();
}

template <typename IntT, bool preauthorised, typename ContextT>
IntT pop(
	ContextT &context
) {
	const auto value = context.memory.template access<IntT, preauthorised ? AccessType::PreauthorisedRead : AccessType::Read>(
		Source::SS,
		context.registers.sp_);
	context.registers.sp_ += sizeof(IntT);
	return value;
}

template <typename ContextT>
void sahf(
	uint8_t &ah,
	ContextT &context
) {
	/*
		EFLAGS(SF:ZF:0:AF:0:PF:1:CF) ← AH;
	*/
	context.flags.template set_from<uint8_t, Flag::Sign>(ah);
	context.flags.template set_from<Flag::Zero>(!(ah & 0x40));
	context.flags.template set_from<Flag::AuxiliaryCarry>(ah & 0x10);
	context.flags.template set_from<Flag::ParityOdd>(!(ah & 0x04));
	context.flags.template set_from<Flag::Carry>(ah & 0x01);
}

template <typename ContextT>
void lahf(
	uint8_t &ah,
	ContextT &context
) {
	/*
		AH ← EFLAGS(SF:ZF:0:AF:0:PF:1:CF);
	*/
	ah =
		(context.flags.template flag<Flag::Sign>() ? 0x80 : 0x00)	|
		(context.flags.template flag<Flag::Zero>() ? 0x40 : 0x00)	|
		(context.flags.template flag<Flag::AuxiliaryCarry>() ? 0x10 : 0x00)	|
		(context.flags.template flag<Flag::ParityOdd>() ? 0x00 : 0x04)	|
		0x02 |
		(context.flags.template flag<Flag::Carry>() ? 0x01 : 0x00);
}

template <typename ContextT>
void popf(
	ContextT &context
) {
	context.flags.set(pop<uint16_t, false>(context));
}

template <typename ContextT>
void pushf(
	ContextT &context
) {
	uint16_t value = context.flags.get();
	push<uint16_t, false>(value, context);
}

template <typename IntT, typename ContextT>
void popa(
	ContextT &context
) {
	context.memory.preauthorise_stack_read(sizeof(IntT) * 8);
	if constexpr (std::is_same_v<IntT, uint32_t>) {
		context.registers.edi() = pop<uint32_t, true>(context);
		context.registers.esi() = pop<uint32_t, true>(context);
		context.registers.ebp() = pop<uint32_t, true>(context);
		context.registers.esp() += 4;
		context.registers.ebx() = pop<uint32_t, true>(context);
		context.registers.edx() = pop<uint32_t, true>(context);
		context.registers.ecx() = pop<uint32_t, true>(context);
		context.registers.eax() = pop<uint32_t, true>(context);
	} else {
		context.registers.di() = pop<uint16_t, true>(context);
		context.registers.si() = pop<uint16_t, true>(context);
		context.registers.bp() = pop<uint16_t, true>(context);
		context.registers.sp() += 2;
		context.registers.bx() = pop<uint16_t, true>(context);
		context.registers.dx() = pop<uint16_t, true>(context);
		context.registers.cx() = pop<uint16_t, true>(context);
		context.registers.ax() = pop<uint16_t, true>(context);
	}
}

template <typename IntT, typename ContextT>
void pusha(
	ContextT &context
) {
	context.memory.preauthorise_stack_read(sizeof(IntT) * 8);
	IntT initial_sp = context.registers.sp();
	if constexpr (std::is_same_v<IntT, uint32_t>) {
		push<uint32_t, true>(context.registers.eax(), context);
		push<uint32_t, true>(context.registers.ecx(), context);
		push<uint32_t, true>(context.registers.edx(), context);
		push<uint32_t, true>(context.registers.ebx(), context);
		push<uint32_t, true>(initial_sp, context);
		push<uint32_t, true>(context.registers.ebp(), context);
		push<uint32_t, true>(context.registers.esi(), context);
		push<uint32_t, true>(context.registers.esi(), context);
	} else {
		push<uint16_t, true>(context.registers.ax(), context);
		push<uint16_t, true>(context.registers.cx(), context);
		push<uint16_t, true>(context.registers.dx(), context);
		push<uint16_t, true>(context.registers.bx(), context);
		push<uint16_t, true>(initial_sp, context);
		push<uint16_t, true>(context.registers.bp(), context);
		push<uint16_t, true>(context.registers.si(), context);
		push<uint16_t, true>(context.registers.si(), context);
	}
}

template <typename IntT, typename ContextT>
void leave(
	ContextT &context
) {
	// TODO: should use StackAddressSize to determine assignment of bp to sp.
	// Probably make that a static constexpr on registers.
	if constexpr (std::is_same_v<IntT, uint32_t>) {
		context.registers.esp() = context.registers.ebp();
		context.registers.ebp() = pop<uint32_t, false>(context);
	} else {
		context.registers.sp() = context.registers.bp();
		context.registers.bp() = pop<uint16_t, false>(context);
	}
}

}

#endif /* Stack_hpp */
