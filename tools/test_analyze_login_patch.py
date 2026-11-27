import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analyze_patch_profile as analyzer  # noqa: E402


def encode_uleb128(value):
    encoded = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        encoded.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(encoded)


def sample_macho(reference=False):
    image_base = 0x100000000
    text_address = 0x100001000
    text_offset = 200
    branch_index = 12
    branch_address = text_address + branch_index * 4
    target_address = text_address + 14 * 4
    branch_immediate = (target_address - branch_address) // 4
    tbz = 0x36000000 | (branch_immediate << 5)
    instructions = [analyzer.NOP] * 60
    instructions[branch_index] = tbz
    if reference:
        instructions[:len(analyzer.REFERENCE_WINDOW)] = analyzer.REFERENCE_WINDOW
        instructions[branch_index] |= (14 - branch_index) << 5
    text = struct.pack('<60I', *instructions)

    data = bytearray(text_offset + len(text) + 3)
    struct.pack_into('<I', data, 0, 0xFEEDFACF)
    struct.pack_into('<I', data, 4, 0x0100000C)
    struct.pack_into('<I', data, 12, 2)
    struct.pack_into('<II', data, 16, 2, 168)

    # LC_SEGMENT_64 with one __TEXT,__text section.
    struct.pack_into('<II', data, 32, 0x19, 152)
    data[40:46] = b'__TEXT'
    struct.pack_into('<QQQQ', data, 56, image_base, 0x2000, 0, len(data))
    struct.pack_into('<IIII', data, 88, 7, 5, 1, 0)
    section = 104
    data[section:section + 6] = b'__text'
    data[section + 16:section + 22] = b'__TEXT'
    struct.pack_into('<QQI', data, section + 32, text_address, len(text), text_offset)

    # LC_FUNCTION_STARTS stores the first function at the start of __text.
    struct.pack_into('<IIII', data, 184, 0x26, 16, 440, 3)
    data[text_offset:text_offset + len(text)] = text
    data[440:443] = encode_uleb128(0x1000) + b'\0'
    return bytes(data)


class LoginPatchAnalyzerTests(unittest.TestCase):
    def test_parse_and_locate_tbz_candidate(self):
        binary = sample_macho()
        image = analyzer.parse_arm64_macho(binary)
        branch_count, candidate_count, _, candidates = analyzer.locate_login_branch(
            binary, image,
        )

        self.assertEqual(branch_count, 1)
        self.assertEqual(candidate_count, 1)
        self.assertEqual(len(candidates), 1)
        self.assertEqual(candidates[0].file_offset, 248)
        self.assertEqual(candidates[0].virtual_address, 0x100001030)
        self.assertEqual(candidates[0].target_address, 0x100001038)
        self.assertEqual(candidates[0].instruction, 'tbz w0, #0')
        self.assertEqual(candidates[0].function_start, 0x100001000)

    def test_recommends_unique_normalized_reference_match(self):
        analysis = analyzer.analyze_login_executable(
            sample_macho(reference=True), version='test', build='2',
        )
        self.assertEqual(analysis['recommended_patch_offset'], '0xf8')
        self.assertEqual(analysis['best_similarity'], 1.0)
        self.assertEqual(analysis['high_confidence_candidate_count'], 1)
        self.assertFalse(analysis['automatic_patch'])

    def test_creates_login_section_for_combined_profile(self):
        analysis = analyzer.analyze_login_executable(
            sample_macho(reference=True), version='26.15.1', build='test-build',
        )
        patch = analyzer.login_patch_section(analysis)
        self.assertEqual(patch['file_offset'], '0xf8')
        self.assertEqual(patch['virtual_address'], '0x100001030')
        self.assertEqual(patch['instruction'], 'tbz w0, #0, 0x100001038')
        self.assertEqual(patch['original_hex'], '40000036')
        self.assertEqual(patch['patched_hex'], '1f2003d5')

    def test_reference_window_tolerates_one_moved_load(self):
        # A moved GOT load still matches; a different frame layout as well does not.
        binary = bytearray(sample_macho(reference=True))
        struct.pack_into('<I', binary, 200 + 25 * 4, 0xF946FD08)
        result = analyzer.analyze_login_executable(bytes(binary))
        self.assertAlmostEqual(result['best_similarity'], 28 / 29)
        self.assertEqual(result['recommended_patch_offset'], '0xf8')
        struct.pack_into('<4I', binary, 200, 0xA90457F6, 0xA9054FF4, 0xA9067BFD, 0x910183FD)
        result = analyzer.analyze_login_executable(bytes(binary))
        self.assertAlmostEqual(result['best_similarity'], 24 / 29)
        self.assertIsNone(result['recommended_patch_offset'])

    def inline_friend_pattern(self, register=26):
        compare = 0x7100141F | register << 5
        store = analyzer.FRIEND_TAB_INLINE_STORE
        return (
            compare, 0x1A9F17E8, *store, 0x94000000, 0x14000000,
            compare, 0x1A9F17E8, *([analyzer.NOP] * 9), *store,
        )

    def test_analyzes_unique_friend_tab_patch_sites(self):
        binary = bytearray(sample_macho())
        struct.pack_into('<19I', binary, 200 + 20 * 4, *self.inline_friend_pattern())

        profile = analyzer.analyze_friend_tab_executable(bytes(binary))

        self.assertEqual(
            [patch['file_offset'] for patch in profile['patches']],
            ['0x11c', '0x134'],
        )
        patched = bytearray(binary)
        for patch in profile['patches']:
            offset = int(patch['file_offset'], 16)
            self.assertEqual(patched[offset:offset + 4].hex(), patch['original_hex'])
            patched[offset:offset + 4] = bytes.fromhex(patch['patched_hex'])
        self.assertEqual(struct.unpack_from('<I', patched, 0x11c)[0], analyzer.FRIEND_TAB_CASE_COLLAPSED)
        analyzer._validate_home_friends_section({'home_friends': profile}, bytes(binary))
        altered = dict(profile)
        altered['patches'] = [dict(patch) for patch in profile['patches']]
        altered['patches'][0]['file_offset'] = '0x120'
        with self.assertRaisesRegex(ValueError, 'Friends'):
            analyzer._validate_home_friends_section({'home_friends': altered}, bytes(binary))

    def test_friend_tab_analysis_needs_both_stores_and_one_compare(self):
        pattern = self.inline_friend_pattern()
        for index, value in ((2, analyzer.NOP), (17, analyzer.NOP), (6, 0x7100141F | 25 << 5),
                             (0, 0x7100181F | 26 << 5)):
            binary = bytearray(sample_macho())
            altered = list(pattern)
            altered[index] = value
            struct.pack_into('<19I', binary, 200 + 20 * 4, *altered)
            with self.subTest(index=index):
                self.assertIsNone(analyzer.analyze_friend_tab_executable(bytes(binary)))

    def test_friend_tab_analysis_refuses_non_unique_signature(self):
        binary = bytearray(sample_macho())
        struct.pack_into('<19I', binary, 200 + 20 * 4, *self.inline_friend_pattern())
        struct.pack_into('<19I', binary, 200 + 40 * 4, *self.inline_friend_pattern())

        self.assertIsNone(analyzer.analyze_friend_tab_executable(bytes(binary)))

    def test_refuses_inconclusive_login_profile_section(self):
        analysis = analyzer.analyze_login_executable(
            sample_macho(), version='test', build='inconclusive',
        )
        with self.assertRaisesRegex(ValueError, 'No high-confidence'):
            analyzer.login_patch_section(analysis)

    def test_decodes_cbz_and_conditional_branch(self):
        cbz = analyzer._decode_branch(0x34000040, 0x1000)
        self.assertEqual(cbz, ('cbz w0', 0x1008, 'zero'))
        beq = analyzer._decode_branch(0x54000040, 0x1000)
        self.assertEqual(beq, ('b.eq', 0x1008, 'eq'))

    def test_rejects_non_arm64_thin_macho(self):
        with self.assertRaisesRegex(ValueError, 'thin little-endian 64-bit Mach-O'):
            analyzer.parse_arm64_macho(b'not a Mach-O')


if __name__ == '__main__':
    unittest.main()
