//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "Interrupts.hpp"

#include <vector>

namespace Electron {

/*!
	Implements the Electron's video subsystem plus appropriate signalling.

	The Electron has an interlaced fully-bitmapped display with six different output modes,
	running either at 40 or 80 columns. Memory is shared between video and CPU; when the video
	is accessing it the CPU may not.
*/
class VideoOutput {
	public:
		/*!
			Instantiates a VideoOutput that will read its pixels from @c memory.

			The pointer supplied should be to address 0 in the unexpanded Electron's memory map.
		*/
		VideoOutput(const uint8_t *memory);

		/// Sets the destination for output.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);

		/// Gets the current scan status.
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

		/// Sets the type of output.
		void set_display_type(Outputs::Display::DisplayType);

		/// Gets the type of output.
		Outputs::Display::DisplayType get_display_type() const;

		/// Produces the next @c cycles of video output.
		///
		/// @returns a bit mask of all interrupts triggered.
		Electron::Interrupt run_for(const Cycles cycles);

		/// @returns The number of 2Mhz cycles that will pass before completion of an attempted
		/// IO [/1Mhz] access that is first signalled in the upcoming cycle.
		Cycles io_delay() {
			return 2 + ((h_count >> 3)&1);
		}

		/// @returns The number of 2Mhz cycles that will pass before completion of an attempted
		/// RAM access that is first signalled in the upcoming cycle.
		Cycles ram_delay() {
			if(!mode_40 && !in_blank()) {
				return 2 + h_active - h_count;
			}
			return io_delay();
		}

		/*!
			Writes @c value to the register at @c address. May mutate the results of @c get_next_interrupt,
			@c get_cycles_until_next_ram_availability and @c get_memory_access_range.
		*/
		void write(int address, uint8_t value);

		/*!
			@returns the number of cycles after (final cycle of last run_for batch + @c from_time)
			before the video circuits will allow the CPU to access RAM.
		*/
		unsigned int get_cycles_until_next_ram_availability(int from_time);

	private:
		const uint8_t *ram_ = nullptr;

		// CRT output
		uint8_t *current_output_target_ = nullptr;
		uint8_t *initial_output_target_ = nullptr;
		int current_output_divider_ = 1;
		Outputs::CRT::CRT crt_;

		// Palette.
		uint8_t palette_[16];

		// User-selected base address; constrained to a 64-byte boundary by the setter.
		uint16_t screen_base;

		// Parameters implied by mode selection.
		uint16_t mode_base = 0;
		bool mode_40 = true;
		bool mode_text = false;
		enum class Bpp {
			One, Two, Four
		} mode_bpp = Bpp::One;

		// Frame position.
		int v_count = 0;
		int h_count = 0;
		bool field = false;

		// Current working address.
		uint16_t row_addr = 0;	// Address, sans character row, adopted at the start of a row.
		uint16_t byte_addr = 0;	// Current working address, incremented as the raster moves across the line.
		int char_row = 0;		// Character row; 0–9 in text mode, 0–7 in graphics.

		// Sync states.
		bool vsync_int = false;	// True => vsync active.
		bool hsync_int = false;	// True => hsync active.

		// Horizontal timing parameters; all in terms of the 16Mhz pixel clock but conveniently all
		// divisible by 8, so it's safe to count time with a 2Mhz input.
		static constexpr int h_active = 640;
		static constexpr int hsync_start = 768;
		static constexpr int hsync_end = 832;
		static constexpr int h_reset_addr = 1016;
		static constexpr int h_total = 1024;	// Minor digression from the FPGA original here;
												// in this implementation the value is tested
												// _after_ position increment rather than before/instead.
												// So it needs to be one higher. Which is baked into
												// the constant to emphasise the all-divisible-by-8 property.
		static constexpr int h_half = h_total / 2;

		// Vertical timing parameters; all in terms of lines. As per the horizontal parameters above,
		// lines begin with their first visible pixel (or the equivalent position).
		static constexpr int v_active_gph = 256;
		static constexpr int v_active_txt = 250;
		static constexpr int v_disp_gph = v_active_gph - 1;
		static constexpr int v_disp_txt = v_active_txt - 1;
		static constexpr int vsync_start = 274;
		static constexpr int vsync_end = 276;
		static constexpr int v_rtc = 99;

		// Various signals that it was convenient to factor out.
		int v_total() const {
			return field ? 312 : 311;
		}

		bool last_line() const {
			return char_row == (mode_text ? 9 : 7);
		}

		bool in_blank() const {
			return h_count >= h_active || (mode_text && v_count >= v_active_txt) || (!mode_text && v_count >= v_active_gph) || char_row >= 8;
		}};
}
