//
//  MFM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Disk_Encodings_MFM_hpp
#define Storage_Disk_Encodings_MFM_hpp

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "Constants.hpp"
#include "Sector.hpp"
#include "../../Track/Track.hpp"
#include "../../../../Numeric/CRC.hpp"

namespace Storage::Encodings::MFM {

template <Density density> struct Defaults;
template <> struct Defaults<Density::Single> {
	static constexpr size_t expected_track_bytes = 6250;

	static constexpr size_t post_index_address_mark_bytes = 26;
	static constexpr uint8_t post_index_address_mark_value = 0xff;

	static constexpr size_t pre_address_mark_bytes = 6;
	static constexpr size_t post_address_address_mark_bytes = 11;
	static constexpr uint8_t post_address_address_mark_value = 0xff;

	static constexpr size_t pre_data_mark_bytes = 6;
	static constexpr size_t post_data_bytes = 27;
	static constexpr uint8_t post_data_value = 0xff;
};
template <> struct Defaults<Density::Double> {
	static constexpr size_t expected_track_bytes = 12500;

	static constexpr size_t post_index_address_mark_bytes = 50;
	static constexpr uint8_t post_index_address_mark_value = 0x4e;

	static constexpr size_t pre_address_mark_bytes = 12;
	static constexpr size_t post_address_address_mark_bytes = 22;
	static constexpr uint8_t post_address_address_mark_value = 0x4e;

	static constexpr size_t pre_data_mark_bytes = 12;
	static constexpr size_t post_data_bytes = 54;
	static constexpr uint8_t post_data_value = 0xff;
};
template <> struct Defaults<Density::High> {
	static constexpr size_t expected_track_bytes = 25000;

	static constexpr size_t post_index_address_mark_bytes = 50;
	static constexpr uint8_t post_index_address_mark_value = 0x4e;

	static constexpr size_t pre_address_mark_bytes = 12;
	static constexpr size_t post_address_address_mark_bytes = 22;
	static constexpr uint8_t post_address_address_mark_value = 0x4e;

	static constexpr size_t pre_data_mark_bytes = 12;
	static constexpr size_t post_data_bytes = 54;
	static constexpr uint8_t post_data_value = 0xff;
};

/*!
	Converts a vector of sectors into a properly-encoded FM or MFM track.

	@param sectors The sectors to write.
	@param sector_gap_length If specified, sets the distance in whole bytes between each ID and its data.
	@param sector_gap_filler_byte If specified, sets the value (unencoded) that is used to populate the gap between each ID and its data.
*/
std::shared_ptr<Storage::Disk::Track> TrackWithSectors(
	Density density,
	const std::vector<Sector> &sectors,
	std::optional<std::size_t> sector_gap_length = std::nullopt,
	std::optional<uint8_t> sector_gap_filler_byte = std::nullopt);

std::shared_ptr<Storage::Disk::Track> TrackWithSectors(
	Density density,
	const std::vector<const Sector *> &sectors,
	std::optional<std::size_t> sector_gap_length = std::nullopt,
	std::optional<uint8_t> sector_gap_filler_byte = std::nullopt);

class Encoder {
	public:
		Encoder(std::vector<bool> &target, std::vector<bool> *fuzzy_target);
		virtual ~Encoder() {}
		virtual void reset_target(std::vector<bool> &target, std::vector<bool> *fuzzy_target = nullptr);

		virtual void add_byte(uint8_t input, uint8_t fuzzy_mask = 0) = 0;
		virtual void add_index_address_mark() = 0;
		virtual void add_ID_address_mark() = 0;
		virtual void add_data_address_mark() = 0;
		virtual void add_deleted_data_address_mark() = 0;
		virtual void output_short(uint16_t value, uint16_t fuzzy_mask = 0);

		template <typename IteratorT> void add_bytes(IteratorT begin, IteratorT end) {
			while(begin != end) {
				add_byte(*begin);
				++begin;
			}
		}

		template <typename ContainerT> void add_bytes(const ContainerT &container) {
			write(std::begin(container), std::end(container));
		}

		/// Outputs the CRC for all data since the last address mask; if @c incorrectly is @c true then outputs an incorrect CRC.
		void add_crc(bool incorrectly);

	protected:
		CRC::CCITT crc_generator_;

	private:
		std::vector<bool> *target_ = nullptr;
		std::vector<bool> *fuzzy_target_ = nullptr;
};

std::unique_ptr<Encoder> GetMFMEncoder(std::vector<bool> &target, std::vector<bool> *fuzzy_target = nullptr);
std::unique_ptr<Encoder> GetFMEncoder(std::vector<bool> &target, std::vector<bool> *fuzzy_target = nullptr);

}

#endif /* MFM_hpp */
