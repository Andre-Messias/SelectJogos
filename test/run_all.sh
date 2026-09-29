#!/bin/bash

TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

chmod +x "$TEST_DIR"/*.sh

echo "========================================================="
echo " SUITE 1: LOCAL & REMOTE MODES INTEGRATION TEST"
echo "========================================================="
"$TEST_DIR/test_both_modes.sh" || exit 1

echo ""
echo "========================================================="
echo " SUITE 2: RULES, SECURITY, AND RECONNECTION TEST"
echo "========================================================="
"$TEST_DIR/test_rules_and_errors.sh" || exit 1

echo ""
echo -e "\033[0;32m>>> ALL TEST SUITES PASSED SUCCESSFULLY! <<<\033[0m"