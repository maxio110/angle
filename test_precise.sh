#!/bin/bash
set -e

echo "Running PreciseMatrixOperationAssertFix test..."
timeout 30s out/Android/bin/run_angle_end2end_tests --gtest_filter="GLSLTest.PreciseMatrixOperationAssertFix/*" --num-retry=0 2>&1 | tee precise_test.log

# Check if the test contains an assert or failure
if grep -q "ASSERT\|assert\|FindPreciseNodes" precise_test.log; then
    echo "Test failed with assert as expected!"
    exit 0
else
    echo "Test did not fail with expected assert"
    exit 1
fi