#!/usr/bin/env python3
"""
ProbLog Knowledge Compilation Benchmark Script

This script runs a ProbLog file with different knowledge compilation techniques
and measures the execution time for each.

The script also preprocesses the input file to remove annotations and
convert syntax for ProbLog compatibility.
"""

import subprocess
import time
import argparse
import os
import csv
import sys
import tempfile
import re
from datetime import datetime

def check_problog_installed():
    """Check if problog is installed and available in PATH."""
    try:
        subprocess.run(["problog", "--version"], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False

def preprocess_file(input_file):
    """
    Preprocess the input file to:
    1. Remove lines with .decl, .input, .output annotations
    2. Replace ! with \+
    3. Remove comments after //

    Args:
        input_file: Path to the original input file

    Returns:
        str: Path to the temporary preprocessed file
    """
    # Create a temporary file
    temp_file = tempfile.NamedTemporaryFile(suffix='.pl', mode='w+', delete=False)

    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            for line in f:
                # Skip lines with .decl, .input, or .output annotations
                if any(pattern in line for pattern in ['.decl', '.input', '.output']):
                    continue

                # Remove comments after //
                line = re.sub(r'//.*', '', line)

                # Replace ! with \+
                line = line.replace('!', '\\+')

                # Write the processed line to the temp file
                temp_file.write(line)

    except Exception as e:
        temp_file.close()
        os.unlink(temp_file.name)
        raise e

    temp_file.close()
    return temp_file.name

def run_problog_with_knowledge(input_file, knowledge, timeout=10, output_file=None):
    """
    Run ProbLog with a specific knowledge compilation technique and measure time.

    Args:
        input_file: Path to the ProbLog input file
        knowledge: Knowledge compilation technique to use
        timeout: Timeout in seconds (default=10)
        output_file: Optional output file path

    Returns:
        tuple: (execution_time, stdout, stderr, status, cmd, process)
    """
    cmd = ["problog", input_file, "--knowledge", knowledge, "--timeout", str(timeout)]

    if output_file:
        cmd.extend(["--output", output_file])

    # Use monotonic clock for timing which is not affected by system clock changes
    start_time = time.monotonic()
    try:
        process = subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        status = "Success"
    except subprocess.CalledProcessError as e:
        process = e
        stderr_text = e.stderr if hasattr(e, 'stderr') else ""
        stdout_text = e.stdout if hasattr(e, 'stdout') else ""

        # Also check the output file if it exists
        output_file_content = ""
        if output_file and os.path.exists(output_file):
            try:
                with open(output_file, 'r', encoding='utf-8') as f:
                    output_file_content = f.read()
            except Exception as file_e:
                print(f"Warning: Could not read output file: {str(file_e)}")

        # Combine all available outputs for error detection
        all_output = (stdout_text + "\n" + stderr_text + "\n" + output_file_content).lower()

        # Check for timeout indicators with more permissive matching
        if any(t in all_output for t in ["timeout", "time limit", "timed out"]):
            status = "Timeout"
        elif "not supported" in all_output or "not implemented" in all_output:
            status = f"Unsupported: {knowledge}"
        else:
            # Use all available outputs for error message extraction
            error_msg = extract_error_message(stdout_text + "\n" + stderr_text + "\n" + output_file_content)
            if error_msg:
                status = f"Error: {error_msg}"
            else:
                # If we can't extract a clear error message, default to timeout
                # This is because most ProbLog failures are timeout-related
                status = "Timeout (assumed)"

    end_time = time.monotonic()

    elapsed_time = end_time - start_time
    # Ensure we don't return negative time (which should never happen with monotonic clock)
    if elapsed_time < 0:
        print(f"\nWarning: Negative time detected ({elapsed_time:.4f}s). Using absolute value.")
        elapsed_time = abs(elapsed_time)

    stdout = process.stdout if hasattr(process, 'stdout') else ""
    stderr = process.stderr if hasattr(process, 'stderr') else ""

    return elapsed_time, stdout, stderr, status, cmd, process

def extract_error_message(text):
    """
    Extract a meaningful error message from problog output.

    Args:
        text: Combined stdout and stderr text

    Returns:
        str: A concise error message
    """
    # If text is empty, return a generic message
    if not text.strip():
        return "Unknown error (no output)"

    # Try to find common error patterns

    # Look for ERROR: lines
    error_lines = re.findall(r'ERROR:\s*(.*?)(?:\n|$)', text)
    if error_lines:
        return error_lines[0].strip()

    # Look for Exception: lines
    exception_lines = re.findall(r'Exception:\s*(.*?)(?:\n|$)', text)
    if exception_lines:
        return exception_lines[0].strip()

    # Look for lines with "error" in them
    error_containing = re.findall(r'(?i).*error.*(?:\n|$)', text)
    if error_containing:
        return error_containing[0].strip()

    # If nothing specific found, return the first non-empty line
    lines = [line.strip() for line in text.split('\n') if line.strip()]
    if lines:
        return lines[0]

    # Last resort
    return text.strip()[:100] if len(text.strip()) > 100 else text.strip()

def main():
    # Check if problog is installed
    if not check_problog_installed():
        print("Error: problog is not installed or not in your PATH")
        print("Please install problog using: pip install problog")
        sys.exit(1)

    parser = argparse.ArgumentParser(description="Benchmark ProbLog knowledge compilation techniques")
    parser.add_argument("input_file", help="Path to the input file to benchmark")
    parser.add_argument("--output_dir", "-o",
                        help="Directory to store problog output files (one per technique)",
                        default="")
    parser.add_argument("--csv",
                        help="Path to save CSV results (defaults to problog_benchmark_<filename>_<timestamp>.csv)",
                        default=None)
    parser.add_argument("--repeat", "-r", type=int,
                        help="Number of times to repeat each test for more accurate benchmarking",
                        default=1)
    parser.add_argument("--timeout", "-t", type=int,
                        help="Set timeout in seconds for each ProbLog run (default=10)",
                        default=10)
    parser.add_argument("--keep_temp", action="store_true",
                        help="Keep the temporary preprocessed file after execution")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Enable verbose output, save logs for all runs")

    args = parser.parse_args()

    # Check if input file exists
    if not os.path.isfile(args.input_file):
        print(f"Error: Input file '{args.input_file}' does not exist")
        sys.exit(1)

    # Generate default CSV filename if not provided
    if args.csv is None:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        input_filename = os.path.basename(args.input_file)
        args.csv = f"problog_benchmark_{input_filename}_{timestamp}.csv"

    # Preprocess input file
    print(f"Preprocessing input file: {args.input_file}")
    try:
        temp_file = preprocess_file(args.input_file)
        print(f"Created temporary file: {temp_file}")
    except Exception as e:
        print(f"Error preprocessing input file: {str(e)}")
        sys.exit(1)

    # List of knowledge compilation techniques
    knowledge_techniques = ["sdd", "bdd", "nnf", "ddnnf", "fsdd", "fbdd"]  # Removed sddx and kbest

    # Create output directory if it doesn't exist
    if args.output_dir and not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)

    results = []

    print(f"Benchmarking ProbLog knowledge compilation techniques")
    print(f"Each technique will be tested {args.repeat} time(s)")
    print("-" * 60)

    try:
        for knowledge in knowledge_techniques:
            print(f"Testing knowledge={knowledge}")

            for run in range(1, args.repeat + 1):
                output_file = None
                if args.output_dir:
                    if args.repeat > 1:
                        output_file = os.path.join(args.output_dir, f"output_{knowledge}_run{run}.txt")
                    else:
                        output_file = os.path.join(args.output_dir, f"output_{knowledge}.txt")

                print(f"  Run {run}/{args.repeat}...", end="", flush=True)
                execution_time, stdout, stderr, status, cmd, process = run_problog_with_knowledge(
                    temp_file, knowledge, args.timeout, output_file
                )

                result = {
                    "knowledge": knowledge,
                    "run": run,
                    "time": execution_time,
                    "status": status
                }

                results.append(result)

                if status == "Success":
                    print(f" Done in {execution_time:.4f} seconds")
                elif status == "Timeout":
                    print(f" Timeout after {execution_time:.4f} seconds")
                elif status.startswith("Unsupported"):
                    print(f" {status}")
                else:
                    print(f" {status}")

                # Save error output to file if output directory specified
                if args.output_dir and (status != "Success" or args.verbose):
                    # Always save error logs for non-success runs or in verbose mode
                    error_file = os.path.join(args.output_dir, f"log_{knowledge}_run{run}.txt")
                    with open(error_file, 'w', encoding='utf-8') as f:
                        f.write(f"Status: {status}\n")
                        f.write(f"Execution time: {execution_time:.4f} seconds\n")
                        f.write(f"Command: {' '.join(cmd)}\n")
                        f.write(f"Return code: {process.returncode if hasattr(process, 'returncode') else 'unknown'}\n\n")
                        f.write("===== STDOUT =====\n")
                        f.write(stdout)
                        f.write("\n\n===== STDERR =====\n")
                        f.write(stderr)

    except KeyboardInterrupt:
        print("\nBenchmark interrupted by user")

    finally:
        # Clean up temporary file unless --keep_temp is specified
        if not args.keep_temp and os.path.exists(temp_file):
            try:
                os.unlink(temp_file)
                print(f"Removed temporary file: {temp_file}")
            except Exception as e:
                print(f"Warning: Could not remove temporary file {temp_file}: {str(e)}")

        # Save results to CSV if we have any
        if results:
            with open(args.csv, 'w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(f, fieldnames=["knowledge", "run", "time", "status"])
                writer.writeheader()
                writer.writerows(results)

            print("-" * 60)
            print(f"Results saved to {args.csv}")

            # Print summary
            print("\nSummary:")
            knowledge_avg_times = {}
            knowledge_status_count = {}

            for knowledge in knowledge_techniques:
                # Count status occurrences for this knowledge technique
                status_counts = {}
                for r in results:
                    if r["knowledge"] == knowledge:
                        status_counts[r["status"]] = status_counts.get(r["status"], 0) + 1

                knowledge_status_count[knowledge] = status_counts

                # Filter successful runs for this knowledge
                successful_runs = [r for r in results if r["knowledge"] == knowledge and r["status"] == "Success"]

                if successful_runs:
                    avg_time = sum(r["time"] for r in successful_runs) / len(successful_runs)
                    knowledge_avg_times[knowledge] = avg_time

                    status_str = ", ".join([f"{count} {status}" for status, count in status_counts.items()])
                    print(f"  {knowledge}: {avg_time:.4f} seconds (avg of {len(successful_runs)} successful run(s), {status_str})")
                else:
                    status_str = ", ".join([f"{count} {status}" for status, count in status_counts.items()])
                    print(f"  {knowledge}: No successful runs ({status_str})")

            # Find the fastest technique
            if knowledge_avg_times:
                fastest = min(knowledge_avg_times.items(), key=lambda x: x[1])
                print(f"\nFastest technique: {fastest[0]} ({fastest[1]:.4f} seconds)")

if __name__ == "__main__":
    main()