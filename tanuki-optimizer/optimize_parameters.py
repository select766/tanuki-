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
from hyperopt_state import HyperoptState
from math import log
import argparse
import codecs
import collections
import datetime
import os
import pickle as pickle
import re
import shutil
import subprocess
import sys
import time


Parameter = collections.namedtuple('Parameter', ('name', 'min', 'max', 'default_value'))


BUILDER = None
COMMANDLINE_ARGS = None
CURRENT_COUNTER = 0
HISTOGRAM_WIDTH = 80
PARAMETERS = None
START_COUNTER = 0
START_TIME_SEC = time.time()
STATE = None
STATE_STORE_PATH = None


def parse_parameters(original_parameter_file_path):
    parameters = list()
    with codecs.open(original_parameter_file_path, 'r', 'utf-8') as f:
        min_value = None
        max_value = None
        name = None
        default_value = None
        for line in f:
            for m in re.finditer('min:(.+?),max:(.+?),', line):
                if min_value or max_value:
                    raise Exception('multiple min_value and max_value were found...')
                min_value = m.group(1)
                max_value = m.group(2)

            for m in re.finditer('PARAM_DEFINE (.+?) = (.+?);', line):
                if not min_value and not_max_value:
                    raise Exception('min_value and max_value were not found...')
                if name and default_value:
                    raise Exception('multiple name and default_value were found...')
                name = m.group(1)
                default_value = m.group(2)

            if (min_value or max_value) and name:
                if name not in ['PARAM_RAZORING_MARGIN1', 'PARAM_QSEARCH_MATE1', 'PARAM_SEARCH_MATE1', 'PARAM_WEAK_MATE_PLY']:
                    parameters.append(Parameter(name, int(eval(min_value)), int(eval(max_value)), int(eval(default_value))))
                min_value = None
                max_value = None
                name = None
                default_value = None
    return parameters


class YaneuraouBuilder(object):
    def __init__(self):
        pass

    def clean(self):
        global COMMANDLINE_ARGS
        try:
            os.remove(COMMANDLINE_ARGS.temporary_parameter_file_path)
        except WindowsError:
            pass

    def build(self, args):
        global COMMANDLINE_ARGS
        global PARAMETERS
        with open(COMMANDLINE_ARGS.temporary_parameter_file_path, 'w') as f:
            f.write('#ifndef _2018_OTAFUKU_PARAMETERS_\n')
            f.write('#define _2018_OTAFUKU_PARAMETERS_\n')
            for key, val in zip(PARAMETERS, args):
                f.write('PARAM_DEFINE {0} = {1};\n'.format(key.name, str(int(val))))
            f.write('PARAM_DEFINE PARAM_RAZORING_MARGIN1 = 0;\n')
            f.write('PARAM_DEFINE PARAM_QSEARCH_MATE1 = 1;\n')
            f.write('PARAM_DEFINE PARAM_SEARCH_MATE1 = 1;\n')
            f.write('PARAM_DEFINE PARAM_WEAK_MATE_PLY = 1;\n')
            f.write('#endif\n')

    def kill(self, process_name):
        subprocess.run(['taskkill', '/T', '/F', '/IM', process_name])


def function(args):
    global BUILDER
    global COMMANDLINE_ARGS
    global CURRENT_COUNTER
    global START_COUNTER
    global START_TIME_SEC

    print('-' * 78, flush=True)
    print(datetime.datetime.today().strftime("%Y-%m-%d %H:%M:%S"), flush=True)
    print(args, flush=True)

    if START_COUNTER < CURRENT_COUNTER:
        current_time_sec = time.time()
        delta = current_time_sec - START_TIME_SEC
        sec_per_one = delta / (CURRENT_COUNTER - START_COUNTER)
        remaining = datetime.timedelta(seconds=sec_per_one * (COMMANDLINE_ARGS.max_evals - CURRENT_COUNTER))
        print(CURRENT_COUNTER, '/', COMMANDLINE_ARGS.max_evals, str(remaining), flush=True)
    CURRENT_COUNTER += 1

    BUILDER.clean()
    BUILDER.build(args)

    engine_invoker_args = [
        COMMANDLINE_ARGS.tanuki_coliseum_file_path,
        '--engine1', COMMANDLINE_ARGS.engine1,
        '--engine2', COMMANDLINE_ARGS.engine2,
        '--eval1', COMMANDLINE_ARGS.eval_dir1,
        '--eval2', COMMANDLINE_ARGS.eval_dir2,
        '--num_concurrent_games', str(COMMANDLINE_ARGS.num_threads),
        '--num_games', str(COMMANDLINE_ARGS.num_threads),
        '--hash', str(COMMANDLINE_ARGS.hash),
        '--nodes1', str(COMMANDLINE_ARGS.num_searched_nodes),
        '--nodes2', str(COMMANDLINE_ARGS.num_searched_nodes),
        '--num_numa_nodes', str(COMMANDLINE_ARGS.num_numa_nodes),
        '--num_book_moves1', '0',
        '--num_book_moves2', '0',
        '--book_file_name1', 'no_book',
        '--book_file_name2', 'no_book',
        '--num_book_moves', '24',
        '--sfen_file_name', COMMANDLINE_ARGS.sfen_file_path,
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

    completed_process = subprocess.run(engine_invoker_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if completed_process.stdout:
        print('-' * 10 + ' stdout ' + '-' * 10, flush=True)
        print(completed_process.stdout, flush=True)
    if completed_process.stderr:
        print('-' * 10 + ' stderr ' + '-' * 10, flush=True)
        print(completed_process.stderr, flush=True)
    if completed_process.returncode:
        sys.exit('Failed to execute engine_invoker...')

    win = 0
    draw = 0
    lose = 0
    for match in re.compile(r'(\d+),(\d+),(\d+)').finditer(completed_process.stdout):
        win = float(match.group(1))
        draw = float(match.group(2))
        lose = float(match.group(3))

    ratio = 0.0
    if lose + draw + win > 0.1:
     ratio = (win * 1.0 + draw * 0.5 + lose * 0.0) / (lose + draw + win)
    print('ratio={0}'.format(ratio), flush=True)

    BUILDER.kill(os.path.split(COMMANDLINE_ARGS.engine1)[1])
    BUILDER.kill(os.path.split(COMMANDLINE_ARGS.engine2)[1])

    global STATE
    STATE.record_iteration(args=args,
            output=completed_process.stdout,
            lose=lose,
            draw=draw,
            win=win,)
    if COMMANDLINE_ARGS.store_interval > 0 and STATE.get_n_accumulated_iterations() % COMMANDLINE_ARGS.store_interval == 0:
        print('Saveing state...', flush=True)
        STATE.save(STATE_STORE_PATH)
        print('Saved state...', flush=True)

    # Show the histogram.
    hist = [0] * (COMMANDLINE_ARGS.num_threads + 1)
    max_win = 0
    for record in STATE.iteration_logs:
        win = int(record['win'])
        hist[win] += 1
        max_win = max(max_win, hist[win])

    for win, count in enumerate(hist):
        print('{0:4d} '.format(win) + '*' * int(count * HISTOGRAM_WIDTH / max_win), flush=True)

    return -ratio


def main():
    global BUILDER
    global COMMANDLINE_ARGS
    global PARAMETERS
    global STATE
    global STATE_STORE_PATH

    parser = argparse.ArgumentParser('optimize_parameters.py')
    parser.add_argument('--store_interval', type=int, default=1,
                        help='store internal state of hyper-parameter search after every <store_interval> iterations. set 0 to disable storing.')
    parser.add_argument('--resume', type=str, default=None,
                        help='resume hyper-parameter search from a file.')
    parser.add_argument('--dump_log', type=str, default=None,
                        help='open a hyper-parameter search file and dump its log.')
    parser.add_argument('--max_evals', type=int, default=1000,
                        help='max evaluation for hyperopt.')
    parser.add_argument('--num_threads', type=int, default=48,
                        help='number of threads.')
    parser.add_argument('--num_searched_nodes', type=int, default=5000000,
                        help='Number of searched nodes.')
    parser.add_argument('--num_numa_nodes', type=int, default=2,
                        help='Number of the NUMA nodes.')
    parser.add_argument('--hash', type=int, default=1024,
                        help='Transposition table hash size.')
    parser.add_argument('--default_n_startup_jobs', type=int, default=200,
                        help='Number of the trials to sample on random distribution.')
    parser.add_argument('--eval_dir1', type=str, default=r'C:\home\nodchip\hakubishin-private\exe\eval',
                        help='Eval directory path for the engine 1.')
    parser.add_argument('--eval_dir2', type=str, default=r'C:\home\nodchip\hakubishin-private\exe\eval',
                        help='Eval directory path for the engine 2.')
    parser.add_argument('--engine1', type=str, default=r'C:\home\nodchip\hakubishin-private\exe\YaneuraOu-by-gcc.exe',
                        help='Engine binary path for the engine 1.')
    parser.add_argument('--engine2', type=str, default=r'C:\home\nodchip\hakubishin-private\exe\YaneuraOu-by-gcc.original.exe',
                        help='Engine binary path for the engine 2.')
    parser.add_argument('--temporary_parameter_file_path', type=str, default=r'C:\home\nodchip\hakubishin-private\exe\param\2018-otafuku-param.h',
                        help='Temporary parameter file path.')
    parser.add_argument('--original_parameter_file_path', type=str, default=r'C:\home\nodchip\hakubishin-private\exe\param\2018-otafuku-param.original.h',
                        help='Original parameter file path.')
    parser.add_argument('--tanuki_coliseum_file_path', type=str, default=r'C:\home\nodchip\hakubishin-private\TanukiColiseum\bin\release\TanukiColiseum.exe',
                        help='TanukiColiseum file path.')
    parser.add_argument('--sfen_file_path', type=str, default=r'C:\home\nodchip\hakubishin-private\exe\records2016_10818.sfen',
                        help='Sfen file path.')

    COMMANDLINE_ARGS = parser.parse_args()

    PARAMETERS = parse_parameters(COMMANDLINE_ARGS.original_parameter_file_path)
    space = [hp.quniform(parameter.name, parameter.min, parameter.max, 1)
             for parameter in PARAMETERS]

    STATE = HyperoptState()
    STATE_STORE_PATH = 'optimize_parameters.hyperopt_state.{}.pickle'.format(datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
    if COMMANDLINE_ARGS.dump_log is not None:
        STATE = HyperoptState.load(commandline_args.dump_log)
        STATE.dump_log()
        sys.exit(0)

    if COMMANDLINE_ARGS.resume is not None:
        STATE = HyperoptState.load(commandline_args.resume)
        START_COUNTER = STATE.get_n_accumulated_iterations()
        CURRENT_COUNTER = START_COUNTER

    # build environment.
    BUILDER = YaneuraouBuilder()

    tpe._default_n_startup_jobs = COMMANDLINE_ARGS.default_n_startup_jobs
    best = fmin(function, space, algo=tpe.suggest, max_evals=STATE.calc_max_evals(COMMANDLINE_ARGS.max_evals), trials=STATE.get_trials())
    print("best estimate parameters", best, flush=True)
    for key in sorted(best.keys()):
        print("{0}={1}".format(key, str(int(best[key]))), flush=True)


# arguments
if __name__ == '__main__':
    main()
