from pathlib import Path

import pytest

from phantomcrash_research import town_tables

EXTRACTED_XBE = Path("artifacts/europe/files/default.xbe")


@pytest.mark.skipif(not EXTRACTED_XBE.is_file(), reason="needs the extracted default.xbe")
def test_town_tables_read_the_classes_contests_and_calendar():
    town = town_tables.town_tables(EXTRACTED_XBE.read_bytes())
    assert len(town["classes"]) == town_tables.CLASS_COUNT
    holy = town["classes"][5]
    assert (holy["name"], holy["regulation"], holy["fee"], holy["maker"]) == (
        "Holy Restricted",
        "Holy Only",
        1000,
        0,
    )
    assert town["classes"][12]["legs"] == [3, 5, 4, 6]
    assert town["classes"][27]["other"] == 1  # no Optic Camouflage
    farenheit = town["contests"][3]
    assert (farenheit["name"], farenheit["class"], farenheit["night"]) == ("Farenheit 451", 1, False)
    # 0x045270: the extra team at +0x1c, the teams entered from +0x20.
    assert (farenheit["extra_team"], farenheit["teams"]) == (2, [3, 4, 5, 6])
    assert (farenheit["levels"], farenheit["reserve_after"], farenheit["roster_size"]) == (
        [0, 99],
        4,
        6,
    )
    assert town["contests"][49]["champion"] == 0  # Yuki, the First Ranker
    assert town["teams"][3] == {"leader": 158, "members": [159, 160, 161] + [-1] * 5}
    assert len(town["entrants"]) == town_tables.ENTRANT_COUNT
    assert town["entrants"][0] == {"pilot": 1, "robot": 6, "word_4": 11, "level": 65}
    assert town["contests"][7]["night"]
    assert len(town["calendar"]) == town_tables.DAYS
    assert town["calendar"][0]["venues"][:3] == [6, 24, 49]
    assert town["calendar"][4]["venues"][1] == -1
    day = town["calendar"][0]
    assert (day["chips"], day["modules"], day["used_svs"]) == (0, [0, 12, 40, 58, 76, 100, 112], 0)
