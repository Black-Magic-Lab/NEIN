#!/usr/bin/env python3
"""Report coarse LINE ad activity from the local macOS unified log."""

import argparse
import json
import subprocess


MARKERS = {
    'request': ('[AdvertisementManager][LAD] requestAdvertisements',
                '[AdvertisementService][LAD] requestAdvertisements.'),
    'install_attempt': ('[AdvertisementPanel][LAD] installAdvertisement =>',),
    'invalid_or_expired': ('[AdvertisementPanel][LAD] onAdvertisementsUpdated adContent is not valid.',
                           '[AdvertisementPanel][LAD] adContent is expired',
                           '[VideoAdvertisementPanel][LAD] installAdvertisement. content is invalid.'),
}


def summarize(lines):
    counts = {key: 0 for key in MARKERS}
    for line in lines:
        for key, markers in MARKERS.items():
            if any(marker in line for marker in markers):
                counts[key] += 1
    # An installation attempt is not proof of a successful render; absent
    # messages are not proof of absence, as LINE may log elsewhere or redact.
    return {
        'ad_presence': 'unknown',
        'events': counts,
        'note': ('These logs cannot prove whether an ad was delivered or rendered. '
                 'No events can also mean that LINE did not emit accessible log messages.'),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--minutes', type=int, default=30)
    args = parser.parse_args()
    if not 1 <= args.minutes <= 1440:
        parser.error('--minutes must be between 1 and 1440')
    result = subprocess.run([
        '/usr/bin/log', 'show', '--last', f'{args.minutes}m', '--style', 'compact',
        '--info', '--predicate', 'process == "LINE" AND eventMessage CONTAINS "[LAD]"',
    ], text=True, capture_output=True, check=True)
    print(json.dumps(summarize(result.stdout.splitlines()), indent=2))


if __name__ == '__main__':
    main()
