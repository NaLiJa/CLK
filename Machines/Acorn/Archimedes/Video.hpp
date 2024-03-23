//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Outputs/Log.hpp"
#include "../../../Outputs/CRT/CRT.hpp"

#include <array>
#include <cstdint>
#include <cstring>

namespace Archimedes {

template <typename InterruptObserverT, typename ClockRateObserverT, typename SoundT>
struct Video {
	Video(InterruptObserverT &interrupt_observer, ClockRateObserverT &clock_rate_observer, SoundT &sound, const uint8_t *ram) :
		interrupt_observer_(interrupt_observer),
		clock_rate_observer_(clock_rate_observer),
		sound_(sound),
		ram_(ram),
		crt_(Outputs::Display::InputDataType::Red4Green4Blue4) {
		set_clock_divider(3);
	}

	void write(uint32_t value) {
		const auto target = (value >> 24) & 0xfc;
		const auto timing_value = [](uint32_t value) -> uint32_t {
			return (value >> 14) & 0x3ff;
		};
		const auto colour = [](uint32_t value) -> uint16_t {
			uint8_t packed[2];
			packed[0] = value & 0xf;
			packed[1] = (value & 0xf0) | ((value & 0xf00) >> 8);

			uint16_t result;
			memcpy(&result, packed, 2);
			return result;
		};

		switch(target) {
			case 0x00:	case 0x04:	case 0x08:	case 0x0c:
			case 0x10:	case 0x14:	case 0x18:	case 0x1c:
			case 0x20:	case 0x24:	case 0x28:	case 0x2c:
			case 0x30:	case 0x34:	case 0x38:	case 0x3c:
				colours_[target >> 2] = colour(value);
			break;

			case 0x40:	border_colour_ = colour(value);	break;

			case 0x44:	case 0x48:	case 0x4c:
				logger.error().append("TODO: Cursor colour %d to %03x", (target - 0x44) >> 2, value & 0x1fff);
			break;

			case 0x80:	horizontal_timing_.period = timing_value(value);		break;
			case 0x84:	horizontal_timing_.sync_width = timing_value(value);	break;
			case 0x88:	horizontal_timing_.border_start = timing_value(value);	break;
			case 0x8c:	horizontal_timing_.display_start = timing_value(value);	break;
			case 0x90:	horizontal_timing_.display_end = timing_value(value);	break;
			case 0x94:	horizontal_timing_.border_end = timing_value(value);	break;
			case 0x98:	horizontal_timing_.cursor_end = timing_value(value);	break;
			case 0x9c:
				logger.error().append("TODO: Video horizontal interlace: %d", (value >> 14) & 0x3ff);
			break;

			case 0xa0:	vertical_timing_.period = timing_value(value);			break;
			case 0xa4:	vertical_timing_.sync_width = timing_value(value);		break;
			case 0xa8:	vertical_timing_.border_start = timing_value(value);	break;
			case 0xac:	vertical_timing_.display_start = timing_value(value);	break;
			case 0xb0:	vertical_timing_.display_end = timing_value(value);		break;
			case 0xb4:	vertical_timing_.border_end = timing_value(value);		break;
			case 0xb8:	vertical_timing_.cursor_start = timing_value(value);	break;
			case 0xbc:	vertical_timing_.cursor_end = timing_value(value);		break;

			case 0xe0:
				logger.error().append("TODO: video control: %08x", value);

				// Set pixel rate. This is the value that a 24Mhz clock should be divided
				// by to get half the pixel rate.
				switch(value & 0b11) {
					case 0b00:	set_clock_divider(6);	break;	// i.e. pixel clock = 8Mhz.
					case 0b01:	set_clock_divider(4);	break;	// 12Mhz.
					case 0b10:	set_clock_divider(3);	break;	// 16Mhz.
					case 0b11:	set_clock_divider(2);	break;	// 24Mhz.
				}
			break;

			//
			// Sound parameters.
			//
			case 0x60:	case 0x64:	case 0x68:	case 0x6c:
			case 0x70:	case 0x74:	case 0x78:	case 0x7c: {
				const uint8_t channel = ((value >> 26) + 7) & 7;
				sound_.set_stereo_image(channel, value & 7);
			} break;

			case 0xc0:
				sound_.set_frequency(value & 0x7f);
			break;

			default:
				logger.error().append("TODO: unrecognised VIDC write of %08x", value);
			break;
		}
	}

	void tick() {
		// Pick new horizontal state, possibly rolling over into the vertical.
		horizontal_state_.increment_position();
		horizontal_state_.phase =
			horizontal_timing_.phase_after(
				horizontal_state_.position,
				horizontal_state_.phase);

		if(horizontal_state_.position == horizontal_timing_.period ) {
			horizontal_state_.position = 0;

			vertical_state_.increment_position();
			vertical_state_.phase =
				vertical_timing_.phase_after(
					vertical_state_.position,
					vertical_state_.phase);

			if(vertical_state_.position == vertical_timing_.period) {
				vertical_state_.position = 0;
				address_ = frame_start_;

				entered_sync_ = true;
				interrupt_observer_.update_interrupts();
			}
		}

		// Accumulate total phase.
		++time_in_phase_;

		// Grab some more pixels if appropriate.
		const auto flush_pixels = [&]() {
			const auto duration = static_cast<int>(time_in_phase_);
			crt_.output_data(duration, static_cast<size_t>(time_in_phase_));
			time_in_phase_ = 0;
			pixels_ = nullptr;
		};

		if(phase_ == Phase::Display) {
			if(pixels_ && time_in_phase_ == PixelBufferSize) {
				flush_pixels();
			}

			if(!pixels_) {
				if(time_in_phase_) {
					flush_pixels();
				}

				pixels_ = reinterpret_cast<uint16_t *>(crt_.begin_data(PixelBufferSize));
			}

			if(pixels_) {
				// Each tick in here is two ticks of the pixel clock, so:
				//
				//	8bpp mode: output two bytes;
				//	4bpp mode: output one byte;
				//	2bpp mode: output one byte every second tick;
				//	1bpp mode: output one byte every fourth tick.

				// TODO: don't assume 4bpp.
				const uint8_t next = ram_[address_];
				++address_;
				if(address_ == buffer_end_) address_ = buffer_start_;

				pixels_[0] = colours_[next & 0xf];
				pixels_[1] = colours_[next >> 4];
				pixels_ += 2;
			} else {
				// TODO: don't assume 4bpp here either.
				++address_;
				if(address_ == buffer_end_) address_ = buffer_start_;
			}
		}

		// Determine current output phase.
		Phase new_phase;
		switch(vertical_state_.phase) {
			case Phase::Sync:	new_phase = Phase::Sync;	break;
			case Phase::Blank:	new_phase = Phase::Blank;	break;
			case Phase::Border:
				new_phase = horizontal_state_.phase == Phase::Display ? Phase::Border : horizontal_state_.phase;
			break;
			case Phase::Display:
				new_phase = horizontal_state_.phase;
			break;
		}

		// Possibly output something.
		if(new_phase != phase_) {
			if(time_in_phase_) {
				const auto duration = static_cast<int>(time_in_phase_);

				switch(phase_) {
					case Phase::Sync:		crt_.output_sync(duration);									break;
					case Phase::Blank:		crt_.output_blank(duration);								break;
					case Phase::Display:	flush_pixels();												break;
					case Phase::Border:		crt_.output_level<uint16_t>(duration, border_colour_);		break;
				}
				time_in_phase_ = 0;
			}

			phase_ = new_phase;
		}
	}

	/// @returns @c true if a vertical retrace interrupt has been signalled since the last call to @c interrupt(); @c false otherwise.
	bool interrupt() {
		// Guess: edge triggered?
		const bool interrupt = entered_sync_;
		entered_sync_ = false;
		return interrupt;
	}

	void set_frame_start(uint32_t address) 	{	frame_start_ = address;		}
	void set_buffer_start(uint32_t address)	{	buffer_start_ = address;	}
	void set_buffer_end(uint32_t address)	{	buffer_end_ = address;		}
	void set_cursor_start(uint32_t address)	{	cursor_start_ = address;	}

	Outputs::CRT::CRT &crt() 				{ return crt_; }
	const Outputs::CRT::CRT &crt() const	{ return crt_; }

	int clock_divider() const {
		return clock_divider_;
	}

private:
	Log::Logger<Log::Source::ARMIOC> logger;
	InterruptObserverT &interrupt_observer_;
	ClockRateObserverT &clock_rate_observer_;
	SoundT &sound_;

	// In the current version of this code, video DMA occurrs costlessly,
	// being deferred to the component itself.
	const uint8_t *ram_ = nullptr;
	Outputs::CRT::CRT crt_;

	// Current video state.
	enum class Phase {
		Sync, Blank, Border, Display,
	};
	struct State {
		uint32_t position = 0;
		Phase phase = Phase::Sync;

		void increment_position() {
			++position;
			if(position == 1024) position = 0;
		}
	};
	State horizontal_state_, vertical_state_;
	Phase phase_ = Phase::Sync;
	uint32_t time_in_phase_ = 0;
	uint16_t *pixels_ = nullptr;
	static constexpr size_t PixelBufferSize = 320;

	// Programmer-set addresses.
	uint32_t buffer_start_ = 0;
	uint32_t buffer_end_ = 0;
	uint32_t frame_start_ = 0;
	uint32_t cursor_start_ = 0;

	// Ephemeral address state.
	uint32_t address_ = 0;

	// Horizontal and vertical timing.
	struct Timing {
		uint32_t period = 0;
		uint32_t sync_width = 0;
		uint32_t border_start = 0;
		uint32_t border_end = 0;
		uint32_t display_start = 0;
		uint32_t display_end = 0;
		uint32_t cursor_start = 0;
		uint32_t cursor_end = 0;

		Phase phase_after(uint32_t position, Phase current_phase) {
			if(position == sync_width) return Phase::Blank;
			if(position == border_start) return Phase::Border;
			if(position == display_start) return Phase::Display;
			if(position == display_end) return Phase::Border;
			if(position == border_end) return Phase::Blank;
			if(position == period) return Phase::Sync;
			return current_phase;
		}
	};
	Timing horizontal_timing_, vertical_timing_;

	// Colour palette, converted to internal format.
	uint16_t border_colour_;
	std::array<uint16_t, 16> colours_{};

	// An interrupt flag; more closely related to the interface by which
	// my implementation of the IOC picks up an interrupt request than
	// to hardware.
	bool entered_sync_ = false;

	// The divider that would need to be applied to a 24Mhz clock to
	// get half the current pixel clock; counting is in units of half
	// the pixel clock because that's the fidelity at which the programmer
	// places horizontal events — display start, end, sync period, etc.
	uint32_t clock_divider_ = 0;

	void set_clock_divider(uint32_t divider) {
		if(divider == clock_divider_) {
			return;
		}

		clock_divider_ = divider;
		const auto cycles_per_line = static_cast<int>(24'000'000 / (divider * 312 * 50));
		crt_.set_new_timing(
			cycles_per_line,
			312,								/* Height of display. */
			Outputs::CRT::PAL::ColourSpace,
			Outputs::CRT::PAL::ColourCycleNumerator,
			Outputs::CRT::PAL::ColourCycleDenominator,
			Outputs::CRT::PAL::VerticalSyncLength,
			Outputs::CRT::PAL::AlternatesPhase);
		clock_rate_observer_.update_clock_rates();
	}
};

}
