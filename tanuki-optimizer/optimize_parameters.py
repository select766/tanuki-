#!/usr/bin/python
# coding:utf-8
#
# Preparation
#
# 1. Install Python 2.7.* 64-bit ver
# - Python 2.7.* 64bit
# -- http://www.python.org
#
# 2. Download the following libraries.
# - numpy-*.*.*+mkl-cp27-cp27m-win_amd64.whl
# -- http://www.lfd.uci.edu/~gohlke/pythonlibs/
# - scipy-*.*.*-cp27-none-win_amd64.whl
# -- http://www.lfd.uci.edu/~gohlke/pythonlibs/
#
# 3. Execute the following command.
# - python -m pip install --upgrade pip
# - pip install numpy-*.*.*+mkl-cp27-cp27m-win_amd64.whl scipy-*.*.*-cp27-none-win_amd64.whl hyperopt pymongo networkx pandas sklearn matplotlib
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
import cPickle as pickle
from hyperopt_state import HyperoptState

space = [
  hp.quniform('PARAM_FUTILITY_MARGIN_ALPHA_ENDING', 112, 188, 1),
  hp.quniform('PARAM_FUTILITY_MARGIN_BETA_ENDING', 150, 240, 1),
  hp.quniform('PARAM_FUTILITY_MARGIN_QUIET_ENDING', 96, 160, 1),
  hp.quniform('PARAM_FUTILITY_RETURN_DEPTH_ENDING', 5, 9, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_DEPTH_ENDING', 5, 9, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1_ENDING', 192, 300, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2_ENDING', 150, 250, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1_ENDING', 26, 44, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2_ENDING', 26, 44, 1),
  hp.quniform('PARAM_NULL_MOVE_DYNAMIC_ALPHA_ENDING', 617, 1029, 1),
  hp.quniform('PARAM_NULL_MOVE_DYNAMIC_BETA_ENDING', 50, 84, 1),
  hp.quniform('PARAM_NULL_MOVE_MARGIN_ENDING', 26, 44, 1),
  hp.quniform('PARAM_NULL_MOVE_RETURN_DEPTH_ENDING', 9, 15, 1),
  hp.quniform('PARAM_PROBCUT_DEPTH_ENDING', 3, 7, 1),
  hp.quniform('PARAM_PROBCUT_MARGIN_ENDING', 150, 250, 1),
  hp.quniform('PARAM_SINGULAR_EXTENSION_DEPTH_ENDING', 6, 10, 1),
  hp.quniform('PARAM_SINGULAR_MARGIN_ENDING', 192, 320, 1),
  hp.quniform('PARAM_SINGULAR_SEARCH_DEPTH_ALPHA_ENDING', 12, 20, 1),
  hp.quniform('PARAM_PRUNING_BY_MOVE_COUNT_DEPTH_ENDING', 12, 20, 1),
  hp.quniform('PARAM_PRUNING_BY_HISTORY_DEPTH_ENDING', 2, 4, 1),
  hp.quniform('PARAM_REDUCTION_BY_HISTORY_ENDING', 6000, 10000, 1),
  hp.quniform('PARAM_IID_MARGIN_ALPHA_ENDING', 192, 320, 1),
  hp.quniform('PARAM_RAZORING_MARGIN1_ENDING', 400, 604, 1),
  hp.quniform('PARAM_RAZORING_MARGIN2_ENDING', 427, 700, 1),
  hp.quniform('PARAM_RAZORING_MARGIN3_ENDING', 452, 700, 1),
  hp.quniform('PARAM_RAZORING_MARGIN4_ENDING', 415, 693, 1),
  hp.quniform('PARAM_REDUCTION_ALPHA_ENDING', 96, 160, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_ALPHA0_ENDING', 180, 300, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_ALPHA1_ENDING', 217, 363, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_BETA0_ENDING', 579, 967, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_BETA1_ENDING', 783, 1307, 1),
  hp.quniform('PARAM_QUIET_SEARCH_COUNT_ENDING', 48, 80, 1),
  ]

build_argument_names = ['PARAM_FUTILITY_MARGIN_ALPHA_ENDING',
  'PARAM_FUTILITY_MARGIN_BETA_ENDING',
  'PARAM_FUTILITY_MARGIN_QUIET_ENDING',
  'PARAM_FUTILITY_RETURN_DEPTH_ENDING',
  'PARAM_FUTILITY_AT_PARENT_NODE_DEPTH_ENDING',
  'PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1_ENDING',
  'PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2_ENDING',
  'PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1_ENDING',
  'PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2_ENDING',
  'PARAM_NULL_MOVE_DYNAMIC_ALPHA_ENDING',
  'PARAM_NULL_MOVE_DYNAMIC_BETA_ENDING',
  'PARAM_NULL_MOVE_MARGIN_ENDING',
  'PARAM_NULL_MOVE_RETURN_DEPTH_ENDING',
  'PARAM_PROBCUT_DEPTH_ENDING',
  'PARAM_PROBCUT_MARGIN_ENDING',
  'PARAM_SINGULAR_EXTENSION_DEPTH_ENDING',
  'PARAM_SINGULAR_MARGIN_ENDING',
  'PARAM_SINGULAR_SEARCH_DEPTH_ALPHA_ENDING',
  'PARAM_PRUNING_BY_MOVE_COUNT_DEPTH_ENDING',
  'PARAM_PRUNING_BY_HISTORY_DEPTH_ENDING',
  'PARAM_REDUCTION_BY_HISTORY_ENDING',
  'PARAM_IID_MARGIN_ALPHA_ENDING',
  'PARAM_RAZORING_MARGIN1_ENDING',
  'PARAM_RAZORING_MARGIN2_ENDING',
  'PARAM_RAZORING_MARGIN3_ENDING',
  'PARAM_RAZORING_MARGIN4_ENDING',
  'PARAM_REDUCTION_ALPHA_ENDING',
  'PARAM_FUTILITY_MOVE_COUNT_ALPHA0_ENDING',
  'PARAM_FUTILITY_MOVE_COUNT_ALPHA1_ENDING',
  'PARAM_FUTILITY_MOVE_COUNT_BETA0_ENDING',
  'PARAM_FUTILITY_MOVE_COUNT_BETA1_ENDING',
  'PARAM_QUIET_SEARCH_COUNT_ENDING',]

START_COUNTER = 0
CURRENT_COUNTER = 0
MAX_EVALS = 10000
START_TIME_SEC = time.time()
EVAL_DIR = r'eval\2017-01-24-19-49-24\500000000'
ENGINE1 = r'YaneuraOu-2017-early-master.exe'
ENGINE2 = r'YaneuraOu-2017-early-slave.exe'
NUM_THREADS = 24
THINKING_TIME_MS = 10000
NUM_NUMA_NODES = 1
HASH = 256
HISTOGRAM_WIDTH = 80


class YaneuraouBuilder(object):
  FILENAME = 'param/2017-early-param-master.h'
  def __init__(self):
    pass

  def clean(self):
    try:
      os.remove(self.FILENAME)
    except WindowsError:
      pass

  def build(self, args):
    with open(self.FILENAME, 'w') as f:
      f.write('#ifndef _2017_EARLY_PARAMETERS_\n')
      f.write('#define _2017_EARLY_PARAMETERS_\n')
      for key, val in zip(build_argument_names, args):
        f.write('PARAM_DEFINE {0} = {1};\n'.format(key, str(int(val))))
      f.write('''
// Created at: 2017-03-31 11:17:05
// Log: optimize_parameters.hyperopt_state.20170324_105241.b10000.pickle
// Parameters CSV: param\parameters.opening.csv
// n_iterations_to_use = 1000
// minima_method = Data

// |--@---+-----------| raw=122.999999999, min=100, max=240 default=150
PARAM_DEFINE PARAM_FUTILITY_MARGIN_ALPHA_OPENING = 123;

// |---------@--+-----| raw=179.999999997, min=100, max=240 default=200
PARAM_DEFINE PARAM_FUTILITY_MARGIN_BETA_OPENING = 180;

// |--------@---+-----| raw=106.999999998, min=50, max=160 default=128
PARAM_DEFINE PARAM_FUTILITY_MARGIN_QUIET_OPENING = 107;

// |----@-------------| raw=6.999999995, min=5, max=13 default=7
PARAM_DEFINE PARAM_FUTILITY_RETURN_DEPTH_OPENING = 7;

// |----@-------------| raw=6.999999995, min=5, max=13 default=7
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_DEPTH_OPENING = 7;

// |----------@--+----| raw=218.999999998, min=100, max=300 default=256
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1_OPENING = 219;

// |-----------+-@----| raw=244.99999999, min=0, max=300 default=200
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2_OPENING = 245;

// |--------+@--------| raw=36.9999999939, min=20, max=50 default=35
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1_OPENING = 37;

// |---@--+-----------| raw=27.9999999989, min=20, max=60 default=35
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2_OPENING = 28;

// |---@-+------------| raw=677.999999999, min=500, max=1500 default=823
PARAM_DEFINE PARAM_NULL_MOVE_DYNAMIC_ALPHA_OPENING = 678;

// |-----@------------| raw=65.9999999952, min=50, max=100 default=67
PARAM_DEFINE PARAM_NULL_MOVE_DYNAMIC_BETA_OPENING = 66;

// |--------+--@------| raw=42.9999999906, min=10, max=60 default=35
PARAM_DEFINE PARAM_NULL_MOVE_MARGIN_OPENING = 43;

// |------------+--@--| raw=13.9999999917, min=4, max=15 default=12
PARAM_DEFINE PARAM_NULL_MOVE_RETURN_DEPTH_OPENING = 14;

// |----+--@----------| raw=5.9999999925, min=3, max=10 default=5
PARAM_DEFINE PARAM_PROBCUT_DEPTH_OPENING = 6;

// |-----@--+---------| raw=166.999999998, min=100, max=300 default=200
PARAM_DEFINE PARAM_PROBCUT_MARGIN_OPENING = 167;

// |-------@----------| raw=7.999999995, min=4, max=13 default=8
PARAM_DEFINE PARAM_SINGULAR_EXTENSION_DEPTH_OPENING = 8;

// |--------+--@------| raw=303.999999991, min=128, max=400 default=256
PARAM_DEFINE PARAM_SINGULAR_MARGIN_OPENING = 304;

// |-----+-@----------| raw=18.9999999913, min=8, max=32 default=16
PARAM_DEFINE PARAM_SINGULAR_SEARCH_DEPTH_ALPHA_OPENING = 19;

// |-----+-@----------| raw=18.9999999913, min=8, max=32 default=16
PARAM_DEFINE PARAM_PRUNING_BY_MOVE_COUNT_DEPTH_OPENING = 19;

// |+@----------------| raw=3.99999999, min=2, max=32 default=3
PARAM_DEFINE PARAM_PRUNING_BY_HISTORY_DEPTH_OPENING = 4;

// |-----@+-----------| raw=7792.0, min=4000, max=15000 default=8000
PARAM_DEFINE PARAM_REDUCTION_BY_HISTORY_OPENING = 7792;

// |--------+-@-------| raw=279.999999993, min=128, max=384 default=256
PARAM_DEFINE PARAM_IID_MARGIN_ALPHA_OPENING = 280;

// |@---+-------------| raw=413.999999999, min=400, max=700 default=483
PARAM_DEFINE PARAM_RAZORING_MARGIN1_OPENING = 414;

// |--@------+--------| raw=448.999999999, min=400, max=700 default=570
PARAM_DEFINE PARAM_RAZORING_MARGIN2_OPENING = 449;

// |-----@-----+------| raw=488.999999999, min=400, max=700 default=603
PARAM_DEFINE PARAM_RAZORING_MARGIN3_OPENING = 489;

// |--------+@--------| raw=566.999999995, min=400, max=700 default=554
PARAM_DEFINE PARAM_RAZORING_MARGIN4_OPENING = 567;

// |-----+-@----------| raw=149.999999991, min=64, max=256 default=128
PARAM_DEFINE PARAM_REDUCTION_ALPHA_OPENING = 150;

// |------@-----------| raw=242.999999995, min=150, max=400 default=240
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_ALPHA0_OPENING = 243;

// |------@--+--------| raw=239.999999998, min=150, max=400 default=290
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_ALPHA1_OPENING = 240;

// |--@+--------------| raw=735.999999996, min=500, max=2000 default=773
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_BETA0_OPENING = 736;

// |-----@+-----------| raw=995.999999996, min=500, max=2000 default=1045
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_BETA1_OPENING = 996;

// |-----+@-----------| raw=70.9999999928, min=32, max=128 default=64
PARAM_DEFINE PARAM_QUIET_SEARCH_COUNT_OPENING = 71;

PARAM_DEFINE PARAM_QSEARCH_MATE1_OPENING = 1;
PARAM_DEFINE PARAM_QSEARCH_MATE1_ENDING = 1;
PARAM_DEFINE PARAM_SEARCH_MATE1_OPENING = 1;
PARAM_DEFINE PARAM_SEARCH_MATE1_ENDING = 1;
PARAM_DEFINE PARAM_WEAK_MATE_PLY_OPENING = 2;
PARAM_DEFINE PARAM_WEAK_MATE_PLY_ENDING = 1;
''')
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
  print(args)
  sys.stdout.flush()

  if START_COUNTER < CURRENT_COUNTER:
    current_time_sec = time.time()
    delta = current_time_sec - START_TIME_SEC
    sec_per_one = delta / (CURRENT_COUNTER - START_COUNTER)
    remaining = datetime.timedelta(seconds=sec_per_one * (MAX_EVALS - CURRENT_COUNTER))
    print(CURRENT_COUNTER, '/', MAX_EVALS, str(remaining))
    sys.stdout.flush()
  CURRENT_COUNTER += 1

  builder.clean()
  builder.build(args)

  engine_invoker_args = [
    'TanukiColiseum.exe',
    '--engine1', ENGINE1,
    '--engine2', ENGINE2,
    '--eval1', EVAL_DIR,
    '--eval2', EVAL_DIR,
    '--num_concurrent_games', str(NUM_THREADS),
    '--num_games', str(NUM_THREADS),
    '--hash', str(HASH),
    '--time', str(THINKING_TIME_MS),
    '--num_numa_nodes', str(NUM_NUMA_NODES)]

  print(engine_invoker_args)
  sys.stdout.flush()

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
  print('ratio={0}'.format(ratio))
  sys.stdout.flush()

  builder.kill(ENGINE1)
  builder.kill(ENGINE2)

  global state
  state.record_iteration(args=args,
      output=stdoutdata,
      lose=lose,
      draw=draw,
      win=win,)
  if commandline_args.store_interval > 0 and state.get_n_accumulated_iterations() % commandline_args.store_interval == 0:
    state.save(state_store_path)

  # Show the histogram.
  hist = [0] * (NUM_THREADS + 1)
  max_win = 0
  for record in state.iteration_logs:
    win = int(record['win'])
    hist[win] += 1
    max_win = max(max_win, hist[win])

  for win, count in enumerate(hist):
    print('{0:4d} '.format(win) + '*' * (count * HISTOGRAM_WIDTH / max_win))
  sys.stdout.flush()

  return -ratio

# arguments
if __name__ == '__main__':
  parser = argparse.ArgumentParser('optimize_parameters.py')
  parser.add_argument('--store-interval', type=int, default=1,
      help=u'store internal state of hyper-parameter search after every <store_interval> iterations. set 0 to disable storing.')
  parser.add_argument('--resume', type=str, default=None,
      help=u'resume hyper-parameter search from a file.')
  parser.add_argument('--dump-log', type=str, default=None,
      help=u'open a hyper-parameter search file and dump its log.')
  parser.add_argument('--max-evals', type=int, default=MAX_EVALS,
      help=u'max evaluation for hyperopt. (default: use MAX_EVALS={})'.format(MAX_EVALS))
  parser.add_argument('--num_threads', type=int, default=NUM_THREADS,
      help=u'number of threads. (default: use NUM_THREADS={})'.format(NUM_THREADS))
  parser.add_argument('--thinking_time_ms', type=int, default=THINKING_TIME_MS,
      help=u'thinking time. (default: use THINKING_TIME_MS={})'.format(THINKING_TIME_MS))
  parser.add_argument('--num_numa_nodes', type=int, default=NUM_NUMA_NODES,
      help=u'Number of the NUMA nodes. (default: use NUM_NUMA_NODES={})'.format(NUM_NUMA_NODES))
  parser.add_argument('--hash', type=int, default=HASH,
      help=u'Transposition table hash size. (default: use HASH={})'.format(HASH))
  commandline_args = parser.parse_args()
  MAX_EVALS = commandline_args.max_evals
  NUM_THREADS = commandline_args.num_threads
  THINKING_TIME_MS = commandline_args.thinking_time_ms
  NUM_NUMA_NODES = commandline_args.num_numa_nodes
  HASH = commandline_args.hash

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

  best = fmin(function, space, algo=tpe.suggest, max_evals=state.calc_max_evals(MAX_EVALS), trials=state.get_trials())
  print("best estimate parameters", best)
  for key in sorted(best.keys()):
    print("{0}={1}".format(key, str(int(best[key]))))
