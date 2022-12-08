from boilerplate_client.boilerplate_cmd import BoilerplateCommand

def test_app_name(backend, firmware):
    client = BoilerplateCommand(backend)
    assert client.get_app_name() == "Boilerplate"
