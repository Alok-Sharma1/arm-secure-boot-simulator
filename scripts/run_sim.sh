#!/usr/bin/env bash
# =============================================================================
# run_sim.sh -- Build and run the ARM Secure Boot Simulator
#
# Usage:
#   ./scripts/run_sim.sh          -- build + run simulation
#   ./scripts/run_sim.sh test     -- build + run unit tests
#   ./scripts/run_sim.sh clean    -- clean build artifacts
# =============================================================================

set -euo pipefail

# Change to the project root (one level above this script)
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

ACTION="${1:-sim}"

case "$ACTION" in
    sim)
        echo "========================================"
        echo "  ARM Cortex-M4 Secure Boot Simulator"
        echo "========================================"
        echo ""
        echo "[1/2] Building simulation binary..."
        make sim
        ;;

    test)
        echo "========================================"
        echo "  Running Unit Tests"
        echo "========================================"
        make test
        ;;

    clean)
        echo "Cleaning build artifacts..."
        make clean
        echo "Done."
        ;;

    *)
        echo "Usage: $0 [sim|test|clean]"
        exit 1
        ;;
esac
    echo "Simulator execution failed."
    exit 1
fi

echo "Simulator executed successfully."