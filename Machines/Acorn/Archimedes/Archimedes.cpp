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
#include "KeyboardMapper.hpp"
#include "MemoryController.hpp"
#include "Sound.hpp"

#include "../../AudioProducer.hpp"
#include "../../KeyboardMachine.hpp"
#include "../../MediaTarget.hpp"
#include "../../MouseMachine.hpp"
#include "../../ScanProducer.hpp"
#include "../../TimedMachine.hpp"

#include "../../../Activity/Source.hpp"

#include "../../../InstructionSets/ARM/Disassembler.hpp"
#include "../../../InstructionSets/ARM/Executor.hpp"
#include "../../../Outputs/Log.hpp"
#include "../../../Components/I2C/I2C.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <vector>

namespace Archimedes {

class ConcreteMachine:
	public Machine,
	public MachineTypes::AudioProducer,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaTarget,
	public MachineTypes::MouseMachine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public Activity::Source
{
	private:
		Log::Logger<Log::Source::Archimedes> logger;

		// This fictitious clock rate just means '24 MIPS, please'; it's divided elsewhere.
		static constexpr int ClockRate = 24'000'000;

		// Runs for 24 cycles, distributing calls to the various ticking subsystems
		// 'correctly' (i.e. correctly for the approximation in use).
		//
		// The implementation of this is coupled to the ClockRate above, hence its
		// appearance here.
		template <int video_divider, bool original_speed>
		void macro_tick() {
			macro_counter_ -= 24;

			// This is a 24-cycle window, so at 24Mhz macro_tick() is called at 1Mhz.
			// Hence, required ticks are:
			//
			// 	* CPU: 24;
			//	* video: 24 / video_divider;
			//	* floppy: 8;
			//	* timers: 2;
			//	* sound: 1.

			tick_cpu_video<0, video_divider, original_speed>();		tick_cpu_video<1, video_divider, original_speed>();
			tick_cpu_video<2, video_divider, original_speed>();		tick_floppy();
			tick_cpu_video<3, video_divider, original_speed>();		tick_cpu_video<4, video_divider, original_speed>();
			tick_cpu_video<5, video_divider, original_speed>();		tick_floppy();
			tick_cpu_video<6, video_divider, original_speed>();		tick_cpu_video<7, video_divider, original_speed>();
			tick_cpu_video<8, video_divider, original_speed>();		tick_floppy();
			tick_cpu_video<9, video_divider, original_speed>();		tick_cpu_video<10, video_divider, original_speed>();
			tick_cpu_video<11, video_divider, original_speed>();	tick_floppy();
			tick_timers();

			tick_cpu_video<12, video_divider, original_speed>();	tick_cpu_video<13, video_divider, original_speed>();
			tick_cpu_video<14, video_divider, original_speed>();	tick_floppy();
			tick_cpu_video<15, video_divider, original_speed>();	tick_cpu_video<16, video_divider, original_speed>();
			tick_cpu_video<17, video_divider, original_speed>();	tick_floppy();
			tick_cpu_video<18, video_divider, original_speed>();	tick_cpu_video<19, video_divider, original_speed>();
			tick_cpu_video<20, video_divider, original_speed>();	tick_floppy();
			tick_cpu_video<21, video_divider, original_speed>();	tick_cpu_video<22, video_divider, original_speed>();
			tick_cpu_video<23, video_divider, original_speed>();	tick_floppy();
			tick_timers();
			tick_sound();
		}
		int macro_counter_ = 0;

		template <int offset, int video_divider, bool original_speed>
		void tick_cpu_video() {
			if constexpr (!(offset % video_divider)) {
				tick_video();
			}

			// Debug mode: run CPU a lot slower. Actually at close to original advertised MIPS speed.
			if constexpr (original_speed && (offset & 7)) return;
			if constexpr (offset & 1) return;
			tick_cpu();
		}

	public:
		ConcreteMachine(
			const Analyser::Static::Target &target,
			const ROMMachine::ROMFetcher &rom_fetcher
		) : executor_(*this, *this, *this) {
			set_clock_rate(ClockRate);

			constexpr ROM::Name risc_os = ROM::Name::AcornRISCOS311;
			ROM::Request request(risc_os);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			executor_.bus.set_rom(roms.find(risc_os)->second);
			insert_media(target.media);

			fill_pipeline(0);
		}

		void update_interrupts() {
			using Exception = InstructionSet::ARM::Registers::Exception;

			const int requests = executor_.bus.interrupt_mask();
			if((requests & InterruptRequests::FIQ) && executor_.registers().would_interrupt<Exception::FIQ>()) {
				pipeline_.reschedule(Pipeline::SWISubversion::FIQ);
				return;
			}
			if((requests & InterruptRequests::IRQ) && executor_.registers().would_interrupt<Exception::IRQ>()) {
				pipeline_.reschedule(Pipeline::SWISubversion::IRQ);
			}
		}

		void did_set_status() {
			// This might have been a change of mode, so...
			trans_ = executor_.registers().mode() == InstructionSet::ARM::Mode::User;
			fill_pipeline(executor_.pc());
			update_interrupts();
		}

		void did_set_pc() {
			fill_pipeline(executor_.pc());
		}

		bool should_swi(uint32_t) {
			using Exception = InstructionSet::ARM::Registers::Exception;
			using SWISubversion = Pipeline::SWISubversion;

			switch(pipeline_.swi_subversion()) {
				case Pipeline::SWISubversion::None:
				return true;

				case SWISubversion::DataAbort:
//					executor_.set_pc(executor_.pc() - 4);
					executor_.registers().exception<Exception::DataAbort>();
				break;

				// FIQ and IRQ decrement the PC because their apperance in the pipeline causes
				// it to look as though they were fetched, but they weren't.
				case SWISubversion::FIQ:
					executor_.set_pc(executor_.pc() - 4);
					executor_.registers().exception<Exception::FIQ>();
				break;
				case SWISubversion::IRQ:
					executor_.set_pc(executor_.pc() - 4);
					executor_.registers().exception<Exception::IRQ>();
				break;
			}

			did_set_pc();
			return false;
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
			return executor_.bus.video().crt().get_scaled_scan_status() * video_divider_;
		}

		// MARK: - TimedMachine.
		int video_divider_ = 1;
		void run_for(Cycles cycles) override {
#ifndef NDEBUG
			// Debug mode: always run 'slowly' because that's less of a burden, and
			// because it allows me to peer at problems with greater leisure.
			const bool use_original_speed = true;
#else
			// As a first, blunt implementation: try to model something close
			// to original speed if there have been 10 frame rate overages in total.
			const bool use_original_speed = executor_.bus.video().frame_rate_overages() > 10;
#endif

			if(use_original_speed) run_for<true>(cycles);
			else run_for<false>(cycles);
		}

		template <bool original_speed>
		void run_for(Cycles cycles) {
			macro_counter_ += cycles.as<int>();

			while(macro_counter_ > 0) {
				switch(video_divider_) {
					default:	macro_tick<2, original_speed>();	break;
					case 3:		macro_tick<3, original_speed>();	break;
					case 4:		macro_tick<4, original_speed>();	break;
					case 6:		macro_tick<6, original_speed>();	break;
				}

			}
		}

		void tick_cpu() {
			const uint32_t instruction = advance_pipeline(executor_.pc() + 8);
			InstructionSet::ARM::execute(instruction, executor_);
		}

		void tick_timers()	{	executor_.bus.tick_timers();	}
		void tick_sound()	{	executor_.bus.sound().tick();	}
		void tick_video()	{	executor_.bus.video().tick();	}
		void tick_floppy()	{	executor_.bus.tick_floppy();	}

		// MARK: - MediaTarget
		bool insert_media(const Analyser::Static::Media &media) override {
			size_t c = 0;
			for(auto &disk : media.disks) {
				executor_.bus.set_disk(disk, c);
				c++;
				if(c == 4) break;
			}
			return true;
		}

		// MARK: - AudioProducer
		Outputs::Speaker::Speaker *get_speaker() override {
			return executor_.bus.speaker();
		}

		// MARK: - Activity::Source.
		void set_activity_observer(Activity::Observer *observer) final {
			executor_.bus.set_activity_observer(observer);
		}

		// MARK: - MappedKeyboardMachine.
		MappedKeyboardMachine::KeyboardMapper *get_keyboard_mapper() override {
			return &keyboard_mapper_;
		}
		Archimedes::KeyboardMapper keyboard_mapper_;

		void set_key_state(uint16_t key, bool is_pressed) override {
			const int row = Archimedes::KeyboardMapper::row(key);
			const int column = Archimedes::KeyboardMapper::column(key);
			executor_.bus.keyboard().set_key_state(row, column, is_pressed);
		}

		// MARK: - MouseMachine.
		Inputs::Mouse &get_mouse() override {
			return executor_.bus.keyboard().mouse();
		}

		// MARK: - ARM execution.
		static constexpr auto arm_model = InstructionSet::ARM::Model::ARMv2;
		using Executor = InstructionSet::ARM::Executor<arm_model, MemoryController<ConcreteMachine, ConcreteMachine>, ConcreteMachine>;
		Executor executor_;
		bool trans_ = false;

		void fill_pipeline(uint32_t pc) {
			if(pipeline_.interrupt_next()) return;
			advance_pipeline(pc);
			advance_pipeline(pc + 4);
		}

		uint32_t advance_pipeline(uint32_t pc) {
			uint32_t instruction = 0;	// Value should never be used; this avoids a spurious GCC warning.
			const bool did_read = executor_.bus.read(pc, instruction, trans_);
			return pipeline_.exchange(
				did_read ? instruction : Pipeline::SWI,
				did_read ? Pipeline::SWISubversion::None : Pipeline::SWISubversion::DataAbort);
		}

		struct Pipeline {
			enum SWISubversion: uint8_t {
				None,
				DataAbort,
				IRQ,
				FIQ,
			};

			static constexpr uint32_t SWI = 0xef'000000;

			uint32_t exchange(uint32_t next, SWISubversion subversion) {
				const uint32_t result = upcoming_[active_].opcode;
				latched_subversion_ = upcoming_[active_].subversion;

				upcoming_[active_].opcode = next;
				upcoming_[active_].subversion = subversion;
				active_ ^= 1;

				return result;
			}

			SWISubversion swi_subversion() const {
				return latched_subversion_;
			}

			// TODO: one day, possibly: schedule the subversion one slot further into the future
			// (i.e. active_ ^ 1) to allow one further instruction to occur as usual before the
			// action paplies. That is, if interrupts take effect one instruction later after a flags
			// change, which I don't yet know.
			//
			// In practice I got into a bit of a race condition between interrupt scheduling and
			// flags changes, so have backed off for now.
			void reschedule(SWISubversion subversion) {
				upcoming_[active_].opcode = SWI;
				upcoming_[active_].subversion = subversion;
			}

			bool interrupt_next() const {
				return upcoming_[active_].subversion == SWISubversion::IRQ || upcoming_[active_].subversion == SWISubversion::FIQ;
			}

		private:
			struct Stage {
				uint32_t opcode;
				SWISubversion subversion = SWISubversion::None;
			};
			Stage upcoming_[2];
			int active_ = 0;

			SWISubversion latched_subversion_;
		} pipeline_;
};

}

using namespace Archimedes;

std::unique_ptr<Machine> Machine::Archimedes(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return std::make_unique<ConcreteMachine>(*target, rom_fetcher);
}
