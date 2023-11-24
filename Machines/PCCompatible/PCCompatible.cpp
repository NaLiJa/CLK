//
//  PCCompatible.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "PCCompatible.hpp"

#include "DMA.hpp"
#include "PIC.hpp"
#include "PIT.hpp"

#include "../../InstructionSets/x86/Decoder.hpp"
#include "../../InstructionSets/x86/Flags.hpp"
#include "../../InstructionSets/x86/Instruction.hpp"
#include "../../InstructionSets/x86/Perform.hpp"

#include "../../Components/6845/CRTC6845.hpp"
#include "../../Components/8255/i8255.hpp"
#include "../../Components/AudioToggle/AudioToggle.hpp"

#include "../../Numeric/RegisterSizes.hpp"

#include "../../Outputs/CRT/CRT.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include "../AudioProducer.hpp"
#include "../ScanProducer.hpp"
#include "../TimedMachine.hpp"

#include <array>
#include <iostream>

namespace PCCompatible {

class KeyboardController {
	public:
		KeyboardController(PIC &pic) : pic_(pic) {}

		// KB Status Port 61h high bits:
		//; 01 - normal operation. wait for keypress, when one comes in,
		//;		force data line low (forcing keyboard to buffer additional
		//;		keypresses) and raise IRQ1 high
		//; 11 - stop forcing data line low. lower IRQ1 and don't raise it again.
		//;		drop all incoming keypresses on the floor.
		//; 10 - lower IRQ1 and force clock line low, resetting keyboard
		//; 00 - force clock line low, resetting keyboard, but on a 01->00 transition,
		//;		IRQ1 would remain high
		void set_mode(uint8_t mode) {
			mode_ = Mode(mode);
			switch(mode_) {
				case Mode::NormalOperation: 	break;
				case Mode::NoIRQsIgnoreInput:
					pic_.apply_edge<1>(false);
				break;
				case Mode::ClearIRQReset:
					pic_.apply_edge<1>(false);
					[[fallthrough]];
				case Mode::Reset:
					reset_delay_ = 5;	// Arbitrarily.
				break;
			}
		}

		void run_for(Cycles cycles) {
			if(reset_delay_ <= 0) {
				return;
			}
			reset_delay_ -= cycles.as<int>();
			if(reset_delay_ <= 0) {
				post(0xaa);
			}
		}

		uint8_t read() {
			pic_.apply_edge<1>(false);
			return input_;
		}

	private:
		void post(uint8_t value) {
			input_ = value;
			pic_.apply_edge<1>(true);
		}

		enum class Mode {
			NormalOperation = 0b01,
			NoIRQsIgnoreInput = 0b11,
			ClearIRQReset = 0b10,
			Reset = 0b00,
		} mode_;

		uint8_t input_ = 0;
		PIC &pic_;

		int reset_delay_ = 0;
};

class MDA {
	public:
		MDA() : crtc_(Motorola::CRTC::Personality::HD6845S, outputter_) {}

		void set_source(const uint8_t *ram, std::vector<uint8_t> font) {
			outputter_.ram = ram;
			outputter_.font = font;
		}

		void run_for(Cycles cycles) {
			// I _think_ the MDA's CRTC is clocked at 14/9ths the PIT clock.
			// Do that conversion here.
			full_clock_ += 14 * cycles.as<int>();
			crtc_.run_for(Cycles(full_clock_ / 9));
			full_clock_ %= 9;
		}

		template <int address>
		void write(uint8_t value) {
			if constexpr (address & 0x8) {
				printf("TODO: write MDA control %02x\n", value);
			} else {
				if constexpr (address & 0x1) {
					crtc_.set_register(value);
				} else {
					crtc_.select_register(value);
				}
			}
		}

		template <int address>
		uint8_t read() {
			if constexpr (address & 0x8) {
				printf("TODO: read MDA control\n");
				return 0xff;
			} else {
				return crtc_.get_register();
			}
		}

		// MARK: - Call-ins for ScanProducer.

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) {
			outputter_.crt.set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const {
			return outputter_.crt.get_scaled_scan_status() / 4.0f;
		}

	private:
		struct CRTCOutputter {
			CRTCOutputter() :
				crt(882, 9, 382, 3, Outputs::Display::InputDataType::Red2Green2Blue2)
				// TODO: really this should be a Luminance8 and set an appropriate modal tint colour;
				// consider whether that's worth building into the scan target.
			{
//				crt.set_visible_area(Outputs::Display::Rect(0.1072f, 0.1f, 0.842105263157895f, 0.842105263157895f));
				crt.set_display_type(Outputs::Display::DisplayType::RGB);
			}

			void perform_bus_cycle_phase1(const Motorola::CRTC::BusState &state) {
				// Determine new output state.
				const OutputState new_state =
					(state.hsync | state.vsync) ? OutputState::Sync :
						(state.display_enable ? OutputState::Pixels : OutputState::Border);

				// Upon either a state change or just having accumulated too much local time...
				if(new_state != output_state || count > 882) {
					// (1) flush preexisting state.
					if(count) {
						switch(output_state) {
							case OutputState::Sync:		crt.output_sync(count);		break;
							case OutputState::Border: 	crt.output_blank(count);	break;
							case OutputState::Pixels:
								crt.output_data(count);
								pixels = pixel_pointer = nullptr;
							break;
						}
					}

					// (2) adopt new state.
					output_state = new_state;
					count = 0;
				}

				// Collect pixels if applicable.
				if(output_state == OutputState::Pixels) {
					if(!pixels) {
						pixel_pointer = pixels = crt.begin_data(DefaultAllocationSize);

						// Flush any period where pixels weren't recorded due to back pressure.
						if(pixels && count) {
							crt.output_blank(count);
							count = 0;
						}
					}

					// TODO: cursor.
					if(pixels) {
						const uint8_t attributes = ram[((state.refresh_address << 1) + 1) & 0xfff];
						const uint8_t glyph = ram[((state.refresh_address << 1) + 0) & 0xfff];
						uint8_t row = font[(glyph * 14) + state.row_address];

						const uint8_t intensity = (attributes & 0x08) ? 0x0d : 0x09;
						uint8_t blank = 0;

						// Handle irregular attributes.
						// Cf. http://www.seasip.info/VintagePC/mda.html#memmap
						switch(attributes) {
							case 0x00:	case 0x08:	case 0x80:	case 0x88:
								row = 0;
							break;
							case 0x70:	case 0x78:	case 0xf0:	case 0xf8:
								row ^= 0xff;
								blank = intensity;
							break;
						}

						if(((attributes & 7) == 1) && state.row_address == 13) {
							// Draw as underline.
							std::fill(pixel_pointer, pixel_pointer + 9, intensity);
						} else {
							// Draw according to ROM contents, possibly duplicating final column.
							pixel_pointer[0] = (row & 0x80) ? intensity : 0;
							pixel_pointer[1] = (row & 0x40) ? intensity : 0;
							pixel_pointer[2] = (row & 0x20) ? intensity : 0;
							pixel_pointer[3] = (row & 0x10) ? intensity : 0;
							pixel_pointer[4] = (row & 0x08) ? intensity : 0;
							pixel_pointer[5] = (row & 0x04) ? intensity : 0;
							pixel_pointer[6] = (row & 0x02) ? intensity : 0;
							pixel_pointer[7] = (row & 0x01) ? intensity : 0;
							pixel_pointer[8] = (glyph >= 0xc0 && glyph < 0xe0) ? pixel_pointer[7] : blank;
						}
						pixel_pointer += 9;
					}
				}

				// Advance.
				count += 9;

				// Output pixel row prematurely if storage is exhausted.
				if(output_state == OutputState::Pixels && pixel_pointer == pixels + DefaultAllocationSize) {
					crt.output_data(count);
					count = 0;

					pixels = pixel_pointer = nullptr;
				}
			}
			void perform_bus_cycle_phase2(const Motorola::CRTC::BusState &) {}

			Outputs::CRT::CRT crt;

			enum class OutputState {
				Sync, Pixels, Border
			} output_state = OutputState::Sync;
			int count = 0;

			uint8_t *pixels = nullptr;
			uint8_t *pixel_pointer = nullptr;
			static constexpr size_t DefaultAllocationSize = 720;

			const uint8_t *ram = nullptr;
			std::vector<uint8_t> font;
		} outputter_;
		Motorola::CRTC::CRTC6845<CRTCOutputter> crtc_;

		int full_clock_;
};

struct PCSpeaker {
	PCSpeaker() :
		toggle(queue),
		speaker(toggle) {}

	void update() {
		speaker.run_for(queue, cycles_since_update);
		cycles_since_update = 0;
	}

	void set_pit(bool pit_input) {
		pit_input_ = pit_input;
		set_level();
	}

	void set_control(bool pit_mask, bool level) {
		pit_mask_ = pit_mask;
		level_ = level;
		set_level();
	}

	void set_level() {
		// TODO: eliminate complete guess of mixing function here.
		const bool new_output = (pit_mask_ & pit_input_) ^ level_;

		if(new_output != output_) {
			update();
			toggle.set_output(new_output);
			output_ = new_output;
		}
	}

	Concurrency::AsyncTaskQueue<false> queue;
	Audio::Toggle toggle;
	Outputs::Speaker::PullLowpass<Audio::Toggle> speaker;
	Cycles cycles_since_update = 0;

	bool pit_input_ = false;
	bool pit_mask_ = false;
	bool level_ = false;
	bool output_ = false;
};

class PITObserver {
	public:
		PITObserver(PIC &pic, PCSpeaker &speaker) : pic_(pic), speaker_(speaker) {}

		template <int channel>
		void update_output(bool new_level) {
			switch(channel) {
				default: break;
				case 0: pic_.apply_edge<0>(new_level);	break;
				case 2: speaker_.set_pit(new_level);	break;
			}
		}

	private:
		PIC &pic_;
		PCSpeaker &speaker_;

	// TODO:
	//
	//	channel 0 is connected to IRQ 0;
	//	channel 1 is used for DRAM refresh (presumably connected to DMA?);
	//	channel 2 is gated by a PPI output and feeds into the speaker.
};
using PIT = i8237<false, PITObserver>;

class i8255PortHandler : public Intel::i8255::PortHandler {
	// Likely to be helpful: https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-XT-Keyboard-Protocol
	public:
		i8255PortHandler(PCSpeaker &speaker, KeyboardController &keyboard) : speaker_(speaker), keyboard_(keyboard) {}

		void set_value(int port, uint8_t value) {
			switch(port) {
				case 1:
					// b7: 0 => enable keyboard read (and IRQ); 1 => don't;
					// b6: 0 => hold keyboard clock low; 1 => don't;
					// b5: 1 => disable IO check; 0 => don't;
					// b4: 1 => disable memory parity check; 0 => don't;
					// b3: [5150] cassette motor control; [5160] high or low switches select;
					// b2: [5150] high or low switches select; [5160] 1 => disable turbo mode;
					// b1, b0: speaker control.
					enable_keyboard_ = !(value & 0x80);
					keyboard_.set_mode(value >> 6);

					high_switches_ = value & 0x08;
					speaker_.set_control(value & 0x01, value & 0x02);
				break;
			}
			printf("PPI: %02x to %d\n", value, port);
		}

		uint8_t get_value(int port) {
			switch(port) {
				case 0:
					printf("PPI: from keyboard\n");
					return enable_keyboard_ ? keyboard_.read() : 0b0011'1100;
						// Guesses that switches is high and low combined as below.

				case 2:
					// Common:
					//
					// b7: 1 => memory parity error; 0 => none;
					// b6: 1 => IO channel error; 0 => none;
					// b5: timer 2 output;	[TODO]
					// b4: cassette data input; [TODO]
					return
						high_switches_ ?
							// b3, b2: drive count; 00 = 1, 01 = 2, etc
							// b1, b0: video mode (00 = ROM; 01 = CGA40; 10 = CGA80; 11 = MDA)
							0b0000'0011
						:
							// b3, b2: RAM on motherboard (64 * bit pattern)
							// b1: 1 => FPU present; 0 => absent;
							// b0: 1 => floppy drive present; 0 => absent.
							0b0000'1100;
			}
			return 0;
		};

	private:
		bool high_switches_ = false;
		PCSpeaker &speaker_;
		KeyboardController &keyboard_;

		bool enable_keyboard_ = false;
};
using PPI = Intel::i8255::i8255<i8255PortHandler>;

struct Registers {
	public:
		static constexpr bool is_32bit = false;

		uint8_t &al()	{	return ax_.halves.low;	}
		uint8_t &ah()	{	return ax_.halves.high;	}
		uint16_t &ax()	{	return ax_.full;		}

		CPU::RegisterPair16 &axp()	{	return ax_;	}

		uint8_t &cl()	{	return cx_.halves.low;	}
		uint8_t &ch()	{	return cx_.halves.high;	}
		uint16_t &cx()	{	return cx_.full;		}

		uint8_t &dl()	{	return dx_.halves.low;	}
		uint8_t &dh()	{	return dx_.halves.high;	}
		uint16_t &dx()	{	return dx_.full;		}

		uint8_t &bl()	{	return bx_.halves.low;	}
		uint8_t &bh()	{	return bx_.halves.high;	}
		uint16_t &bx()	{	return bx_.full;		}

		uint16_t &sp()	{	return sp_;				}
		uint16_t &bp()	{	return bp_;				}
		uint16_t &si()	{	return si_;				}
		uint16_t &di()	{	return di_;				}

		uint16_t &ip()	{	return ip_;				}

		uint16_t &es()		{	return es_;			}
		uint16_t &cs()		{	return cs_;			}
		uint16_t &ds()		{	return ds_;			}
		uint16_t &ss()		{	return ss_;			}
		uint16_t es() const	{	return es_;			}
		uint16_t cs() const	{	return cs_;			}
		uint16_t ds() const	{	return ds_;			}
		uint16_t ss() const	{	return ss_;			}

		void reset() {
			cs_ = 0xffff;
			ip_ = 0;
		}

	private:
		CPU::RegisterPair16 ax_;
		CPU::RegisterPair16 cx_;
		CPU::RegisterPair16 dx_;
		CPU::RegisterPair16 bx_;

		uint16_t sp_;
		uint16_t bp_;
		uint16_t si_;
		uint16_t di_;
		uint16_t es_, cs_, ds_, ss_;
		uint16_t ip_;
};

class Segments {
	public:
		Segments(const Registers &registers) : registers_(registers) {}

		using Source = InstructionSet::x86::Source;

		/// Posted by @c perform after any operation which *might* have affected a segment register.
		void did_update(Source segment) {
			switch(segment) {
				default: break;
				case Source::ES:	es_base_ = uint32_t(registers_.es()) << 4;	break;
				case Source::CS:	cs_base_ = uint32_t(registers_.cs()) << 4;	break;
				case Source::DS:	ds_base_ = uint32_t(registers_.ds()) << 4;	break;
				case Source::SS:	ss_base_ = uint32_t(registers_.ss()) << 4;	break;
			}
		}

		void reset() {
			did_update(Source::ES);
			did_update(Source::CS);
			did_update(Source::DS);
			did_update(Source::SS);
		}

		uint32_t es_base_, cs_base_, ds_base_, ss_base_;

		bool operator ==(const Segments &rhs) const {
			return
				es_base_ == rhs.es_base_ &&
				cs_base_ == rhs.cs_base_ &&
				ds_base_ == rhs.ds_base_ &&
				ss_base_ == rhs.ss_base_;
		}

	private:
		const Registers &registers_;
};

// TODO: send writes to the ROM area off to nowhere.
struct Memory {
	public:
		using AccessType = InstructionSet::x86::AccessType;

		// Constructor.
		Memory(Registers &registers, const Segments &segments) : registers_(registers), segments_(segments) {}

		//
		// Preauthorisation call-ins. Since only an 8088 is currently modelled, all accesses are implicitly authorised.
		//
		void preauthorise_stack_write([[maybe_unused]] uint32_t length) {}
		void preauthorise_stack_read([[maybe_unused]] uint32_t length) {}
		void preauthorise_read([[maybe_unused]] InstructionSet::x86::Source segment, [[maybe_unused]] uint16_t start, [[maybe_unused]] uint32_t length) {}
		void preauthorise_read([[maybe_unused]] uint32_t start, [[maybe_unused]] uint32_t length) {}

		//
		// Access call-ins.
		//

		// Accesses an address based on segment:offset.
		template <typename IntT, AccessType type>
		typename InstructionSet::x86::Accessor<IntT, type>::type access(InstructionSet::x86::Source segment, uint16_t offset) {
			const uint32_t physical_address = address(segment, offset);

			if constexpr (std::is_same_v<IntT, uint16_t>) {
				// If this is a 16-bit access that runs past the end of the segment, it'll wrap back
				// to the start. So the 16-bit value will need to be a local cache.
				if(offset == 0xffff) {
					return split_word<type>(physical_address, address(segment, 0));
				}
			}

			return access<IntT, type>(physical_address);
		}

		// Accesses an address based on physical location.
//		int mda_delay = -1;	// HACK.
		template <typename IntT, AccessType type>
		typename InstructionSet::x86::Accessor<IntT, type>::type access(uint32_t address) {

			// TEMPORARY HACK.
//			if(mda_delay > 0) {
//				--mda_delay;
//				if(!mda_delay) {
//					print_mda();
//				}
//			}
//			if(address >= 0xb'0000 && is_writeable(type)) {
//				mda_delay = 100;
//			}

			// Dispense with the single-byte case trivially.
			if constexpr (std::is_same_v<IntT, uint8_t>) {
				return memory[address];
			} else if(address != 0xf'ffff) {
				return *reinterpret_cast<IntT *>(&memory[address]);
			} else {
				return split_word<type>(address, 0);
			}
		}

		template <typename IntT>
		void write_back() {
			if constexpr (std::is_same_v<IntT, uint16_t>) {
				if(write_back_address_[0] != NoWriteBack) {
					memory[write_back_address_[0]] = write_back_value_ & 0xff;
					memory[write_back_address_[1]] = write_back_value_ >> 8;
					write_back_address_[0]  = 0;
				}
			}
		}

		//
		// Direct write.
		//
		template <typename IntT>
		void preauthorised_write(InstructionSet::x86::Source segment, uint16_t offset, IntT value) {
			// Bytes can be written without further ado.
			if constexpr (std::is_same_v<IntT, uint8_t>) {
				memory[address(segment, offset) & 0xf'ffff] = value;
				return;
			}

			// Words that straddle the segment end must be split in two.
			if(offset == 0xffff) {
				memory[address(segment, offset) & 0xf'ffff] = value & 0xff;
				memory[address(segment, 0x0000) & 0xf'ffff] = value >> 8;
				return;
			}

			const uint32_t target = address(segment, offset) & 0xf'ffff;

			// Words that straddle the end of physical RAM must also be split in two.
			if(target == 0xf'ffff) {
				memory[0xf'ffff] = value & 0xff;
				memory[0x0'0000] = value >> 8;
				return;
			}

			// It's safe just to write then.
			*reinterpret_cast<uint16_t *>(&memory[target]) = value;
		}

		//
		// Helper for instruction fetch.
		//
		std::pair<const uint8_t *, size_t> next_code() {
			const uint32_t start = segments_.cs_base_ + registers_.ip();
			return std::make_pair(&memory[start], 0x10'000 - start);
		}

		std::pair<const uint8_t *, size_t> all() {
			return std::make_pair(memory.data(), 0x10'000);
		}

		//
		// External access.
		//
		void install(size_t address, const uint8_t *data, size_t length) {
			std::copy(data, data + length, memory.begin() + std::vector<uint8_t>::difference_type(address));
		}

		const uint8_t *at(uint32_t address) {
			return &memory[address];
		}

		// TEMPORARY HACK.
//		void print_mda() {
//			uint32_t pointer = 0xb'0000;
//			for(int y = 0; y < 25; y++) {
//				for(int x = 0; x < 80; x++) {
//					printf("%c", memory[pointer]);
//					pointer += 2;	// MDA goes [character, attributes]...; skip the attributes.
//				}
//				printf("\n");
//			}
//		}

	private:
		std::array<uint8_t, 1024*1024> memory{0xff};
		Registers &registers_;
		const Segments &segments_;

		uint32_t segment_base(InstructionSet::x86::Source segment) {
			using Source = InstructionSet::x86::Source;
			switch(segment) {
				default:			return segments_.ds_base_;
				case Source::ES:	return segments_.es_base_;
				case Source::CS:	return segments_.cs_base_;
				case Source::SS:	return segments_.ss_base_;
			}
		}

		uint32_t address(InstructionSet::x86::Source segment, uint16_t offset) {
			return (segment_base(segment) + offset) & 0xf'ffff;
		}

		template <AccessType type>
		typename InstructionSet::x86::Accessor<uint16_t, type>::type
		split_word(uint32_t low_address, uint32_t high_address) {
			if constexpr (is_writeable(type)) {
				write_back_address_[0] = low_address;
				write_back_address_[1] = high_address;

				// Prepopulate only if this is a modify.
				if constexpr (type == AccessType::ReadModifyWrite) {
					write_back_value_ = uint16_t(memory[write_back_address_[0]] | (memory[write_back_address_[1]] << 8));
				}

				return write_back_value_;
			} else {
				return uint16_t(memory[low_address] | (memory[high_address] << 8));
			}
		}

		static constexpr uint32_t NoWriteBack = 0;	// A low byte address of 0 can't require write-back.
		uint32_t write_back_address_[2] = {NoWriteBack, NoWriteBack};
		uint16_t write_back_value_;
};

class IO {
	public:
		IO(PIT &pit, DMA &dma, PPI &ppi, PIC &pic, MDA &mda) :
			pit_(pit), dma_(dma), ppi_(ppi), pic_(pic), mda_(mda) {}

		template <typename IntT> void out(uint16_t port, IntT value) {
			switch(port) {
				default:
					if constexpr (std::is_same_v<IntT, uint8_t>) {
						printf("Unhandled out: %02x to %04x\n", value, port);
					} else {
						printf("Unhandled out: %04x to %04x\n", value, port);
					}
				break;

				// On the XT the NMI can be masked by setting bit 7 on I/O port 0xA0.
				case 0x00a0:
					printf("TODO: NMIs %s\n", (value & 0x80) ? "masked" : "unmasked");
				break;

				case 0x0000:	dma_.write<0>(value);	break;
				case 0x0001:	dma_.write<1>(value);	break;
				case 0x0002:	dma_.write<2>(value);	break;
				case 0x0003:	dma_.write<3>(value);	break;
				case 0x0004:	dma_.write<4>(value);	break;
				case 0x0005:	dma_.write<5>(value);	break;
				case 0x0006:	dma_.write<6>(value);	break;
				case 0x0007:	dma_.write<7>(value);	break;

				case 0x0008:	case 0x0009:	case 0x000a:	case 0x000b:
				case 0x000c:	case 0x000f:
					printf("TODO: DMA write of %02x at %04x\n", value, port);
				break;

				case 0x000d:	dma_.master_reset();	break;
				case 0x000e:	dma_.mask_reset();		break;

				case 0x0020:	pic_.write<0>(value);	break;
				case 0x0021:	pic_.write<1>(value);	break;

				case 0x0060:	case 0x0061:	case 0x0062:	case 0x0063:
				case 0x0064:	case 0x0065:	case 0x0066:	case 0x0067:
				case 0x0068:	case 0x0069:	case 0x006a:	case 0x006b:
				case 0x006c:	case 0x006d:	case 0x006e:	case 0x006f:
					ppi_.write(port, value);
				break;

				case 0x0080:	case 0x0081:	case 0x0082:	case 0x0083:
				case 0x0084:	case 0x0085:	case 0x0086:	case 0x0087:
				case 0x0088:	case 0x0089:	case 0x008a:	case 0x008b:
				case 0x008c:	case 0x008d:	case 0x008e:	case 0x008f:
					printf("TODO: DMA page write of %02x at %04x\n", value, port);
				break;

				case 0x03b0:	case 0x03b2:	case 0x03b4:	case 0x03b6:
					if constexpr (std::is_same_v<IntT, uint16_t>) {
						mda_.write<0>(value);
						mda_.write<1>(value >> 8);
					} else {
						mda_.write<0>(value);
					}
				break;

				case 0x03b1:	case 0x03b3:	case 0x03b5:	case 0x03b7:
					if constexpr (std::is_same_v<IntT, uint16_t>) {
						mda_.write<1>(value);
						mda_.write<0>(value >> 8);
					} else {
						mda_.write<1>(value);
					}
				break;

				case 0x03b8:	case 0x03b9:	case 0x03ba:	case 0x03bb:
				case 0x03bc:	case 0x03bd:	case 0x03be:	case 0x03bf:
					mda_.write<8>(value);
				break;

				case 0x03d0:	case 0x03d1:	case 0x03d2:	case 0x03d3:
				case 0x03d4:	case 0x03d5:	case 0x03d6:	case 0x03d7:
				case 0x03d8:	case 0x03d9:	case 0x03da:	case 0x03db:
				case 0x03dc:	case 0x03dd:	case 0x03de:	case 0x03df:
					printf("TODO: CGA write of %02x at %04x\n", value, port);
				break;

				case 0x0040:	pit_.write<0>(uint8_t(value));	break;
				case 0x0041:	pit_.write<1>(uint8_t(value));	break;
				case 0x0042:	pit_.write<2>(uint8_t(value));	break;
				case 0x0043:	pit_.set_mode(uint8_t(value));	break;
			}
		}
		template <typename IntT> IntT in([[maybe_unused]] uint16_t port) {
			switch(port) {
				default:
					printf("Unhandled in: %04x\n", port);
				break;

				case 0x0000:	return dma_.read<0>();
				case 0x0001:	return dma_.read<1>();
				case 0x0002:	return dma_.read<2>();
				case 0x0003:	return dma_.read<3>();
				case 0x0004:	return dma_.read<4>();
				case 0x0005:	return dma_.read<5>();
				case 0x0006:	return dma_.read<6>();
				case 0x0007:	return dma_.read<7>();

				case 0x0020:	return pic_.read<0>();
				case 0x0021:	return pic_.read<1>();

				case 0x0040:	return pit_.read<0>();
				case 0x0041:	return pit_.read<1>();
				case 0x0042:	return pit_.read<2>();

				case 0x0060:	case 0x0061:	case 0x0062:	case 0x0063:
				case 0x0064:	case 0x0065:	case 0x0066:	case 0x0067:
				case 0x0068:	case 0x0069:	case 0x006a:	case 0x006b:
				case 0x006c:	case 0x006d:	case 0x006e:	case 0x006f:
				return ppi_.read(port);
			}
			return IntT(~0);
		}

	private:
		PIT &pit_;
		DMA &dma_;
		PPI &ppi_;
		PIC &pic_;
		MDA &mda_;
};

class FlowController {
	public:
		FlowController(Registers &registers, Segments &segments) :
			registers_(registers), segments_(segments) {}

		// Requirements for perform.
		void jump(uint16_t address) {
			registers_.ip() = address;
		}

		void jump(uint16_t segment, uint16_t address) {
			registers_.cs() = segment;
			segments_.did_update(Segments::Source::CS);
			registers_.ip() = address;
		}

		void halt() {}
		void wait() {}

		void repeat_last() {
			should_repeat_ = true;
		}

		// Other actions.
		void begin_instruction() {
			should_repeat_ = false;
		}
		bool should_repeat() const {
			return should_repeat_;
		}

	private:
		Registers &registers_;
		Segments &segments_;
		bool should_repeat_ = false;
};

class ConcreteMachine:
	public Machine,
	public MachineTypes::TimedMachine,
	public MachineTypes::AudioProducer,
	public MachineTypes::ScanProducer
{
	public:
		ConcreteMachine(
			[[maybe_unused]] const Analyser::Static::Target &target,
			const ROMMachine::ROMFetcher &rom_fetcher
		) :
			keyboard_(pic_),
			pit_observer_(pic_, speaker_),
			ppi_handler_(speaker_, keyboard_),
			pit_(pit_observer_),
			ppi_(ppi_handler_),
			context(pit_, dma_, ppi_, pic_, mda_)
		{
			// Use clock rate as a MIPS count; keeping it as a multiple or divisor of the PIT frequency is easy.
			static constexpr int pit_frequency = 1'193'182;
			set_clock_rate(double(pit_frequency));
			speaker_.speaker.set_input_rate(double(pit_frequency));

			// Fetch the BIOS. [8088 only, for now]
			const auto bios = ROM::Name::PCCompatibleGLaBIOS;
			const auto font = ROM::Name::PCCompatibleMDAFont;

			ROM::Request request = ROM::Request(bios) && ROM::Request(font);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			const auto &bios_contents = roms.find(bios)->second;
			context.memory.install(0x10'0000 - bios_contents.size(), bios_contents.data(), bios_contents.size());

			// Give the MDA something to read from.
			const auto &font_contents = roms.find(font)->second;
			mda_.set_source(context.memory.at(0xb'0000), font_contents);
		}

		~ConcreteMachine() {
			speaker_.queue.flush();
		}

		// MARK: - TimedMachine.
//		bool log = false;
//		std::string previous;
		void run_for(const Cycles duration) override {
			const auto pit_ticks = duration.as_integral();
			cpu_divisor_ += pit_ticks;
			int ticks = cpu_divisor_ / 3;
			cpu_divisor_ %= 3;

			while(ticks--) {
				//
				// First draft: all hardware runs in lockstep, as a multiple or divisor of the PIT frequency.
				//

				//
				// Advance the PIT and audio.
				//
				pit_.run_for(1);
				++speaker_.cycles_since_update;
				pit_.run_for(1);
				++speaker_.cycles_since_update;
				pit_.run_for(1);
				++speaker_.cycles_since_update;

				//
				// Advance CRTC at a more approximate rate.
				//
				mda_.run_for(Cycles(3));

				//
				// Perform one CPU instruction every three PIT cycles.
				// i.e. CPU instruction rate is 1/3 * ~1.19Mhz ~= 0.4 MIPS.
				//

				keyboard_.run_for(Cycles(1));

				// Query for interrupts and apply if pending.
				if(pic_.pending() && context.flags.flag<InstructionSet::x86::Flag::Interrupt>()) {
					// Regress the IP if a REP is in-progress so as to resume it later.
					if(context.flow_controller.should_repeat()) {
						context.registers.ip() = decoded_ip_;
						context.flow_controller.begin_instruction();
					}

					// Signal interrupt.
					InstructionSet::x86::interrupt(
						pic_.acknowledge(),
						context
					);
				}

				// Get the next thing to execute.
				if(!context.flow_controller.should_repeat()) {
					// Decode from the current IP.
					decoded_ip_ = context.registers.ip();
					const auto remainder = context.memory.next_code();
					decoded = decoder.decode(remainder.first, remainder.second);

					// If that didn't yield a whole instruction then the end of memory must have been hit;
					// continue from the beginning.
					if(decoded.first <= 0) {
						const auto all = context.memory.all();
						decoded = decoder.decode(all.first, all.second);
					}

					context.registers.ip() += decoded.first;

//					log |= decoded.second.operation() == InstructionSet::x86::Operation::STI;
				} else {
					context.flow_controller.begin_instruction();
				}

//				if(log) {
//					const auto next = to_string(decoded, InstructionSet::x86::Model::i8086);
//					if(next != previous) {
//						std::cout << next << std::endl;
//						previous = next;
//					}
//				}

				// Execute it.
				InstructionSet::x86::perform(
					decoded.second,
					context
				);
			}
		}

		// MARK: - ScanProducer.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			mda_.set_scan_target(scan_target);
		}
		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return mda_.get_scaled_scan_status();
		}

		// MARK: - AudioProducer.
		Outputs::Speaker::Speaker *get_speaker() override {
			return &speaker_.speaker;
		}

		void flush_output(int outputs) final {
			if(outputs & Output::Audio) {
				speaker_.update();
				speaker_.queue.perform();
			}
		}

	private:
		PIC pic_;
		DMA dma_;
		PCSpeaker speaker_;
		MDA mda_;

		KeyboardController keyboard_;
		PITObserver pit_observer_;
		i8255PortHandler ppi_handler_;

		PIT pit_;
		PPI ppi_;

		struct Context {
			Context(PIT &pit, DMA &dma, PPI &ppi, PIC &pic, MDA &mda) :
				segments(registers),
				memory(registers, segments),
				flow_controller(registers, segments),
				io(pit, dma, ppi, pic, mda)
			{
				reset();
			}

			void reset() {
				registers.reset();
				segments.reset();
			}

			InstructionSet::x86::Flags flags;
			Registers registers;
			Segments segments;
			Memory memory;
			FlowController flow_controller;
			IO io;
			static constexpr auto model = InstructionSet::x86::Model::i8086;
		} context;

		// TODO: eliminate use of Decoder8086 and Decoder8086 in gneral in favour of the templated version, as soon
		// as whatever error is preventing GCC from picking up Decoder's explicit instantiations becomes apparent.
		InstructionSet::x86::Decoder8086 decoder;
//		InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder;

		uint16_t decoded_ip_ = 0;
		std::pair<int, InstructionSet::x86::Instruction<false>> decoded;

		int cpu_divisor_ = 0;
};


}

using namespace PCCompatible;

// See header; constructs and returns an instance of the Amstrad CPC.
Machine *Machine::PCCompatible(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new PCCompatible::ConcreteMachine(*target, rom_fetcher);
}

Machine::~Machine() {}
