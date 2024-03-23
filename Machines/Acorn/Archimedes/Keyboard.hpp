//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "HalfDuplexSerial.hpp"

namespace Archimedes {

// Resource for the keyboard protocol: https://github.com/tmk/tmk_keyboard/wiki/ACORN-ARCHIMEDES-Keyboard
struct Keyboard {
	Keyboard(HalfDuplexSerial &serial) : serial_(serial) {}

	void set_key_state(int row, int column, bool is_pressed) {
		const uint8_t prefix = is_pressed ? 0b1100'0000 : 0b1101'0000;
		queue_.push_back(static_cast<uint8_t>(prefix | row));
		queue_.push_back(static_cast<uint8_t>(prefix | column));
		dequeue_next();
	}

	void update() {
		if(serial_.events(KeyboardParty) & HalfDuplexSerial::Receive) {
			const uint8_t input = serial_.input(KeyboardParty);
			switch(input) {
				case HRST:
					// TODO:
				case RAK1:
				case RAK2:
					serial_.output(KeyboardParty, input);
				break;

				case RQID:
					serial_.output(KeyboardParty, 0x81);	// TODO: what keyboard type?
				break;

				case BACK:
					dequeue_next();
				break;

				default:
					printf("Keyboard declines to respond to %02x\n", input);
				break;
			}
		}
	}

private:
	HalfDuplexSerial &serial_;

	std::vector<uint8_t> queue_;
	void dequeue_next() {
		if(queue_.empty()) return;
		serial_.output(KeyboardParty, queue_[0]);
		queue_.erase(queue_.begin());
	}

	static constexpr uint8_t HRST	= 0b1111'1111;	// Keyboard reset.
	static constexpr uint8_t RAK1	= 0b1111'1110;	// Reset response #1.
	static constexpr uint8_t RAK2	= 0b1111'1101;	// Reset response #2.

	static constexpr uint8_t RQID	= 0b0010'0000;	// Request for keyboard ID.
	static constexpr uint8_t RQMP	= 0b0010'0010;	// Request for mouse data.

	static constexpr uint8_t BACK	= 0b0011'1111;	// Acknowledge for first keyboard data byte pair.
	static constexpr uint8_t NACK	= 0b0011'0000;	// Acknowledge for last keyboard data byte pair, selects scan/mouse mode.
	static constexpr uint8_t SACK	= 0b0011'0001;	// Last data byte acknowledge.
	static constexpr uint8_t MACK	= 0b0011'0010;	// Last data byte acknowledge.
	static constexpr uint8_t SMAK	= 0b0011'0011;	// Last data byte acknowledge.
	static constexpr uint8_t PRST	= 0b0010'0001;	// Does nothing.
};

}
