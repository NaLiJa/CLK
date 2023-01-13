//
//  MemorySlotHandler.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "MemorySlotHandler.hpp"

#include <cassert>

using namespace MSX;

MemorySlot::MemorySlot() {
	for(int subslot = 0; subslot < 4; subslot++) {
		for(int region = 0; region < 8; region++) {
			read_pointers_[subslot][region] = unmapped.data();
			write_pointers_[subslot][region] = scratch.data();
		}
	}
}

void MemorySlot::set_secondary_paging(uint8_t value) {
	secondary_paging_ = value;
}

uint8_t MemorySlot::secondary_paging() const {
	return secondary_paging_;
}

const uint8_t *MemorySlot::read_pointer(int segment) const {
	const int subslot = (secondary_paging_ >> (segment & ~1)) & 3;
	return read_pointers_[subslot][segment];
}

uint8_t *MemorySlot::write_pointer(int segment) const {
	const int subslot = (secondary_paging_ >> (segment & ~1)) & 3;
	return write_pointers_[subslot][segment];
}

void MemorySlot::apply_mapping(uint8_t port, uint8_t value) {
	// TODO.
	(void)port;
	(void)value;
}

void MemorySlot::set_source(const std::vector<uint8_t> &source) {
	source_ = source;
}

const std::vector<uint8_t> &MemorySlot::source() const {
	return source_;
}

void MemorySlot::map(int subslot, std::size_t source_address, uint16_t destination_address, std::size_t length) {
	assert(!(destination_address & 8191));
	assert(!(length & 8191));
	assert(size_t(destination_address) + length <= 65536);

	for(std::size_t c = 0; c < (length >> 13); ++c) {
		source_address %= source_.size();
		read_pointers_[subslot][(destination_address >> 13) + c] = &source_[source_address];
		source_address += 8192;
	}

	// TODO: allow write_pointers_ to be set.
	// TODO: need to indicate that mapping changed.
}

void MemorySlot::unmap(int subslot, uint16_t destination_address, std::size_t length) {
	assert(!(destination_address & 8191));
	assert(!(length & 8191));
	assert(size_t(destination_address) + length <= 65536);

	for(std::size_t c = 0; c < (length >> 13); ++c) {
		read_pointers_[subslot][(destination_address >> 13) + c] = nullptr;
	}

	// TODO: need to indicate that mapping changed.
}
