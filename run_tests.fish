#!/usr/bin/env fish
# Test runner for JCC 
# Runs all test_*.c files in tests/ directory and reports results

set -l script_dir (dirname (status -f))
set -l jcc "$script_dir/jcc"
set -l tests_dir "$script_dir/tests"

# Check if jcc exists
if not test -f "$jcc"
    echo "Error: jcc executable not found. Please run 'make' first."
    exit 1
end

# Check if tests directory exists
if not test -d "$tests_dir"
    echo "Error: tests directory not found."
    exit 1
end

# Initialize counters
set -l passed 0
set -l failed 0
set -l total 0
set -l failed_tests

# Get all test files
set -l test_files (find "$tests_dir" -name "test_*.c" | sort)

if test (count $test_files) -eq 0
    echo "No test files found in $tests_dir"
    exit 1
end

echo "Running JCC tests..."
echo "======================="
echo ""

# Run each test
for test_file in $test_files
    set -l test_name (basename "$test_file")
    set total (math $total + 1)
    
    # Run the test and capture exit code
    "$jcc" -I./include "$test_file" >/dev/null 2>&1
    set -l exit_code $status
    
    # Check if test passed (non-zero exit code indicates successful test execution)
    # JCC tests return their result as exit code (e.g., 42, 30, etc.)
    if test $exit_code -ne 0
        set passed (math $passed + 1)
        echo "✓ $test_name (exit code: $exit_code)"
    else
        set failed (math $failed + 1)
        set -a failed_tests "$test_name"
        echo "✗ $test_name (unexpected exit code: 0)"
    end
end

echo ""
echo "======================="
echo "Test Results Summary"
echo "======================="
echo "Total:  $total"
echo "Passed: $passed"
echo "Failed: $failed"

if test $failed -gt 0
    echo ""
    echo "Failed tests:"
    for test in $failed_tests
        echo "  - $test"
    end
    exit 1
else
    echo ""
    echo "All tests passed! 🎉"
    exit 0
end
