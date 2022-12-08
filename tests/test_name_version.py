from boilerplate_client.boilerplate_cmd import BoilerplateCommand


def test_get_app_and_version(backend, backend_name, firmware):
    client = BoilerplateCommand(backend)
    # for now, Speculos always returns app:1.33.7, so this test is not very meaningful
    # however, its failure will mean that Speculos may have been improved on this front
    app_name, version = client.get_app_and_version()

    if backend_name == "speculos":
        assert app_name == "app"
        assert version == "1.33.7"
    else:
        assert app_name == "Boilerplate"
        assert version == "1.0.1"
