//
//  8088Tests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 13/09/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <array>
#include <cassert>

#include <iostream>
#include <sstream>
#include <fstream>

#include "NSData+dataWithContentsOfGZippedFile.h"

#include "../../../InstructionSets/x86/Decoder.hpp"
#include "../../../InstructionSets/x86/Perform.hpp"
#include "../../../Numeric/RegisterSizes.hpp"

namespace {

// The tests themselves are not duplicated in this repository;
// provide their real path here.
constexpr char TestSuiteHome[] = "/Users/tharte/Projects/ProcessorTests/8088/v1";

using Status = InstructionSet::x86::Status;
struct Registers {
	CPU::RegisterPair16 ax_;
	uint8_t &al()	{	return ax_.halves.low;	}
	uint8_t &ah()	{	return ax_.halves.high;	}
	uint16_t &ax()	{	return ax_.full;		}

	CPU::RegisterPair16 &axp()	{	return ax_;	}

	CPU::RegisterPair16 cx_;
	uint8_t &cl()	{	return cx_.halves.low;	}
	uint8_t &ch()	{	return cx_.halves.high;	}
	uint16_t &cx()	{	return cx_.full;		}

	CPU::RegisterPair16 dx_;
	uint8_t &dl()	{	return dx_.halves.low;	}
	uint8_t &dh()	{	return dx_.halves.high;	}
	uint16_t &dx()	{	return dx_.full;		}

	CPU::RegisterPair16 bx_;
	uint8_t &bl()	{	return bx_.halves.low;	}
	uint8_t &bh()	{	return bx_.halves.high;	}
	uint16_t &bx()	{	return bx_.full;		}

	uint16_t sp_;
	uint16_t &sp()	{	return sp_;				}

	uint16_t bp_;
	uint16_t &bp()	{	return bp_;				}

	uint16_t si_;
	uint16_t &si()	{	return si_;				}

	uint16_t di_;
	uint16_t &di()	{	return di_;				}

	uint16_t es_, cs_, ds_, ss_;

	uint16_t ip_;
	uint16_t ip()	{	return ip_;				}

	uint16_t &es()	{	return es_;				}
	uint16_t &cs()	{	return cs_;				}
	uint16_t &ds()	{	return ds_;				}
	uint16_t &ss()	{	return ss_;				}

//	uint32_t zero_ = 0;
//	template <typename IntT> IntT &zero() {
//		return static_cast<IntT>(zero);
//	}

	bool operator ==(const Registers &rhs) const {
		return
			ax_.full == rhs.ax_.full &&
			cx_.full == rhs.cx_.full &&
			dx_.full == rhs.dx_.full &&
			bx_.full == rhs.bx_.full &&
			sp_ == rhs.sp_ &&
			bp_ == rhs.bp_ &&
			si_ == rhs.si_ &&
			di_ == rhs.di_ &&
			es_ == rhs.es_ &&
			cs_ == rhs.cs_ &&
			ds_ == rhs.ds_ &&
			si_ == rhs.si_ &&
			ip_ == rhs.ip_;
	}
};
struct Memory {
	enum class Tag {
		Seeded,
		AccessExpected,
		Accessed,
		FlagsL,
		FlagsH
	};

	std::unordered_map<uint32_t, Tag> tags;
	std::vector<uint8_t> memory;
	const Registers &registers_;

	Memory(Registers &registers) : registers_(registers) {
		memory.resize(1024*1024);
	}

	void clear() {
		tags.clear();
	}

	void seed(uint32_t address, uint8_t value) {
		memory[address] = value;
		tags[address] = Tag::Seeded;
	}

	void touch(uint32_t address) {
		tags[address] = Tag::AccessExpected;
	}

	uint32_t segment_base(InstructionSet::x86::Source segment) {
		uint32_t physical_address;
		using Source = InstructionSet::x86::Source;
		switch(segment) {
			default:			physical_address = registers_.ds_;	break;
			case Source::ES:	physical_address = registers_.es_;	break;
			case Source::CS:	physical_address = registers_.cs_;	break;
			case Source::SS:	physical_address = registers_.ss_;	break;
		}
		return physical_address << 4;
	}

	// Entry point used by the flow controller so that it can mark up locations at which the flags were written,
	// so that defined-flag-only masks can be applied while verifying RAM contents.
	template <typename IntT> IntT &access(InstructionSet::x86::Source segment, uint16_t address, Tag tag) {
		const uint32_t physical_address = (segment_base(segment) + address) & 0xf'ffff;
		return access<IntT>(physical_address, tag);
	}

	// An additional entry point for the flow controller; on the original 8086 interrupt vectors aren't relative
	// to a selector, they're just at an absolute location.
	template <typename IntT> IntT &access(uint32_t address, Tag tag) {
		if(tags.find(address) == tags.end()) {
			printf("Access to unexpected RAM address");
		}
		tags[address] = tag;
		return *reinterpret_cast<IntT *>(&memory[address]);
	}

	// Entry point for the 8086; simply notes that memory was accessed.
	template <typename IntT> IntT &access([[maybe_unused]] InstructionSet::x86::Source segment, uint32_t address) {
		if constexpr (std::is_same_v<IntT, uint16_t>) {
			// If this is a 16-bit access that runs past the end of the segment, it'll wrap back
			// to the start. So the 16-bit value will need to be a local cache.
			if(address == 0xffff) {
				write_back_address_ = (segment_base(segment) + address) & 0xf'ffff;
				write_back_value_ = memory[write_back_address_] | (memory[write_back_address_ - 65535] << 8);
				return write_back_value_;
			}
		}
		return access<IntT>(segment, address, Tag::Accessed);
	}

	template <typename IntT> 
	void write_back() {
		if constexpr (std::is_same_v<IntT, uint16_t>) {
			if(write_back_address_) {
				memory[write_back_address_] = write_back_value_ & 0xff;
				memory[write_back_address_ - 65535] = write_back_value_ >> 8;
				write_back_address_ = 0;
			}
		}
	}

	static constexpr uint32_t NoWriteBack = 0;	// Zero can never be an address that triggers write back, conveniently.
	uint32_t write_back_address_ = NoWriteBack;
	uint16_t write_back_value_;
};
struct IO {
};
class FlowController {
	public:
		FlowController(Memory &memory, Registers &registers, Status &status) :
			memory_(memory), registers_(registers), status_(status) {}

		void interrupt(int index) {
			const uint16_t address = static_cast<uint16_t>(index) << 2;
			const uint16_t new_ip = memory_.access<uint16_t>(address, Memory::Tag::Accessed);
			const uint16_t new_cs = memory_.access<uint16_t>(address + 2, Memory::Tag::Accessed);

			push(status_.get(), true);

			status_.interrupt = 0;
			status_.trap = 0;

			// Push CS and IP.
			push(registers_.cs_);
			push(registers_.ip_);

			registers_.cs_ = new_cs;
			registers_.ip_ = new_ip;
		}

		void call(uint16_t address) {
			push(registers_.ip_);
			registers_.ip_ = address;
		}

		void call(uint16_t segment, uint16_t offset) {
			push(registers_.cs_);
			push(registers_.ip_);
			registers_.cs_ = segment;
			registers_.ip_ = offset;
		}

	private:
		Memory &memory_;
		Registers &registers_;
		Status &status_;

		void push(uint16_t value, bool is_flags = false) {
			// Perform the push in two steps because it's possible for SP to underflow, and so that FlagsL and
			// FlagsH can be set separately.
			--registers_.sp_;
			memory_.access<uint8_t>(
				InstructionSet::x86::Source::SS,
				registers_.sp_,
				is_flags ? Memory::Tag::FlagsH : Memory::Tag::Accessed
			) = value >> 8;
			--registers_.sp_;
			memory_.access<uint8_t>(
				InstructionSet::x86::Source::SS,
				registers_.sp_,
				is_flags ? Memory::Tag::FlagsL : Memory::Tag::Accessed
			) = value & 0xff;
		}
};

struct ExecutionSupport {
	InstructionSet::x86::Status status;
	Registers registers;
	Memory memory;
	FlowController flow_controller;
	IO io;

	ExecutionSupport() : memory(registers), flow_controller(memory, registers, status) {}

	void clear() {
		memory.clear();
	}
};

struct FailedExecution {
	std::string file, test_name;
	InstructionSet::x86::Instruction<false> instruction;
};

}

@interface i8088Tests : XCTestCase
@end

@implementation i8088Tests {
	ExecutionSupport execution_support;
	std::vector<FailedExecution> execution_failures;
}

- (NSArray<NSString *> *)testFiles {
	NSString *path = [NSString stringWithUTF8String:TestSuiteHome];
	NSSet *allowList = [NSSet setWithArray:@[
		@"37.json.gz",	// AAA
		@"3F.json.gz",	// AAS
		@"D4.json.gz",	// AAM
		@"D5.json.gz",	// AAD
		@"27.json.gz",	// DAA
		@"2F.json.gz",	// DAS

		@"98.json.gz",	// CBW
		@"99.json.gz",	// CWD

		// ESC
		@"D8.json.gz",	@"D9.json.gz",	@"DA.json.gz",	@"DB.json.gz",
		@"DC.json.gz",	@"DD.json.gz",	@"DE.json.gz",	@"DE.json.gz",

		// NOP
		@"90.json.gz",

		// ADC
		@"10.json.gz",	@"11.json.gz",	@"12.json.gz",	@"13.json.gz",	@"14.json.gz",	@"15.json.gz",
		@"80.2.json.gz", @"81.2.json.gz", @"83.2.json.gz",

		// ADD
		@"00.json.gz",	@"01.json.gz",	@"02.json.gz",	@"03.json.gz",	@"04.json.gz",	@"05.json.gz",
		@"80.0.json.gz", @"81.0.json.gz", @"83.0.json.gz",

		// AND
		@"20.json.gz",	@"21.json.gz",	@"22.json.gz",	@"23.json.gz",	@"24.json.gz",	@"25.json.gz",
		@"80.4.json.gz", @"81.4.json.gz", @"83.4.json.gz",

		// CALL
		@"E8.json.gz",	@"FF.2.json.gz",
		@"9A.json.gz",	@"FF.3.json.gz",

		@"F8.json.gz",	// CLC
		@"FC.json.gz",	// CLD
		@"FA.json.gz",	// CLI
		@"F5.json.gz",	// CMC
	]];

	NSSet *ignoreList = nil;

	NSArray<NSString *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:nil];
	files = [files filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSString* evaluatedObject, NSDictionary<NSString *,id> *) {
		if(allowList.count && ![allowList containsObject:[evaluatedObject lastPathComponent]]) {
			return NO;
		}
		if([ignoreList containsObject:[evaluatedObject lastPathComponent]]) {
			return NO;
		}
		return [evaluatedObject hasSuffix:@"json.gz"];
	}]];

	NSMutableArray<NSString *> *fullPaths = [[NSMutableArray alloc] init];
	for(NSString *file in files) {
		[fullPaths addObject:[path stringByAppendingPathComponent:file]];
	}

	return [fullPaths sortedArrayUsingSelector:@selector(compare:)];
}

- (NSArray<NSDictionary *> *)testsInFile:(NSString *)file {
	NSData *data = [NSData dataWithContentsOfGZippedFile:file];
	return [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
}

- (NSDictionary *)metadata {
	NSString *path = [[NSString stringWithUTF8String:TestSuiteHome] stringByAppendingPathComponent:@"8088.json"];
	return [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfGZippedFile:path] options:0 error:nil];
}

- (NSString *)toString:(const InstructionSet::x86::Instruction<false> &)instruction offsetLength:(int)offsetLength immediateLength:(int)immediateLength {
	const auto operation = to_string(instruction, InstructionSet::x86::Model::i8086, offsetLength, immediateLength);
	return [[NSString stringWithUTF8String:operation.c_str()] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}

- (std::vector<uint8_t>)bytes:(NSArray<NSNumber *> *)encoding {
	std::vector<uint8_t> data;
	data.reserve(encoding.count);
	for(NSNumber *number in encoding) {
		data.push_back([number intValue]);
	}
	return data;
}

- (bool)applyDecodingTest:(NSDictionary *)test file:(NSString *)file assert:(BOOL)assert {
	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder;

	// Build a vector of the instruction bytes; this makes manual step debugging easier.
	const auto data = [self bytes:test[@"bytes"]];
	auto hex_instruction = [&]() -> NSString * {
		NSMutableString *hexInstruction = [[NSMutableString alloc] init];
		for(uint8_t byte: data) {
			[hexInstruction appendFormat:@"%02x ", byte];
		}
		return hexInstruction;
	};

	const auto decoded = decoder.decode(data.data(), data.size());
	const bool sizeMatched = decoded.first == data.size();
	if(assert) {
		XCTAssert(
			sizeMatched,
			"Wrong length of instruction decoded for %@ — decoded %d rather than %lu from %@; file %@",
				test[@"name"],
				decoded.first,
				(unsigned long)data.size(),
				hex_instruction(),
				file
		);
	}
	if(!sizeMatched) {
		return false;
	}

	// The decoder doesn't preserve the original offset length, which makes no functional difference but
	// does affect the way that offsets are printed in the test set.
	NSSet<NSString *> *decodings = [NSSet setWithObjects:
		[self toString:decoded.second offsetLength:4 immediateLength:4],
		[self toString:decoded.second offsetLength:2 immediateLength:4],
		[self toString:decoded.second offsetLength:0 immediateLength:4],
		[self toString:decoded.second offsetLength:4 immediateLength:2],
		[self toString:decoded.second offsetLength:2 immediateLength:2],
		[self toString:decoded.second offsetLength:0 immediateLength:2],
		nil];

	auto compare_decoding = [&](NSString *name) -> bool {
		return [decodings containsObject:name];
	};

	bool isEqual = compare_decoding(test[@"name"]);

	// Attempt clerical reconciliation:
	//
	// TEMPORARY HACK: the test set incorrectly states 'bp+si' whenever it means 'bp+di'.
	// Though it also uses 'bp+si' correctly when it means 'bp+si'. Until fixed, take
	// a pass on potential issues there.
	//
	// SEPARATELY: The test suite retains a distinction between SHL and SAL, which the decoder doesn't. So consider that
	// a potential point of difference.
	//
	// Also, the decoder treats INT3 and INT 3 as the same thing. So allow for a meshing of those.
	int adjustment = 7;
	while(!isEqual && adjustment) {
		NSString *alteredName = [test[@"name"] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

		if(adjustment & 4) {
			alteredName = [alteredName stringByReplacingOccurrencesOfString:@"bp+si" withString:@"bp+di"];
		}
		if(adjustment & 2) {
			alteredName = [alteredName stringByReplacingOccurrencesOfString:@"shl" withString:@"sal"];
		}
		if(adjustment & 1) {
			alteredName = [alteredName stringByReplacingOccurrencesOfString:@"int3" withString:@"int 03h"];
		}

		isEqual = compare_decoding(alteredName);
		--adjustment;
	}

	if(assert) {
		XCTAssert(
			isEqual,
			"%@ doesn't match %@ or similar, was %@ within %@",
				test[@"name"],
				[decodings anyObject],
				hex_instruction(),
				file
		);
	}

	return isEqual;
}

- (void)populate:(Registers &)registers status:(InstructionSet::x86::Status &)status value:(NSDictionary *)value {
	registers.ax_.full = [value[@"ax"] intValue];
	registers.bx_.full = [value[@"bx"] intValue];
	registers.cx_.full = [value[@"cx"] intValue];
	registers.dx_.full = [value[@"dx"] intValue];

	registers.bp_ = [value[@"bp"] intValue];
	registers.cs_ = [value[@"cs"] intValue];
	registers.di_ = [value[@"di"] intValue];
	registers.ds_ = [value[@"ds"] intValue];
	registers.es_ = [value[@"es"] intValue];
	registers.si_ = [value[@"si"] intValue];
	registers.sp_ = [value[@"sp"] intValue];
	registers.ss_ = [value[@"ss"] intValue];
	registers.ip_ = [value[@"ip"] intValue];

	status.set([value[@"flags"] intValue]);
}

- (void)applyExecutionTest:(NSDictionary *)test file:(NSString *)file metadata:(NSDictionary *)metadata {
	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder;
	const auto data = [self bytes:test[@"bytes"]];
	const auto decoded = decoder.decode(data.data(), data.size());

	execution_support.clear();

	const uint16_t flags_mask = metadata[@"flags-mask"] ? [metadata[@"flags-mask"] intValue] : 0xffff;
	NSDictionary *const initial_state = test[@"initial"];
	NSDictionary *const final_state = test[@"final"];

	// Apply initial state.
	InstructionSet::x86::Status initial_status;
	for(NSArray<NSNumber *> *ram in initial_state[@"ram"]) {
		execution_support.memory.seed([ram[0] intValue], [ram[1] intValue]);
	}
	for(NSArray<NSNumber *> *ram in final_state[@"ram"]) {
		execution_support.memory.touch([ram[0] intValue]);
	}
	[self populate:execution_support.registers status:initial_status value:initial_state[@"regs"]];
	execution_support.status = initial_status;

	// Execute instruction.
	execution_support.registers.ip_ += decoded.first;
	InstructionSet::x86::perform<InstructionSet::x86::Model::i8086>(
		decoded.second,
		execution_support.status,
		execution_support.flow_controller,
		execution_support.registers,
		execution_support.memory,
		execution_support.io
	);

	// Compare final state.
	Registers intended_registers;
	InstructionSet::x86::Status intended_status;

	bool ramEqual = true;
	for(NSArray<NSNumber *> *ram in final_state[@"ram"]) {
		const uint32_t address = [ram[0] intValue];

		uint8_t mask = 0xff;
		if(const auto tag = execution_support.memory.tags.find(address); tag != execution_support.memory.tags.end()) {
			switch(tag->second) {
				default: break;
				case Memory::Tag::FlagsH:	mask = flags_mask >> 8;		break;
				case Memory::Tag::FlagsL:	mask = flags_mask & 0xff;	break;
			}
		}

		ramEqual &= (execution_support.memory.memory[address] & mask) == ([ram[1] intValue] & mask);
	}

	[self populate:intended_registers status:intended_status value:final_state[@"regs"]];
	const bool registersEqual = intended_registers == execution_support.registers;
	const bool statusEqual = (intended_status.get() & flags_mask) == (execution_support.status.get() & flags_mask);

	if(!statusEqual || !registersEqual || !ramEqual) {
		FailedExecution failure;
		failure.instruction = decoded.second;
//		failure.file =	// TODO
		failure.test_name = std::string([test[@"name"] UTF8String]);
		execution_failures.push_back(std::move(failure));
	}

/*	if(assert) {
		XCTAssert(
			statusEqual,
			"Status doesn't match despite mask %04x — differs in %04x after %@; executing %@",
				flags_mask,
				(intended_status.get() ^ execution_support.status.get()) & flags_mask,
				test[@"name"],
				[self toString:decoded.second offsetLength:4 immediateLength:4]
		);
		// TODO: should probably say more about the following two.
		XCTAssert(
			registersEqual,
			"Register mismatch after %@; executing %@",
				test[@"name"],
				[self toString:decoded.second offsetLength:4 immediateLength:4]
		);
		XCTAssert(
			ramEqual,
			"Memory contents mismatch after %@; executing %@",
				test[@"name"],
				[self toString:decoded.second offsetLength:4 immediateLength:4]
		);
	}

	return statusEqual && registersEqual && ramEqual;*/
}

- (void)printFailures:(NSArray<NSString *> *)failures {
	NSLog(
		@"%ld failures out of %ld tests: %@",
		failures.count,
		[self testFiles].count,
		[failures sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]);
}

- (void)testDecoding {
	NSMutableArray<NSString *> *failures = [[NSMutableArray alloc] init];
	for(NSString *file in [self testFiles]) {
		for(NSDictionary *test in [self testsInFile:file]) {
			// A single failure per instruction is fine.
			if(![self applyDecodingTest:test file:file assert:YES]) {
				[failures addObject:file];

				// Attempt a second decoding, to provide a debugger hook.
				[self applyDecodingTest:test file:file assert:NO];
				break;
			}
		}
	}

	[self printFailures:failures];
}

- (void)testExecution {
	NSDictionary *metadata = [self metadata];

	for(NSString *file in [self testFiles]) {
		// Determine the metadata key.
		NSString *const name = [file lastPathComponent];
		NSRange first_dot = [name rangeOfString:@"."];
		NSString *metadata_key = [name substringToIndex:first_dot.location];

		// Grab the metadata. If it wants a reg field, inspect a little further.
		NSDictionary *test_metadata = metadata[metadata_key];
		if(test_metadata[@"reg"]) {
			test_metadata = test_metadata[@"reg"][[NSString stringWithFormat:@"%c", [name characterAtIndex:first_dot.location+1]]];
		}

		for(NSDictionary *test in [self testsInFile:file]) {
			[self applyExecutionTest:test file:file metadata:test_metadata];
		}
	}

	XCTAssertEqual(execution_failures.size(), 0);

	for(const auto &failure: execution_failures) {
		NSLog(@"Failed %s", failure.test_name.c_str());
	}
}

@end
