//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Amiga_Target_h
#define Analyser_Static_Amiga_Target_h

#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace Amiga {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(FastRAM,
		None,
		OneMegabyte,
		TwoMegabytes,
		FourMegabytes,
		EightMegabytes);

	FastRAM fast_ram = FastRAM::TwoMegabytes;

	Target() : Analyser::Static::Target(Machine::Amiga) {
		if(needs_declare()) {
			DeclareField(fast_ram);
			AnnounceEnum(FastRAM);
		}
	}
};

}
}
}

#endif /* Analyser_Static_Amiga_Target_h */
