#!/bin/sh
set -eu

REPORT="report.txt"
: > "$REPORT"

log() {
    printf '%s\n' "$1" | tee -a "$REPORT"
}

log "Compiling submitted C++ program..."

if compile_output=$(docker run --rm \
    -v "$PWD":/app -w /app gcc:14 \
    g++ -std=c++17 -Wall -Wextra \
    -o codetrust src/submission.cpp 2>&1); then

    if [ -n "$compile_output" ]; then
        log "$compile_output"
    fi

    log "Compilation successful."

else
    if [ -n "$compile_output" ]; then
        log "$compile_output"
    fi

    log "COMPILATION FAILED. Tests were not run."
    exit 1
fi

if [ ! -f testcases.txt ]; then
    log "Error: testcases.txt file nahi mili."
    exit 1
fi

passed=0
failed=0
total=0
test_number=0

run_test() {
    test_input="$1"
    expected_output="$2"

    test_number=$((test_number + 1))
    total=$((total + 1))

    if actual=$(printf '%s\n' "$test_input" | docker run --rm -i \
        --network none \
        --memory=128m \
        --cpus=1 \
        --pids-limit=64 \
        --read-only \
        --tmpfs /tmp:rw,nosuid,nodev,size=16m \
        -v "$PWD/codetrust":/codetrust:ro \
        -w /tmp \
        gcc:14 sh -c 'timeout 2s /codetrust' 2>&1); then

        if [ "$actual" = "$expected_output" ]; then
            log "Test $test_number: PASS"
            passed=$((passed + 1))
        else
            log "Test $test_number: FAIL"
            log "Input: $test_input"
            log "Expected: $expected_output"
            log "Actual: $actual"
            failed=$((failed + 1))
        fi

    else
        status=$?
        log "Test $test_number: FAIL (program error or timeout)"
        log "Input: $test_input"
        log "Expected: $expected_output"

        if [ -n "$actual" ]; then
            log "Program output/error: $actual"
        fi

        log "Exit status: $status"
        failed=$((failed + 1))
    fi
}

log "Reading test cases from testcases.txt..."
log "Running tests..."

while IFS='|' read -r input expected || [ -n "${input:-}${expected:-}" ]; do
    if [ -z "${input:-}" ] && [ -z "${expected:-}" ]; then
        continue
    fi

    run_test "$input" "$expected"
done < testcases.txt

log "--------------------"
log "Total tests: $total"
log "Passed: $passed"
log "Failed: $failed"

if [ "$total" -gt 0 ] && [ "$passed" -eq "$total" ]; then
    log "Final result: ALL TESTS PASSED"
    exit 0
else
    log "Final result: TESTING FAILED"
    exit 1
fi