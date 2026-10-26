"""S.L.A.I.'s opponents as the port fights them (slai/opponent_table.py)."""

import struct

from phantomcrash_research.slai.opponent_table import (AIM_KIND, EMBLEM_NONE, WEAPON_CLASS,
                                                       _check, _robot_record, part_facts)


def part(port_part, level=1):
    return {"port_part": port_part, "level": level}


def test_a_robot_record_puts_each_weapon_on_the_port_slot_test_drive_fits():
    build = {"body": part(10, 3), "legs": part(20, 2),
             "arms": [part(30), None], "shoulders": [None, part(40, 4)]}
    record = _robot_record(None, build, "ZEPHYR")
    assert record == {"name": "ZEPHYR", "modules": [], "paint": 0, "emblem": EMBLEM_NONE,
                      "parts": {"body": {"id": 10, "level": 3}, "legs": {"id": 20, "level": 2},
                                "right_arm": {"id": 30, "level": 1},
                                "left_shoulder": {"id": 40, "level": 4}}}


def test_parts_the_port_has_not_imported_are_left_off():
    build = {"body": part(None), "legs": part(20), "arms": [part(None), None],
             "shoulders": [None, None]}
    record = _robot_record(None, build, None)
    assert record["name"] == ""
    assert record["parts"] == {"legs": {"id": 20, "level": 1}}


class Rules:
    def body_maker(self, raw):
        return 3

    def leg_type(self, raw):
        return 7

    def _weapon(self, slot, part_id):
        record = bytearray(312)
        struct.pack_into("<h", record, WEAPON_CLASS, 2 if slot < 2 else 15)
        record[AIM_KIND] = 9
        return bytes(record)


def test_the_check_carries_what_the_opponent_check_reads():
    raw = {"arms": [{"id": 1}, None], "shoulders": [None, {"id": 5}], "options": [], "cpu": 1}
    check = _check(Rules(), None, raw)
    assert check == {"maker": 3, "leg_type": 7, "arms": [{"class": 2, "aim": 9}, None],
                     "shoulders": [None, {"class": 15, "aim": 9}], "option_points": 0, "cpu": 1}


def test_part_facts_answer_the_entry_check_by_port_part():
    registration = {"parts": [
        {"part": 77, "category": "shoulder", "source_record": 4},
        {"part": 76, "category": "arm", "source_record": 0},
        {"part": 43, "category": "leg", "source_record": 0},
        {"part": 16, "category": "body", "source_record": 0},
        {"part": None, "category": "body", "source_record": 1}]}
    assert part_facts(Rules(), registration) == {
        "bodies": [{"part": 16, "maker": 3}], "legs": [{"part": 43, "leg_type": 7}],
        "arm_weapons": [{"part": 76, "class": 2, "aim": 9}],
        "shoulder_weapons": [{"part": 77, "class": 15, "aim": 9}]}
