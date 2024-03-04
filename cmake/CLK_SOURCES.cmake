# Generated by generate_CLK_SOURCES.

set(CLK_SOURCES
	Analyser/Dynamic/ConfidenceCounter.cpp
	Analyser/Dynamic/ConfidenceSummary.cpp
	Analyser/Dynamic/MultiMachine/Implementation/MultiConfigurable.cpp
	Analyser/Dynamic/MultiMachine/Implementation/MultiJoystickMachine.cpp
	Analyser/Dynamic/MultiMachine/Implementation/MultiKeyboardMachine.cpp
	Analyser/Dynamic/MultiMachine/Implementation/MultiMediaTarget.cpp
	Analyser/Dynamic/MultiMachine/Implementation/MultiProducer.cpp
	Analyser/Dynamic/MultiMachine/Implementation/MultiSpeaker.cpp
	Analyser/Dynamic/MultiMachine/MultiMachine.cpp
	Analyser/Static/Acorn/Disk.cpp
	Analyser/Static/Acorn/StaticAnalyser.cpp
	Analyser/Static/Acorn/Tape.cpp
	Analyser/Static/Amiga/StaticAnalyser.cpp
	Analyser/Static/AmstradCPC/StaticAnalyser.cpp
	Analyser/Static/AppleII/StaticAnalyser.cpp
	Analyser/Static/AppleIIgs/StaticAnalyser.cpp
	Analyser/Static/Atari2600/StaticAnalyser.cpp
	Analyser/Static/AtariST/StaticAnalyser.cpp
	Analyser/Static/Coleco/StaticAnalyser.cpp
	Analyser/Static/Commodore/Disk.cpp
	Analyser/Static/Commodore/File.cpp
	Analyser/Static/Commodore/StaticAnalyser.cpp
	Analyser/Static/Commodore/Tape.cpp
	Analyser/Static/Disassembler/6502.cpp
	Analyser/Static/Disassembler/Z80.cpp
	Analyser/Static/DiskII/StaticAnalyser.cpp
	Analyser/Static/Enterprise/StaticAnalyser.cpp
	Analyser/Static/FAT12/StaticAnalyser.cpp
	Analyser/Static/MSX/StaticAnalyser.cpp
	Analyser/Static/MSX/Tape.cpp
	Analyser/Static/Macintosh/StaticAnalyser.cpp
	Analyser/Static/Oric/StaticAnalyser.cpp
	Analyser/Static/Oric/Tape.cpp
	Analyser/Static/PCCompatible/StaticAnalyser.cpp
	Analyser/Static/Sega/StaticAnalyser.cpp
	Analyser/Static/StaticAnalyser.cpp
	Analyser/Static/ZX8081/StaticAnalyser.cpp
	Analyser/Static/ZXSpectrum/StaticAnalyser.cpp

	Components/1770/1770.cpp
	Components/5380/ncr5380.cpp
	Components/6522/Implementation/IRQDelegatePortHandler.cpp
	Components/6560/6560.cpp
	Components/6850/6850.cpp
	Components/68901/MFP68901.cpp
	Components/8272/i8272.cpp
	Components/8530/z8530.cpp
	Components/9918/Implementation/9918.cpp
	Components/AY38910/AY38910.cpp
	Components/AudioToggle/AudioToggle.cpp
	Components/DiskII/DiskII.cpp
	Components/DiskII/DiskIIDrive.cpp
	Components/DiskII/IWM.cpp
	Components/DiskII/MacintoshDoubleDensityDrive.cpp
	Components/KonamiSCC/KonamiSCC.cpp
	Components/OPx/OPLL.cpp
	Components/RP5C01/RP5C01.cpp
	Components/SN76489/SN76489.cpp
	Components/Serial/Line.cpp

	Inputs/Keyboard.cpp

	InstructionSets/M50740/Decoder.cpp
	InstructionSets/M50740/Executor.cpp
	InstructionSets/M68k/Decoder.cpp
	InstructionSets/M68k/Instruction.cpp
	InstructionSets/PowerPC/Decoder.cpp
	InstructionSets/x86/Decoder.cpp
	InstructionSets/x86/Instruction.cpp

	Machines/Acorn/Archimedes/Archimedes.cpp
	Machines/Acorn/Electron/Electron.cpp
	Machines/Acorn/Electron/Keyboard.cpp
	Machines/Acorn/Electron/Plus3.cpp
	Machines/Acorn/Electron/SoundGenerator.cpp
	Machines/Acorn/Electron/Tape.cpp
	Machines/Acorn/Electron/Video.cpp
	Machines/Amiga/Amiga.cpp
	Machines/Amiga/Audio.cpp
	Machines/Amiga/Bitplanes.cpp
	Machines/Amiga/Blitter.cpp
	Machines/Amiga/Chipset.cpp
	Machines/Amiga/Copper.cpp
	Machines/Amiga/Disk.cpp
	Machines/Amiga/Keyboard.cpp
	Machines/Amiga/MouseJoystick.cpp
	Machines/Amiga/Sprites.cpp
	Machines/AmstradCPC/AmstradCPC.cpp
	Machines/AmstradCPC/Keyboard.cpp
	Machines/Apple/ADB/Bus.cpp
	Machines/Apple/ADB/Keyboard.cpp
	Machines/Apple/ADB/Mouse.cpp
	Machines/Apple/ADB/ReactiveDevice.cpp
	Machines/Apple/AppleII/AppleII.cpp
	Machines/Apple/AppleII/DiskIICard.cpp
	Machines/Apple/AppleII/Joystick.cpp
	Machines/Apple/AppleII/SCSICard.cpp
	Machines/Apple/AppleII/Video.cpp
	Machines/Apple/AppleIIgs/ADB.cpp
	Machines/Apple/AppleIIgs/AppleIIgs.cpp
	Machines/Apple/AppleIIgs/MemoryMap.cpp
	Machines/Apple/AppleIIgs/Sound.cpp
	Machines/Apple/AppleIIgs/Video.cpp
	Machines/Apple/Macintosh/Audio.cpp
	Machines/Apple/Macintosh/DriveSpeedAccumulator.cpp
	Machines/Apple/Macintosh/Keyboard.cpp
	Machines/Apple/Macintosh/Macintosh.cpp
	Machines/Apple/Macintosh/Video.cpp
	Machines/Atari/2600/Atari2600.cpp
	Machines/Atari/2600/TIA.cpp
	Machines/Atari/2600/TIASound.cpp
	Machines/Atari/ST/AtariST.cpp
	Machines/Atari/ST/DMAController.cpp
	Machines/Atari/ST/IntelligentKeyboard.cpp
	Machines/Atari/ST/Video.cpp
	Machines/ColecoVision/ColecoVision.cpp
	Machines/Commodore/1540/Implementation/C1540.cpp
	Machines/Commodore/SerialBus.cpp
	Machines/Commodore/Vic-20/Keyboard.cpp
	Machines/Commodore/Vic-20/Vic20.cpp
	Machines/Enterprise/Dave.cpp
	Machines/Enterprise/EXDos.cpp
	Machines/Enterprise/Enterprise.cpp
	Machines/Enterprise/Keyboard.cpp
	Machines/Enterprise/Nick.cpp
	Machines/KeyboardMachine.cpp
	Machines/MSX/DiskROM.cpp
	Machines/MSX/Keyboard.cpp
	Machines/MSX/MSX.cpp
	Machines/MSX/MemorySlotHandler.cpp
	Machines/MasterSystem/MasterSystem.cpp
	Machines/Oric/BD500.cpp
	Machines/Oric/Jasmin.cpp
	Machines/Oric/Keyboard.cpp
	Machines/Oric/Microdisc.cpp
	Machines/Oric/Oric.cpp
	Machines/Oric/Video.cpp
	Machines/PCCompatible/PCCompatible.cpp
	Machines/Sinclair/Keyboard/Keyboard.cpp
	Machines/Sinclair/ZX8081/Video.cpp
	Machines/Sinclair/ZX8081/ZX8081.cpp
	Machines/Sinclair/ZXSpectrum/ZXSpectrum.cpp
	Machines/Utility/MachineForTarget.cpp
	Machines/Utility/MemoryFuzzer.cpp
	Machines/Utility/MemoryPacker.cpp
	Machines/Utility/ROMCatalogue.cpp
	Machines/Utility/StringSerialiser.cpp
	Machines/Utility/Typer.cpp

	Outputs/CRT/CRT.cpp
	Outputs/DisplayMetrics.cpp
	Outputs/OpenGL/Primitives/Rectangle.cpp
	Outputs/OpenGL/Primitives/Shader.cpp
	Outputs/OpenGL/Primitives/TextureTarget.cpp
	Outputs/OpenGL/ScanTarget.cpp
	Outputs/OpenGL/ScanTargetGLSLFragments.cpp
	Outputs/ScanTarget.cpp
	Outputs/ScanTargets/BufferingScanTarget.cpp

	Processors/6502/Implementation/6502Storage.cpp
	Processors/6502/State/State.cpp
	Processors/65816/Implementation/65816Base.cpp
	Processors/65816/Implementation/65816Storage.cpp
	Processors/Z80/Implementation/PartialMachineCycle.cpp
	Processors/Z80/Implementation/Z80Base.cpp
	Processors/Z80/Implementation/Z80Storage.cpp
	Processors/Z80/State/State.cpp

	Reflection/Struct.cpp

	SignalProcessing/FIRFilter.cpp

	Storage/Cartridge/Cartridge.cpp
	Storage/Cartridge/Encodings/CommodoreROM.cpp
	Storage/Cartridge/Formats/BinaryDump.cpp
	Storage/Cartridge/Formats/PRG.cpp
	Storage/Data/Commodore.cpp
	Storage/Data/ZX8081.cpp
	Storage/Disk/Controller/DiskController.cpp
	Storage/Disk/Controller/MFMDiskController.cpp
	Storage/Disk/DiskImage/Formats/2MG.cpp
	Storage/Disk/DiskImage/Formats/AcornADF.cpp
	Storage/Disk/DiskImage/Formats/AmigaADF.cpp
	Storage/Disk/DiskImage/Formats/AppleDSK.cpp
	Storage/Disk/DiskImage/Formats/CPCDSK.cpp
	Storage/Disk/DiskImage/Formats/D64.cpp
	Storage/Disk/DiskImage/Formats/DMK.cpp
	Storage/Disk/DiskImage/Formats/FAT12.cpp
	Storage/Disk/DiskImage/Formats/G64.cpp
	Storage/Disk/DiskImage/Formats/HFE.cpp
	Storage/Disk/DiskImage/Formats/IMD.cpp
	Storage/Disk/DiskImage/Formats/IPF.cpp
	Storage/Disk/DiskImage/Formats/MFMSectorDump.cpp
	Storage/Disk/DiskImage/Formats/MSA.cpp
	Storage/Disk/DiskImage/Formats/MacintoshIMG.cpp
	Storage/Disk/DiskImage/Formats/NIB.cpp
	Storage/Disk/DiskImage/Formats/OricMFMDSK.cpp
	Storage/Disk/DiskImage/Formats/PCBooter.cpp
	Storage/Disk/DiskImage/Formats/SSD.cpp
	Storage/Disk/DiskImage/Formats/STX.cpp
	Storage/Disk/DiskImage/Formats/Utility/ImplicitSectors.cpp
	Storage/Disk/DiskImage/Formats/WOZ.cpp
	Storage/Disk/Drive.cpp
	Storage/Disk/Encodings/AppleGCR/Encoder.cpp
	Storage/Disk/Encodings/AppleGCR/SegmentParser.cpp
	Storage/Disk/Encodings/CommodoreGCR.cpp
	Storage/Disk/Encodings/MFM/Encoder.cpp
	Storage/Disk/Encodings/MFM/Parser.cpp
	Storage/Disk/Encodings/MFM/SegmentParser.cpp
	Storage/Disk/Encodings/MFM/Shifter.cpp
	Storage/Disk/Parsers/CPM.cpp
	Storage/Disk/Parsers/FAT.cpp
	Storage/Disk/Track/PCMSegment.cpp
	Storage/Disk/Track/PCMTrack.cpp
	Storage/Disk/Track/TrackSerialiser.cpp
	Storage/Disk/Track/UnformattedTrack.cpp
	Storage/FileHolder.cpp
	Storage/MassStorage/Encodings/MacintoshVolume.cpp
	Storage/MassStorage/Formats/DAT.cpp
	Storage/MassStorage/Formats/DSK.cpp
	Storage/MassStorage/Formats/HDV.cpp
	Storage/MassStorage/Formats/HFV.cpp
	Storage/MassStorage/MassStorageDevice.cpp
	Storage/MassStorage/SCSI/DirectAccessDevice.cpp
	Storage/MassStorage/SCSI/SCSI.cpp
	Storage/MassStorage/SCSI/Target.cpp
	Storage/State/SNA.cpp
	Storage/State/SZX.cpp
	Storage/State/Z80.cpp
	Storage/Tape/Formats/CAS.cpp
	Storage/Tape/Formats/CSW.cpp
	Storage/Tape/Formats/CommodoreTAP.cpp
	Storage/Tape/Formats/OricTAP.cpp
	Storage/Tape/Formats/TZX.cpp
	Storage/Tape/Formats/TapePRG.cpp
	Storage/Tape/Formats/TapeUEF.cpp
	Storage/Tape/Formats/ZX80O81P.cpp
	Storage/Tape/Formats/ZXSpectrumTAP.cpp
	Storage/Tape/Parsers/Acorn.cpp
	Storage/Tape/Parsers/Commodore.cpp
	Storage/Tape/Parsers/MSX.cpp
	Storage/Tape/Parsers/Oric.cpp
	Storage/Tape/Parsers/Spectrum.cpp
	Storage/Tape/Parsers/ZX8081.cpp
	Storage/Tape/PulseQueuedTape.cpp
	Storage/Tape/Tape.cpp
	Storage/TimedEventLoop.cpp
)

if(CLK_UI STREQUAL "SDL")
	list(APPEND CLK_SOURCES
		OSBindings/SDL/main.cpp
	)
endif()
