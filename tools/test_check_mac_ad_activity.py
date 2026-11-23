import unittest

from check_mac_ad_activity import summarize


class AdActivityTests(unittest.TestCase):
    def test_empty_log_is_unknown(self):
        self.assertEqual(summarize([])['ad_presence'], 'unknown')

    def test_attempt_is_not_a_render(self):
        result = summarize(['[AdvertisementPanel][LAD] installAdvertisement => example'])
        self.assertEqual(result['events']['install_attempt'], 1)
        self.assertEqual(result['ad_presence'], 'unknown')

    def test_uninstall_is_not_counted_as_install(self):
        result = summarize(['[AdvertisementPanel][LAD] uninstallAdvertisement => example'])
        self.assertEqual(result['events']['install_attempt'], 0)


if __name__ == '__main__':
    unittest.main()
