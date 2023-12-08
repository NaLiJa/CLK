//
//  NIB.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "NIB.hpp"

#include "../../Track/PCMTrack.hpp"
#include "../../Track/TrackSerialiser.hpp"
#include "../../Encodings/AppleGCR/Encoder.hpp"

#include "../../Encodings/AppleGCR/Encoder.hpp"
#include "../../Encodings/AppleGCR/SegmentParser.hpp"

#include <vector>

using namespace Storage::Disk;

namespace {

const long track_length = 6656;
const std::size_t number_of_tracks = 35;

}

NIB::NIB(const std::string &file_name) :
	file_(file_name) {
	// A NIB should be 35 tracks, each 6656 bytes long.
	if(file_.stats().st_size != track_length*number_of_tracks) {
		throw Error::InvalidFormat;
	}

	// A real NIB should have every single top bit set. Yes, 1/8th of the
	// file size is a complete waste. But it provides a hook for validation.
	while(true) {
		uint8_t next = file_.get8();
		if(file_.eof()) break;
		if(!(next & 0x80)) throw Error::InvalidFormat;
	}
}

HeadPosition NIB::get_maximum_head_position() {
	return HeadPosition(number_of_tracks);
}

bool NIB::get_is_read_only() {
	return file_.get_is_known_read_only();
}

long NIB::file_offset(Track::Address address) {
	return long(address.position.as_int()) * track_length;
}

std::shared_ptr<::Storage::Disk::Track> NIB::get_track_at_position(::Storage::Disk::Track::Address address) {
	// NIBs contain data for even-numbered tracks underneath a single head only.
	if(address.head) return nullptr;

	long offset = file_offset(address);
	std::vector<uint8_t> track_data;
	{
		std::lock_guard lock_guard(file_.get_file_access_mutex());
		file_.seek(offset, SEEK_SET);
		track_data = file_.read(track_length);
	}

	// NIB files leave sync bytes implicit and make no guarantees
	// about overall track positioning. My current best-guess attempt
	// is to seek sector prologues then work backwards, inserting sync
	// bits into [at most 5] preceding FFs. This is intended to put the
	// Disk II into synchronisation just before each sector.
	std::size_t start_index = 0;
	std::set<size_t> sync_starts;

	// Establish where syncs start by finding instances of 0xd5 0xaa and then regressing
	// from each along all preceding FFs.
	for(size_t index = 0; index < track_data.size(); ++index) {
		// This is a D5 AA...
		if(track_data[index] == 0xd5 && track_data[(index+1)%track_data.size()] == 0xaa) {
			// ... count backwards to find out where the preceding FFs started.
			size_t start = index - 1;
			size_t length = 0;
			while(track_data[start] == 0xff && length < 5) {
				start = (start + track_data.size() - 1) % track_data.size();
				++length;
			}

			// Record a sync position only if there were at least five FFs, and
			// sync only in the final five. One of the many crazy fictions of NIBs
			// is the fixed track length in bytes, which is quite long. So the aim
			// is to be as conservative as possible with sync placement.
			if(length == 5) {
				sync_starts.insert((start + 1) % track_data.size());

				// If the apparent start of this sync area is 'after' the start, then
				// this sync period overlaps position zero. So this track will start
				// in a sync block.
				if(start > index)
					start_index = start;
			}
		}
	}

	// If searching for sector prologues didn't work, look for runs of FF FF FF FF FF.
	if(sync_starts.empty()) {
		for(size_t index = 0; index < track_data.size(); ++index) {
			if(track_data[index] == 0xff) {
				size_t length = 0;
				size_t end = index;
				while(track_data[end] == 0xff && length < 5) {
					end = (end + 1) % track_data.size();
					++length;
				}

				if(length == 5) {
					sync_starts.insert(index);

					while(track_data[index] == 0xff && index < track_data.size()) {
						++index;
					}
				}
			}
		}
	}

	PCMSegment segment;

	// If the track started in a sync block, write sync first.
	if(start_index) {
		segment += Encodings::AppleGCR::six_and_two_sync(int(start_index));
	}

	// Cap slip bits per location to avoid packing too many bits onto the track
	// and thereby making it over-dense.
	//
	// The magic constant 51,024 comes from the quantity that most DSKs are encoded to;
	// the minimum of 5 is the minimum number of FFs that must have slip bits in order to
	// guarantee synchronisation.
	const int max_slip_bytes_per_location =
		std::max(5, int((51024 - (track_data.size() * 8)) / sync_starts.size()));

	std::size_t index = start_index;
	for(const auto location: sync_starts) {
		// Write data from index to sync_start.
		if(location > index) {
			// This is the usual case; the only occasion on which it won't be true is
			// when the initial sync was detected to carry over the index hole,
			// in which case there's nothing to copy.
			std::vector<uint8_t> data_segment(
				track_data.begin() + ptrdiff_t(index),
				track_data.begin() + ptrdiff_t(location));
			segment += PCMSegment(data_segment);
		}

		// Add a sync from sync_start to end of 0xffs, if there are
		// any before the end of data.
		index = location;
		while(index < track_length && track_data[index] == 0xff)
			++index;

		int leadin_length = int(index - location);
		if(leadin_length) {
			// If this is more bytes than are permitted slip bits, encode the first bunch as non-slipping FFs.
			if(leadin_length > max_slip_bytes_per_location) {
				std::vector<uint8_t> ffs(size_t(leadin_length - max_slip_bytes_per_location), 0xff);
				segment += PCMSegment(ffs);
				leadin_length = max_slip_bytes_per_location;
			}

			segment += Encodings::AppleGCR::six_and_two_sync(leadin_length);
		}
	}

	// If there's still data remaining on the track, write it out. If a sync ran over
	// the notional index hole, the loop above will already have completed the track
	// with sync, so no need to deal with that case here.
	if(index < track_length) {
		std::vector<uint8_t> data_segment(
			track_data.begin() + ptrdiff_t(index),
			track_data.end());
		segment += PCMSegment(data_segment);
	}

	return std::make_shared<PCMTrack>(segment);
}

void NIB::set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) {
	std::map<Track::Address, std::vector<uint8_t>> tracks_by_address;

	// Convert to a map from address to a vector of data that contains the NIB representation
	// of the track.
	for(const auto &pair: tracks) {
		// Grab the track bit stream.
		auto segment = Storage::Disk::track_serialisation(*pair.second, Storage::Time(1, 50000));

		// Process to eliminate all sync bits.
		std::vector<uint8_t> track;
		track.reserve(track_length);
		uint8_t shifter = 0;
		int bit_count = 0;
		size_t sync_location = 0, location = 0;
		for(const auto bit: segment.data) {
			shifter = uint8_t((shifter << 1) | (bit ? 1 : 0));
			++bit_count;
			++location;
			if(shifter & 0x80) {
				track.push_back(shifter);
				if(bit_count == 10) {
					sync_location = location;
				}
				shifter = 0;
				bit_count = 0;
			}
		}

		// Trim or pad out to track_length.
		if(track.size() > track_length) {
			track.resize(track_length);
		} else {
			while(track.size() < track_length) {
				std::vector<uint8_t> extra_data(size_t(track_length) - track.size(), 0xff);
				track.insert(track.begin() + ptrdiff_t(sync_location), extra_data.begin(), extra_data.end());
			}
		}

		tracks_by_address[pair.first] = std::move(track);
	}

	// Lock the file and spool out.
	std::lock_guard lock_guard(file_.get_file_access_mutex());
	for(const auto &track: tracks_by_address) {
		file_.seek(file_offset(track.first), SEEK_SET);
		file_.write(track.second);
	}
}
