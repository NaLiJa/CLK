//
//  Audio.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

namespace Archimedes {

template <typename InterruptObserverT>
struct Sound {
	Sound(InterruptObserverT &observer) : observer_(observer) {}

	void set_next_end(uint32_t value) {
		next_.end = value;
	}

	void set_next_start(uint32_t value) {
		next_.start = value;
		set_buffer_valid(true);	// My guess: this is triggered on next buffer start write.
	}

	bool interrupt() const {
		return !next_buffer_valid_;
	}

	void swap() {
		current_.start = next_.start;
		std::swap(current_.end, next_.end);
		set_buffer_valid(false);
		halted_ = false;
	}

	void tick() {
		if(halted_) {
			return;
		}

		current_.start += 16;
		if(current_.start == current_.end) {
			if(next_buffer_valid_) {
				swap();
			} else {
				halted_ = true;
			}
		}
	}

private:
	void set_buffer_valid(bool valid) {
		next_buffer_valid_ = valid;
		observer_.update_sound_interrupt();
	}

	bool next_buffer_valid_ = false;
	bool halted_ = true;				// This is a bit of a guess.

	struct Buffer {
		uint32_t start = 0, end = 0;
	};
	Buffer current_, next_;

	InterruptObserverT &observer_;
};

}
