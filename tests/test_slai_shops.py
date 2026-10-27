"""S.L.A.I.'s shops in an installed pack (slai/shop_table.py): every lineup
the table names exists in the pack's garage and holds only S.L.A.I.'s
records, and every shop SV has a CHIP."""

from pathlib import Path
import json

import pytest

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / 'artifacts/slai/assets'
SHOPS = 'artifacts/slai/assets/tables/slai_shops.json'
CATEGORIES = ('body', 'legs', 'arm', 'shoulder', None, 'paint', 'emblem')


def _read(relative):
    return json.loads((PACK / relative).read_text(encoding='utf-8'))


@pytest.mark.private_inputs(SHOPS)
def test_installed_shop_lineups_hold_only_slai_records():
    table, garage = _read('tables/slai_shops.json'), _read('tables/garage.json')
    parts = _read('tables/parts.json')
    stock = table['slai_stock']
    bases = table['bases']
    assert stock['chip'] == [bases['chips'], len(parts['chips'])]

    def held(kind, ids):
        first, end = stock[kind]
        return all(first <= id < end for id in ids)

    for row in table['city_days']:
        if row['chips'] >= 0:
            assert bases['chip_lineups'] <= row['chips'] < len(garage['chip_lineups'])
            assert held('chip', [c['id'] for c in garage['chip_lineups'][row['chips']]])
        for kind, lineup in zip(CATEGORIES, row['modules']):
            if kind is None:
                assert lineup == -1  # S.L.A.I.'s options are not the port's modules
            elif lineup >= 0:
                assert bases['module_lineups'] <= lineup < len(garage['module_lineups'])
                assert held(kind, [p['id'] for p in garage['module_lineups'][lineup]])
        if row['used_svs'] >= 0:
            assert bases['used_sv_lineups'] <= row['used_svs'] < len(garage['used_sv_lineups'])
    robots = garage['robots']
    sold = [entry['robot'] for lineup in garage['dealer_lineups'][bases['dealer_lineups']:]
            for entry in lineup]
    sold += [robot for lineup in garage['used_sv_lineups'][bases['used_sv_lineups']:]
             for robot in lineup]
    for robot in sold:
        parts_of = robots[robot]['parts']
        assert held('body', [parts_of['body']['id']])
        assert held('chip', [parts_of['chip']['id']])


@pytest.mark.private_inputs(SHOPS)
def test_installed_junk_and_dealers():
    table = _read('tables/slai_shops.json')
    junk = table['junk']
    assert len(junk['cycle']) == len(table['cities'])
    assert all(0 <= row['sv_row'] < len(junk['sv_lineups']) for row in junk['rows'])
    assert all(-1 <= row['junk'] < len(junk['rows']) for row in table['city_days'])
    assert {name: dealer['shop'] for name, dealer in table['dealers'].items()} == {
        'dealer_omsk': 3, 'dealer_ma_fabrik': 4}
    assert all(len(dealer['profile']) == 11 for dealer in table['dealers'].values())
