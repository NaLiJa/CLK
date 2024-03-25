//
//  InputOutputController.h
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Outputs/Log.hpp"

#include "CMOSRAM.hpp"
#include "Keyboard.hpp"
#include "Sound.hpp"
#include "Video.hpp"


namespace Archimedes {

// IRQ A flags
namespace IRQA {
	// The first four of these are taken from the A500 documentation and may be inaccurate.
	static constexpr uint8_t PrinterBusy		= 0x01;
	static constexpr uint8_t SerialRinging		= 0x02;
	static constexpr uint8_t PrinterAcknowledge	= 0x04;
	static constexpr uint8_t VerticalFlyback	= 0x08;
	static constexpr uint8_t PowerOnReset		= 0x10;
	static constexpr uint8_t Timer0				= 0x20;
	static constexpr uint8_t Timer1				= 0x40;
	static constexpr uint8_t SetAlways			= 0x80;
}

// IRQ B flags
namespace IRQB {
	// These are taken from the A3010 documentation.
	static constexpr uint8_t PoduleFIQRequest		= 0x01;
	static constexpr uint8_t SoundBufferPointerUsed	= 0x02;
	static constexpr uint8_t SerialLine				= 0x04;
	static constexpr uint8_t IDE					= 0x08;
	static constexpr uint8_t FloppyDiscInterrupt	= 0x10;
	static constexpr uint8_t PoduleIRQRequest		= 0x20;
	static constexpr uint8_t KeyboardTransmitEmpty	= 0x40;
	static constexpr uint8_t KeyboardReceiveFull	= 0x80;
}

// FIQ flags
namespace FIQ {
	// These are taken from the A3010 documentation.
	static constexpr uint8_t FloppyDiscData			= 0x01;
	static constexpr uint8_t SerialLine				= 0x10;
	static constexpr uint8_t PoduleFIQRequest		= 0x40;
	static constexpr uint8_t SetAlways				= 0x80;
}

namespace InterruptRequests {
	static constexpr int IRQ = 0x01;
	static constexpr int FIQ = 0x02;
};

template <typename InterruptObserverT, typename ClockRateObserverT>
struct InputOutputController {
	int interrupt_mask() const {
		return
			((irq_a_.request() | irq_b_.request()) ? InterruptRequests::IRQ : 0) |
			(fiq_.request() ? InterruptRequests::FIQ : 0);
	}

	template <int c>
	bool tick_timer() {
		if(!counters_[c].value && !counters_[c].reload) {
			return false;
		}

		--counters_[c].value;
		if(!counters_[c].value) {
			counters_[c].value = counters_[c].reload;

			switch(c) {
				case 0:	return irq_a_.set(IRQA::Timer0);
				case 1:	return irq_a_.set(IRQA::Timer1);
				case 3: {
					serial_.shift();
					keyboard_.update();

					const uint8_t events = serial_.events(IOCParty);
					bool did_interrupt = false;
					if(events & HalfDuplexSerial::Receive) {
						did_interrupt |= irq_b_.set(IRQB::KeyboardReceiveFull);
					}
					if(events & HalfDuplexSerial::Transmit) {
						did_interrupt |= irq_b_.set(IRQB::KeyboardTransmitEmpty);
					}

					return did_interrupt;
				}
				default: break;
			}
			// TODO: events for timers 2 (baud).
		}

		return false;
	}

	void tick_timers() {
		bool did_change_interrupts = false;
		did_change_interrupts |= tick_timer<0>();
		did_change_interrupts |= tick_timer<1>();
		did_change_interrupts |= tick_timer<2>();
		did_change_interrupts |= tick_timer<3>();
		if(did_change_interrupts) {
			observer_.update_interrupts();
		}
	}

	/// Decomposes an Archimedes bus address into bank, offset and type.
	struct Address {
		constexpr Address(uint32_t bus_address) noexcept {
			bank = (bus_address >> 16) & 0b111;
			type = Type((bus_address >> 19) & 0b11);
			offset = bus_address & 0b1111100;
		}

		/// A value from 0 to 7 indicating the device being addressed.
		uint32_t bank;
		/// A seven-bit value which is a multiple of 4, indicating the address within the bank.
		uint32_t offset;
		/// Access type.
		enum class Type {
			Sync = 0b00,
			Fast = 0b01,
			Medium = 0b10,
			Slow = 0b11
		} type;
	};

	bool read(uint32_t address, uint8_t &value) {
		const Address target(address);
		value = 0xff;

		switch(target.bank) {
			default:
				logger.error().append("Unrecognised IOC read from %08x i.e. bank %d / type %d", address, target.bank, target.type);
			break;

			// Bank 0: internal registers.
			case 0:
				switch(target.offset) {
					default:
						logger.error().append("Unrecognised IOC bank 0 read; offset %02x", target.offset);
					break;

					case 0x00:
						value = control_ | 0xc0;
						value &= ~(i2c_.clock() ? 2 : 0);
						value &= ~(i2c_.data() ? 1 : 0);
//						logger.error().append("IOC control read: C:%d D:%d", !(value & 2), !(value & 1));
					break;

					case 0x04:
						value = serial_.input(IOCParty);
						irq_b_.clear(IRQB::KeyboardReceiveFull);
						observer_.update_interrupts();
//						logger.error().append("IOC keyboard receive: %02x", value);
					break;

					// IRQ A.
					case 0x10:
						value = irq_a_.status;
//						logger.error().append("IRQ A status is %02x", value);
					break;
					case 0x14:
						value = irq_a_.request();
//						logger.error().append("IRQ A request is %02x", value);
					break;
					case 0x18:
						value = irq_a_.mask;
//						logger.error().append("IRQ A mask is %02x", value);
					break;

					// IRQ B.
					case 0x20:
						value = irq_b_.status;
//						logger.error().append("IRQ B status is %02x", value);
					break;
					case 0x24:
						value = irq_b_.request();
//						logger.error().append("IRQ B request is %02x", value);
					break;
					case 0x28:
						value = irq_b_.mask;
//						logger.error().append("IRQ B mask is %02x", value);
					break;

					// FIQ.
					case 0x30:
						value = fiq_.status;
						logger.error().append("FIQ status is %02x", value);
					break;
					case 0x34:
						value = fiq_.request();
						logger.error().append("FIQ request is %02x", value);
					break;
					case 0x38:
						value = fiq_.mask;
						logger.error().append("FIQ mask is %02x", value);
					break;

					// Counters.
					case 0x40:	case 0x50:	case 0x60:	case 0x70:
						value = counters_[(target.offset >> 4) - 0x4].output & 0xff;
//						logger.error().append("%02x: Counter %d low is %02x", target, (target >> 4) - 0x4, value);
					break;

					case 0x44:	case 0x54:	case 0x64:	case 0x74:
						value = counters_[(target.offset >> 4) - 0x4].output >> 8;
//						logger.error().append("%02x: Counter %d high is %02x", target, (target >> 4) - 0x4, value);
					break;
				}
			return true;
		}

		return true;
	}

	bool write(uint32_t address, uint8_t value) {
		const Address target(address);
		switch(target.bank) {
			default:
				logger.error().append("Unrecognised IOC write of %02x to %08x i.e. bank %d / type %d", value, address, target.bank, target.type);
			break;

			// Bank 0: internal registers.
			case 0:
				switch(target.offset) {
					default:
						logger.error().append("Unrecognised IOC bank 0 write; %02x to offset %02x", value, target.offset);
					break;

					case 0x00:
						// TODO: does the rest of the control register relate to anything?
						control_ = value;
						i2c_.set_clock_data(!(value & 2), !(value & 1));
					break;

					case 0x04:
						serial_.output(IOCParty, value);
						irq_b_.clear(IRQB::KeyboardTransmitEmpty);
						observer_.update_interrupts();
					break;

					case 0x14:
						// b2: clear IF.
						// b3: clear IR.
						// b4: clear POR.
						// b5: clear TM[0].
						// b6: clear TM[1].
						irq_a_.clear(value & 0x7c);
						observer_.update_interrupts();
					break;

					// Interrupts.
					case 0x18:	irq_a_.mask = value;	break;
					case 0x28:	irq_b_.mask = value;	break;
					case 0x38:	fiq_.mask = value;		break;

					// Counters.
					case 0x40:	case 0x50:	case 0x60:	case 0x70:
						counters_[(target.offset >> 4) - 0x4].reload = uint16_t(
							(counters_[(target.offset >> 4) - 0x4].reload & 0xff00) | value
						);
					break;

					case 0x44:	case 0x54:	case 0x64:	case 0x74:
						counters_[(target.offset >> 4) - 0x4].reload = uint16_t(
							(counters_[(target.offset >> 4) - 0x4].reload & 0x00ff) | (value << 8)
						);
					break;

					case 0x48:	case 0x58:	case 0x68:	case 0x78:
						counters_[(target.offset >> 4) - 0x4].value = counters_[(target.offset >> 4) - 0x4].reload;
					break;

					case 0x4c:	case 0x5c:	case 0x6c:	case 0x7c:
						counters_[(target.offset >> 4) - 0x4].output = counters_[(target.offset >> 4) - 0x4].value;
					break;
				}
				return true;
		}

//			case 0x327'0000 & AddressMask:	// Bank 7
//				logger.error().append("TODO: exteded external podule space");
//			return true;
//
//			case 0x331'0000 & AddressMask:
//				logger.error().append("TODO: 1772 / disk write");
//			return true;
//
//			case 0x335'0000 & AddressMask:
//				logger.error().append("TODO: LS374 / printer data write");
//			return true;
//
//			case 0x335'0018 & AddressMask:
//				logger.error().append("TODO: latch B write: %02x", value);
//			return true;
//
//			case 0x335'0040 & AddressMask:
//				logger.error().append("TODO: latch A write: %02x", value);
//			return true;
//
//			case 0x335'0048 & AddressMask:
//				logger.error().append("TODO: latch C write: %02x", value);
//			return true;
//
//			case 0x336'0000 & AddressMask:
//				logger.error().append("TODO: podule interrupt request");
//			return true;
//
//			case 0x336'0004 & AddressMask:
//				logger.error().append("TODO: podule interrupt mask");
//			return true;
//
//			case 0x33a'0000 & AddressMask:
//				logger.error().append("TODO: 6854 / econet write");
//			return true;
//
//			case 0x33b'0000 & AddressMask:
//				logger.error().append("TODO: 6551 / serial line write");
//			return true;
		return true;
	}

	InputOutputController(InterruptObserverT &observer, ClockRateObserverT &clock_observer, const uint8_t *ram) :
		observer_(observer),
		keyboard_(serial_),
		sound_(*this, ram),
		video_(*this, clock_observer, sound_, ram)
	{
		irq_a_.status = IRQA::SetAlways | IRQA::PowerOnReset;
		irq_b_.status = 0x00;
		fiq_.status = 0x80;				// 'set always'.

		i2c_.add_peripheral(&cmos_, 0xa0);
		update_interrupts();
	}

	auto &sound() 					{	return sound_;	}
	const auto &sound() const		{	return sound_;	}
	auto &video()			 		{	return video_;	}
	const auto &video() const 		{	return video_;	}
	auto &keyboard()			 	{	return keyboard_;	}
	const auto &keyboard() const 	{	return keyboard_;	}

	void update_interrupts() {
		if(sound_.interrupt()) {
			irq_b_.set(IRQB::SoundBufferPointerUsed);
		} else {
			irq_b_.clear(IRQB::SoundBufferPointerUsed);
		}

		if(video_.interrupt()) {
			irq_a_.set(IRQA::VerticalFlyback);
		}

		observer_.update_interrupts();
	}

private:
	Log::Logger<Log::Source::ARMIOC> logger;
	InterruptObserverT &observer_;

	// IRQA, IRQB and FIQ states.
	struct Interrupt {
		uint8_t status, mask;
		uint8_t request() const {
			return status & mask;
		}
		bool set(uint8_t value) {
			status |= value;
			return status & mask;
		}
		void clear(uint8_t bits) {
			status &= ~bits;
		}
	};
	Interrupt irq_a_, irq_b_, fiq_;

	// The IOCs four counters.
	struct Counter {
		uint16_t value;
		uint16_t reload;
		uint16_t output;
	};
	Counter counters_[4];

	// The KART and keyboard beyond it.
	HalfDuplexSerial serial_;
	Keyboard keyboard_;

	// The control register.
	uint8_t control_ = 0xff;

	// The I2C bus.
	I2C::Bus i2c_;
	CMOSRAM cmos_;

	// Audio and video.
	Sound<InputOutputController> sound_;
	Video<InputOutputController, ClockRateObserverT, Sound<InputOutputController>> video_;
};

}

