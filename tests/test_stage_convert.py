import pytest

from phantomcrash_research.stage_convert import colour_meshes


def test_city_slots_use_resource_base_and_exclude_static_volumes():
    # Shinjuku's object list begins at 91, but resource 186 begins at 31.
    meshes, sky = colour_meshes([(91, 9), (134, 7), (143, 1), (145, 2), (185, 8)], 31, 155)
    assert meshes == {60, 112, 114, 154}
    assert sky == ["mesh112_node0"]


@pytest.mark.parametrize("slot", [30, 186])
def test_stage_object_outside_resource_is_rejected(slot):
    with pytest.raises(ValueError, match="outside"):
        colour_meshes([(slot, 2)], 31, 155)
