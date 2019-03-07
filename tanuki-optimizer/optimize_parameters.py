#!/usr/bin/python
# coding:utf-8
#
# Preparation
#
# 1. Install Python 3.6.* 64-bit ver
# - Python 3.6.* 64bit
# -- http://www.python.org
#
# 2. Download the following libraries.
# - numpy-*.*.*+mkl-cp36-cp36m-win_amd64.whl
# -- http://www.lfd.uci.edu/~gohlke/pythonlibs/
# - scipy-*.*.*-cp36-none-win_amd64.whl
# -- http://www.lfd.uci.edu/~gohlke/pythonlibs/
#
# 3. Execute the following command.
# - python -m pip install --upgrade pip
# - pip install numpy-*.*.*+mkl-cp36-cp36m-win_amd64.whl scipy-*.*.*-cp36-none-win_amd64.whl hyperopt pymongo networkx pandas sklearn matplotlib
#
# 4. If using MSVC instead of MSYS (Windows)
# - install MSVC14 (Visual Studio 2015)
# - uncomment the line: buider = MSVCBuilder()
# - open developer console
# - move in src directory
# - run this script
from hyperopt import fmin, tpe, hp, rand, Trials
from math import log
import os
import sys
import datetime
import re
import shutil
import subprocess
import time
import argparse
import pickle as pickle
from hyperopt_state import HyperoptState

space = [
    hp.quniform('PARAM_FUTILITY_MARGIN_ALPHA1', 100, 240, 1),
    hp.quniform('PARAM_FUTILITY_MARGIN_ALPHA2', 25, 100, 1),
    hp.quniform('PARAM_FUTILITY_MARGIN_BETA', 100, 240, 1),
    hp.quniform('PARAM_FUTILITY_MARGIN_QUIET', 50, 160, 1),
    hp.quniform('PARAM_FUTILITY_RETURN_DEPTH', 5, 15, 1),
    hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_DEPTH', 5, 20, 1),
    hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1', 100, 300, 1),
    hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2', 0, 300, 1),
    hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1', 20, 50, 1),
    hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2', 20, 60, 1),
    hp.quniform('PARAM_NULL_MOVE_DYNAMIC_ALPHA', 500, 1500, 1),
    hp.quniform('PARAM_NULL_MOVE_DYNAMIC_BETA', 50, 100, 1),
    hp.quniform('PARAM_NULL_MOVE_MARGIN', 10, 60, 1),
    hp.quniform('PARAM_NULL_MOVE_RETURN_DEPTH', 4, 16, 1),
    hp.quniform('PARAM_PROBCUT_DEPTH', 3, 10, 1),
    hp.quniform('PARAM_PROBCUT_MARGIN1', 100, 300, 1),
    hp.quniform('PARAM_PROBCUT_MARGIN2', 20, 80, 1),
    hp.quniform('PARAM_SINGULAR_EXTENSION_DEPTH', 4, 13, 1),
    hp.quniform('PARAM_SINGULAR_MARGIN', 128, 400, 1),
    hp.quniform('PARAM_SINGULAR_SEARCH_DEPTH_ALPHA', 8, 32, 1),
    hp.quniform('PARAM_PRUNING_BY_MOVE_COUNT_DEPTH', 8, 32, 1),
    hp.quniform('PARAM_PRUNING_BY_HISTORY_DEPTH', 2, 32, 1),
    hp.quniform('PARAM_REDUCTION_BY_HISTORY', 2000, 8000, 1),
    hp.quniform('PARAM_RAZORING_MARGIN2', 400, 700, 1),
    hp.quniform('PARAM_RAZORING_MARGIN3', 400, 700, 1),
    hp.quniform('PARAM_REDUCTION_ALPHA', 64, 256, 1),
    hp.quniform('PARAM_FUTILITY_MOVE_COUNT_ALPHA0', 150, 400, 1),
    hp.quniform('PARAM_FUTILITY_MOVE_COUNT_ALPHA1', 300, 600, 1),
    hp.quniform('PARAM_FUTILITY_MOVE_COUNT_BETA0', 500, 2000, 1),
    hp.quniform('PARAM_FUTILITY_MOVE_COUNT_BETA1', 500, 2000, 1),
    hp.quniform('PARAM_QUIET_SEARCH_COUNT', 32, 128, 1),
    hp.quniform('PARAM_ASPIRATION_SEARCH_DELTA', 12, 40, 1),
    hp.quniform('PARAM_EVAL_TEMPO', 10, 50, 1),
]

build_argument_names = [
    'PARAM_FUTILITY_MARGIN_ALPHA1',
    'PARAM_FUTILITY_MARGIN_ALPHA2',
    'PARAM_FUTILITY_MARGIN_BETA',
    'PARAM_FUTILITY_MARGIN_QUIET',
    'PARAM_FUTILITY_RETURN_DEPTH',
    'PARAM_FUTILITY_AT_PARENT_NODE_DEPTH',
    'PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1',
    'PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2',
    'PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1',
    'PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2',
    'PARAM_NULL_MOVE_DYNAMIC_ALPHA',
    'PARAM_NULL_MOVE_DYNAMIC_BETA',
    'PARAM_NULL_MOVE_MARGIN',
    'PARAM_NULL_MOVE_RETURN_DEPTH',
    'PARAM_PROBCUT_DEPTH',
    'PARAM_PROBCUT_MARGIN1',
    'PARAM_PROBCUT_MARGIN2',
    'PARAM_SINGULAR_EXTENSION_DEPTH',
    'PARAM_SINGULAR_MARGIN',
    'PARAM_SINGULAR_SEARCH_DEPTH_ALPHA',
    'PARAM_PRUNING_BY_MOVE_COUNT_DEPTH',
    'PARAM_PRUNING_BY_HISTORY_DEPTH',
    'PARAM_REDUCTION_BY_HISTORY',
    'PARAM_RAZORING_MARGIN2',
    'PARAM_RAZORING_MARGIN3',
    'PARAM_REDUCTION_ALPHA',
    'PARAM_FUTILITY_MOVE_COUNT_ALPHA0',
    'PARAM_FUTILITY_MOVE_COUNT_ALPHA1',
    'PARAM_FUTILITY_MOVE_COUNT_BETA0',
    'PARAM_FUTILITY_MOVE_COUNT_BETA1',
    'PARAM_QUIET_SEARCH_COUNT',
    'PARAM_ASPIRATION_SEARCH_DELTA',
    'PARAM_EVAL_TEMPO',
]

START_COUNTER = 0
CURRENT_COUNTER = 0
MAX_EVALS = 10000
START_TIME_SEC = time.time()
EVAL_DIR1 = r'F:\hnoda\nnue\eval\halfkp_256x2-32-32.iteration=4.nodes_searched=50000\final'
EVAL_DIR2 = r'F:\hnoda\hakubishin\exe\eval\qqr_rel'
ENGINE1 = r'F:\hnoda\nnue.git\source\YaneuraOu-by-gcc.exe'
ENGINE2 = r'F:\hnoda\YaneuraOu-2018-Otafuku-KPPT_V482\YaneuraOu-2018-Otafuku.exe'
NUM_THREADS = 24
NUM_SEARCH_NODES = 5000000
NUM_NUMA_NODES = 2
HASH = 256
HISTOGRAM_WIDTH = 80
DEFAULT_N_STARTUP_JOBS = 200


class YaneuraouBuilder(object):
    FILENAME = r'param\2018-otafuku-param.h'
    def __init__(self):
        pass

    def clean(self):
        try:
            os.remove(self.FILENAME)
        except WindowsError:
            pass

    def build(self, args):
        with open(self.FILENAME, 'w') as f:
            f.write('#ifndef _2018_OTAFUKU_PARAMETERS_\n')
            f.write('#define _2018_OTAFUKU_PARAMETERS_\n')
            for key, val in zip(build_argument_names, args):
                f.write('PARAM_DEFINE {0} = {1};\n'.format(key, str(int(val))))
            f.write('PARAM_DEFINE PARAM_RAZORING_MARGIN1 = 0;\n')
            f.write('PARAM_DEFINE PARAM_QSEARCH_MATE1 = 1;\n')
            f.write('PARAM_DEFINE PARAM_SEARCH_MATE1 = 1;\n')
            f.write('PARAM_DEFINE PARAM_WEAK_MATE_PLY = 1;\n')
            f.write('#endif\n')

    def kill(self, process_name):
        subprocess.call(['taskkill', '/T', '/F', '/IM', process_name])


def function(args):
    print('-' * 78)

    print(datetime.datetime.today().strftime("%Y-%m-%d %H:%M:%S"))

    global START_COUNTER
    global CURRENT_COUNTER
    global MAX_EVALS
    global START_TIME_SEC
    global EVAL_DIR
    global ENGINE1
    global ENGINE2
    global NUM_THREADS
    global THINKING_TIME_MS
    global NUM_NUMA_NODES
    global HASH
    print(args, flush=True)

    if START_COUNTER < CURRENT_COUNTER:
        current_time_sec = time.time()
        delta = current_time_sec - START_TIME_SEC
        sec_per_one = delta / (CURRENT_COUNTER - START_COUNTER)
        remaining = datetime.timedelta(seconds=sec_per_one * (MAX_EVALS - CURRENT_COUNTER))
        print(CURRENT_COUNTER, '/', MAX_EVALS, str(remaining), flush=True)
    CURRENT_COUNTER += 1

    builder.clean()
    builder.build(args)

    engine_invoker_args = [
        r'E:\Jenkins\workspace\optimize_parameters.2018-07-05\TanukiColiseum\bin\Release\TanukiColiseum.exe',
        '--engine1', ENGINE1,
        '--engine2', ENGINE2,
        '--eval1', EVAL_DIR1,
        '--eval2', EVAL_DIR2,
        '--num_concurrent_games', str(NUM_THREADS),
        '--num_games', str(NUM_THREADS),
        '--hash', str(HASH),
        '--nodes1', str(NUM_SEARCH_NODES),
        '--nodes2', str(NUM_SEARCH_NODES),
        '--num_numa_nodes', str(NUM_NUMA_NODES),
        '--num_book_moves1', '0',
        '--num_book_moves2', '0',
        '--book_file_name1', 'no_book',
        '--book_file_name2', 'no_book',
        '--num_book_moves', '24',
        '--sfen_file_name', r'F:\hnoda\hakubishin\exe\records2016_10818.sfen',
        '--no_gui',
        '--progress_interval_ms', str(60 * 60 * 1000),
        '--num_threads1', '1',
        '--num_threads2', '1',
        '--book_eval_diff1', '10000',
        '--book_eval_diff2', '10000',
        '--consider_book_move_count1', 'true',
        '--consider_book_move_count2', 'true',
        ]

    print(engine_invoker_args, flush=True)

    process = subprocess.Popen(engine_invoker_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    stdoutdata, stderrdata = process.communicate()
    if stdoutdata:
        print('-' * 78)
        print('- stdout')
        print('-' * 78)
        print('')
        print(stdoutdata)
        print('')
    if stderrdata:
        print('-' * 78)
        print('- sterr')
        print('-' * 78)
        print('')
        print(stderrdata)
        print('')
    sys.stdout.flush()

    if process.returncode:
        sys.exit('Failed to execute engine_invoker...')

    win = 0
    draw = 0
    lose = 0
    for match in re.compile(',(\\d+) - (\\d+) - (\\d+)\\(').finditer(stdoutdata):
        win = float(match.group(1))
        draw = float(match.group(2))
        lose = float(match.group(3))

    ratio = 0.0
    if lose + draw + win > 0.1:
     ratio = (win * 1.0 + draw * 0.5 + lose * 0.0) / (lose + draw + win)
    print('ratio={0}'.format(ratio), flush=True)

    builder.kill(ENGINE1)
    builder.kill(ENGINE2)

    global state
    state.record_iteration(args=args,
            output=stdoutdata,
            lose=lose,
            draw=draw,
            win=win,)
    if commandline_args.store_interval > 0 and state.get_n_accumulated_iterations() % commandline_args.store_interval == 0:
        print('Saveing state...', flush=True)
        state.save(state_store_path)
        print('Saved state...', flush=True)

    # Show the histogram.
    hist = [0] * (NUM_THREADS + 1)
    max_win = 0
    for record in state.iteration_logs:
        win = int(record['win'])
        hist[win] += 1
        max_win = max(max_win, hist[win])

    for win, count in enumerate(hist):
        print('{0:4d} '.format(win) + '*' * int(count * HISTOGRAM_WIDTH / max_win))
    sys.stdout.flush()

    return -ratio

# arguments
if __name__ == '__main__':
    parser = argparse.ArgumentParser('optimize_parameters.py')
    parser.add_argument('--store-interval', type=int, default=1,
                        help='store internal state of hyper-parameter search after every <store_interval> iterations. set 0 to disable storing.')
    parser.add_argument('--resume', type=str, default=None,
                        help='resume hyper-parameter search from a file.')
    parser.add_argument('--dump-log', type=str, default=None,
                        help='open a hyper-parameter search file and dump its log.')
    parser.add_argument('--max-evals', type=int, default=MAX_EVALS,
                        help='max evaluation for hyperopt. (default: use MAX_EVALS={})'.format(MAX_EVALS))
    parser.add_argument('--num_threads', type=int, default=NUM_THREADS,
                        help='number of threads. (default: use NUM_THREADS={})'.format(NUM_THREADS))
    parser.add_argument('--num_searched_nodes', type=int, default=NUM_SEARCH_NODES,
                        help='thinking time. (default: use NUM_SEARCH_NODES={})'.format(NUM_SEARCH_NODES))
    parser.add_argument('--num_numa_nodes', type=int, default=NUM_NUMA_NODES,
                        help='Number of the NUMA nodes. (default: use NUM_NUMA_NODES={})'.format(NUM_NUMA_NODES))
    parser.add_argument('--hash', type=int, default=HASH,
                        help='Transposition table hash size. (default: use HASH={})'.format(HASH))
    parser.add_argument('--default_n_startup_jobs', type=int, default=DEFAULT_N_STARTUP_JOBS,
                        help='Number of the trials to sample on random distribution.')
    commandline_args = parser.parse_args()
    MAX_EVALS = commandline_args.max_evals
    NUM_THREADS = commandline_args.num_threads
    NUM_SEARCH_NODES = commandline_args.num_searched_nodes
    NUM_NUMA_NODES = commandline_args.num_numa_nodes
    HASH = commandline_args.hash
    DEFAULT_N_STARTUP_JOBS = commandline_args.default_n_startup_jobs

    state = HyperoptState()
    state_store_path = 'optimize_parameters.hyperopt_state.{}.pickle'.format(datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
    if commandline_args.dump_log is not None:
        state = HyperoptState.load(commandline_args.dump_log)
        state.dump_log()
        sys.exit(0)

    if commandline_args.resume is not None:
        state = HyperoptState.load(commandline_args.resume)
        START_COUNTER = state.get_n_accumulated_iterations()
        CURRENT_COUNTER = START_COUNTER

    # build environment.
    builder = YaneuraouBuilder()

    tpe._default_n_startup_jobs = DEFAULT_N_STARTUP_JOBS
    best = fmin(function, space, algo=tpe.suggest, max_evals=state.calc_max_evals(MAX_EVALS), trials=state.get_trials())
    print("best estimate parameters", best)
    for key in sorted(best.keys()):
        print("{0}={1}".format(key, str(int(best[key]))))
