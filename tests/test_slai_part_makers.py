"""S.L.A.I. model files take the maker whose resource folders hold them."""

from types import SimpleNamespace

from phantomcrash_research.slai.native_parts import FOLDERS, SOURCE_MAKERS, source_records

# The disc's body folders, as listed from PC2.STM: each holds one family's
# body beside that family's legs.
FOLDER_FAMILIES = {"HOLY": 3, "PHOTON": 1, "AERON": 5, "BRINEMAN": 2, "KUBEL": 4}


class Disc:
    def __init__(self, packages):
        self._packages = packages

    def packages(self):
        return self._packages


def file(name):
    return SimpleNamespace(name=name)


def test_each_family_maps_to_the_maker_whose_folders_hold_it():
    for maker, folder in FOLDERS.items():
        assert SOURCE_MAKERS[FOLDER_FAMILIES[folder]] == maker


def test_a_body_its_legs_and_its_weapons_share_one_maker():
    # KHT (maker 3) keeps its 2-legs model in BRINEMAN2LEGS beside S2B01H.
    disc = Disc({100: [file("S2B01H.PO2"), file("S2L01H.PO2")]})
    resources = {"Z/M/BRINEMAN2LEGS": 100}
    stats = {"bodies": [{"maker": 2, "model": 4, "name": "HARTMAN"},
                        {"maker": 3, "model": 1, "name": "KHT"}],
             "legs": [{"maker": 2, "model": 12, "name": "HARTMAN-2Legs"},
                      {"maker": 3, "model": 18, "name": "KHT-2Legs"}],
             "arm_weapons": [{"maker": 2, "model": 18, "name": "SV Drill"},
                             {"maker": 3, "model": 26, "name": "CP42"}],
             "shoulder_weapons": []}
    models = [{"source": "S2B01H.PO2", "category": "body"},
              {"source": "S2L01H.PO2", "category": "leg"},
              {"source": "S2A03H.PO2", "category": "arm"},
              {"source": "S5A03H.PO2", "category": "arm"}]
    records = source_records(stats, models, resources, disc)
    assert records["S2B01H"]["name"] == "KHT"
    assert records["S2L01H"]["name"] == "KHT-2Legs"
    assert records["S2A03H"]["name"] == "CP42"
    # AERONA_DRILL holds S5A03H: HARTMAN's drill.
    assert records["S5A03H"]["name"] == "SV Drill"
