"""Complete imported robot records for the existing Fill Hangars cheat."""

import copy
import json

from .weapon_balance import value

TABLES = {'body': 'bodies', 'leg': 'legs', 'arm': 'arm_weapons', 'shoulder': 'shoulder_weapons'}


def _at_level_zero(parts, category, part, field):
    records = parts.get(TABLES[category], [])
    entry = records[part].get(field) if part < len(records) else None
    return value(entry, 0) if isinstance(entry, dict) else float(entry or 0)


def _legs_that_carry(legs, carried, parts):
    """The family's first legs that carry the rest of the robot at level 0
    without overload (no Phantom Crash starter is overloaded), else the
    legs that come closest."""
    def spare(leg):
        return (_at_level_zero(parts, 'leg', leg['part'], 'capacity')
                - _at_level_zero(parts, 'leg', leg['part'], 'weight') - carried)
    return next((leg for leg in legs if spare(leg) >= 0), max(legs, key=spare))


def add_cheat_robots(garage, cheats, registered, parts):
    """Keep the configured Phantom Crash order, then one of each SLAI body.

    Generated records carry their source model so repeated registration updates
    the same indices. These are native starter loadouts, not recovered campaign
    robot records. Weapons and legs come from the body's own source family:
    the first two arms and shoulders, on the first legs that carry them.
    """
    garage, cheats = copy.deepcopy(garage), copy.deepcopy(cheats)
    records = garage['robots']
    existing = {r['slai_model']: i for i, r in enumerate(records) if r.get('slai_model')}
    base_order = [i for i in cheats['hangar_robots'] if i not in existing.values()]
    template = next((records[i] for i in base_order if 0 <= i < len(records)
                     and records[i]['parts'].get('chip')), None)
    if template is None:
        raise ValueError('Fill Hangars needs a Phantom Crash starter with a CHIP')
    indices = []
    for body in sorted((p for p in registered if p['category'] == 'body'),
                       key=lambda p: p['model']):
        family = body['model'][1]
        group = sorted((p for p in registered if p['model'][1] == family),
                       key=lambda p: p['model'])
        select = lambda kind: [p for p in group if p['category'] == kind]
        legs, arms, shoulders = select('leg'), select('arm'), select('shoulder')
        if not legs:
            raise ValueError(f"S.L.A.I. body {body['model']} has no registered matching legs")
        slots = {'body': {'id': body['part'], 'level': 0}}
        carried = _at_level_zero(parts, 'body', body['part'], 'weight')
        for name, weapons in (('arm', arms), ('shoulder', shoulders)):
            for side, weapon in zip(('right', 'left'), weapons[:2]):
                slots[f'{side}_{name}'] = {'id': weapon['part'], 'level': 0}
                carried += _at_level_zero(parts, name, weapon['part'], 'weight')
        slots['legs'] = {'id': _legs_that_carry(legs, carried, parts)['part'], 'level': 0}
        slots['chip'] = copy.deepcopy(template['parts']['chip'])
        label = parts['bodies'][body['part']]['name'].removeprefix('S.L.A.I. ').removesuffix(' Body')
        record = {'name': 'SLAI ' + label, 'slai_model': body['model'],
                  'parts': slots, 'modules': [], 'paint': template['paint'],
                  'emblem': template['emblem']}
        index = existing.get(body['model'], len(records))
        if index == len(records):
            records.append(record)
        else:
            records[index] = record
        indices.append(index)
    cheats['hangar_robots'] = base_order + indices
    return garage, cheats, indices


def register_cheat_robots(data, mods, registered, parts):
    from .assets import save
    garage_path = mods / 'tables/garage.json'
    cheat_path = mods / 'data/debug/cheats.json'
    base = cheat_path if cheat_path.exists() else data / 'debug/cheats.json'
    garage = json.loads(garage_path.read_text(encoding='utf-8'))
    cheats = json.loads(base.read_text(encoding='utf-8'))
    # Progression regenerates the base garage, so previous generated robot
    # indices can be outside this fresh table. Remove them before appending.
    old = set(cheats.get('slai_hangar_robots', []))
    cheats['hangar_robots'] = [i for i in cheats['hangar_robots'] if i not in old]
    garage, cheats, indices = add_cheat_robots(garage, cheats, registered, parts)
    cheats['slai_hangar_robots'] = indices
    save(garage_path, garage)
    save(cheat_path, cheats)
    return len(indices)
