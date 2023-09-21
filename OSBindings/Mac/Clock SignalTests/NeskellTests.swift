//
//  KlausDormanTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

import Foundation
import XCTest

// The below reuses the Neskell — https://github.com/blitzcode/neskell — tests and therefore attempts to transcribe
// https://github.com/blitzcode/neskell/blob/b4bfec6d6f0cf88d8d2de61585017d16a37e3b9a/src/Test.hs

class NeskellTests: XCTestCase {
	private func runTest(resource: String) -> CSTestMachine6502? {
		if let filename = Bundle(for: type(of: self)).path(forResource: resource, ofType: "bin") {
			if let functionalTest = try? Data(contentsOf: URL(fileURLWithPath: filename)) {
				let machine = CSTestMachine6502(processor: .processorNES6502)

				machine.setData(functionalTest, atAddress: 0x0600)
				machine.setValue(0x0600, for: .programCounter)

				// Install the halt-forever trailer.
				let targetAddress = UInt32(0x0600 + functionalTest.count)
				let infiniteStop = Data([0x38, 0xb0, 0xfe])	// i.e. SEC; BCS -2
				machine.setData(infiniteStop, atAddress: targetAddress)

				while true {
					let oldPC = machine.value(for: .lastOperationAddress)
					machine.runForNumber(ofCycles: 1000)
					let newPC = machine.value(for: .lastOperationAddress)

					if newPC == oldPC {
						return machine
					}
				}
			}
		}

		return nil
	}

	func testAHX_TAS_SHX_SHY() {
		if let result = runTest(resource: "ahx_tas_shx_shy_test") {
			XCTAssertEqual(result.value(for: .stackPointer), 0xf2)

			let stackData = result.data(atAddress: 0x1f3, length: 0x200 - 0x1f3)
			XCTAssertEqual(stackData,
				Data([0x01, 0xC9, 0x01, 0x80, 0xC0, 0xE0, 0x01, 0x55, 0x80, 0x80, 0x01, 0x34, 0x10]));
		}
	}
}
