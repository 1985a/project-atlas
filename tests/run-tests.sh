#!/usr/bin/env bash
set -e

cd "$(dirname "$0")/.."

# TEST_CC permite compilar las pruebas con otro compilador o con
# sanitizadores (lo usa `make test-asan`).
make -C tests clean >/dev/null
make -C tests ${TEST_CC:+CC="$TEST_CC"}

passed=0
failed=0

run_test() {
    local name="$1"

    echo
    echo "Running $name..."
    echo

    # Aislar del usuario real: configuración y estado en un directorio temporal
    local tmp
    tmp=$(mktemp -d /tmp/atlas-run-XXXXXX)
    if ATLAS_CONFIG_DIR="$tmp/config" ATLAS_STATE_DIR="$tmp/state" "./tests/$name"; then
        echo "$name PASS"
        passed=$((passed + 1))
    else
        echo "$name FAIL"
        failed=$((failed + 1))
    fi
    rm -rf "$tmp"
}

run_test test_vector
run_test test_string
run_test test_runtime
run_test test_backend
run_test test_profile
run_test test_profile_apply
run_test test_probe
run_test test_hwdb
run_test test_auto
run_test test_base
run_test test_steam
run_test test_conf

echo
echo "=========================="
echo "Summary"
echo "=========================="
echo "Passed : $passed"
echo "Failed : $failed"

if [ "$failed" -ne 0 ]; then
    exit 1
fi
