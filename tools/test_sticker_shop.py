import struct
import sys
import unittest
from dataclasses import replace
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analyze_patch_profile as analyzer  # noqa: E402
import main  # noqa: E402


class StickerShopTests(unittest.TestCase):
    def native(self):
        native = {
            'kind': 'native_sticker_routes',
            'guard_address': '0x1000004e0', 'deny_address': '0x10000053c',
            'kind_resolver_address': '0x100000780',
            'command_resolver_address': '0x1000007a0',
            'file_offset': '0x4e0', 'original_hex': 'f5020036',
            'patched_hex': struct.pack('<I', analyzer._branch26(0x1000004E0, 0x100000500)).hex(),
            'code_cave': {'address': '0x100000500', 'file_offset': '0x500'},
        }
        code = analyzer.sticker_shop_native_code(native)
        native['code_cave'].update(original_hex=bytes(len(code)).hex(), patched_hex=code.hex())
        return native

    def sample(self):
        base = 0x100000000
        binary = bytearray(0xD00)

        def call(offset, target):
            struct.pack_into('<I', binary, offset, 0x94000000 | (((target - offset) // 4) & 0x3FFFFFF))

        def page_address(offset, register, target):
            struct.pack_into('<2I', binary, offset, 0x90000000 | register,
                             0x91000000 | target << 10 | register << 5 | register)

        # ObjC thunk calls a type accessor, then the shared Swift gate.
        struct.pack_into('<4I', binary, 0x80, 0x94000004, 0x9400003F,
                         0x12000000, 0xD65F03C0)
        struct.pack_into('<8I', binary, 0xB0,
                         0xA9BF7BFD, 0x910003FD, 0x90000000, 0x91000000,
                         0x94000000, 0xD2800001, 0xA8C17BFD, 0xD65F03C0)
        # The gate keeps 1 in w19 and returns it through `and w0, w19, #1`.
        struct.pack_into('<20I', binary, 0x180,
                         0xD10143FF, 0xA9015FF8, 0xA90257F6, 0xA9034FF4, 0xA9047BFD,
                         0x910103FD, 0xF100D81F, 0x54001D48, 0x52800033, 0x90000008,
                         0x91000108 | (0x400 << 10), 0x10000009, 0xB8A0790A, 0x8B0A0129,
                         0xD61F0120, 0x94000000, 0xAA0003F4, 0x94000000, 0x12000000,
                         analyzer._branch26(0x180 + 76, 0x1E0))
        struct.pack_into('<3I', binary, 0x1D8, 0x52800013, 0x12000260, 0xA9447BFD)
        # Feature 4 already returns through that case.
        struct.pack_into('<55i', binary, 0x400, *([16] * 4 + [0x30] + [16] * 50))
        struct.pack_into('<I', binary, 0x4E0, 0x360002F5)

        # Sticker, theme and shop path prefixes share the matcher at 0x7A0.
        branch = 0x6C8
        struct.pack_into('<4I', binary, branch - 60,
                         0x528E0C28, 0x72A00E08, 0xD2FC6009, 0xA9022408)
        call(branch - 44, 0x780)
        call(branch - 24, 0x7A0)
        call(branch - 20, 0x790)
        struct.pack_into('<I', binary, 0x790, 0xAA0003F5)
        page_address(branch - 16, 0, 0xA80)
        struct.pack_into('<I', binary, branch - 8, 0xAA1303E1)
        call(branch - 4, 0x7A0)
        binary[branch:branch + 16] = bytes.fromhex('95000037600000371400805209000014')
        struct.pack_into('<I', binary, branch + 44, 0x520002B4)
        page_address(branch + 48, 0, 0xB00)
        struct.pack_into('<I', binary, branch + 56, 0xAA1303E1)
        call(branch + 60, 0x7A0)
        struct.pack_into('<2I', binary, branch + 64, 0x2A140008, 0x12000100)
        struct.pack_into('<3I', binary, 0x780, 0x90000000, 0x91240000, 0xD65F03C0)
        struct.pack_into('<QQ', binary, 0x900, 0xD000000000000013, 0x9E0)
        binary[0xA00:0xA14] = b'1359301715-JKd7Y7j1\0'
        binary[0xA30:0xA44] = b'1359301715-lw9jxjqV\0'
        struct.pack_into('<6Q', binary, 0xA90, 2, 4, int.from_bytes(b'app', 'little'),
                         0xE3 << 56, 0xD000000000000013, 0xA10)
        struct.pack_into('<4Q', binary, 0xB10, 1, 2, int.from_bytes(b'shop', 'little'),
                         0xE4 << 56)

        struct.pack_into('<19I', binary, 0xC00,
                         0xB24403BD, 0xD10083FF, 0xA9017BFD, 0xF90007F6, 0x910043FD,
                         0xF9400AC8, 0, 0xD2800000, 0, 0xAA0003F4, 0x52800060,
                         0, 0xF94006C1, 0x12000000, 0xD2800014, 0xA9417BFD,
                         0x9243FBBD, 0x910083FF, 0xD61F0020)
        call(0xC18, 0x100)
        call(0xC20, 0xB0)
        call(0xC2C, 0x180)
        binary = bytes(binary)
        image = analyzer.Image(
            data=binary, image_base=base, text_address=base + 0x80, text_offset=0x80,
            text_size=len(binary) - 0x80,
            function_starts=(
                base + 0x80, base + 0x90, base + 0xB0, base + 0xD0, base + 0x180, base + 0x400,
                base + 0x600, base + 0x768, base + 0x780, base + 0x78C,
                base + 0xC00, base + 0xC4C,
            ),
            segments=((base, len(binary), 0, len(binary)),), header_end=32, text_protection=5,
        )
        methods = [{
            'class': 'UniversalFeatureController',
            'is_class_method': True,
            'selector': 'canUseFeature:',
            'implementation': base + 0x80,
        }]
        return binary, image, methods

    def mocked(self, image, methods):
        return mock.patch.multiple(
            analyzer, parse_image=mock.Mock(return_value=image),
            _objc_methods=mock.Mock(return_value=methods),
            _analyze_native_sticker_routes=mock.Mock(return_value=self.native()),
        )

    def analyze(self, binary, image, methods):
        image = replace(image, data=binary)
        with self.mocked(image, methods):
            result = analyzer.analyze_sticker_shop_executable(binary)
            analyzer._objc_methods.assert_called_once_with(image, 'UniversalFeatureController')
            return result

    def test_patch_only_changes_feature_zero_and_sticker_route(self):
        binary, image, methods = self.sample()
        profile = self.analyze(binary, image, methods)
        self.assertEqual(profile['gate_address'], '0x100000180')
        self.assertEqual(profile['file_offset'], '0x400')
        self.assertEqual(profile['feature'], 0)
        self.assertEqual(profile['liff_route']['file_offset'], '0x6c8')
        self.assertEqual(profile['liff_route']['liff_id'], '1359301715-JKd7Y7j1')
        native = profile['native_routes']
        sites = [profile, profile['liff_route'], native, native['code_cave'],
                 profile['coin_entry']]
        patched, _ = main.apply_patch_sites(binary, sites)
        self.assertEqual(patched[:0x400], binary[:0x400])
        self.assertEqual(patched[0x404:0x4E0], binary[0x404:0x4E0])
        self.assertEqual(patched[0x4E4:0x500], binary[0x4E4:0x500])
        self.assertEqual(patched[0x538:0x6C8], binary[0x538:0x6C8])
        self.assertEqual(patched[0x6CC:0xC28], binary[0x6CC:0xC28])
        self.assertEqual(patched[0xC2C:], binary[0xC2C:])
        self.assertEqual(struct.unpack_from('<i', patched, 0x400)[0], 0x30)
        self.assertEqual(patched[0x4E0:0x4E4].hex(), profile['native_routes']['patched_hex'])
        self.assertEqual(patched[0x500:0x538].hex(), profile['native_routes']['code_cave']['patched_hex'])
        self.assertEqual(patched[0x6C8:0x6CC], main.NOP)
        self.assertEqual(patched[0xC28:0xC2C].hex(), '00008052')
        self.assertEqual(profile['coin_entry']['original_feature'], 3)
        self.assertEqual(profile['coin_entry']['enabled_feature'], 0)
        with self.assertRaisesRegex(ValueError, 'Expected original'):
            main.apply_patch_sites(patched, sites)

    def test_changed_coin_callback_or_targets_are_rejected(self):
        binary, image, methods = self.sample()
        for index in range(19):
            altered = bytearray(binary)
            struct.pack_into('<I', altered, 0xC00 + index * 4, analyzer.NOP)
            with self.subTest(index=index):
                self.assertIsNone(self.analyze(bytes(altered), image, methods))
        for index in range(8):
            altered = bytearray(binary)
            struct.pack_into('<I', altered, 0xB0 + index * 4, analyzer.NOP)
            with self.subTest(metadata_index=index):
                self.assertIsNone(self.analyze(bytes(altered), image, methods))

    def with_coin_copies(self, binary, image, copies):
        altered = bytearray(binary) + bytearray(0x100 * copies)
        starts = image.function_starts
        for copy in range(copies):
            base = 0xD00 + copy * 0x100
            altered[base:base + 0x4C] = binary[0xC00:0xC4C]
            for offset, target in ((0x18, 0x100), (0x20, 0xB0), (0x2C, 0x180)):
                struct.pack_into('<I', altered, base + offset,
                                 analyzer._branch26(base + offset, target, link=True))
            starts += (image.image_base + base, image.image_base + base + 0x4C)
        image = replace(image, text_size=len(altered) - 0x80, function_starts=starts,
                        segments=((image.image_base, len(altered), 0, len(altered)),))
        return bytes(altered), image

    def test_each_coin_side_effect_callback_is_patched(self):
        # 26.19.0 gives each coin side effect its own resume function.
        binary, image, methods = self.sample()
        altered, image = self.with_coin_copies(binary, image, 1)
        coin = self.analyze(altered, image, methods)['coin_entry']
        self.assertEqual((coin['callback_address'], coin['file_offset']), ('0x100000c00', '0xc28'))
        self.assertEqual(coin['additional_sites'], [{
            'callback_address': '0x100000d00', 'file_offset': '0xd28',
            'original_hex': '60008052', 'patched_hex': '00008052',
        }])
        patched, _ = main.apply_patch_sites(altered, [coin, *coin['additional_sites']])
        self.assertEqual(patched[0xC28:0xC2C] + patched[0xD28:0xD2C], bytes.fromhex('00008052') * 2)

    def test_more_or_unaudited_coin_callbacks_are_rejected(self):
        binary, image, methods = self.sample()
        altered, wide = self.with_coin_copies(binary, image, 2)
        self.assertIsNone(self.analyze(altered, wide, methods))
        altered, wide = self.with_coin_copies(binary, image, 1)
        altered = bytearray(altered)
        struct.pack_into('<I', altered, 0xD00 + 15 * 4, analyzer.NOP)
        self.assertIsNone(self.analyze(bytes(altered), wide, methods))

    def outlined_coin_sample(self):
        binary, image, methods = self.sample()
        altered = bytearray(binary)
        altered[0xC00:0xC4C] = bytes(0x4C)
        words = [0xB24403BD, 0xD10083FF, 0xA9017BFD, 0xF90007F6, 0x910043FD,
                 0, 0, 0xAA0003F4, 0x52800060, 0, 0, 0xA9417BFD, 0]
        struct.pack_into('<13I', altered, 0xC00, *words)
        for offset, target in ((0xC14, 0xC40), (0xC18, 0xC48), (0xC24, 0x180), (0xC28, 0xC60)):
            struct.pack_into('<I', altered, offset, analyzer._branch26(offset, target, link=True))
        struct.pack_into('<I', altered, 0xC30, analyzer._branch26(0xC30, 0xC70))
        struct.pack_into('<2I', altered, 0xC40, 0xF9400AC8, analyzer._branch26(0xC44, 0x100))
        struct.pack_into('<2I', altered, 0xC48, 0xD2800000, analyzer._branch26(0xC4C, 0xB0))
        struct.pack_into('<4I', altered, 0xC60, 0xF94006C1, 0x12000000, 0xD2800014, 0xD65F03C0)
        struct.pack_into('<3I', altered, 0xC70, 0x9243FBBD, 0x910083FF, 0xD61F0020)
        image = replace(image, function_starts=tuple(
            start for start in image.function_starts if start != image.image_base + 0xC4C
        ) + tuple(image.image_base + offset for offset in (0xC34, 0xC40, 0xC48, 0xC50, 0xC60, 0xC70, 0xC7C)))
        image = replace(image, function_starts=tuple(sorted(image.function_starts)))
        return bytes(altered), image, methods

    def test_outlined_coin_callback_is_recognized(self):
        binary, image, methods = self.outlined_coin_sample()
        coin = self.analyze(binary, image, methods)['coin_entry']
        self.assertEqual((coin['callback_address'], coin['file_offset']), ('0x100000c00', '0xc20'))
        self.assertNotIn('additional_sites', coin)
        for offset in (0xC1C, 0xC40, 0xC48, 0xC64, 0xC74):
            altered = bytearray(binary)
            struct.pack_into('<I', altered, offset, analyzer.NOP)
            with self.subTest(offset=hex(offset)):
                self.assertIsNone(self.analyze(bytes(altered), image, methods))

    def test_profile_cannot_redirect_liff_patch_to_theme(self):
        binary, image, methods = self.sample()
        profile = self.analyze(binary, image, methods)
        route = {**profile['liff_route'], 'file_offset': '0x6cc'}
        with self.mocked(image, methods), self.assertRaisesRegex(ValueError, 'dispatch mismatch'):
            analyzer._validate_sticker_shop_section(
                {'sticker_shop': {**profile, 'liff_route': route}}, binary,
            )

    def test_theme_section_adds_feature_one_entry_and_theme_liff_guard(self):
        binary, image, methods = self.sample()
        sticker = self.analyze(binary, image, methods)
        with self.mocked(image, methods):
            theme = analyzer.analyze_theme_shop_executable(binary)
        self.assertEqual(theme['feature'], 1)
        self.assertEqual(theme['file_offset'], '0x404')
        self.assertEqual(theme['kind_case'], 2)
        self.assertEqual(theme['liff_route']['file_offset'], '0x6cc')
        self.assertEqual(theme['liff_route']['liff_id'], '1359301715-lw9jxjqV')
        self.assertEqual(theme['code_cave']['file_offset'],
                         sticker['native_routes']['code_cave']['file_offset'])
        native = sticker['native_routes']
        sites = [sticker, sticker['liff_route'], native, theme['code_cave'],
                 sticker['coin_entry'], theme, theme['liff_route']]
        patched, _ = main.apply_patch_sites(binary, sites)
        self.assertEqual(struct.unpack_from('<2i', patched, 0x400), (0x30, 0x30))
        self.assertEqual(patched[0x408:0x4E0], binary[0x408:0x4E0])
        self.assertEqual(patched[0x500:0x540].hex(), theme['code_cave']['patched_hex'])
        self.assertEqual(patched[0x540:0x6C8], binary[0x540:0x6C8])
        self.assertEqual(patched[0x6C8:0x6D0], main.NOP * 2)
        self.assertEqual(patched[0x6D0:0xC28], binary[0x6D0:0xC28])
        with self.assertRaisesRegex(ValueError, 'Expected original'):
            main.apply_patch_sites(patched, sites)

    def test_theme_section_rejects_patched_entry_or_changed_theme_guard(self):
        binary, image, methods = self.sample()
        for offset in (0x404, 0x6CC):
            altered = bytearray(binary)
            struct.pack_into('<I', altered, offset, 32)
            altered = bytes(altered)
            with self.subTest(offset=hex(offset)),                     self.mocked(replace(image, data=altered), methods):
                self.assertIsNone(analyzer.analyze_theme_shop_executable(altered))

    def test_profile_cannot_redirect_theme_patch(self):
        binary, image, methods = self.sample()
        with self.mocked(image, methods):
            theme = analyzer.analyze_theme_shop_executable(binary)
            analyzer._validate_theme_shop_section({}, binary)
            analyzer._validate_theme_shop_section({'theme_shop': theme}, binary)
            for key, value in (
                ('file_offset', '0x400'), ('feature', 0), ('kind_case', 0),
                ('liff_route', {**theme['liff_route'], 'file_offset': '0x6c8'}),
            ):
                with self.subTest(key=key),                         self.assertRaisesRegex(ValueError, 'theme-shop dispatch mismatch'):
                    analyzer._validate_theme_shop_section(
                        {'theme_shop': {**theme, key: value}}, binary,
                    )
            with self.assertRaises(ValueError):
                analyzer._validate_theme_shop_section({'theme_shop': []}, binary)

    def test_missing_ambiguous_or_instance_method_is_rejected(self):
        binary, image, methods = self.sample()
        for candidates in ([], methods * 2, [{**methods[0], 'is_class_method': False}]):
            with self.subTest(candidates=candidates):
                self.assertIsNone(self.analyze(binary, image, candidates))

    def test_modified_switch_or_account_predicate_is_rejected(self):
        binary, image, methods = self.sample()
        for offset in (*range(0x180 + 24, 0x180 + 80, 4), 0x1D8, 0x1DC, 0x1E0):
            altered = bytearray(binary)
            struct.pack_into('<I', altered, offset, analyzer.NOP)
            with self.subTest(offset=hex(offset)):
                self.assertIsNone(self.analyze(bytes(altered), image, methods))
        # No other feature returns through the true case.
        altered = bytearray(binary)
        struct.pack_into('<I', altered, 0x410, 16)
        self.assertIsNone(self.analyze(bytes(altered), image, methods))
        altered = bytearray(binary)
        struct.pack_into('<I', altered, 0x80, analyzer.NOP)
        self.assertIsNone(self.analyze(bytes(altered), image, methods))

    def test_invalid_table_or_already_patched_entry_is_rejected(self):
        binary, image, methods = self.sample()
        for table in (0x181, 0x180, 0x5F0):
            altered = bytearray(binary)
            struct.pack_into('<I', altered, 0x180 + 40, 0x91000108 | (table << 10))
            with self.subTest(table=hex(table)):
                self.assertIsNone(self.analyze(bytes(altered), image, methods))
        altered = bytearray(binary)
        struct.pack_into('<I', altered, 0x400, 32)
        self.assertIsNone(self.analyze(bytes(altered), image, methods))

    def test_profile_cannot_redirect_patch_to_another_feature(self):
        binary, image, methods = self.sample()
        profile = self.analyze(binary, image, methods)
        with self.mocked(image, methods):
            analyzer._validate_sticker_shop_section({}, binary)
            analyzer._validate_sticker_shop_section({'sticker_shop': profile}, binary)
            for key, value in (('file_offset', '0x404'), ('feature', 1),
                               ('patched_hex', '20000000'), ('selector', 'isSecondary'),
                               ('coin_entry', {**profile['coin_entry'], 'file_offset': '0xc2c'})):
                with self.subTest(key=key), self.assertRaisesRegex(ValueError, 'dispatch mismatch'):
                    analyzer._validate_sticker_shop_section(
                        {'sticker_shop': {**profile, key: value}}, binary,
                    )
            with self.assertRaises(ValueError):
                analyzer._validate_sticker_shop_section({'sticker_shop': []}, binary)

class NativeStickerRouteTests(unittest.TestCase):
    def sample(self):
        base = 0x100000000
        binary = bytearray(0x3000)

        def call(offset, target):
            struct.pack_into('<I', binary, offset, analyzer._branch26(offset, target, link=True))

        site = 0x12F0
        struct.pack_into('<14I', binary, site,
                         0, 0x36000340, 0, 0, 0x910182C1, 0, 0, 0xD63F02A0,
                         0, 0x360002F5, 0xF9404AD4, 0xAA1303E0, 0, 0x36000300)
        for offset, target in ((site, 0x1100), (site + 20, 0x1280), (site + 24, 0x1288),
                               (site + 32, 0x1290), (site + 48, 0x1500)):
            call(offset, target)
        # Outlined fragments: load the route closure, keep its result in x21.
        struct.pack_into('<I', binary, 0x1288, 0xA9405295)
        struct.pack_into('<I', binary, 0x1290, 0xAA0003F5)
        struct.pack_into('<7I', binary, 0x1538,
                         0xAA1303E0, 0, 0x12001C16, 0xAA1303E0,
                         0, 0x35000496, 0x12001C08)
        call(0x153C, 0x1600)
        call(0x1548, 0x1700)
        for offset in (0x1610, 0x1710):
            call(offset, 0x1800)
        call(0x1630, 0x1900)
        call(0x1734, 0x1940)
        struct.pack_into('<2I', binary, 0x1634, 0x12001C00, 0x7100101F)
        struct.pack_into('<I', binary, 0x1648, 0x52800000)
        struct.pack_into('<2I', binary, 0x1738, 0x12001C00, 0x7100281F)
        struct.pack_into('<I', binary, 0x174C, 0x52800120)
        string_offset = 0x2500
        for resolver, table, values in (
            (0x1900, 0x2000, ['sticker', 'emoji', 'theme', 'accountRegist']),
            (0x1940, 0x2200, ['new', 'hot', 'event', 'category', 'author', 'detail',
                             'premium', 'download', 'preview', 'invalid']),
        ):
            struct.pack_into('<2I', binary, resolver + 20,
                             0xB0000000, 0x91000000 | ((table & 4095) << 10))
            struct.pack_into('<Q', binary, table + 16, len(values))
            for index, value in enumerate(values):
                struct.pack_into('<QQQ', binary, table + 32 + index * 24,
                                 string_offset, len(value), 2)
                encoded = value.encode() + b'\0'
                binary[string_offset:string_offset + len(encoded)] = encoded
                string_offset += len(encoded)
        binary = bytes(binary)
        image = analyzer.Image(
            data=binary, image_base=base, text_address=base + 0x1000, text_offset=0x1000,
            text_size=0x1000,
            function_starts=(
                base + 0x1100, base + 0x1264, base + 0x1500, base + 0x1600,
                base + 0x1700, base + 0x1800, base + 0x1900, base + 0x1940,
            ),
            segments=((base, len(binary), 0, len(binary)),), header_end=32 + 72,
            text_protection=5,
        )
        return binary, image, {'matcher_address': hex(base + 0x1100)}

    def test_native_guard_uses_original_kind_and_command_resolvers(self):
        binary, image, route = self.sample()
        result = analyzer._analyze_native_sticker_routes(binary, image, route)
        self.assertEqual(result['file_offset'], '0x1314')
        self.assertEqual(result['deny_address'], '0x100001370')
        self.assertEqual(result['original_hex'], 'f5020036')
        self.assertEqual(result['kind_resolver_address'], '0x100001600')
        self.assertEqual(result['command_resolver_address'], '0x100001700')
        self.assertEqual(result['code_cave']['file_offset'], '0xf00')
        code = bytes.fromhex(result['code_cave']['patched_hex'])
        self.assertEqual(len(code), 56)
        self.assertEqual(struct.unpack_from('<I', code)[0], 0x36000055)
        self.assertEqual(struct.unpack_from('<2I', code, 32), (0x7100241F, 0x54000062))
        self.assertEqual(binary, image.data)

    def test_theme_code_cave_allows_the_theme_kind(self):
        binary, image, route = self.sample()
        native = analyzer._analyze_native_sticker_routes(binary, image, route)
        cave = int(native['code_cave']['address'], 16)
        success = int(native['guard_address'], 16) + 4
        deny = int(native['deny_address'], 16)
        code = analyzer.sticker_shop_native_code(native, allow_theme=True)
        words = struct.unpack('<16I', code)

        def conditional(index):
            delta = (words[index] >> 5) & 0x7FFFF
            delta -= 0x80000 if delta & 0x40000 else 0
            return cave + index * 4 + delta * 4, words[index] & 0xFF00001F

        self.assertEqual(code[:20], bytes.fromhex(native['code_cave']['patched_hex'])[:20])
        self.assertEqual(words[5], 0x7100081F)
        self.assertEqual(conditional(6), (cave + 32, 0x54000000))
        self.assertEqual(conditional(7), (cave + 56, 0x35000000))
        self.assertEqual(analyzer._branch_target(cave + 36, words[9]),
                         int(native['command_resolver_address'], 16))
        self.assertEqual(words[10], 0x7100241F)
        self.assertEqual(conditional(11), (cave + 56, 0x54000002))
        self.assertEqual(analyzer._branch_target(cave + 52, words[13]), success)
        self.assertEqual(analyzer._branch_target(cave + 60, words[15]), deny)

    def test_changed_guard_types_commands_or_executable_padding_is_rejected(self):
        binary, image, route = self.sample()
        for offset in (0x1288, 0x1290, 0x1314, 0x1318, 0x131C, 0x1538, 0x1638, 0x1648,
                       0x173C, 0x174C, 0x2500, 0x2000 + 16, 0xF00):
            altered = bytearray(binary)
            struct.pack_into('<I', altered, offset, analyzer.NOP)
            with self.subTest(offset=hex(offset)):
                self.assertIsNone(analyzer._analyze_native_sticker_routes(
                    bytes(altered), replace(image, data=bytes(altered)), route,
                ))
        for fields in ({'text_protection': 7}, {'header_end': 0xEC0}):
            with self.subTest(fields=fields):
                self.assertIsNone(analyzer._analyze_native_sticker_routes(
                    binary, replace(image, **fields), route,
                ))

    def test_branch_generation_rejects_unaligned_and_out_of_range_targets(self):
        for target in (1, -(1 << 27) - 4, 1 << 27):
            with self.subTest(target=target), self.assertRaises(ValueError):
                analyzer._branch26(0, target)



class PrefixLiffRouteTests(unittest.TestCase):
    def sample(self):
        base = 0x100000000
        binary = bytearray(0x1000)
        function, address = 0x100, 0x170

        def word(offset, value):
            struct.pack_into('<I', binary, offset, value)

        def call(offset, target):
            word(offset, analyzer._branch26(offset, target, link=True))

        def address_of(offset, target):
            struct.pack_into('<2I', binary, offset, 0x90000000, 0x91000000 | target << 10)

        def small(text):
            return int.from_bytes(text.encode(), 'little'), (0xE0 | len(text)) << 56

        def large(target):
            return 0xD000000000000013, target - 32

        binary[0x900:0x914] = b'1359301715-JKd7Y7j1\0'
        binary[0x940:0x954] = b'1359301715-lw9jxjqV\0'
        struct.pack_into('<QQ', binary, 0x800, *large(0x900))
        for array, values in ((0xA00, (small('app'), large(0x940))), (0xA80, (small('shop'),))):
            struct.pack_into('<QQ', binary, array + 16, len(values), len(values) * 2)
            for index, value in enumerate(values):
                struct.pack_into('<QQ', binary, array + 32 + index * 16, *value)
        struct.pack_into('<4I', binary, address - 60, 0x528E0C28, 0x72A00E08, 0xD2FC6009, 0xA9022408)
        call(address - 44, 0x600)
        struct.pack_into('<3I', binary, 0x600, 0x90000000, 0x91000000 | 0x800 << 10, 0xD65F03C0)
        call(address - 24, 0x700)
        call(address - 20, 0x680)
        word(0x680, 0xAA0003F5)
        address_of(address - 16, 0xA00)
        word(address - 8, 0xAA1303E1)
        call(address - 4, 0x700)
        struct.pack_into('<4I', binary, address, 0x37000095, 0x37000060, 0x52800014, 0x14000009)
        word(address + 44, 0x520002B4)
        address_of(address + 48, 0xA80)
        word(address + 56, 0xAA1303E1)
        call(address + 60, 0x700)
        struct.pack_into('<2I', binary, address + 64, 0x2A140008, 0x12000100)
        binary = bytes(binary)
        image = analyzer.Image(
            data=binary, image_base=base, text_address=base + 0x80, text_offset=0x80,
            text_size=0x780,
            function_starts=(base + 0x80, base + function, base + 0x200, base + 0x600,
                             base + 0x680, base + 0x700),
            segments=((base, len(binary), 0, len(binary)),), header_end=32, text_protection=5,
        )
        return binary, image

    def test_sticker_guard_precedes_the_theme_prefix_guard(self):
        binary, image = self.sample()
        route = analyzer._analyze_sticker_liff_route(binary, image)
        self.assertEqual(route, {
            'kind': 'sticker_liff_fallback', 'scheme': 'LineShopScheme',
            'liff_id': '1359301715-JKd7Y7j1', 'theme_liff_id': '1359301715-lw9jxjqV',
            'matcher_address': '0x100000100', 'file_offset': '0x170',
            'original_hex': '95000037', 'patched_hex': '1f2003d5',
        })

    def test_changed_prefixes_ids_or_result_are_rejected(self):
        binary, image = self.sample()
        for offset in (0x170 - 60, 0x170 - 20, 0x680, 0x170 - 8, 0x170 + 8, 0x170 + 44,
                       0x170 + 56, 0x170 + 64, 0x170 + 68, 0x600, 0x800, 0x900, 0x940,
                       0xA20, 0xA80 + 32, 0xA80 + 16):
            altered = bytearray(binary)
            struct.pack_into('<I', altered, offset, 0x41414141)
            with self.subTest(offset=hex(offset)):
                self.assertIsNone(analyzer._analyze_sticker_liff_route(
                    bytes(altered), replace(image, data=bytes(altered)),
                ))
        altered = bytearray(binary)
        altered[0x300:0x310] = binary[0x170:0x180]
        self.assertIsNone(analyzer._analyze_sticker_liff_route(
            bytes(altered), replace(image, data=bytes(altered)),
        ))

if __name__ == '__main__':
    unittest.main()
