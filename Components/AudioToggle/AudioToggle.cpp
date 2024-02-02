//
//  AudioToggle.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "AudioToggle.hpp"

#include <algorithm>

using namespace Audio;

Audio::Toggle::Toggle(Concurrency::AsyncTaskQueue<false> &audio_queue) :
	audio_queue_(audio_queue) {}

void Toggle::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	std::fill(target, target + number_of_samples, level_);
}

void Toggle::set_sample_volume_range(std::int16_t range) {
	volume_ = range;
	level_ = level_active_ ? volume_ : 0;
}

void Toggle::skip_samples(std::size_t) {}

void Toggle::set_output(bool enabled) {
	if(is_enabled_ == enabled) return;
	is_enabled_ = enabled;
	audio_queue_.enqueue([this, enabled] {
		level_active_ = enabled;
		level_ = enabled ? volume_ : 0;
	});
}

bool Toggle::get_output() const {
	return is_enabled_;
}
