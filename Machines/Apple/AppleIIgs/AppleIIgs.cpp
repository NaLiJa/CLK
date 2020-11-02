//
//  AppleIIgs.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

#include "AppleIIgs.hpp"

#include "../../MachineTypes.hpp"
#include "../../../Processors/65816/65816.hpp"

#include "../../../Analyser/Static/AppleIIgs/Target.hpp"
#include "ADB.hpp"
#include "MemoryMap.hpp"
#include "Video.hpp"

#include "../../../Components/8530/z8530.hpp"
#include "../../../Components/AppleClock/AppleClock.hpp"
#include "../../../Components/DiskII/IWM.hpp"

#include "../../Utility/MemoryFuzzer.hpp"

#include <cassert>
#include <array>

namespace Apple {
namespace IIgs {

class ConcreteMachine:
	public Apple::IIgs::Machine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public CPU::MOS6502Esque::BusHandler<uint32_t> {

	public:
		ConcreteMachine(const Analyser::Static::AppleIIgs::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			m65816_(*this) {

			set_clock_rate(14318180.0);

			using Target = Analyser::Static::AppleIIgs::Target;
			std::vector<ROMMachine::ROM> rom_descriptions;
			const std::string machine_name = "AppleIIgs";
			switch(target.model) {
				case Target::Model::ROM00:
					/* TODO */
				case Target::Model::ROM01:
					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM01", "apple2gs.rom", 128*1024, 0x42f124b0);
				break;

				case Target::Model::ROM03:
					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM03", "apple2gs.rom2", 256*1024, 0xde7ddf29);
				break;
			}
			const auto roms = rom_fetcher(rom_descriptions);
			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}
			rom_ = *roms[0];

			size_t ram_size = 0;
			switch(target.memory_model) {
				case Target::MemoryModel::TwoHundredAndFiftySixKB:
					ram_size = 256;
				break;

				case Target::MemoryModel::OneMB:
					ram_size = 128 + 1024;
				break;

				case Target::MemoryModel::EightMB:
					ram_size = 128 + 8 * 1024;
				break;
			}
			ram_.resize(ram_size * 1024);

			memory_.set_storage(ram_, rom_);

			// TODO: enable once machine is otherwise sane.
//			Memory::Fuzz(ram_);

			// Sync up initial values.
			memory_.set_speed_register(speed_register_);
		}

		void run_for(const Cycles cycles) override {
			m65816_.run_for(cycles);
		}

		void set_scan_target(Outputs::Display::ScanTarget *) override {
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return Outputs::Display::ScanStatus();
		}

		forceinline Cycles perform_bus_operation(const CPU::WDC65816::BusOperation operation, const uint32_t address, uint8_t *const value) {
			const auto &region = MemoryMapRegion(memory_, address);
			static bool log = false;

			// TODO: potentially push time to clock_.

			if(region.flags & MemoryMap::Region::IsIO) {
				// Ensure classic auxiliary and language card accesses have effect.
				const bool is_read = isReadOperation(operation);
				memory_.access(uint16_t(address), is_read);

				const auto address_suffix = address & 0xffff;
				switch(address_suffix) {

					// New video register.
					case 0xc029:
						if(is_read) {
							*value = 0x01;
						} else {
							printf("New video: %02x\n", *value);
							// TODO: this bit should affect memory bank selection, somehow?
							// Cf. Page 90.
						}
					break;

					// Shadow register.
					case 0xc035:
						if(is_read) {
							*value = memory_.get_shadow_register();
						} else {
							memory_.set_shadow_register(*value);
						}
					break;

					// Clock data.
					case 0xc033:
						if(is_read) {
							*value = clock_.get_data();
						} else {
							clock_.set_data(*value);
						}
					break;

					// Clock and border control.
					case 0xc034:
						if(is_read) {
							*value = clock_.get_control();
						} else {
							clock_.set_control(*value);
							// TODO: also set border colour.
						}
					break;

					// Speed register.
					case 0xc036:
						if(is_read) {
							*value = speed_register_;
						} else {
							memory_.set_speed_register(*value);
							speed_register_ = *value;
							printf("[Unimplemented] most of speed register: %02x\n", *value);
						}
					break;

					// [Memory] State register.
					case 0xc068:
						if(is_read) {
							*value = memory_.get_state_register();
						} else {
							memory_.set_state_register(*value);
						}
					break;

					// Various independent memory switch reads [TODO: does the IIe-style keyboard the low seven?].
#define SwitchRead(s) *value = memory_.s ? 0x80 : 0x00
#define LanguageRead(s) SwitchRead(language_card_switches().state().s)
#define AuxiliaryRead(s) SwitchRead(auxiliary_switches().switches().s)
#define VideoRead(s) video_.s
					case 0xc011:	LanguageRead(bank1);						break;
					case 0xc012:	LanguageRead(read);							break;
					case 0xc013:	AuxiliaryRead(read_auxiliary_memory);		break;
					case 0xc014:	AuxiliaryRead(write_auxiliary_memory);		break;
					case 0xc015:	AuxiliaryRead(internal_CX_rom);				break;
					case 0xc016:	AuxiliaryRead(alternative_zero_page);		break;
					case 0xc017:	AuxiliaryRead(slot_C3_rom);					break;
					case 0xc018:	VideoRead(get_80_store());										break;
//					case 0xc019:	VideoRead(get_is_vertical_blank(cycles_since_video_update_));	break;
					case 0xc01a:	VideoRead(get_text());											break;
					case 0xc01b:	VideoRead(get_mixed());											break;
					case 0xc01c:	VideoRead(get_page2());											break;
					case 0xc01d:	VideoRead(get_high_resolution());								break;
					case 0xc01e:	VideoRead(get_alternative_character_set());						break;
					case 0xc01f:	VideoRead(get_80_columns());									break;
					case 0xc046:	VideoRead(get_annunciator_3());									break;
#undef VideoRead
#undef AuxiliaryRead
#undef LanguageRead
#undef SwitchRead

					// Video switches (and annunciators).
					case 0xc050: case 0xc051:
						video_.set_text(address & 1);
					break;
					case 0xc052: case 0xc053:
						video_.set_mixed(address & 1);
					break;
					case 0xc054: case 0xc055:
						video_.set_page2(address&1);
					break;
					case 0xc056: case 0xc057:
						video_.set_high_resolution(address&1);
					break;
					case 0xc058: case 0xc059:
					case 0xc05a: case 0xc05b:
					case 0xc05c: case 0xc05d:
						// Annunciators 0, 1 and 2.
					break;
					case 0xc05e: case 0xc05f:
						video_.set_annunciator_3(!(address&1));
					break;
					case 0xc001:	/* 0xc000 is dealt with in the ADB section. */
						if(!is_read) video_.set_80_store(true);
					break;
					case 0xc00c: case 0xc00d:
						if(!is_read) video_.set_80_columns(address & 1);
					break;
					case 0xc00e: case 0xc00f:
						if(!is_read) video_.set_alternative_character_set(address & 1);
					break;

					// ADB.
					case 0xc000:
						if(is_read) {
							*value = adb_glu_.get_keyboard_data();
						} else {
							video_.set_80_store(false);
						}
					break;
					case 0xc024:
						if(is_read) {
							*value = adb_glu_.get_mouse_data();
						}
					break;
					case 0xc025:
						if(is_read) {
							*value = adb_glu_.get_modifier_status();
						}
					break;
					case 0xc026:
						if(is_read) {
							*value = adb_glu_.get_data();
						} else {
							adb_glu_.set_command(*value);
						}
					break;
					case 0xc027:
						if(is_read) {
							*value = adb_glu_.get_status();
						} else {
							adb_glu_.set_status(*value);
						}
					break;

					// The SCC.
					case 0xc038: case 0xc039: case 0xc03a: case 0xc03b:
						if(is_read) {
							*value = scc_.read(int(address));
						} else {
							scc_.write(int(address), *value);
						}
					break;

					// These were all dealt with by the call to memory_.access.
					// TODO: subject to read data? Does vapour lock apply?
					case 0xc002: case 0xc003: case 0xc004: case 0xc005:
					case 0xc006: case 0xc007: case 0xc008: case 0xc009: case 0xc00a: case 0xc00b:
					break;

					// Interrupt ROM addresses; Cf. P25 of the Hardware Reference.
					case 0xc071: case 0xc072: case 0xc073: case 0xc074: case 0xc075: case 0xc076: case 0xc077:
					case 0xc078: case 0xc079: case 0xc07a: case 0xc07b: case 0xc07c: case 0xc07d: case 0xc07e: case 0xc07f:
						if(is_read) {
							*value = rom_[rom_.size() - 65536 + address_suffix];
						}
					break;

					// Analogue inputs. All TODO.
					case 0xc060: case 0xc061: case 0xc062: case 0xc063:
						// Joystick buttons (and keyboard modifiers).
						if(is_read) {
							*value = 0x00;
						}
					break;

					case 0xc064: case 0xc065: case 0xc066: case 0xc067:
						// Analogue inputs.
						if(is_read) {
							*value = 0x00;
						}
					break;

					case 0xc070:
						// TODO: begin analogue channel charge.
					break;

					case 0xc02d:
						// TODO: slot register selection.
						// b7: 0 = internal ROM code for slot 7;
						// b6: 0 = internal ROM code for slot 6;
						// b5: 0 = internal ROM code for slot 5;
						// b4: 0 = internal ROM code for slot 4;
						// b3: reserved;
						// b2: internal ROM code for slot 2;
						// b1: internal ROM code for slot 1;
						// b0: reserved.
						if(is_read) {
							*value = card_mask_;
						} else {
							card_mask_ = *value;
						}
					break;

					// Addresses that seemingly map to nothing; provided as a separate break out for now,
					// while I have an assert on unknown reads.
					case 0xc049: case 0xc04a: case 0xc04b: case 0xc04c: case 0xc04d: case 0xc04e: case 0xc04f:
					case 0xc069: case 0xc06a: case 0xc06b: case 0xc06c:
						printf("Ignoring %04x\n", address_suffix);
						log = true;
					break;

					// 'Test Mode', whatever that is (?)
					case 0xc06e: case 0xc06f:
						test_mode_ = address & 1;
					break;
					case 0xc06d:
						if(is_read) {
							*value = test_mode_ * 0x80;
						}
					break;

					default:
						// Check for a card access.
						if(address_suffix >= 0xc080 && address_suffix < 0xc800) {
							// This is an abridged version of the similar code in AppleII.cpp from
							// line 653; it would be good to factor that out and support cards here.
							// For now just either supply the internal ROM or nothing as per the
							// current card mask.

							size_t card_number = 0;
							if(address >= 0xc100) {
								/*
									Decode the area conventionally used by cards for ROMs:
										0xCn00 to 0xCnff: card n.
								*/
								card_number = (address - 0xc000) >> 8;
							} else {
								/*
									Decode the area conventionally used by cards for registers:
										C0n0 to C0nF: card n - 8.
								*/
								card_number = (address - 0xc080) >> 4;
							}

							const uint8_t permitted_card_mask_ = card_mask_ & 0xf6;
							if(permitted_card_mask_ & (1 << card_number)) {
								// TODO: Access an actual card.
								if(is_read) {
									*value = 0xff;
								}
							} else {
								// TODO: disk-port soft switches should be in COEx.
								printf("Internal card-area access: %04x\n", address_suffix);
								if(is_read) {
									*value = rom_[rom_.size() - 65536 + address_suffix];
								}
							}
						} else {
							if(address_suffix < 0xc080) {
								// TODO: all other IO accesses.
								printf("Unhandled IO: %04x\n", address_suffix);
								assert(false);
							}
						}
				}
			} else {
				// For debugging purposes; if execution heads off into an unmapped page then
				// it's pretty certain that my 65816 still has issues.
				assert(operation != CPU::WDC65816::BusOperation::ReadOpcode || region.read);

				if(isReadOperation(operation)) {
					MemoryMapRead(region, address, value);
				} else {
					MemoryMapWrite(memory_, region, address, value);
				}
			}

//			log |= (address >= 0xffa6d9) && (address < 0xffa6ec);
			if(log) {
				printf("%06x %s %02x", address, isReadOperation(operation) ? "->" : "<-", *value);
				if(operation == CPU::WDC65816::BusOperation::ReadOpcode) {
					printf(" a:%04x x:%04x y:%04x s:%04x e:%d p:%02x db:%02x pb:%02x d:%04x\n",
						m65816_.get_value_of_register(CPU::WDC65816::Register::A),
						m65816_.get_value_of_register(CPU::WDC65816::Register::X),
						m65816_.get_value_of_register(CPU::WDC65816::Register::Y),
						m65816_.get_value_of_register(CPU::WDC65816::Register::StackPointer),
						m65816_.get_value_of_register(CPU::WDC65816::Register::EmulationFlag),
						m65816_.get_value_of_register(CPU::WDC65816::Register::Flags),
						m65816_.get_value_of_register(CPU::WDC65816::Register::DataBank),
						m65816_.get_value_of_register(CPU::WDC65816::Register::ProgramBank),
						m65816_.get_value_of_register(CPU::WDC65816::Register::Direct)
					);
				} else printf("\n");
			}

			Cycles duration = Cycles(5);

			// TODO: determine the cost of this access.
//			if((mapping.flags & BankMapping::Is1Mhz) || ((mapping.flags & BankMapping::IsShadowed) && !isReadOperation(operation))) {
//				// TODO: (i) get into phase; (ii) allow for the 1Mhz bus length being sporadically 16 rather than 14.
//				duration = Cycles(14);
//			} else {
//				// TODO: (i) get into phase; (ii) allow for collisions with the refresh cycle.
//				duration = Cycles(5);
//			}
			fast_access_phase_ = (fast_access_phase_ + duration.as<int>()) % 5;		// TODO: modulo something else, to allow for refresh.
			slow_access_phase_ = (slow_access_phase_ + duration.as<int>()) % 14;	// TODO: modulo something else, to allow for stretched cycles.
			return duration;
		}

	private:
		CPU::WDC65816::Processor<ConcreteMachine, false> m65816_;
		MemoryMap memory_;

		// MARK: - Timing.

		int fast_access_phase_ = 0;
		int slow_access_phase_ = 0;

		uint8_t speed_register_ = 0x40;	// i.e. Power-on status. (TODO: only if ROM03?)

		// MARK: - Memory storage.

		std::vector<uint8_t> ram_{};
		std::vector<uint8_t> rom_;

		// MARK: - Other components.

		Apple::Clock::ParallelClock clock_;
		Apple::IIgs::Video::Video video_;
		Apple::IIgs::ADB::GLU adb_glu_;
 		Zilog::SCC::z8530 scc_;

		// MARK: - Cards.

		// TODO: most of cards.
		uint8_t card_mask_ = 0x00;

		bool test_mode_ = false;
};

}
}

using namespace Apple::IIgs;

Machine *Machine::AppleIIgs(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(*dynamic_cast<const Analyser::Static::AppleIIgs::Target *>(target), rom_fetcher);
}

Machine::~Machine() {}
