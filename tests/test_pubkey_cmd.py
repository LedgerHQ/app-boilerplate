from boilerplate_client.boilerplate_cmd import BoilerplateCommand

def test_get_public_key(backend, firmware):
    client = BoilerplateCommand(backend)
    pub_key, chain_code = client.get_public_key(
        bip32_path="m/44'/0'/0'/0/0",
        display=False
    )  # type: bytes, bytes

    assert len(pub_key) == 65
    assert len(chain_code) == 32

    pub_key2, chain_code2 = client.get_public_key(
        bip32_path="m/44'/1'/0'/0/0",
        display=False
    )  # type: bytes, bytes

    assert len(pub_key2) == 65
    assert len(chain_code2) == 32
