//
//  Archimedes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Configurable/Configurable.hpp"
#include "../../../Configurable/StandardOptions.hpp"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include <memory>

namespace Archimedes {

class Machine {
	public:
		virtual ~Machine() = default;
		static std::unique_ptr<Machine> Archimedes(
			const Analyser::Static::Target *target,
			const ROMMachine::ROMFetcher &rom_fetcher
		);

		class Options: public Reflection::StructImpl<Options>, public Configurable::QuickloadOption<Options> {
			friend Configurable::QuickloadOption<Options>;
			public:
				Options(Configurable::OptionsType type) :
					Configurable::QuickloadOption<Options>(type == Configurable::OptionsType::UserFriendly) {
					if(needs_declare()) {
						declare_quickload_option();
					}
				}
		};
};

}
