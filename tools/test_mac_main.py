import struct
import hashlib
import unittest
from unittest import mock
import zlib

import mac_main
import macho


class MacMainTests(unittest.TestCase):
    def test_collapses_only_audited_chat_layout_in_both_slices(self):
        anchor = b'"chatTabAdvertisementPanel.top"'
        panel = (b'chatTabAdvertisementPanel(AdvertisementPanel) : {\n'
                 b'                        anchorLeft : "parent.left"\n'
                 b'                        anchorBottom : "allUnreadsBox.top"')
        friend = (b'friendTabAdvertisementPanel(AdvertisementPanel) : {\n'
                  b'                        anchorLeft : "parent.left"\n'
                  b'                        anchorBottom : "parent.bottom"')
        layout = ((anchor + b'\n') * 3 + panel + friend +
                  b'"friendTabAdvertisementPanel.top"' + b'"allUnreadsBox.top"' * 20 +
                  b'"parent.bottom"' * 20 + b'\n                        bottomMargin : -1000' * 20)
        layout += b' ' * (62979 - len(layout))
        compressed = zlib.compress(layout, 9)
        resource = struct.pack('>II', len(compressed) + 4, len(layout)) + compressed
        data = b'prefix' + resource + b'middle' + resource + b'suffix'
        with mock.patch.object(mac_main, 'MAIN_LAYOUT_SHA256', hashlib.sha256(layout).hexdigest()):
            result = mac_main.collapse_chat_ad_layout(data)
            with self.assertRaises(ValueError):
                mac_main.collapse_chat_ad_layout(b'prefix' + resource)
        self.assertEqual(len(data), len(result))
        pos = result.index(b'\x78\xda')
        size = struct.unpack_from('>I', result, pos - 8)[0]
        patched = zlib.decompress(result[pos:pos - 4 + size])
        self.assertEqual(patched.count(b'"allUnreadsBox.top"'), 24)
        self.assertNotIn(anchor, patched)
        self.assertIn(panel + b'\n                        bottomMargin : -1000', patched)
        self.assertIn(friend + b'\n                        bottomMargin : -1000', patched)
        self.assertNotIn(b'"friendTabAdvertisementPanel.top"', patched)

    def make_slice(self, command_count=1, command_size=152, section_offset=512):
        data = bytearray(1024)
        struct.pack_into('<I', data, 0, 0xFEEDFACF)
        struct.pack_into('<II', data, 16, command_count, command_size)
        # LC_SEGMENT_64 with one section and enough zero-filled header slack.
        struct.pack_into('<II', data, 32, 0x19, 72 + 80)
        struct.pack_into('<I', data, 32 + 64, 1)
        struct.pack_into('<I', data, 32 + 72 + 48, section_offset)
        return bytes(data)

    def test_injects_thin_slice_without_changing_size(self):
        source = self.make_slice()
        patched = mac_main.add_dylib_to_universal(source)
        self.assertEqual(len(source), len(patched))
        self.assertEqual(struct.unpack_from('<I', patched, 16)[0], 2)
        self.assertIn(mac_main.HOOK_LOAD_PATH.encode(), patched)
        with self.assertRaisesRegex(ValueError, 'already referenced'):
            macho.add_dylib(patched, mac_main.HOOK_LOAD_PATH)

    def test_injects_each_universal_slice(self):
        first = self.make_slice()
        second = self.make_slice()
        header_size = 8 + 2 * 20
        first_offset = header_size
        second_offset = first_offset + len(first)
        header = struct.pack('>II', 0xCAFEBABE, 2)
        header += struct.pack('>iiIII', 0x0100000C, 0, first_offset, len(first), 2)
        header += struct.pack('>iiIII', 0x01000007, 3, second_offset, len(second), 2)
        source = header + first + second
        patched = mac_main.add_dylib_to_universal(source)
        self.assertEqual(len(source), len(patched))
        self.assertEqual(struct.unpack_from('<I', patched, first_offset + 16)[0], 2)
        self.assertEqual(struct.unpack_from('<I', patched, second_offset + 16)[0], 2)


if __name__ == '__main__':
    unittest.main()
