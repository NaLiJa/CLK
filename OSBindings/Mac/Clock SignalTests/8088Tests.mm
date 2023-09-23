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

namespace {

// The tests themselves are not duplicated in this repository;
// provide their real path here.
constexpr char TestSuiteHome[] = "/Users/tharte/Projects/ProcessorTests/8088/v1";

}

@interface i8088Tests : XCTestCase
@end

@implementation i8088Tests

- (NSArray<NSString *> *)testFiles {
	NSString *path = [NSString stringWithUTF8String:TestSuiteHome];
	NSSet *allowList = nil;
//		[[NSSet alloc] initWithArray:@[
//			@"00.json.gz",
//		]];

	// Unofficial opcodes; ignored for now.
	NSSet *ignoreList =
		[[NSSet alloc] initWithArray:@[
			@"60.json.gz",		@"61.json.gz",		@"62.json.gz",		@"63.json.gz",
			@"64.json.gz",		@"65.json.gz",		@"66.json.gz",		@"67.json.gz",
			@"68.json.gz",		@"69.json.gz",		@"6a.json.gz",		@"6b.json.gz",
			@"6c.json.gz",		@"6d.json.gz",		@"6e.json.gz",		@"6f.json.gz",

			@"82.0.json.gz",	@"82.1.json.gz",	@"82.2.json.gz",	@"82.3.json.gz",
			@"82.4.json.gz",	@"82.5.json.gz",	@"82.6.json.gz",	@"82.7.json.gz",

			@"c0.json.gz",		@"c1.json.gz",		@"c8.json.gz",		@"c9.json.gz",

			@"f6.1.json.gz",	@"f7.1.json.gz",
			@"ff.7.json.gz",
		]];

	NSArray<NSString *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:nil];
	files = [files filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSString* evaluatedObject, NSDictionary<NSString *,id> *) {
		if(allowList && ![allowList containsObject:[evaluatedObject lastPathComponent]]) {
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

	return fullPaths;
}

- (bool)applyDecodingTest:(NSDictionary *)test {
	using Decoder = InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086>;
	Decoder decoder;

	// Build a vector of the instruction; this makes manual step debugging easier.
	NSArray<NSNumber *> *encoding = test[@"bytes"];
	std::vector<uint8_t> data;
	data.reserve(encoding.count);
	for(NSNumber *number in encoding) {
		data.push_back([number intValue]);
	}

	const auto decoded = decoder.decode(data.data(), data.size());
	XCTAssert(
		decoded.first == [encoding count],
		"Wrong length of instruction decoded for %@ — decoded %d rather than %lu",
			test[@"name"],
			decoded.first,
			(unsigned long)[encoding count]
	);

	auto log_hex = [&] {
		NSMutableString *hexInstruction = [[NSMutableString alloc] init];
		for(uint8_t byte: data) {
			[hexInstruction appendFormat:@"%02x ", byte];
		}
		NSLog(@"Instruction was %@", hexInstruction);
	};

	if(decoded.first != [encoding count]) {
		log_hex();

		// Repeat the decoding, for ease of debugging.
		Decoder().decode(data.data(), data.size());
		return false;
	}

	// Form string version, compare.
	std::string operation;

	using Repetition = InstructionSet::x86::Repetition;
	switch(decoded.second.repetition()) {
		case Repetition::None: break;
		case Repetition::RepE: operation += "repe ";	break;
		case Repetition::RepNE: operation += "repne ";	break;
	}

	operation += to_string(decoded.second.operation, decoded.second.operation_size());

	auto to_hex = [] (int value, int digits) -> std::string {
		auto stream = std::stringstream();
		stream << std::setfill('0') << std::uppercase << std::hex << std::setw(digits);
		switch(digits) {
			case 2: stream << +uint8_t(value);	break;
			case 4: stream << +uint16_t(value);	break;
			default: stream << value;	break;
		}
		stream << 'h';
		return stream.str();
	};

	auto to_string = [&to_hex] (InstructionSet::x86::DataPointer pointer, const auto &instruction) -> std::string {
		std::string operand;

		using Source = InstructionSet::x86::Source;
		const Source source = pointer.source<false>();
		switch(source) {
			// to_string handles all direct register names correctly.
			default:	return InstructionSet::x86::to_string(source, instruction.operation_size());

			case Source::Immediate:
				return to_hex(
					instruction.operand(),
					instruction.operation_size() == InstructionSet::x86::DataSize::Byte ? 2 : 4
				);

			case Source::DirectAddress:
			case Source::Indirect:
			case Source::IndirectNoBase: {
				std::stringstream stream;

				if(!InstructionSet::x86::mnemonic_implies_data_size(instruction.operation)) {
					stream << InstructionSet::x86::to_string(instruction.operation_size()) << ' ';
				}

				Source segment = instruction.data_segment();
				if(segment == Source::None) {
					segment = pointer.default_segment();
					if(segment == Source::None) {
						segment = Source::DS;
					}
				}
				stream << InstructionSet::x86::to_string(segment, InstructionSet::x86::DataSize::None) << ':';

				stream << '[';
				switch(source) {
					default: break;
					case Source::Indirect:
						stream << InstructionSet::x86::to_string(pointer.base(), data_size(instruction.address_size()));
						stream << '+' << InstructionSet::x86::to_string(pointer.index(), data_size(instruction.address_size()));
						if(instruction.offset()) {
							stream << '+' << to_hex(instruction.offset(), 4);
						}
					break;
					case Source::IndirectNoBase:
						stream << InstructionSet::x86::to_string(pointer.index(), data_size(instruction.address_size()));
						if(instruction.offset()) {
							stream << '+' << to_hex(instruction.offset(), 4);
						}
					break;
					case Source::DirectAddress:
						stream << to_hex(instruction.offset(), 4);
					break;
				}
				stream << ']';
				return stream.str();
			}
		}

		return operand;
	};

	const int operands = num_operands(decoded.second.operation);
	const bool displacement = has_displacement(decoded.second.operation);
	operation += " ";
	if(operands > 1) {
		operation += to_string(decoded.second.destination(), decoded.second);
		operation += ", ";
	}
	if(operands > 0) {
		operation += to_string(decoded.second.source(), decoded.second);
	}
	if(displacement) {
		operation += to_hex(decoded.second.displacement(), 2);
	}

	const NSString *objcOperation = [NSString stringWithUTF8String:operation.c_str()];
	XCTAssertEqualObjects(objcOperation, test[@"name"]);

	if(![objcOperation isEqualToString:test[@"name"]]) {
		log_hex();

		// Repeat operand conversions, for debugging.
		Decoder decoder;
		const auto instruction = decoder.decode(data.data(), data.size());
		const InstructionSet::x86::Source sources[] = {
			instruction.second.source().source<false>(),
			instruction.second.destination().source<false>(),
		};
		(void)sources;

		const auto destination = instruction.second.destination();
		to_string(destination, instruction.second);
		const auto source = instruction.second.source();
		to_string(source, instruction.second);
		return false;
	}

	return true;
}

- (void)testDecoding {
	NSMutableSet<NSString *> *failures = [[NSMutableSet alloc] init];
	NSArray<NSString *> *testFiles = [self testFiles];

	for(NSString *file in testFiles) {
		NSData *data = [NSData dataWithContentsOfGZippedFile:file];
		NSArray<NSDictionary *> *testsInFile = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
		NSUInteger successes = 0;
		for(NSDictionary *test in testsInFile) {
			// A single failure per instruction is fine.
			if(![self applyDecodingTest:test]) {
				[failures addObject:file];
				break;
			}
			++successes;
		}
		if(successes != [testsInFile count]) {
			NSLog(@"Failed after %ld successes", successes);
		}
	}

	NSLog(@"%ld failures out of %ld tests: %@", failures.count, testFiles.count, [[failures allObjects] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]);
}

@end
