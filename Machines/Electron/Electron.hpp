//
//  Electron.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Electron_hpp
#define Electron_hpp

#include "../../Configurable/Configurable.hpp"
#include "../../Configurable/StandardOptions.hpp"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace Electron {

/*!
	@abstract Represents an Acorn Electron.

	@discussion An instance of Electron::Machine represents the current state of an
	Acorn Electron.
*/
class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an Electron.
		static Machine *Electron(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		/// Defines the runtime options available for an Electron.
		class Options: public Reflection::StructImpl<Options> {
			public:
				Configurable::Display output;
				bool quickload;

				Options(Configurable::OptionsType type) :
					output(type == Configurable::OptionsType::UserFriendly ? Configurable::Display::RGB : Configurable::Display::CompositeColour),
					quickload(type == Configurable::OptionsType::UserFriendly) {

					if(needs_declare()) {
						DeclareField(output);
						DeclareField(quickload);
						AnnounceEnumNS(Configurable, Display);
						limit_enum(&output, Configurable::Display::RGB, Configurable::Display::CompositeColour, -1);
					}
				}
		};
};

}

#endif /* Electron_hpp */
