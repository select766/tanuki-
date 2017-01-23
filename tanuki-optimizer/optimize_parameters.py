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
  hp.qnormal('PARAM_FUTILITY_MARGIN_ALPHA_OPENING', 150, 23.3333333333, 1),
  hp.qnormal('PARAM_FUTILITY_MARGIN_ALPHA_ENDING', 150, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_MARGIN_BETA_OPENING', 200, 23.3333333333, 1),
  hp.qnormal('PARAM_FUTILITY_MARGIN_BETA_ENDING', 200, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_MARGIN_QUIET_OPENING', 128, 18.3333333333, 1),
  hp.qnormal('PARAM_FUTILITY_MARGIN_QUIET_ENDING', 128, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_RETURN_DEPTH_OPENING', 7, 1.33333333333, 1),
  hp.qnormal('PARAM_FUTILITY_RETURN_DEPTH_ENDING', 7, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_AT_PARENT_NODE_DEPTH_OPENING', 7, 1.33333333333, 1),
  hp.qnormal('PARAM_FUTILITY_AT_PARENT_NODE_DEPTH_ENDING', 7, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1_OPENING', 256, 33.3333333333, 1),
  hp.qnormal('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1_ENDING', 256, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2_OPENING', 200, 50.0, 1),
  hp.qnormal('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2_ENDING', 200, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1_OPENING', 35, 5.0, 1),
  hp.qnormal('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1_ENDING', 35, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2_OPENING', 35, 6.66666666667, 1),
  hp.qnormal('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2_ENDING', 35, 1e-08, 1),
  hp.qnormal('PARAM_NULL_MOVE_DYNAMIC_ALPHA_OPENING', 823, 166.666666667, 1),
  hp.qnormal('PARAM_NULL_MOVE_DYNAMIC_ALPHA_ENDING', 823, 1e-08, 1),
  hp.qnormal('PARAM_NULL_MOVE_DYNAMIC_BETA_OPENING', 67, 8.33333333333, 1),
  hp.qnormal('PARAM_NULL_MOVE_DYNAMIC_BETA_ENDING', 67, 1e-08, 1),
  hp.qnormal('PARAM_NULL_MOVE_MARGIN_OPENING', 35, 8.33333333333, 1),
  hp.qnormal('PARAM_NULL_MOVE_MARGIN_ENDING', 35, 1e-08, 1),
  hp.qnormal('PARAM_NULL_MOVE_RETURN_DEPTH_OPENING', 12, 1.83333333333, 1),
  hp.qnormal('PARAM_NULL_MOVE_RETURN_DEPTH_ENDING', 12, 1e-08, 1),
  hp.qnormal('PARAM_PROBCUT_DEPTH_OPENING', 5, 1.16666666667, 1),
  hp.qnormal('PARAM_PROBCUT_DEPTH_ENDING', 5, 1e-08, 1),
  hp.qnormal('PARAM_PROBCUT_MARGIN_OPENING', 200, 33.3333333333, 1),
  hp.qnormal('PARAM_PROBCUT_MARGIN_ENDING', 200, 1e-08, 1),
  hp.qnormal('PARAM_SINGULAR_EXTENSION_DEPTH_OPENING', 8, 1.5, 1),
  hp.qnormal('PARAM_SINGULAR_EXTENSION_DEPTH_ENDING', 8, 1e-08, 1),
  hp.qnormal('PARAM_SINGULAR_MARGIN_OPENING', 256, 45.3333333333, 1),
  hp.qnormal('PARAM_SINGULAR_MARGIN_ENDING', 256, 1e-08, 1),
  hp.qnormal('PARAM_SINGULAR_SEARCH_DEPTH_ALPHA_OPENING', 16, 4.0, 1),
  hp.qnormal('PARAM_SINGULAR_SEARCH_DEPTH_ALPHA_ENDING', 16, 1e-08, 1),
  hp.qnormal('PARAM_PRUNING_BY_MOVE_COUNT_DEPTH_OPENING', 16, 4.0, 1),
  hp.qnormal('PARAM_PRUNING_BY_MOVE_COUNT_DEPTH_ENDING', 16, 1e-08, 1),
  hp.qnormal('PARAM_PRUNING_BY_HISTORY_DEPTH_OPENING', 3, 5.0, 1),
  hp.qnormal('PARAM_PRUNING_BY_HISTORY_DEPTH_ENDING', 3, 1e-08, 1),
  hp.qnormal('PARAM_REDUCTION_BY_HISTORY_OPENING', 8000, 1833.33333333, 1),
  hp.qnormal('PARAM_REDUCTION_BY_HISTORY_ENDING', 8000, 1e-08, 1),
  hp.qnormal('PARAM_IID_MARGIN_ALPHA_OPENING', 256, 42.6666666667, 1),
  hp.qnormal('PARAM_IID_MARGIN_ALPHA_ENDING', 256, 1e-08, 1),
  hp.qnormal('PARAM_RAZORING_MARGIN1_OPENING', 483, 50.0, 1),
  hp.qnormal('PARAM_RAZORING_MARGIN1_ENDING', 483, 1e-08, 1),
  hp.qnormal('PARAM_RAZORING_MARGIN2_OPENING', 570, 50.0, 1),
  hp.qnormal('PARAM_RAZORING_MARGIN2_ENDING', 570, 1e-08, 1),
  hp.qnormal('PARAM_RAZORING_MARGIN3_OPENING', 603, 50.0, 1),
  hp.qnormal('PARAM_RAZORING_MARGIN3_ENDING', 603, 1e-08, 1),
  hp.qnormal('PARAM_RAZORING_MARGIN4_OPENING', 554, 50.0, 1),
  hp.qnormal('PARAM_RAZORING_MARGIN4_ENDING', 554, 1e-08, 1),
  hp.qnormal('PARAM_REDUCTION_ALPHA_OPENING', 128, 32.0, 1),
  hp.qnormal('PARAM_REDUCTION_ALPHA_ENDING', 128, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_MOVE_COUNT_ALPHA0_OPENING', 240, 41.6666666667, 1),
  hp.qnormal('PARAM_FUTILITY_MOVE_COUNT_ALPHA0_ENDING', 240, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_MOVE_COUNT_ALPHA1_OPENING', 290, 41.6666666667, 1),
  hp.qnormal('PARAM_FUTILITY_MOVE_COUNT_ALPHA1_ENDING', 290, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_MOVE_COUNT_BETA0_OPENING', 773, 250.0, 1),
  hp.qnormal('PARAM_FUTILITY_MOVE_COUNT_BETA0_ENDING', 773, 1e-08, 1),
  hp.qnormal('PARAM_FUTILITY_MOVE_COUNT_BETA1_OPENING', 1045, 250.0, 1),
  hp.qnormal('PARAM_FUTILITY_MOVE_COUNT_BETA1_ENDING', 1045, 1e-08, 1),
  hp.qnormal('PARAM_QUIET_SEARCH_COUNT_OPENING', 64, 16.0, 1),
  hp.qnormal('PARAM_QUIET_SEARCH_COUNT_ENDING', 64, 1e-08, 1),
  hp.qnormal('PARAM_QSEARCH_MATE1_OPENING', 1, 0.166666666667, 1),
  hp.qnormal('PARAM_QSEARCH_MATE1_ENDING', 1, 1e-08, 1),
  hp.qnormal('PARAM_SEARCH_MATE1_OPENING', 1, 0.166666666667, 1),
  hp.qnormal('PARAM_SEARCH_MATE1_ENDING', 1, 1e-08, 1),
  hp.qnormal('PARAM_WEAK_MATE_PLY_OPENING', 1, 0.666666666667, 1),
  hp.qnormal('PARAM_WEAK_MATE_PLY_ENDING', 1, 1e-08, 1),
  ]

build_argument_names = [
  'PARAM_FUTILITY_MARGIN_ALPHA_OPENING',
  'PARAM_FUTILITY_MARGIN_ALPHA_ENDING',
  'PARAM_FUTILITY_MARGIN_BETA_OPENING',
  'PARAM_FUTILITY_MARGIN_BETA_ENDING',
  'PARAM_FUTILITY_MARGIN_QUIET_OPENING',
  'PARAM_FUTILITY_MARGIN_QUIET_ENDING',
  'PARAM_FUTILITY_RETURN_DEPTH_OPENING',
  'PARAM_FUTILITY_RETURN_DEPTH_ENDING',
  'PARAM_FUTILITY_AT_PARENT_NODE_DEPTH_OPENING',
  'PARAM_FUTILITY_AT_PARENT_NODE_DEPTH_ENDING',
  'PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1_OPENING',
  'PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1_ENDING',
  'PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2_OPENING',
  'PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2_ENDING',
  'PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1_OPENING',
  'PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1_ENDING',
  'PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2_OPENING',
  'PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2_ENDING',
  'PARAM_NULL_MOVE_DYNAMIC_ALPHA_OPENING',
  'PARAM_NULL_MOVE_DYNAMIC_ALPHA_ENDING',
  'PARAM_NULL_MOVE_DYNAMIC_BETA_OPENING',
  'PARAM_NULL_MOVE_DYNAMIC_BETA_ENDING',
  'PARAM_NULL_MOVE_MARGIN_OPENING',
  'PARAM_NULL_MOVE_MARGIN_ENDING',
  'PARAM_NULL_MOVE_RETURN_DEPTH_OPENING',
  'PARAM_NULL_MOVE_RETURN_DEPTH_ENDING',
  'PARAM_PROBCUT_DEPTH_OPENING',
  'PARAM_PROBCUT_DEPTH_ENDING',
  'PARAM_PROBCUT_MARGIN_OPENING',
  'PARAM_PROBCUT_MARGIN_ENDING',
  'PARAM_SINGULAR_EXTENSION_DEPTH_OPENING',
  'PARAM_SINGULAR_EXTENSION_DEPTH_ENDING',
  'PARAM_SINGULAR_MARGIN_OPENING',
  'PARAM_SINGULAR_MARGIN_ENDING',
  'PARAM_SINGULAR_SEARCH_DEPTH_ALPHA_OPENING',
  'PARAM_SINGULAR_SEARCH_DEPTH_ALPHA_ENDING',
  'PARAM_PRUNING_BY_MOVE_COUNT_DEPTH_OPENING',
  'PARAM_PRUNING_BY_MOVE_COUNT_DEPTH_ENDING',
  'PARAM_PRUNING_BY_HISTORY_DEPTH_OPENING',
  'PARAM_PRUNING_BY_HISTORY_DEPTH_ENDING',
  'PARAM_REDUCTION_BY_HISTORY_OPENING',
  'PARAM_REDUCTION_BY_HISTORY_ENDING',
  'PARAM_IID_MARGIN_ALPHA_OPENING',
  'PARAM_IID_MARGIN_ALPHA_ENDING',
  'PARAM_RAZORING_MARGIN1_OPENING',
  'PARAM_RAZORING_MARGIN1_ENDING',
  'PARAM_RAZORING_MARGIN2_OPENING',
  'PARAM_RAZORING_MARGIN2_ENDING',
  'PARAM_RAZORING_MARGIN3_OPENING',
  'PARAM_RAZORING_MARGIN3_ENDING',
  'PARAM_RAZORING_MARGIN4_OPENING',
  'PARAM_RAZORING_MARGIN4_ENDING',
  'PARAM_REDUCTION_ALPHA_OPENING',
  'PARAM_REDUCTION_ALPHA_ENDING',
  'PARAM_FUTILITY_MOVE_COUNT_ALPHA0_OPENING',
  'PARAM_FUTILITY_MOVE_COUNT_ALPHA0_ENDING',
  'PARAM_FUTILITY_MOVE_COUNT_ALPHA1_OPENING',
  'PARAM_FUTILITY_MOVE_COUNT_ALPHA1_ENDING',
  'PARAM_FUTILITY_MOVE_COUNT_BETA0_OPENING',
  'PARAM_FUTILITY_MOVE_COUNT_BETA0_ENDING',
  'PARAM_FUTILITY_MOVE_COUNT_BETA1_OPENING',
  'PARAM_FUTILITY_MOVE_COUNT_BETA1_ENDING',
  'PARAM_QUIET_SEARCH_COUNT_OPENING',
  'PARAM_QUIET_SEARCH_COUNT_ENDING',
  'PARAM_QSEARCH_MATE1_OPENING',
  'PARAM_QSEARCH_MATE1_ENDING',
  'PARAM_SEARCH_MATE1_OPENING',
  'PARAM_SEARCH_MATE1_ENDING',
  'PARAM_WEAK_MATE_PLY_OPENING',
  'PARAM_WEAK_MATE_PLY_ENDING',
  ]

START_COUNTER = 0
CURRENT_COUNTER = 0
MAX_EVALS = 10000
START_TIME_SEC = time.time()
EVAL_DIR = 'eval/2017-01-10-20-31-40/1000000000'
ENGINE1 = 'YaneuraOu-2017-early-master.exe'
ENGINE2 = 'YaneuraOu-2017-early-slave.exe'
NUM_THREADS = 24
THINKING_TIME_MS = 10000
NUM_NUMA_NODES = 1


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
      f.write('#endif\n')

  def kill(self, process_name):
    subprocess.call(['taskkill', '/T', '/F', '/IM', process_name])


def function(args):
  print('-' * 78)

  print(datetime.datetime.today().strftime("%Y-%m-%d %H:%M:%S"))

  global START_COUNTER
  global CURRENT_COUNTER
  global NUM_THREADS
  global THINKING_TIME_MS
  print(args)
  sys.stdout.flush()

  if START_COUNTER < CURRENT_COUNTER:
    current_time_sec = time.time()
    delta = current_time_sec - START_TIME_SEC
    sec_per_one = delta / (CURRENT_COUNTER - START_COUNTER)
    remaining = datetime.timedelta(seconds=sec_per_one*(MAX_EVALS-CURRENT_COUNTER))
    print(CURRENT_COUNTER, '/', MAX_EVALS, str(remaining))
    sys.stdout.flush()
  CURRENT_COUNTER += 1

  builder.clean()
  builder.build(args)

  engine_invoker_args = [
    'C:\\Python27\\python.exe',
    '..\script\engine_invoker5.py',
    # hakubishin-private\exe以下から実行していると仮定する
    'home:{0}'.format(os.getcwd()),
    # {home}\exeからの相対パスに変換する
    'engine1:{0}'.format(os.path.relpath(os.path.abspath(ENGINE1), os.path.join(os.getcwd(), 'exe'))),
    # {home}\evalからの相対パスに変換する
    'eval1:{0}'.format(os.path.relpath(os.path.abspath(EVAL_DIR), os.path.join(os.getcwd(), 'eval'))),
    'engine2:{0}'.format(os.path.relpath(os.path.abspath(ENGINE2), os.path.join(os.getcwd(), 'exe'))),
    'eval2:{0}'.format(os.path.relpath(os.path.abspath(EVAL_DIR), os.path.join(os.getcwd(), 'eval'))),
    'cores:{0}'.format(NUM_THREADS),
    'loop:{0}'.format(NUM_THREADS),
    'cpu:{0}'.format(NUM_NUMA_NODES),
    'engine_threads:1',
    'hash1:1024',
    'hash2:1024',
    'time:b{0}'.format(THINKING_TIME_MS),
    'rand_book:1'
    ]
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
   ratio = win / (lose + draw + win)
  print('ratio={0}'.format(ratio))
  sys.stdout.flush()

  builder.kill(ENGINE1)
  builder.kill(ENGINE2)

  global state
  state.record_iteration(
      args=args,
      output=stdoutdata,
      lose=lose,
      draw=draw,
      win=win,
      )
  if commandline_args.store_interval > 0 and state.get_n_accumulated_iterations() % commandline_args.store_interval == 0:
    state.save(state_store_path)

  return -ratio

# arguments
if __name__=='__main__':
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
  parser.add_argument('--num_numa_nodes', type=int, default=1,
      help=u'Number of the NUMA nodes. (default: use NUM_NUMA_NODES={})'.format(NUM_NUMA_NODES))
  commandline_args = parser.parse_args()
  MAX_EVALS = commandline_args.max_evals
  NUM_THREADS = commandline_args.num_threads
  THINKING_TIME_MS = commandline_args.thinking_time_ms
  NUM_NUMA_NODES = commandline_args.num_numa_nodes

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

