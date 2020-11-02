def test_version(cmd):
    assert cmd.get_version() == (1, 0, 0)


def test_app_name(cmd):
    assert cmd.get_app_name() == "Boilerplate"
