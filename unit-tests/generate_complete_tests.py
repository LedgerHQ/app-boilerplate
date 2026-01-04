#!/usr/bin/env python3
"""
Generate complete test files with the tx_fixture_t + run_fixture pattern.
"""
# Requires the ragger venv (e.g., tests/standalone/venv) on PYTHONPATH or activated.

import re
from pathlib import Path

BASE_PATH = Path('/home/jan/praca/vacuumlabs/cardano/ledger-app-cardano/unit-tests')

ERAS = {
    'byron': ('test_sign_tx_fixtures_byron.h', 'test_sign_tx_byron.c', 'BYRON'),
    'shelley': ('test_sign_tx_fixtures_shelley.h', 'test_sign_tx_shelley.c', 'SHELLEY'),
    'mary': ('test_sign_tx_fixtures_mary.h', 'test_sign_tx_mary.c', 'MARY'),
    'allegra': ('test_sign_tx_fixtures_allegra.h', 'test_sign_tx_allegra.c', 'ALLEGRA'),
    'alonzo': ('test_sign_tx_fixtures_alonzo.h', 'test_sign_tx_alonzo.c', 'ALONZO'),
    'babbage': ('test_sign_tx_fixtures_babbage.h', 'test_sign_tx_babbage.c', 'BABBAGE'),
    'alonzo_catalyst': ('test_sign_tx_fixtures_alonzo_catalyst.h', 'test_sign_tx_alonzo_catalyst.c', 'ALONZO_CATALYST'),
    'alonzo_cip36': ('test_sign_tx_fixtures_alonzo_cip36.h', 'test_sign_tx_alonzo_cip36.c', 'ALONZO_CIP36'),
    'conway': ('test_sign_tx_fixtures_conway.h', 'test_sign_tx_conway.c', 'CONWAY'),
    'conway_voting': ('test_sign_tx_fixtures_conway_voting.h', 'test_sign_tx_conway_voting.c', 'CONWAY_VOTING'),
    'conway_without_certificates': ('test_sign_tx_fixtures_conway_without_certificates.h',
                                    'test_sign_tx_conway_without_certificates.c',
                                    'CONWAY_WITHOUT_CERTIFICATES'),
    'shelley_certificates': ('test_sign_tx_fixtures_shelley_certificates.h', 'test_sign_tx_shelley_certificates.c', 'SHELLEY_CERTIFICATES'),
    'multisig': ('test_sign_tx_fixtures_multisig.h', 'test_sign_tx_multisig.c', 'MULTISIG'),
}

ERA_COMMENT_OVERRIDES = {
    'conway_without_certificates': 'CONWAY_WITHOUT_CERTIFICATES Era Tests',
    'alonzo_catalyst': 'ALONZO_CATALYST Era Tests',
    'alonzo_cip36': 'ALONZO_CIP36 Era Tests',
}


def sanitize_test_name(name: str) -> str:
    """Return a snake_case suffix (= without the leading test_) from a fixture name."""
    lower = name.lower()
    cleaned = re.sub(r'[^a-z0-9_]+', '_', lower)
    cleaned = re.sub(r'_+', '_', cleaned).strip('_')
    return cleaned


def extract_fixtures_from_header(fixture_path: Path):
    """Parse fixtures from the header and preserve their order."""
    content = fixture_path.read_text()
    fixtures = []
    pattern = re.compile(r'static const tx_fixture_t (FIXTURE_[A-Z0-9_]+)\s*=\s*\{(.*?)\};', re.S)
    for match in pattern.finditer(content):
        fixture_name = match.group(1)
        body = match.group(2)
        name_match = re.search(r'\.name\s*=\s*"([^"]+)"', body)
        if not name_match:
            continue
        display_name = name_match.group(1)
        fixtures.append((fixture_name, display_name))
    return fixtures


def build_test_functions(fixtures):
    """Create function definitions and keep their C identifiers."""
    functions = []
    names = []
    for fixture_name, display_name in fixtures:
        func_suffix = sanitize_test_name(display_name)
        assert func_suffix, f"unable to sanitise fixture name {display_name}"
        test_name = f"test_{func_suffix}"
        for suffix, expert_flag in [("expert_off", "false"), ("expert_on", "true")]:
            function_name = f"{test_name}_{suffix}"
            functions.append(
                f"static void {function_name}(void **state) {{\n"
                f"    (void) state;\n"
                f"    run_fixture_with_expert_mode(&{fixture_name}, {expert_flag});\n"
                f"}}"
            )
            names.append(function_name)
    return functions, names


def build_main_function(test_names, test_c_file):
    """Return the main() function string that registers every test."""
    registrations = ',\n        '.join(f"cmocka_unit_test({name})" for name in test_names)
    return (
        "// ======================================================================\n"
        "// Main\n"
        "// ======================================================================\n\n"
        "int main(void) {\n"
        "    const struct CMUnitTest tests[] = {\n"
        f"        {registrations},\n"
        "    };\n"
        f"    return _cmocka_run_group_tests(\"{Path(test_c_file).stem}\", tests, ARRAY_LEN(tests), NULL, NULL);\n"
        "}\n"
    )


def generate_complete_test_file(era, fixture_file, test_c_file, era_upper):
    """Generate the tests/main section for a single era."""
    fixture_path = BASE_PATH / fixture_file
    test_path = BASE_PATH / test_c_file
    existing = test_path.read_text()

    era_block_match = re.search(r'^// =+\n// ([^\n]+ Era Tests)\n// =+\n', existing, re.MULTILINE)
    if era_block_match:
        boilerplate = existing[:era_block_match.start()]
        era_heading = era_block_match.group(1)
    else:
        placeholder_match = re.search(r'^// Placeholder test - actual tests are generated from fixtures', existing, re.MULTILINE)
        if not placeholder_match:
            print(f"Could not find test section marker in {test_c_file}")
            return False
        boilerplate = existing[:placeholder_match.start()]
        era_heading = ERA_COMMENT_OVERRIDES.get(era, f"{era_upper} Era Tests")

    fixtures = extract_fixtures_from_header(fixture_path)
    if not fixtures:
        print(f"No fixtures found in {fixture_file}")
        return False

    test_functions, test_names = build_test_functions(fixtures)
    tests_block = "\n\n".join(test_functions)
    main_block = build_main_function(test_names, test_c_file)

    era_comment_block = (
        "// ======================================================================\n"
        f"// {era_heading}\n"
        "// ======================================================================\n\n"
    )

    complete_file = (
        boilerplate.rstrip()
        + "\n\n"
        + era_comment_block
        + tests_block
        + "\n\n"
        + main_block
    )

    test_path.write_text(complete_file)
    print(f"Generated {test_c_file}: {len(fixtures)} tests")
    return True


if __name__ == "__main__":
    for era, (fixture_file, test_c_file, era_upper) in ERAS.items():
        generate_complete_test_file(era, fixture_file, test_c_file, era_upper)

    print("\nAll test files generated successfully!")
