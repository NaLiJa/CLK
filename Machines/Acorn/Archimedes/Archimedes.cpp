//
//  Archimedes.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "Archimedes.hpp"

#include "HalfDuplexSerial.hpp"
#include "InputOutputController.hpp"
#include "Keyboard.hpp"
#include "MemoryController.hpp"
#include "Sound.hpp"

#include "../../AudioProducer.hpp"
#include "../../KeyboardMachine.hpp"
#include "../../MediaTarget.hpp"
#include "../../ScanProducer.hpp"
#include "../../TimedMachine.hpp"

#include "../../../InstructionSets/ARM/Disassembler.hpp"
#include "../../../InstructionSets/ARM/Executor.hpp"
#include "../../../Outputs/Log.hpp"
#include "../../../Components/I2C/I2C.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <vector>

namespace {

Log::Logger<Log::Source::Archimedes> logger;

}

namespace Archimedes {

class ConcreteMachine:
	public Machine,
	public MachineTypes::MediaTarget,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer
{
	private:
		// TODO: pick a sensible clock rate; this is just code for '24 MIPS, please'.
		static constexpr int ClockRate = 24'000'000;

		// Runs for 24 cycles, distributing calls to the various ticking subsystems
		// 'correctly' (i.e. correctly for the approximation in use).
		//
		// The implementation of this is coupled to the ClockRate above, hence its
		// appearance here.
		template <int video_divider>
		void macro_tick() {
			macro_counter_ -= 24;

			// This is a 24-cycle window, so at 24Mhz macro_tick() is called at 1Mhz.
			// Hence, required ticks are:
			//
			// 	* CPU: 24;
			//	* video: 24 / video_divider;
			//	* timers: 2;
			//	* sound: 1.

			tick_cpu_video<0, video_divider>();		tick_cpu_video<1, video_divider>();
			tick_cpu_video<2, video_divider>();		tick_cpu_video<3, video_divider>();
			tick_cpu_video<4, video_divider>();		tick_cpu_video<5, video_divider>();
			tick_cpu_video<6, video_divider>();		tick_cpu_video<7, video_divider>();
			tick_cpu_video<8, video_divider>();		tick_cpu_video<9, video_divider>();
			tick_cpu_video<10, video_divider>();	tick_cpu_video<11, video_divider>();
			tick_timers();

			tick_cpu_video<12, video_divider>();	tick_cpu_video<13, video_divider>();
			tick_cpu_video<14, video_divider>();	tick_cpu_video<15, video_divider>();
			tick_cpu_video<16, video_divider>();	tick_cpu_video<17, video_divider>();
			tick_cpu_video<18, video_divider>();	tick_cpu_video<19, video_divider>();
			tick_cpu_video<20, video_divider>();	tick_cpu_video<21, video_divider>();
			tick_timers();
			tick_sound();
		}
		int macro_counter_ = 0;

		template <int offset, int video_divider>
		void tick_cpu_video() {
			tick_cpu();
			if constexpr (!(offset % video_divider)) {
				tick_video();
			}
		}

	public:
		ConcreteMachine(
			const Analyser::Static::Target &target,
			const ROMMachine::ROMFetcher &rom_fetcher
		) : executor_(*this, *this) {
			set_clock_rate(ClockRate);

			constexpr ROM::Name risc_os = ROM::Name::AcornRISCOS319;
			ROM::Request request(risc_os);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			executor_.bus.set_rom(roms.find(risc_os)->second);
			insert_media(target.media);
		}

		void update_interrupts() {
			using Exception = InstructionSet::ARM::Registers::Exception;

			const int requests = executor_.bus.interrupt_mask();
			if((requests & InterruptRequests::FIQ) && executor_.registers().interrupt<Exception::FIQ>()) {
				return;
			}
			if(requests & InterruptRequests::IRQ) {
//				logger.info().append("[%d] IRQ at %08x prior:[r13:%08x r14:%08x]",
//					instr_count,
//					executor_.pc(),
//					executor_.registers()[13],
//					executor_.registers()[14]);

				executor_.registers().interrupt<Exception::IRQ>();
			}
		}

		void update_clock_rates() {
			video_divider_ = executor_.bus.video().clock_divider();
		}

	private:
		// MARK: - ScanProducer.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			executor_.bus.video().crt().set_scan_target(scan_target);
		}
		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return executor_.bus.video().crt().get_scaled_scan_status();
		}

		std::array<uint32_t, 750> pc_history;
		std::size_t pc_history_ptr = 0;
		uint32_t instr_count = 0;

		// MARK: - TimedMachine.
		void run_for(Cycles cycles) override {
			macro_counter_ += cycles.as<int>();

			while(macro_counter_ > 0) {
				switch(video_divider_) {
					default:	macro_tick<2>();	break;
					case 3:		macro_tick<3>();	break;
					case 4:		macro_tick<4>();	break;
					case 6:		macro_tick<6>();	break;
				}

			}
		}
		int video_divider_ = 1;

		void tick_cpu() {
			static uint32_t last_pc = 0;
			static bool log = false;

//			if(executor_.pc() == 0x03803400) {
//				printf("At %08x; after last PC %08x and %zu ago was %08x\n", executor_.pc(), pc_history[(pc_history_ptr - 2 + pc_history.size()) % pc_history.size()], pc_history.size(), pc_history[pc_history_ptr]);
//			}

			uint32_t instruction;
			pc_history[pc_history_ptr] = executor_.pc();
			pc_history_ptr = (pc_history_ptr + 1) % pc_history.size();
			if(!executor_.bus.read(executor_.pc(), instruction, executor_.registers().mode(), false)) {
				logger.info().append("Prefetch abort at %08x; last good was at %08x", executor_.pc(), last_pc);
				executor_.prefetch_abort();

				// TODO: does a double abort cause a reset?
				executor_.bus.read(executor_.pc(), instruction, executor_.registers().mode(), false);
			} else {
				last_pc = executor_.pc();
			}
			// TODO: pipeline prefetch?

//			log |= executor_.pc() == 0x3961244;
//			log |= instr_count == 72766815;
//			log &= executor_.pc() != 0x000000a0;

			if(log) {
				InstructionSet::ARM::Disassembler<arm_model> disassembler;
				InstructionSet::ARM::dispatch<arm_model>(instruction, disassembler);

				auto info = logger.info();
				info.append("[%d] %08x: %08x\t\t%s\t prior:[",
					instr_count,
					executor_.pc(),
					instruction,
					disassembler.last().to_string(executor_.pc()).c_str());
				for(uint32_t c = 0; c < 15; c++) {
					info.append("r%d:%08x ", c, executor_.registers()[c]);
				}
				info.append("]");
			}
//			logger.info().append("%08x: %08x", executor_.pc(), instruction);
			InstructionSet::ARM::execute(instruction, executor_);
			++instr_count;

//			if(
//				executor_.pc() > 0x038021d0 &&
//					last_r1 != executor_.registers()[1]
//					 ||
//				(
//					last_link != executor_.registers()[14] ||
//					last_r0 != executor_.registers()[0] ||
//					last_r10 != executor_.registers()[10] ||
//					last_r1 != executor_.registers()[1]
//				)
//			) {
//				logger.info().append("%08x modified R14 to %08x; R0 to %08x; R10 to %08x; R1 to %08x",
//					last_pc,
//					executor_.registers()[14],
//					executor_.registers()[0],
//					executor_.registers()[10],
//					executor_.registers()[1]
//				);
//				logger.info().append("%08x modified R1 to %08x",
//					last_pc,
//					executor_.registers()[1]
//				);
//				last_link = executor_.registers()[14];
//				last_r0 = executor_.registers()[0];
//				last_r10 = executor_.registers()[10];
//				last_r1 = executor_.registers()[1];
//			}
		}

		void tick_timers()	{	executor_.bus.tick_timers();	}
		void tick_sound()	{	executor_.bus.sound().tick();	}
		void tick_video()	{	executor_.bus.video().tick();	}

		// MARK: - MediaTarget
		bool insert_media(const Analyser::Static::Media &) override {
//			int c = 0;
//			for(auto &disk : media.disks) {
//				fdc_.set_disk(disk, c);
//				c++;
//				if(c == 4) break;
//			}
//			return true;
			return false;
		}

		// MARK: - ARM execution
		static constexpr auto arm_model = InstructionSet::ARM::Model::ARMv2;
		InstructionSet::ARM::Executor<arm_model, MemoryController<ConcreteMachine, ConcreteMachine>> executor_;
};

}

using namespace Archimedes;

std::unique_ptr<Machine> Machine::Archimedes(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return std::make_unique<ConcreteMachine>(*target, rom_fetcher);
}
