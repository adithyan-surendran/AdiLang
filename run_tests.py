#!/usr/bin/env python3
import os
import subprocess
import sys

# Configuration
INTERPRETER_PATH = "./bin/adilang"
TEST_DIR = "tests"

def run_tests():
    if not os.path.exists(INTERPRETER_PATH):
        print(f"Error: Interpreter binary not found at '{INTERPRETER_PATH}'. Build the project first.")
        sys.exit(1)

    if not os.path.exists(TEST_DIR):
        os.makedirs(TEST_DIR)
        print(f"Created '{TEST_DIR}/' directory. Add your .adi test files there.")
        return

    test_files = [f for f in os.listdir(TEST_DIR) if f.endswith(".adi")]
    if not test_files:
        print(f"No test files found in '{TEST_DIR}/'.")
        return

    passed = 0
    failed = 0

    print(f"Running AdiLang Test Suite (v0.18.0)...\n")

    for filename in sorted(test_files):
        filepath = os.path.join(TEST_DIR, filename)
        
        # Read expected outputs from inline comments like: // expect: <output>
        expected_outputs = []
        with open(filepath, "r") as f:
            for line in f:
                if "// expect:" in line:
                    expected_outputs.append(line.split("// expect:")[1].strip())

        # Run the interpreter binary
        start_time = os.times()[4]
        result = subprocess.run([INTERPRETER_PATH, filepath], capture_output=True, text=True)
        
        actual_output = [line.strip() for line in result.stdout.splitlines() if line.strip()]

        # Validate results
        if result.returncode != 0:
            print(f"❌ FAIL: {filename} (Runtime/Compile Error)")
            print(result.stderr)
            failed += 1
            continue

        if expected_outputs:
            match = True
            if len(actual_output) < len(expected_outputs):
                match = False
            else:
                for exp, act in zip(expected_outputs, actual_output[-len(expected_outputs):]):
                    if exp != act:
                        match = False
                        break
            
            if match:
                print(f"✅ PASS: {filename}")
                passed += 1
            else:
                print(f"❌ FAIL: {filename} (Output Mismatch)")
                print(f"  Expected (last): {expected_outputs}")
                print(f"  Got (actual):    {actual_output}")
                failed += 1
        else:
            # If no explicit expectations, just verify clean execution (exit code 0)
            print(f"✅ PASS: {filename} (Clean Execution)")
            passed += 1

    print(f"\nTest Summary: {passed} passed, {failed} failed.")
    if failed > 0:
        sys.exit(1)

if __name__ == "__main__":
    run_tests()