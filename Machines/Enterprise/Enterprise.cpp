//
//  Enterprise.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Enterprise.hpp"

#include "../MachineTypes.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Analyser/Static/Enterprise/Target.hpp"


namespace Enterprise {

class ConcreteMachine:
	public CPU::Z80::BusHandler,
	public Machine,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine {
	public:
		ConcreteMachine([[maybe_unused]] const Analyser::Static::Enterprise::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			z80_(*this) {
			// Request a clock of 4Mhz; this'll be mapped upwards for Nick and Dave elsewhere.
			set_clock_rate(4'000'000);

			constexpr ROM::Name exos_name = ROM::Name::EnterpriseEXOS;
			const auto request = ROM::Request(exos_name);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			const auto &exos = roms.find(exos_name)->second;
			memcpy(exos_.data(), exos.data(), std::min(exos_.size(), exos.size()));

			// Take a reasonable guess at the initial memory configuration.
			page<0>(0x00);
			page<1>(0x00);
			page<2>(0x00);
			page<3>(0x00);
		}

		// MARK: - Z80::BusHandler.
		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			using PartialMachineCycle = CPU::Z80::PartialMachineCycle;
			const uint16_t address = cycle.address ? *cycle.address : 0x0000;

			// TODO: possibly apply an access penalty.

			switch(cycle.operation) {
				default: break;

				case CPU::Z80::PartialMachineCycle::Input:
					printf("Unhandled input: %04x\n", address);
					assert(false);
				break;

				case CPU::Z80::PartialMachineCycle::Output:
					printf("Unhandled output: %04x\n", address);
					assert(false);
				break;

				case CPU::Z80::PartialMachineCycle::Read:
				case CPU::Z80::PartialMachineCycle::ReadOpcode:
					if(read_pointers_[address >> 14]) {
						*cycle.value = read_pointers_[address >> 14][address];
					} else {
						*cycle.value = 0xff;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Write:
					if(write_pointers_[address >> 14]) {
						write_pointers_[address >> 14][address] = *cycle.value;
					}
				break;
			}

			return HalfCycles(0);
		}

	private:
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;

		std::array<uint8_t, 32 * 1024> exos_;
		std::array<uint8_t, 256 * 1024> ram_;
		const uint8_t min_ram_slot_ = 0xff - 3;

		const uint8_t *read_pointers_[4];
		uint8_t *write_pointers_[4];
		uint8_t pages_[4];

		template <size_t slot> void page(uint8_t offset) {
			pages_[slot] = offset;

			if(offset < 2) {
				page<slot>(&exos_[offset * 0x4000], nullptr);
				return;
			}

			if(offset >= min_ram_slot_) {
				const size_t address = (offset - min_ram_slot_) * 0x4000;
				page<slot>(&ram_[address], &ram_[address]);
				return;
			}

			page<slot>(nullptr, nullptr);
		}

		template <size_t slot> void page(const uint8_t *read, uint8_t *write) {
			read_pointers_[slot] = read ? read - (slot * 0x4000) : nullptr;
			write_pointers_[slot] = write ? write - (slot * 0x4000) : nullptr;
		}

		// MARK: - ScanProducer
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			(void)scan_target;
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return Outputs::Display::ScanStatus();
		}

		// MARK: - TimedMachine
		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);
		}
};

}

using namespace Enterprise;

Machine *Machine::Enterprise(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Enterprise::Target;
	const Target *const enterprise_target = dynamic_cast<const Target *>(target);

	return new Enterprise::ConcreteMachine(*enterprise_target, rom_fetcher);
}

Machine::~Machine() {}
