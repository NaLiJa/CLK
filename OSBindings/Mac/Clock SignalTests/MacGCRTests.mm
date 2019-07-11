//
//  MacGCRTests.mm
//  Clock SignalTests
//
//  Created by Thomas Harte on 15/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Storage/Disk/Encodings/AppleGCR/Encoder.hpp"

#include "../../../Storage/Disk/Track/TrackSerialiser.hpp"
#include "../../../Storage/Disk/Encodings/AppleGCR/SegmentParser.hpp"

@interface MacGCRTests : XCTestCase
@end

@implementation MacGCRTests {
}

- (void)testSector0Track0Side0Header {
	const auto header = Storage::Encodings::AppleGCR::Macintosh::header(0x22, 0, 0, false);
	const std::vector<uint8_t> expected_mark = {
		0xd5, 0xaa, 0x96,
		0x96, 0x96, 0x96, 0xd9, 0xd9,
		0xde, 0xaa, 0xeb
	};
	const auto mark_segment = Storage::Disk::PCMSegment(expected_mark);

	XCTAssertEqual(mark_segment.data, header.data);
}

- (void)testSector9Track11Side1Header {
	const auto header = Storage::Encodings::AppleGCR::Macintosh::header(0x22, 11, 9, true);
	const std::vector<uint8_t> expected_mark = {
		0xd5, 0xaa, 0x96,
		0xad, 0xab, 0xd6, 0xd9, 0x96,
		0xde, 0xaa, 0xeb
	};
	const auto mark_segment = Storage::Disk::PCMSegment(expected_mark);

	XCTAssertEqual(mark_segment.data, header.data);
}

- (void)testData {
	// This encoding was generated by the first version of my GCR encoder that produced data a Mac
	// would accept.
	const std::vector<uint8_t> expected_data = {
		/* Standard prologue. */
		0xd5, 0xaa, 0xad,

		/* Sector number. */
		0x96,

		/* Tags, after GCR encoding. */
		0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,

		/* Sector body. */
		0xb4, 0xae, 0xa6, 0xe7, 0xf6, 0x96, 0xae, 0xaf, 0xdc, 0xae, 0xcf, 0xce, 0x9d, 0xbe, 0xad, 0xed,
		0xe6, 0xd7, 0xf4, 0xd9, 0xf4, 0xfd, 0x9f, 0xdb, 0xba, 0xb7, 0xe9, 0x9d, 0x9f, 0xa6, 0xaf, 0xdb,
		0xe6, 0xeb, 0xea, 0xd9, 0xbe, 0xec, 0xfd, 0xda, 0xb9, 0xba, 0xcd, 0x97, 0xbb, 0xde, 0xfa, 0xf2,
		0xf5, 0xeb, 0xbf, 0xb7, 0xfb, 0xfd, 0xfb, 0xf2, 0xfd, 0xfc, 0xeb, 0x97, 0xf6, 0xbe, 0xf4, 0xf5,
		0xb7, 0xaf, 0xcd, 0xcb, 0xdf, 0xb4, 0xfb, 0xfb, 0xdf, 0x9b, 0xcd, 0xcb, 0xe9, 0xde, 0xb5, 0x9b,
		0xf2, 0xfe, 0xf3, 0xbf, 0xea, 0x9a, 0xa7, 0xad, 0xf6, 0x9b, 0xac, 0xfd, 0xcd, 0xb5, 0xff, 0xda,
		0xdf, 0xac, 0xaf, 0xeb, 0xa6, 0xfc, 0xf4, 0xf9, 0xee, 0xb7, 0x96, 0xbe, 0xdf, 0xd3, 0xf5, 0xfc,
		0xef, 0xbb, 0xef, 0xd6, 0xd3, 0xfc, 0xdc, 0x9e, 0xbe, 0xd9, 0xaf, 0x9a, 0xf6, 0xf2, 0xcf, 0xf4,
		0xbf, 0xf5, 0xae, 0xb4, 0xd3, 0xdf, 0xe9, 0xed, 0xff, 0xed, 0xae, 0xb4, 0xef, 0x97, 0x9b, 0xfc,
		0x96, 0xdb, 0xb4, 0xbd, 0xac, 0xb5, 0xf2, 0xf5, 0xed, 0xcb, 0xe7, 0xcd, 0xff, 0xb7, 0xff, 0xab,
		0xd3, 0xde, 0xcb, 0x9e, 0x9b, 0xb2, 0x9e, 0xb6, 0x9a, 0xed, 0x9e, 0x9e, 0xf2, 0xdd, 0x9e, 0x9f,
		0xcb, 0xaf, 0x9f, 0x9f, 0xdd, 0xcb, 0xa6, 0x9f, 0xac, 0xcb, 0xb6, 0xff, 0xce, 0xfc, 0xfa, 0xa6,
		0xce, 0xd7, 0xfe, 0xa6, 0xfa, 0xcf, 0xea, 0xbe, 0xb7, 0xfe, 0xf4, 0xdd, 0xb6, 0x97, 0xeb, 0xf5,
		0xec, 0xae, 0xcf, 0xe6, 0xfe, 0xb9, 0xdf, 0xe7, 0x9b, 0xd3, 0xbc, 0xb7, 0xfa, 0xec, 0xd6, 0xcb,
		0xbc, 0xb5, 0xec, 0xe9, 0xb3, 0xfa, 0x9e, 0xf9, 0xad, 0xb9, 0xfd, 0xe6, 0xf7, 0xdb, 0xf3, 0xf4,
		0x9b, 0xbe, 0xfe, 0xfe, 0xdc, 0x9e, 0xfa, 0xff, 0xec, 0xf5, 0xad, 0xfc, 0xdb, 0xf4, 0xde, 0xda,
		0xcd, 0x96, 0xcd, 0xb7, 0xf5, 0xcd, 0xb6, 0xb4, 0xd7, 0xbd, 0xce, 0xf6, 0x9b, 0xd7, 0xac, 0xdb,
		0xae, 0xdb, 0xd3, 0xff, 0xea, 0xf4, 0x9a, 0xb5, 0xee, 0xbb, 0xac, 0xf7, 0xf5, 0xb7, 0xa7, 0xee,
		0xe5, 0xb7, 0xe9, 0x9b, 0xb2, 0xd7, 0xfc, 0xbf, 0xf4, 0xfc, 0xb5, 0xfb, 0xb7, 0xac, 0xbd, 0xb9,
		0xbb, 0xdd, 0x97, 0xdc, 0xb6, 0xec, 0xf7, 0xa7, 0xff, 0xfc, 0xcd, 0xdc, 0xfb, 0x9e, 0xfa, 0xfc,
		0xbc, 0xed, 0xaf, 0xbe, 0xb2, 0xdf, 0xb7, 0xae, 0xf5, 0xbf, 0xef, 0xae, 0xf2, 0xb9, 0xbb, 0xfa,
		0x96, 0xfd, 0xdb, 0xd3, 0xfd, 0xd9, 0xbd, 0xac, 0xae, 0xa7, 0xb2, 0xb7, 0xe5, 0xcb, 0xda, 0xbc,
		0xf6, 0xbf, 0xde, 0x97, 0xbd, 0xdd, 0xec, 0xe7, 0xce, 0x97, 0xae, 0xcf, 0xd6, 0xdf, 0xfa, 0xcf,
		0xdf, 0xf2, 0xad, 0xba, 0x97, 0xbc, 0xcd, 0xe9, 0xbe, 0xbb, 0xf7, 0xf5, 0xdd, 0xdc, 0xf9, 0xff,
		0xbf, 0xe5, 0xb6, 0xfe, 0xa7, 0xbf, 0xb4, 0xd6, 0xce, 0xce, 0xaf, 0xdc, 0xd6, 0xd3, 0xfe, 0xdc,
		0xf9, 0xbb, 0xe9, 0xb5, 0xbf, 0xdf, 0xdd, 0xa6, 0xac, 0xe5, 0xf6, 0xb5, 0xb5, 0xef, 0xfd, 0xf3,
		0xff, 0xbe, 0xcf, 0xf3, 0xa6, 0x9e, 0xb2, 0x96, 0xf3, 0xcf, 0xb7, 0xac, 0xf2, 0xb2, 0xd7, 0xd9,
		0xdc, 0xfc, 0xfc, 0xfc, 0xb3, 0xde, 0x9a, 0xe6, 0xa7, 0xf2, 0xab, 0xb4, 0xf7, 0xab, 0xb4, 0xbe,
		0xcb, 0x97, 0xe7, 0xad, 0x96, 0xaf, 0xe9, 0xdf, 0xb4, 0xcf, 0x96, 0xbd, 0xd3, 0xfa, 0xde, 0xb3,
		0xac, 0xb4, 0xe6, 0xbf, 0xa7, 0xdb, 0x96, 0xa7, 0xb7, 0xde, 0xce, 0x9e, 0xeb, 0xe9, 0xd6, 0x9d,
		0xbc, 0xe7, 0xfe, 0x9e, 0xfa, 0xd3, 0xba, 0xe6, 0xbe, 0xab, 0xcf, 0xd3, 0xfd, 0xdd, 0xf5, 0xad,
		0xdf, 0xf7, 0xbe, 0xad, 0xfb, 0xfc, 0xbc, 0xcd, 0xe5, 0x9b, 0xab, 0xe7, 0xea, 0xb2, 0xdb, 0xbb,
		0xbc, 0xda, 0xe6, 0xa6, 0xdb, 0xea, 0xb7, 0x9b, 0xed, 0xaf, 0xb2, 0xf5, 0xd9, 0xb9, 0xbe, 0xdb,
		0x9f, 0xfd, 0xf3, 0xef, 0xff, 0xae, 0xfa, 0xf9, 0xba, 0xff, 0xe9, 0xf3, 0xf4, 0xcf, 0xe6, 0xbf,
		0xdb, 0xae, 0xad, 0xb3, 0xad, 0xaf, 0xe9, 0xbf, 0xa7, 0xaf, 0xbd, 0xdf, 0xae, 0xb4, 0xbb, 0xdd,
		0xce, 0xcf, 0xb7, 0xfd, 0xec, 0xce, 0xbe, 0xde, 0xf2, 0xbd, 0xe7, 0x9d, 0xdf, 0xce, 0xac, 0xbf,
		0xd3, 0xfe, 0xe7, 0xdd, 0xfc, 0xb6, 0xe7, 0xb4, 0xfa, 0xbd, 0xef, 0xae, 0xa7, 0xdf, 0xbd, 0xf7,
		0xa7, 0xdd, 0xbe, 0x9e, 0x9a, 0xea, 0x97, 0xdb, 0xb2, 0x9b, 0xa7, 0xd7, 0x96, 0xf5, 0xb4, 0xbf,
		0xbc, 0xd7, 0x9a, 0xcb, 0xce, 0xb9, 0xb3, 0xb5, 0xaf, 0xb9, 0xfa, 0xf2, 0xbf, 0xb3, 0xdc, 0xdb,
		0xad, 0xd7, 0xbe, 0x9d, 0xea, 0xda, 0xb2, 0x9e, 0xea, 0xbd, 0xbe, 0xb7, 0xcb, 0xac, 0x9d, 0xb7,
		0xe7, 0xee, 0xd3, 0xb9, 0xcf, 0xce, 0xf9, 0x96, 0xf4, 0xbd, 0xab, 0xb5, 0xed, 0xe5, 0xea, 0xf4,
		0xfb, 0xd7, 0xf7, 0xbe, 0xf4, 0xfb, 0xb4, 0xbf, 0xee, 0xac, 0xde, 0xbd, 0xdc, 0xb6, 0xcf, 0xab,
		0xb7, 0xaf, 0xf7, 0xfb, 0xb3, 0xb3, 0xee, 0xe9, 0xcd, 0xb7, 0xd9, 0xfc, 0xd9, 0x9d, 0xef,

		/* Standard epilogue. */
		0xde, 0xaa, 0xeb
	};

	// This is the first sector, taken from a random disk image from the internet.
	const uint8_t source_data[524] = {
		/* Tags. */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		/* Sector body. */
		0x4c, 0x4b, 0x60, 0x00, 0x00, 0x86, 0x00, 0x12, 0x00, 0x00, 0x06, 0x53, 0x79, 0x73, 0x74, 0x65,
		0x6d, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x06, 0x46, 0x69, 0x6e, 0x64, 0x65,
		0x72, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x07, 0x4d, 0x61, 0x63, 0x73, 0x62,
		0x75, 0x67, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x0c, 0x44, 0x69, 0x73, 0x61, 0x73,
		0x73, 0x65, 0x6d, 0x62, 0x6c, 0x65, 0x72, 0x20, 0x20, 0x20, 0x0d, 0x53, 0x74, 0x61, 0x72, 0x74,
		0x55, 0x70, 0x53, 0x63, 0x72, 0x65, 0x65, 0x6e, 0x20, 0x20, 0x06, 0x46, 0x69, 0x6e, 0x64, 0x65,
		0x72, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x0e, 0x43, 0x6c, 0x69, 0x70, 0x62,
		0x6f, 0x61, 0x72, 0x64, 0x20, 0x46, 0x69, 0x6c, 0x65, 0x20, 0x00, 0x0a, 0x00, 0x14, 0x00, 0x00,
		0x43, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x22, 0x38, 0x01, 0x08, 0x48, 0x41,
		0x0c, 0x41, 0x00, 0x04, 0x6e, 0x2c, 0x72, 0x00, 0x50, 0xf9, 0x00, 0x01, 0xff, 0xf0, 0x42, 0xb9,
		0x00, 0x03, 0xff, 0xf0, 0x4a, 0xb9, 0x00, 0x01, 0xff, 0xf0, 0x67, 0x16, 0x70, 0x02, 0x48, 0x40,
		0xd1, 0xb8, 0x01, 0x08, 0xd1, 0xb8, 0x08, 0x24, 0xd1, 0xb8, 0x02, 0x66, 0xd1, 0xb8, 0x01, 0x0c,
		0x72, 0x04, 0x20, 0x78, 0x02, 0xa6, 0x0c, 0x41, 0x00, 0x08, 0x6f, 0x02, 0x72, 0x08, 0xd1, 0xfb,
		0x10, 0xae, 0x2f, 0x08, 0x70, 0x07, 0x41, 0xf8, 0x0a, 0xb8, 0x42, 0x98, 0x51, 0xc8, 0xff, 0xfc,
		0x34, 0x3a, 0xff, 0x9a, 0x70, 0x16, 0xc0, 0xc2, 0x22, 0x00, 0xa7, 0x1e, 0x43, 0xf8, 0x01, 0x54,
		0x53, 0x42, 0x32, 0x82, 0x42, 0xa1, 0x42, 0xa1, 0x42, 0x61, 0x23, 0x08, 0x46, 0x58, 0x55, 0x41,
		0x66, 0xfa, 0x33, 0x3c, 0xff, 0xef, 0x42, 0x78, 0x01, 0x84, 0x72, 0xfc, 0x70, 0x0f, 0x14, 0x38,
		0x02, 0x06, 0xc0, 0x02, 0xd0, 0x40, 0x48, 0x40, 0x10, 0x02, 0xe4, 0x48, 0xc0, 0x41, 0x48, 0x40,
		0x21, 0xc0, 0x01, 0x8e, 0x70, 0x0f, 0x14, 0x38, 0x02, 0x09, 0xc0, 0x02, 0xe5, 0x48, 0x21, 0xc0,
		0x02, 0xf4, 0x10, 0x02, 0xe4, 0x48, 0xc0, 0x41, 0x21, 0xc0, 0x02, 0xf0, 0x41, 0xf8, 0x03, 0x40,
		0x72, 0x50, 0x42, 0x58, 0x51, 0xc9, 0xff, 0xfc, 0x70, 0x1e, 0xc0, 0xfa, 0xff, 0x2e, 0x32, 0x38,
		0x01, 0x08, 0xe2, 0x49, 0xc0, 0xc1, 0x54, 0x40, 0x32, 0x00, 0xa7, 0x1e, 0x21, 0xc8, 0x03, 0x4e,
		0x30, 0xc1, 0x31, 0xfc, 0x00, 0x02, 0x03, 0x4c, 0x9e, 0xfc, 0x00, 0x32, 0x20, 0x4f, 0x31, 0x78,
		0x02, 0x10, 0x00, 0x16, 0xa0, 0x0f, 0x66, 0x00, 0x01, 0xb2, 0xde, 0xfc, 0x00, 0x32, 0x43, 0xf8,
		0x0a, 0xd8, 0x41, 0xfa, 0xfe, 0x86, 0x70, 0x10, 0xa0, 0x2e, 0x55, 0x4f, 0x2f, 0x0f, 0x48, 0x78,
		0x09, 0xfa, 0x20, 0x78, 0x08, 0x10, 0x4e, 0x90, 0x30, 0x1f, 0xe6, 0x48, 0x31, 0xc0, 0x01, 0x06,
		0x08, 0x38, 0x00, 0x06, 0x02, 0x0b, 0x56, 0xf8, 0x08, 0xd3, 0xa8, 0x52, 0x43, 0xfa, 0xfe, 0x9c,
		0x76, 0x01, 0x61, 0x00, 0x01, 0x98, 0x0c, 0x44, 0x40, 0x00, 0x6e, 0x02, 0x70, 0xff, 0x3f, 0x00,
		0x66, 0x04, 0x61, 0x00, 0x01, 0xf0, 0xa8, 0x53, 0x55, 0x4f, 0x42, 0xb8, 0x0a, 0xf2, 0xa9, 0x95,
		0x4a, 0x5f, 0x6b, 0x00, 0x01, 0x56, 0x3e, 0x1f, 0x20, 0x5f, 0xa0, 0x57, 0x21, 0xf8, 0x02, 0xa6,
		0x01, 0x18, 0x59, 0x4f, 0x2f, 0x3c, 0x44, 0x53, 0x41, 0x54, 0x42, 0x67, 0xa9, 0xa0, 0x2a, 0x1f,
		0x67, 0x00, 0x01, 0x1e, 0x20, 0x45, 0x21, 0xd0, 0x02, 0xba, 0xa8, 0xfe, 0x70, 0x28, 0x61, 0x00,
	};
	const auto data = Storage::Encodings::AppleGCR::Macintosh::data(0, source_data);
	const auto expected = Storage::Disk::PCMSegment(expected_data);
	XCTAssertEqual(data.data, expected.data);
}

- (void)testDecoding {
	const uint8_t format = 0x22;
	const uint8_t track_id = 23;
	const bool is_side_two = true;

	// Prepare a test track of 8 sectors.
	Storage::Disk::PCMSegment segment;
	segment += Storage::Encodings::AppleGCR::six_and_two_sync(24);
	for(int c = 0; c < 8; ++c) {
		uint8_t sector_id = uint8_t(c);

		uint8_t sector_plus_tags[524];

		// Provide tags plus a sector body that are just the sector number ad infinitum.
		memset(sector_plus_tags, sector_id, sizeof(sector_plus_tags));

		// NB: sync lengths below are identical to those for
		// the Apple II, as I have no idea whatsoever what they
		// should be.

		segment += Storage::Encodings::AppleGCR::Macintosh::header(
			format,
			track_id,
			sector_id,
			is_side_two
		);
		segment += Storage::Encodings::AppleGCR::six_and_two_sync(7);
		segment += Storage::Encodings::AppleGCR::Macintosh::data(sector_id, sector_plus_tags);
		segment += Storage::Encodings::AppleGCR::six_and_two_sync(20);
	}

	// Parse the prepared track to look for sectors.
	const auto decoded_sectors = Storage::Encodings::AppleGCR::sectors_from_segment(segment);

	// Assert that all sectors fed in were found and correctly decoded.
	XCTAssertEqual(decoded_sectors.size(), 8);
}

@end
