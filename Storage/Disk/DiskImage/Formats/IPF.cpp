//
//  IPF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/12/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "IPF.hpp"

using namespace Storage::Disk;

namespace {

constexpr uint32_t block(const char (& src)[5]) {
	return uint32_t(
		(uint32_t(src[0]) << 24) |
		(uint32_t(src[1]) << 16) |
		(uint32_t(src[2]) << 8) |
		uint32_t(src[3])
	);
}

constexpr size_t block_size(Storage::FileHolder &file, uint8_t header) {
	uint8_t size_width = header >> 5;
	size_t length = 0;
	while(size_width--) {
		length = (length << 8) | file.get8();
	}
	return length;
}

}

IPF::IPF(const std::string &file_name) : file_(file_name) {
	std::map<uint32_t, Track::Address> tracks_by_data_key;

	// For now, just build up a list of tracks that exist, noting the file position at which their data begins
	// plus the other fields that'll be necessary to convert them into flux on demand later.
	while(true) {
		const auto start_of_block = file_.tell();
		const uint32_t type = file_.get32be();
		uint32_t length = file_.get32be();						// Can't be const because of the dumb encoding of DATA blocks.
		[[maybe_unused]] const uint32_t crc = file_.get32be();
		if(file_.eof()) break;

		// Sanity check: the first thing in a file should be the CAPS record.
		if(!start_of_block && type != block("CAPS")) {
			throw Error::InvalidFormat;
		}

		switch(type) {
			default:
				printf("Ignoring %c%c%c%c, starting at %ld of length %d\n", (type >> 24), (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff, start_of_block, length);
			break;

			case block("CAPS"):
				// Analogously to the sanity check above, if a CAPS block is anywhere other
				// than first then something is amiss.
				if(start_of_block) {
					throw Error::InvalidFormat;
				}
			break;

			case block("INFO"): {
				// There are a lot of useful archival fields in the info chunk, which for emulation
				// aren't that interesting.

				// Make sure this is a floppy disk.
				const uint32_t media_type = file_.get32be();
				if(media_type != 1) {
					throw Error::InvalidFormat;
				}

				// Determine whether this is a newer SPS-style file.
				is_sps_format_ = file_.get32be() > 1;

				// Skip: revision, file key and revision, CRC of the original .ctr, and minimum track.
				file_.seek(20, SEEK_CUR);
				track_count_ = int(1 + file_.get32be());

				// Skip: min side.
				file_.seek(4, SEEK_CUR);
				head_count_ = int(1 + file_.get32be());

				// Skip: creation date, time.
				file_.seek(8, SEEK_CUR);

				platform_type_ = 0;
				for(int c = 0; c < 4; c++) {
					const uint8_t platform = file_.get8();
					switch(platform) {
						default: break;
						case 1:	platform_type_ |= TargetPlatform::Amiga;		break;
						case 2:	platform_type_ |= TargetPlatform::AtariST;		break;
						/* Omitted: 3 -> IBM PC */
						case 4:	platform_type_ |= TargetPlatform::AmstradCPC;	break;
						case 5:	platform_type_ |= TargetPlatform::ZXSpectrum;	break;
						/* Omitted: 6 -> Sam Coupé */
						/* Omitted: 7 -> Archimedes */
						/* Omitted: 8 -> C64 */
						/* Omitted: 9 -> Atari 8-bit */
					}
				}

				// If the file didn't declare anything, default to supporting everything.
				if(!platform_type_) {
					platform_type_ = ~0;
				}

				// Ignore: disk number, creator ID, reserved area.
			} break;

			case block("IMGE"): {
				// Get track location.
				const uint32_t track = file_.get32be();
				const uint32_t side = file_.get32be();
				const Track::Address address{int(side), HeadPosition(int(track))};

				// Hence generate a TrackDescription.
				auto pair = tracks_.emplace(address, TrackDescription());
				TrackDescription &description = pair.first->second;

				// Read those fields of interest...

				// Bit density. I've no idea why the density can't just be given as a measurement.
				description.density = TrackDescription::Density(file_.get32be());
				if(description.density > TrackDescription::Density::Max) {
					description.density = TrackDescription::Density::Unknown;
				}

				file_.seek(12, SEEK_CUR);	// Skipped: signal type, track bytes, start byte position.
				description.start_bit_pos = file_.get32be();
				description.data_bits = file_.get32be();
				description.gap_bits = file_.get32be();

				file_.seek(4, SEEK_CUR);	// Skipped: track bits, which is entirely redundant.
				description.block_count = file_.get32be();

				file_.seek(4, SEEK_CUR);	// Skipped: encoder process.
				description.has_fuzzy_bits = file_.get32be() & 1;

				// For some reason the authors decided to introduce another primary key,
				// in addition to that which naturally exists of (track, side). So set up
				// a mapping from the one to the other.
				const uint32_t data_key = file_.get32be();
				tracks_by_data_key.emplace(data_key, address);
			} break;

			case block("DATA"): {
				length += file_.get32be();

				file_.seek(8, SEEK_CUR);	// Skipped: bit size, CRC.

				// Grab the data key and use that to establish the file starting
				// position for this track.
				//
				// Assumed here: DATA records will come after corresponding IMGE records.
				const uint32_t data_key = file_.get32be();
				const auto pair = tracks_by_data_key.find(data_key);
				if(pair == tracks_by_data_key.end()) {
					break;
				}

				auto description = tracks_.find(pair->second);
				if(description == tracks_.end()) {
					break;
				}
				description->second.file_offset = file_.tell();
			} break;
		}

		file_.seek(start_of_block + length, SEEK_SET);
	}
}

HeadPosition IPF::get_maximum_head_position() {
	return HeadPosition(track_count_);
}

int IPF::get_head_count() {
	return head_count_;
}

std::shared_ptr<Track> IPF::get_track_at_position([[maybe_unused]] Track::Address address) {
	// Get the track description, if it exists, and check either that the file has contents for the track.
	auto pair = tracks_.find(address);
	if(pair == tracks_.end()) {
		return nullptr;
	}
	const TrackDescription &description = pair->second;
	if(!description.file_offset) {
		return nullptr;
	}

	// Seek to track content.
	file_.seek(description.file_offset, SEEK_SET);

	// Read the block descriptions up front.
	//
	// This is less efficient than just seeking for each block in turn,
	// but is a useful crutch to comprehension of the file format on a
	// first run through.
	struct BlockDescriptor {
		uint32_t data_bits = 0;
		uint32_t gap_bits = 0;
		uint32_t gap_offset = 0;
		bool is_mfm = false;
		bool has_forward_gap = false;
		bool has_backwards_gap = false;
		bool data_unit_is_bits = false;
		uint32_t default_gap_value = 0;
		uint32_t data_offset = 0;
	};
	std::vector<BlockDescriptor> blocks;
	blocks.reserve(description.block_count);
	for(uint32_t c = 0; c < description.block_count; c++) {
		auto &block = blocks.emplace_back();
		block.data_bits = file_.get32be();
		block.gap_bits = file_.get32be();
		if(is_sps_format_) {
			block.gap_offset = file_.get32be();
			file_.seek(4, SEEK_CUR);	// Skip 'cell type' which appears to provide no content.
		} else {
			// Skip potlower-resolution copies of data_bits and gap_bits.
			file_.seek(8, SEEK_CUR);
		}
		block.is_mfm = file_.get32be() == 1;

		const uint32_t flags = file_.get32be();
		block.has_forward_gap = flags & 1;
		block.has_backwards_gap = flags & 2;
		block.data_unit_is_bits = flags & 4;

		block.default_gap_value = file_.get32be();
		block.data_offset = file_.get32be();
	}

	// TODO: Append as necessary for each gap and data stream as per above.
	for(auto &block: blocks) {
		if(block.gap_offset) {
			file_.seek(description.file_offset + block.gap_offset, SEEK_SET);
			while(true) {
				const uint8_t gap_header = file_.get8();
				if(!gap_header) break;

				// Decompose the header and read the length.
				enum class Type {
					None, GapLength, SampleLength
				} type = Type(gap_header & 0x1f);
				const size_t length = block_size(file_, gap_header);

				// TODO: write the gap.
				switch(type) {
					case Type::GapLength:
						printf("Unhandled gap length %zu bytes\n", length);
					break;

					default:
					case Type::SampleLength:
						printf("Unhandled sampled gap length %zu bytes\n", length);
						file_.seek(long(length >> 3), SEEK_CUR);
					break;
				}
			}
		}

		if(block.data_offset) {
			file_.seek(description.file_offset + block.data_offset, SEEK_SET);
			while(true) {
				const uint8_t data_header = file_.get8();
				if(!data_header) break;

				// Decompose the header and read the length.
				enum class Type {
					None, Sync, Data, Gap, Raw, Fuzzy
				} type = Type(data_header & 0x1f);
				const size_t length = block_size(file_, data_header) * (block.data_unit_is_bits ? 1 : 8);

				// TODO: write the data.
				switch(type) {
					default:
						printf("Unhandled data type %d, length %zu bits\n", int(type), length);
						file_.seek(long(length >> 3), SEEK_CUR);
					break;
				}
			}
		}
		printf("\n");
	}

	return nullptr;
}

/// @returns A vector of the length of a bit in each block for a count of @c blocks in an area of data density @c density.
///
/// @discussion At least to me, this is the least well-designed part] of the IPF specification; rather than just dictating cell
/// densities (or, equivalently, lengths) in the file, densities are named according to their protection scheme and the decoder
/// is required to know all named protection schemes. Which makes IPF unable to handle arbitrary disks (or, indeed, disks
/// with multiple protection schemes on a single track).
std::vector<Storage::Time> IPF::bit_lengths(TrackDescription::Density density, size_t blocks) {
	std::vector<Storage::Time> result;
	result.reserve(size_t(blocks));

	// Establish the default density of 2 µs.
	for(size_t c = 0; c < blocks; c++) {
		result.push_back(Storage::Time(1, 500'000));	// i.e. default to 2µs.
	}

	switch(density) {
		default:
		break;

		case TrackDescription::Density::CopylockAmiga:
			if(blocks > 4) result[4] = Storage::Time(189, 100'000'000);		// 1.89µs
			if(blocks > 5) result[5] = Storage::Time(199, 100'000'000);		// 1.99µs
			if(blocks > 6) result[6] = Storage::Time(209, 100'000'000);		// 2.09µs
		break;

		case TrackDescription::Density::CopylockAmigaNew:
			if(blocks > 0) result[0] = Storage::Time(189, 100'000'000);		// 1.89µs
			if(blocks > 1) result[1] = Storage::Time(199, 100'000'000);		// 1.99µs
			if(blocks > 2) result[2] = Storage::Time(209, 100'000'000);		// 2.09µs
		break;

		case TrackDescription::Density::CopylockST:
			if(blocks > 5) result[5] = Storage::Time(21, 10'000'000);		// 2.1µs
		break;

		case TrackDescription::Density::SpeedlockAmiga:
			if(blocks > 1) result[1] = Storage::Time(11, 5'000'000);		// 2.2µs
			if(blocks > 2) result[2] = Storage::Time(9, 5'000'000);			// 1.8µs
		break;

		case TrackDescription::Density::OldSpeedlockAmiga:
			if(blocks > 1) result[1] = Storage::Time(21, 10'000'000);		// 2.1µs
		break;

		case TrackDescription::Density::AdamBrierleyAmiga:
			if(blocks > 1) result[1] = Storage::Time(11, 5'000'000);		// 2.2µs
			if(blocks > 2) result[2] = Storage::Time(21, 10'000'000);		// 2.1µs

			if(blocks > 4) result[3] = Storage::Time(19, 10'000'000);		// 1.9µs
			if(blocks > 5) result[5] = Storage::Time(9, 5'000'000);			// 1.8µs
			if(blocks > 6) result[6] = Storage::Time(17, 10'000'000);		// 1.7µs
		break;

		// TODO: AdamBrierleyDensityKeyAmiga.
	}

	return result;
}
