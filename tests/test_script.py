#!/usr/bin/env python3
import os
import sys
import unittest
import argparse
from unittest import case
import shutil
import subprocess

def check_output_suffix(output_dir, result_dir, suffix, result_suffix='.full'):
    """
    Check exact equivalence between output/filename and result/filename.full

    Args:
        output_dir: Directory containing output files
        result_dir: Directory containing result files

    Returns:
        bool: True if all files match exactly, False otherwise
    """
    import os
    import filecmp

    # Get list of output files
    output_files = [f for f in os.listdir(output_dir) if os.path.isfile(os.path.join(output_dir, f)) and (any(f.endswith(suf)for suf in suffix))]

    for output_file in output_files:
        output_path = os.path.join(output_dir, output_file)
        result_path = os.path.join(result_dir, output_file + result_suffix)

        # Check if result file exists
        if not os.path.exists(result_path):
            print(f"Missing result file: {result_path}")
            return False

        # Compare files
        if not filecmp.cmp(output_path, result_path, shallow=False):
            print(f"Files differ: {output_path} vs {result_path}", flush=True)
            return False

    return True

class TestBasic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # folders

        cls.source_dir = os.environ['SOURCE_DIR']
        cls.build_dir = os.environ['BUILD_DIR']
        cls.test_dir = os.environ['TEST_INPUT_DIR']
        os.chdir(cls.test_dir)
        cls.cur_dir = os.getcwd()
        print(f'{cls.test_dir}: setting up test environment ...\n', flush=True)

        cls.souffle_path = cls.build_dir + '/src/souffle'
        cls._input_dir = cls.cur_dir + '/data'
        cls.input_dir = cls.cur_dir + '/input'
        cls.output_dir = cls.cur_dir + '/output'
        cls.result_dir = cls.cur_dir + '/result'
        if not os.path.exists(cls.souffle_path):
            raise unittest.TestCase.failureException("please build souffle first")
        if os.path.exists(cls.input_dir):
            shutil.rmtree(cls.input_dir)
        if os.path.exists(cls.output_dir):
            shutil.rmtree(cls.output_dir)
        shutil.copytree(cls._input_dir, cls.input_dir)
        os.mkdir(cls.output_dir)
        # print(cls.source_dir, cls.build_dir, cls.test_dir, cls.cur_dir)
        # souffle
        cls.dl_path = cls.cur_dir + '/test.dl'

        cls.full_comp_cmds = \
            [cls.souffle_path, cls.dl_path, '-F', cls.input_dir, '-D', cls.output_dir, '-o', 'test-full']
        cls.full_with_delta_cmds = \
            [cls.souffle_path, '--full-with-delta',
             cls.dl_path, '-F', cls.input_dir, '-D', cls.output_dir, '-o', 'test-full-with-delta']
        cls.inc_cmds = \
            [cls.souffle_path, '--inc',
             cls.dl_path, '-F', cls.input_dir, '-D', cls.output_dir, '-o', 'test-inc']

        # 1. full compilation
        print(f'{cls.test_dir}: {cls.full_comp_cmds} ...\n', flush=True)
        result = subprocess.run(cls.full_comp_cmds, capture_output=True, text=True)
        if result.returncode != 0:
            raise unittest.TestCase.failureException(
                f'{cls.test_dir}: souffle full comp failed with msg <{result.stdout}> <{result.stderr}>')
        # 2. full-with-delta compilation
        print(f'{cls.test_dir}: {cls.full_with_delta_cmds} ...\n', flush=True)
        result = subprocess.run(cls.full_with_delta_cmds, capture_output=True, text=True)
        if result.returncode != 0:
            raise unittest.TestCase.failureException(
                f'{cls.test_dir}: souffle full-with-delta comp failed with msg <{result.stdout}> <{result.stderr}>')
        # 3. inc compilation
        print(f'{cls.test_dir}: {cls.inc_cmds} ...\n', flush=True)
        result = subprocess.run(cls.inc_cmds, capture_output=True, text=True)
        if result.returncode != 0:
            raise unittest.TestCase.failureException(
                f'{cls.test_dir}: souffle inc comp failed with msg <{result.stdout}> <{result.stderr}>')
        print(f'{cls.test_dir}: dl successfully compiled!!!\n', flush=True)

        # now compile c++
        cls.cpp_script_path = cls.build_dir + '/src/souffle-compile.py'
        cls.cpp_comp_full_cmds = ['python', cls.cpp_script_path, 'test-full.cpp', '-o', 'test-full', '--with-cudd']
        cls.cpp_comp_full_with_delta_cmds = \
            ['python', cls.cpp_script_path, 'test-full-with-delta.cpp', '-o', 'test-full-with-delta', '--with-cudd']
        cls.cpp_comp_inc_cmds = ['python', cls.cpp_script_path, 'test-inc.cpp', '-o', 'test-inc', '--with-cudd']

        # 1. cpp comp full
        print(f'{cls.test_dir}: {cls.cpp_comp_full_cmds} ...\n', flush=True)
        result = subprocess.run(cls.cpp_comp_full_cmds, capture_output=True, text=True)
        if result.returncode != 0:
            raise unittest.TestCase.failureException(
                f'{cls.test_dir}: cpp full comp failed with msg <{result.stdout}> <{result.stderr}>')
        # 2. full-with-delta compilation
        print(f'{cls.test_dir}: {cls.cpp_comp_full_with_delta_cmds} ...\n', flush=True)
        result = subprocess.run(cls.cpp_comp_full_with_delta_cmds, capture_output=True, text=True)
        if result.returncode != 0:
            raise unittest.TestCase.failureException(
                f'{cls.test_dir}: cpp full-with-delta comp failed with msg <{result.stdout}> <{result.stderr}>')
        # 3. inc compilation
        print(f'{cls.test_dir}: {cls.cpp_comp_inc_cmds} ...\n', flush=True)
        result = subprocess.run(cls.cpp_comp_inc_cmds, capture_output=True, text=True)
        if result.returncode != 0:
            raise unittest.TestCase.failureException(
                f'{cls.test_dir}: cpp inc comp failed with msg <{result.stdout}> <{result.stderr}>')
        print(f'{cls.test_dir}: cpp successfully compiled!!!\n', flush=True)

        cls.run_full_cmds = ['./test-full']
        cls.run_full_with_delta_cmds = ['./test-full-with-delta']
        cls.run_inc_cmds = ['./test-inc']

    def setUp(self):
        pass

    def test_full(self):
        if os.path.exists(self.__class__.output_dir):
            shutil.rmtree(self.__class__.output_dir)
            os.mkdir(self.__class__.output_dir)
        result = subprocess.run(self.__class__.run_full_cmds, capture_output=True, text=True)
        if result.returncode != 0:
            raise unittest.TestCase.failureException(
                f'{self.__class__.test_dir}: {self.run_full_cmds} failed with msg <{result.stdout}> <{result.stderr}>'
            )
        if os.path.exists(self.result_dir):
            result = check_output_suffix(self.__class__.output_dir, self.__class__.result_dir, suffix=[".csv"], result_suffix=".full")
            if result:
                print(f'{self.__class__.test_dir}: full comp correct')
            else:
                raise unittest.TestCase.failureException(
                    f'{self.__class__.test_dir}: {self.run_full_cmds} failed, results differ'
                )

    def test_full_with_delta(self):
        if os.path.exists(self.__class__.output_dir):
            shutil.rmtree(self.__class__.output_dir)
            os.mkdir(self.__class__.output_dir)
            print(f'{self.__class__.output_dir}....')
        result = subprocess.run(self.__class__.run_full_with_delta_cmds, capture_output=True, text=True)
        if result.returncode != 0:
            raise unittest.TestCase.failureException(
                f'{self.__class__.test_dir}: {self.run_full_with_delta_cmds} failed with msg <{result.stdout}> <{result.stderr}>')
        if os.path.exists(self.result_dir):
            # full with delta = full + inc
            result = check_output_suffix(self.__class__.output_dir, self.__class__.result_dir, suffix=[".csv", "test.dl.json"], result_suffix=".incr")
            if result:
                print(f'{self.__class__.test_dir}: full-with-delta comp correct')
            else:
                raise unittest.TestCase.failureException(
                    f'{self.__class__.test_dir}: {self.run_full_with_delta_cmds} failed, results differ')

    def test_inc(self):
        if os.path.exists(self.__class__.output_dir):
            shutil.rmtree(self.__class__.output_dir)
            os.mkdir(self.__class__.output_dir)
        # run test-full again first
        _ = subprocess.run(self.__class__.run_full_cmds, text=True)
        result = subprocess.run(self.__class__.run_inc_cmds, capture_output=True, text=True)
        if result.returncode != 0:
            raise unittest.TestCase.failureException(
                f'{self.__class__.test_dir}: {self.run_inc_cmds} failed with msg <{result.stdout}> <{result.stderr}>')
        if os.path.exists(self.result_dir):
            # full with delta = full + inc
            result = check_output_suffix(self.__class__.output_dir, self.__class__.result_dir, suffix=[".csv", "json"], result_suffix=".incr")
            if result:
                print(f'{self.__class__.test_dir}: inc comp correct')
            else:
                raise unittest.TestCase.failureException(
                    f'{self.__class__.test_dir}: {self.run_inc_cmds} failed, results differ')
        pass

    def test_diff(self):
        pass


    def test_something(self):
        # 实际的测试逻辑
        self.assertTrue(True)

    def test_with_file(self):
        input_file = os.path.join(self.test_data_dir, 'input.txt')
        if os.path.exists(input_file):
            with open(input_file, 'r') as f:
                content = f.read()
            self.assertNotEqual(content, '')

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dir', help='Input directory for testing', required=True)
    parser.add_argument('--full', action='store_true', help='Enable testing full compilation')
    parser.add_argument('--delta', action='store_true', help='Enable testing delta compilation, including full compilation with delta, and incremental compilation')
    parser.add_argument('--diff', action='store_true', help='Enable differential testing between full compilation & incremental compilation and full with delta compilation')
    parser.add_argument('--perf', action='store_true', help='Enable evaluating performance')
    parser.add_argument('--rand', action='store_true', help='Force random tests on all selected test type')
    return parser.parse_args()

if __name__ == '__main__':
    args = parse_args()
    if args.dir:
        os.environ['TEST_INPUT_DIR'] = args.dir
    if args.rand:
        assert (False and "not supported")
    suite = unittest.TestSuite()

    if args.full:
        suite.addTest(TestBasic('test_full'))
    if args.delta:
        suite.addTest(TestBasic('test_full_with_delta'))
        suite.addTest(TestBasic('test_inc'))
    if args.diff:
        suite.addTest(TestBasic('test_diff'))
    if args.perf:
        pass # TODO
    ret = unittest.TextTestRunner().run(suite)
    sys.exit(not ret.wasSuccessful())