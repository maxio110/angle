#!/bin/bash

# ANGLE Android Test Runner
# Usage: ./run_test.sh <test_query>

set -e

# Check if query provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <test_query>"
    echo ""
    echo "Examples:"
    echo "  $0 texture           # Run all texture tests"
    echo "  $0 cubemap           # Run cube map tests"
    echo "  $0 'Texture2DTest.*' # Run specific test pattern"
    echo "  $0 shader            # Run shader tests"
    echo ""
    echo "Available categories:"
    echo "  texture, cubemap, shader, framebuffer, blend, depth, stencil, multisample"
    echo ""
    echo "Log files will be generated in logs/ directory:"
    echo "  - full_device_log.txt    (complete device log)"
    echo "  - angle_verbose.txt      (V ANGLE messages)"
    echo "  - angle_debug.txt        (D ANGLE messages)"  
    echo "  - angle_info.txt         (I ANGLE messages)"
    echo "  - angle_warning.txt      (W ANGLE messages)"
    echo "  - angle_error.txt        (E ANGLE messages)"
    exit 1
fi

QUERY="$1"

# Create logs directory
LOGS_DIR="logs"
mkdir -p "$LOGS_DIR"

# Generate timestamp for log files
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
TEST_NAME=$(echo "$QUERY" | tr '/' '_' | tr '*' 'star' | tr '.' '_')
LOG_PREFIX="${LOGS_DIR}/${TIMESTAMP}_${TEST_NAME}"

# Map common queries to test patterns
case "$QUERY" in
    "texture")
        FILTER="*Texture*"
        ;;
    "cubemap")
        FILTER="*Cube*"
        ;;
    "shader")
        FILTER="*Shader*"
        ;;
    "framebuffer")
        FILTER="*Framebuffer*"
        ;;
    "blend")
        FILTER="*Blend*"
        ;;
    "depth")
        FILTER="*Depth*"
        ;;
    "stencil")
        FILTER="*Stencil*"
        ;;
    "multisample")
        FILTER="*Multisample*"
        ;;
    *)
        FILTER="$QUERY"
        ;;
esac

# Check if device is connected
if ! adb devices | tr -d '\r' | grep -q "device$"; then
    echo "❌ No Android device found. Connect device and enable USB debugging."
    echo "Current devices:"
    adb devices
    exit 1
fi

DEVICE_SERIAL=$(adb devices | tr -d '\r' | grep "device$" | head -n1 | awk '{print $1}')
echo "📱 Running tests on device: $DEVICE_SERIAL"
echo "🔍 Test filter: $FILTER"
echo "📂 Logs will be saved to: ${LOG_PREFIX}_*.txt"
echo ""

# Clear logcat buffer
echo "🧹 Clearing device logs..."
adb logcat -c

# Start logcat in background to capture full device log
FULL_LOG="${LOG_PREFIX}_full_device_log.txt"
echo "📊 Starting full device log capture..."
adb logcat > "$FULL_LOG" &
LOGCAT_PID=$!

# Function to clean up background processes
cleanup() {
    echo ""
    echo "🛑 Stopping log capture..."
    kill $LOGCAT_PID 2>/dev/null || true
    wait $LOGCAT_PID 2>/dev/null || true
    
    echo "📋 Processing ANGLE logs..."
    # Extract ANGLE logs by log level
    awk '$6 == "V" && /ANGLE/' "$FULL_LOG" > "${LOG_PREFIX}_angle_verbose.txt" 2>/dev/null || touch "${LOG_PREFIX}_angle_verbose.txt"
    awk '$6 == "D" && /ANGLE/' "$FULL_LOG" > "${LOG_PREFIX}_angle_debug.txt" 2>/dev/null || touch "${LOG_PREFIX}_angle_debug.txt"
    awk '$6 == "I" && /ANGLE/' "$FULL_LOG" > "${LOG_PREFIX}_angle_info.txt" 2>/dev/null || touch "${LOG_PREFIX}_angle_info.txt"
    awk '$6 == "W" && /ANGLE/' "$FULL_LOG" > "${LOG_PREFIX}_angle_warning.txt" 2>/dev/null || touch "${LOG_PREFIX}_angle_warning.txt"
    awk '$6 == "E" && /ANGLE/' "$FULL_LOG" > "${LOG_PREFIX}_angle_error.txt" 2>/dev/null || touch "${LOG_PREFIX}_angle_error.txt"
    
    # Create summary of ANGLE log counts
    SUMMARY_FILE="${LOG_PREFIX}_angle_summary.txt"
    echo "ANGLE Log Summary for Test: $QUERY" > "$SUMMARY_FILE"
    echo "Generated: $(date)" >> "$SUMMARY_FILE"
    echo "Device: $DEVICE_SERIAL" >> "$SUMMARY_FILE"
    echo "Filter: $FILTER" >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"
    echo "ANGLE Log Counts:" >> "$SUMMARY_FILE"
    echo "  Verbose: $(wc -l < "${LOG_PREFIX}_angle_verbose.txt")" >> "$SUMMARY_FILE"
    echo "  Debug:   $(wc -l < "${LOG_PREFIX}_angle_debug.txt")" >> "$SUMMARY_FILE"
    echo "  Info:    $(wc -l < "${LOG_PREFIX}_angle_info.txt")" >> "$SUMMARY_FILE"
    echo "  Warning: $(wc -l < "${LOG_PREFIX}_angle_warning.txt")" >> "$SUMMARY_FILE"
    echo "  Error:   $(wc -l < "${LOG_PREFIX}_angle_error.txt")" >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"
    
    # Show log file sizes
    echo "📊 Log files generated:"
    ls -lh "${LOG_PREFIX}"_*.txt | awk '{printf "  %-25s %8s\n", $9, $5}'
}

# Set up cleanup trap
trap cleanup EXIT INT TERM

# Wait a moment for logcat to start
sleep 1

# Run the test
echo "⏳ Running ANGLE end-to-end tests..."
echo ""
echo "============== LIVE TEST OUTPUT =============="
(adb logcat | grep --line-buffered -E '\[(RUN|OK|FAILED)\]|INSTRUMENTATION_STATUS_CODE:') &
GREP_PID=$!

# Give grep a moment to start
sleep 1

adb shell am instrument -w -e gtest_filter "$FILTER" -e org.chromium.native_test.NativeTestInstrumentationTestRunner.NativeTestActivity com.android.angle.test.AngleUnitTestActivity com.android.angle.test/org.chromium.build.gtest_apk.NativeTestInstrumentationTestRunner

RESULT=$?

# Stop the live log display
kill $GREP_PID 2>/dev/null || true
echo "=========== END LIVE TEST OUTPUT ==========="

# Wait a moment for final logs
sleep 2

if [ $RESULT -eq 0 ]; then
    echo ""
    echo "✅ Tests completed successfully (INSTRUMENTATION_CODE: 0)"
else
    echo ""
    echo "❌ Tests failed with code: $RESULT"
    echo "🔧 Try: adb install -r out/Android/angle_end2end_tests_apk/angle_end2end_tests-debug.apk"
fi

echo ""
echo "📄 Check ANGLE logs:"
echo "  Info level:    ${LOG_PREFIX}_angle_info.txt"
echo "  Warnings:      ${LOG_PREFIX}_angle_warning.txt"  
echo "  Errors:        ${LOG_PREFIX}_angle_error.txt"
echo "  Full device:   ${LOG_PREFIX}_full_device_log.txt"