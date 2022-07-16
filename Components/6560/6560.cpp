//
//  6560.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "6560.hpp"

#include <cstring>

using namespace MOS::MOS6560;

AudioGenerator::AudioGenerator(Concurrency::AsyncTaskQueue<false> &audio_queue) :
	audio_queue_(audio_queue) {}


void AudioGenerator::set_volume(uint8_t volume) {
	audio_queue_.enqueue([this, volume]() {
		volume_ = int16_t(volume) * range_multiplier_;
	});
}

void AudioGenerator::set_control(int channel, uint8_t value) {
	audio_queue_.enqueue([this, channel, value]() {
		control_registers_[channel] = value;
	});
}

// Source: VICE. Not original.
static uint8_t noise_pattern[] = {
	0x07, 0x1e, 0x1e, 0x1c, 0x1c, 0x3e, 0x3c, 0x38, 0x78, 0xf8, 0x7c, 0x1e, 0x1f, 0x8f, 0x07, 0x07,
	0xc1, 0xc0, 0xe0, 0xf1, 0xe0, 0xf0, 0xe3, 0xe1, 0xc0, 0xe0, 0x78, 0x7e, 0x3c, 0x38, 0xe0, 0xe1,
	0xc3, 0xc3, 0x87, 0xc7, 0x07, 0x1e, 0x1c, 0x1f, 0x0e, 0x0e, 0x1e, 0x0e, 0x0f, 0x0f, 0xc3, 0xc3,
	0xf1, 0xe1, 0xe3, 0xc1, 0xe3, 0xc3, 0xc3, 0xfc, 0x3c, 0x1e, 0x0f, 0x83, 0xc3, 0xc1, 0xc1, 0xc3,
	0xc3, 0xc7, 0x87, 0x87, 0xc7, 0x0f, 0x0e, 0x3c, 0x7c, 0x78, 0x3c, 0x3c, 0x3c, 0x38, 0x3e, 0x1c,
	0x7c, 0x1e, 0x3c, 0x0f, 0x0e, 0x3e, 0x78, 0xf0, 0xf0, 0xe0, 0xe1, 0xf1, 0xc1, 0xc3, 0xc7, 0xc3,
	0xe1, 0xf1, 0xe0, 0xe1, 0xf0, 0xf1, 0xe3, 0xc0, 0xf0, 0xe0, 0xf8, 0x70, 0xe3, 0x87, 0x87, 0xc0,
	0xf0, 0xe0, 0xf1, 0xe1, 0xe1, 0xc7, 0x83, 0x87, 0x83, 0x8f, 0x87, 0x87, 0xc7, 0x83, 0xc3, 0x83,
	0xc3, 0xf1, 0xe1, 0xc3, 0xc7, 0x81, 0xcf, 0x87, 0x03, 0x87, 0xc7, 0xc7, 0x87, 0x83, 0xe1, 0xc3,
	0x07, 0xc3, 0x87, 0x87, 0x07, 0x87, 0xc3, 0x87, 0x83, 0xe1, 0xc3, 0xc7, 0xc3, 0x87, 0x87, 0x8f,
	0x0f, 0x87, 0x87, 0x0f, 0xcf, 0x1f, 0x87, 0x8e, 0x0e, 0x07, 0x81, 0xc3, 0xe3, 0xc1, 0xe0, 0xf0,
	0xe0, 0xe3, 0x83, 0x87, 0x07, 0x87, 0x8e, 0x1e, 0x0f, 0x07, 0x87, 0x8f, 0x1f, 0x07, 0x87, 0xc1,
	0xf0, 0xe1, 0xe1, 0xe3, 0xc7, 0x0f, 0x03, 0x8f, 0x87, 0x0e, 0x1e, 0x1e, 0x0f, 0x87, 0x87, 0x0f,
	0x87, 0x1f, 0x0f, 0xc3, 0xc3, 0xf0, 0xf8, 0xf0, 0x70, 0xf1, 0xf0, 0xf0, 0xe1, 0xf0, 0xe0, 0x78,
	0x7c, 0x78, 0x7c, 0x70, 0x71, 0xe1, 0xe1, 0xc3, 0xc3, 0xc7, 0x87, 0x1c, 0x3c, 0x3c, 0x1c, 0x3c,
	0x7c, 0x1e, 0x1e, 0x1e, 0x1c, 0x3c, 0x78, 0xf8, 0xf8, 0xe1, 0xc3, 0x87, 0x1e, 0x1e, 0x3c, 0x3e,
	0x0f, 0x0f, 0x87, 0x1f, 0x8e, 0x0f, 0x0f, 0x8e, 0x1e, 0x1e, 0x1e, 0x1e, 0x0f, 0x0f, 0x8f, 0x87,
	0x87, 0xc3, 0x83, 0xc1, 0xe1, 0xc3, 0xc1, 0xc3, 0xc7, 0x8f, 0x0f, 0x0f, 0x0f, 0x0f, 0x83, 0xc7,
	0xc3, 0xc1, 0xe1, 0xe0, 0xf8, 0x3e, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x78, 0x3e, 0x1e, 0x1e, 0x1e,
	0x0f, 0x0f, 0x0f, 0x1e, 0x0e, 0x1e, 0x1e, 0x0f, 0x0f, 0x87, 0x1f, 0x87, 0x87, 0x1c, 0x3e, 0x1f,
	0x0f, 0x0f, 0x8e, 0x3e, 0x0e, 0x3e, 0x1e, 0x1c, 0x3c, 0x7c, 0xfc, 0x38, 0x78, 0x78, 0x38, 0x78,
	0x70, 0xf8, 0x7c, 0x1e, 0x3c, 0x3c, 0x30, 0xf1, 0xf0, 0x70, 0x70, 0xe0, 0xf8, 0xf0, 0xf8, 0x78,
	0x78, 0x71, 0xe1, 0xf0, 0xe3, 0xc1, 0xf0, 0x71, 0xe3, 0xc7, 0x87, 0x8e, 0x3e, 0x0e, 0x1e, 0x3e,
	0x0f, 0x07, 0x87, 0x0c, 0x3e, 0x0f, 0x87, 0x0f, 0x1e, 0x3c, 0x3c, 0x38, 0x78, 0xf1, 0xe7, 0xc3,
	0xc3, 0xc7, 0x8e, 0x3c, 0x38, 0xf0, 0xe0, 0x7e, 0x1e, 0x3e, 0x0e, 0x0f, 0x0f, 0x0f, 0x03, 0xc3,
	0xc3, 0xc7, 0x87, 0x1f, 0x0e, 0x1e, 0x1c, 0x3c, 0x3c, 0x0f, 0x07, 0x07, 0xc7, 0xc7, 0x87, 0x87,
	0x8f, 0x0f, 0xc0, 0xf0, 0xf8, 0x60, 0xf0, 0xf0, 0xe1, 0xe3, 0xe3, 0xc3, 0xc3, 0xc3, 0x87, 0x0f,
	0x87, 0x8e, 0x1e, 0x1e, 0x3f, 0x1e, 0x0e, 0x1c, 0x3c, 0x7e, 0x1e, 0x3c, 0x38, 0x78, 0x78, 0x78,
	0x38, 0x78, 0x3c, 0xe1, 0xe3, 0x8f, 0x1f, 0x1c, 0x78, 0x70, 0x7e, 0x0f, 0x87, 0x07, 0xc3, 0xc7,
	0x0f, 0x1e, 0x3c, 0x0e, 0x0f, 0x0e, 0x1e, 0x03, 0xf0, 0xf0, 0xf1, 0xe3, 0xc1, 0xc7, 0xc0, 0xe1,
	0xe1, 0xe1, 0xe1, 0xe0, 0x70, 0xe1, 0xf0, 0x78, 0x70, 0xe3, 0xc7, 0x0f, 0xc1, 0xe1, 0xe3, 0xc3,
	0xc0, 0xf0, 0xfc, 0x1c, 0x3c, 0x70, 0xf8, 0x70, 0xf8, 0x78, 0x3c, 0x70, 0xf0, 0x78, 0x70, 0x7c,
	0x7c, 0x3c, 0x38, 0x1e, 0x3e, 0x3c, 0x7e, 0x07, 0x83, 0xc7, 0xc1, 0xc1, 0xe1, 0xc3, 0xc3, 0xc3,
	0xe1, 0xe1, 0xf0, 0x78, 0x7c, 0x3e, 0x0f, 0x1f, 0x07, 0x8f, 0x0f, 0x83, 0x87, 0xc1, 0xe3, 0xe3,
	0xc3, 0xc3, 0xe1, 0xf0, 0xf8, 0xf0, 0x3c, 0x7c, 0x3c, 0x0f, 0x8e, 0x0e, 0x1f, 0x1f, 0x0e, 0x3c,
	0x38, 0x78, 0x70, 0x70, 0xf0, 0xf0, 0xf8, 0x70, 0x70, 0x78, 0x38, 0x3c, 0x70, 0xe0, 0xf0, 0x78,
	0xf1, 0xf0, 0x78, 0x3e, 0x3c, 0x0f, 0x07, 0x0e, 0x3e, 0x1e, 0x3f, 0x1e, 0x0e, 0x0f, 0x87, 0x87,
	0x07, 0x0f, 0x07, 0xc7, 0x8f, 0x0f, 0x87, 0x1e, 0x1e, 0x1f, 0x1e, 0x1e, 0x3c, 0x1e, 0x1c, 0x3e,
	0x0f, 0x03, 0xc3, 0x81, 0xe0, 0xf0, 0xfc, 0x38, 0x3c, 0x3e, 0x0e, 0x1e, 0x1c, 0x7c, 0x1e, 0x1f,
	0x0e, 0x3e, 0x1c, 0x78, 0x78, 0x7c, 0x1e, 0x3e, 0x1e, 0x3c, 0x1f, 0x0f, 0x1f, 0x0f, 0x0f, 0x8f,
	0x1c, 0x3c, 0x78, 0xf8, 0xf0, 0xf8, 0x70, 0xf0, 0x78, 0x78, 0x3c, 0x3c, 0x78, 0x3c, 0x1f, 0x0f,
	0x07, 0x86, 0x1c, 0x1e, 0x1c, 0x1e, 0x1e, 0x1f, 0x03, 0xc3, 0xc7, 0x8e, 0x3c, 0x3c, 0x1c, 0x18,
	0xf0, 0xe1, 0xc3, 0xe1, 0xc1, 0xe1, 0xe3, 0xc3, 0xc3, 0xe3, 0xc3, 0x83, 0x87, 0x83, 0x87, 0x0f,
	0x07, 0x07, 0xe1, 0xe1, 0xe0, 0x7c, 0x78, 0x38, 0x78, 0x78, 0x3c, 0x1f, 0x0f, 0x8f, 0x0e, 0x07,
	0x0f, 0x07, 0x83, 0xc3, 0xc3, 0x81, 0xf0, 0xf8, 0xf1, 0xe0, 0xe3, 0xc7, 0x1c, 0x3e, 0x1e, 0x0f,
	0x0f, 0xc3, 0xf0, 0xf0, 0xe3, 0x83, 0xc3, 0xc7, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x87,
	0x0f, 0x0f, 0x0e, 0x0f, 0x0f, 0x1e, 0x0f, 0x0f, 0x87, 0x87, 0x87, 0x8f, 0xc7, 0xc7, 0x83, 0x83,
	0xc3, 0xc7, 0x8f, 0x87, 0x07, 0xc3, 0x8e, 0x1e, 0x38, 0x3e, 0x3c, 0x38, 0x7c, 0x1f, 0x1c, 0x38,
	0x3c, 0x78, 0x7c, 0x1e, 0x1c, 0x3c, 0x3f, 0x1e, 0x0e, 0x3e, 0x1c, 0x3c, 0x1f, 0x0f, 0x07, 0xc3,
	0xe3, 0x83, 0x87, 0x81, 0xc1, 0xe3, 0xcf, 0x0e, 0x0f, 0x1e, 0x3e, 0x1e, 0x1f, 0x0f, 0x8f, 0xc3,
	0x87, 0x0e, 0x03, 0xf0, 0xf0, 0x70, 0xe0, 0xe1, 0xe1, 0xc7, 0x8e, 0x0f, 0x0f, 0x1e, 0x0e, 0x1e,
	0x1f, 0x1c, 0x78, 0xf0, 0xf1, 0xf1, 0xe0, 0xf1, 0xe1, 0xe1, 0xe0, 0xe0, 0xf1, 0xc1, 0xf0, 0x71,
	0xe1, 0xc3, 0x83, 0xc7, 0x83, 0xe1, 0xe1, 0xf8, 0x70, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x70, 0xf8,
	0x70, 0x70, 0x61, 0xe0, 0xf0, 0xe1, 0xe0, 0x78, 0x71, 0xe0, 0xf0, 0xf8, 0x38, 0x1e, 0x1c, 0x38,
	0x70, 0xf8, 0x60, 0x78, 0x38, 0x3c, 0x3f, 0x1f, 0x0f, 0x1f, 0x0f, 0x1f, 0x87, 0x87, 0x83, 0x87,
	0x83, 0xe1, 0xe1, 0xf0, 0x78, 0xf1, 0xf0, 0x70, 0x38, 0x38, 0x70, 0xe0, 0xe3, 0xc0, 0xe0, 0xf8,
	0x78, 0x78, 0xf8, 0x38, 0xf1, 0xe1, 0xe1, 0xc3, 0x87, 0x87, 0x0e, 0x1e, 0x1f, 0x0e, 0x0e, 0x0f,
	0x0f, 0x87, 0xc3, 0x87, 0x07, 0x83, 0xc0, 0xf0, 0x38, 0x3c, 0x3c, 0x38, 0xf0, 0xfc, 0x3e, 0x1e,
	0x1c, 0x1c, 0x38, 0x70, 0xf0, 0xf1, 0xe0, 0xf0, 0xe0, 0xe0, 0xf1, 0xe3, 0xe0, 0xe1, 0xf0, 0xf0,
	0x78, 0x7c, 0x78, 0x3c, 0x78, 0x78, 0x38, 0x78, 0x78, 0x78, 0x78, 0x70, 0xe3, 0x83, 0x83, 0xe0,
	0xc3, 0xc1, 0xe1, 0xc1, 0xc1, 0xc1, 0xe3, 0xc3, 0xc7, 0x1e, 0x0e, 0x1f, 0x1e, 0x1e, 0x0f, 0x0f,
	0x0e, 0x0e, 0x0e, 0x07, 0x83, 0x87, 0x87, 0x0e, 0x07, 0x8f, 0x0f, 0x0f, 0x0f, 0x0e, 0x1c, 0x70,
	0xe1, 0xe0, 0x71, 0xc1, 0x83, 0x83, 0x87, 0x0f, 0x1e, 0x18, 0x78, 0x78, 0x7c, 0x3e, 0x1c, 0x38,
	0xf0, 0xe1, 0xe0, 0x78, 0x70, 0x38, 0x3c, 0x3e, 0x1e, 0x3c, 0x1e, 0x1c, 0x70, 0x3c, 0x38, 0x3f,
};

#define shift(r) shift_registers_[r] = (shift_registers_[r] << 1) | (((shift_registers_[r]^0x80)&control_registers_[r]) >> 7)
#define increment(r) shift_registers_[r] = (shift_registers_[r]+1)%8191
#define update(r, m, up) counters_[r]++; if((counters_[r] >> m) == 0x80) { up(r); counters_[r] = unsigned(control_registers_[r]&0x7f) << m; }
// Note on slightly askew test: as far as I can make out, if the value in the register is 0x7f then what's supposed to happen
// is that the 0x7f is loaded, on the next clocked cycle the Vic spots a 0x7f, pumps the output, reloads, etc. No increment
// ever occurs. It's conditional. I don't really want two conditionals if I can avoid it so I'm incrementing regardless and
// testing against 0x80. The effect should be the same: loading with 0x7f means an output update every cycle, loading with 0x7e
// means every second cycle, etc.

void AudioGenerator::get_samples(std::size_t number_of_samples, int16_t *target) {
	for(unsigned int c = 0; c < number_of_samples; ++c) {
		update(0, 2, shift);
		update(1, 1, shift);
		update(2, 0, shift);
		update(3, 1, increment);

		// this sums the output of all three sounds channels plus a DC offset for volume;
		// TODO: what's the real ratio of this stuff?
		target[c] = int16_t(
			(shift_registers_[0]&1) +
			(shift_registers_[1]&1) +
			(shift_registers_[2]&1) +
			((noise_pattern[shift_registers_[3] >> 3] >> (shift_registers_[3]&7))&(control_registers_[3] >> 7)&1)
		) * volume_ + (volume_ >> 4);
	}
}

void AudioGenerator::skip_samples(std::size_t number_of_samples) {
	for(unsigned int c = 0; c < number_of_samples; ++c) {
		update(0, 2, shift);
		update(1, 1, shift);
		update(2, 0, shift);
		update(3, 1, increment);
	}
}

void AudioGenerator::set_sample_volume_range(std::int16_t range) {
	range_multiplier_ = int16_t(range / 64);
}

#undef shift
#undef increment
#undef update
