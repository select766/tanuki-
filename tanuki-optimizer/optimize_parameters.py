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

space = [
  hp.quniform('PARAM_FUTILITY_MARGIN_ALPHA_OPENING', 100, 240, 1),
  hp.quniform('PARAM_FUTILITY_MARGIN_ALPHA_ENDING', 100, 240, 1),
  hp.quniform('PARAM_FUTILITY_MARGIN_BETA_OPENING', 100, 240, 1),
  hp.quniform('PARAM_FUTILITY_MARGIN_BETA_ENDING', 100, 240, 1),
  hp.quniform('PARAM_FUTILITY_MARGIN_QUIET_OPENING', 50, 160, 1),
  hp.quniform('PARAM_FUTILITY_MARGIN_QUIET_ENDING', 50, 160, 1),
  hp.quniform('PARAM_FUTILITY_RETURN_DEPTH_OPENING', 5, 13, 1),
  hp.quniform('PARAM_FUTILITY_RETURN_DEPTH_ENDING', 5, 13, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_DEPTH_OPENING', 5, 13, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_DEPTH_ENDING', 5, 13, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1_OPENING', 100, 300, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1_ENDING', 100, 300, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2_OPENING', 0, 300, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2_ENDING', 0, 300, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1_OPENING', 20, 50, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1_ENDING', 20, 50, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2_OPENING', 20, 60, 1),
  hp.quniform('PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2_ENDING', 20, 60, 1),
  hp.quniform('PARAM_NULL_MOVE_DYNAMIC_ALPHA_OPENING', 500, 1500, 1),
  hp.quniform('PARAM_NULL_MOVE_DYNAMIC_ALPHA_ENDING', 500, 1500, 1),
  hp.quniform('PARAM_NULL_MOVE_DYNAMIC_BETA_OPENING', 50, 100, 1),
  hp.quniform('PARAM_NULL_MOVE_DYNAMIC_BETA_ENDING', 50, 100, 1),
  hp.quniform('PARAM_NULL_MOVE_MARGIN_OPENING', 10, 60, 1),
  hp.quniform('PARAM_NULL_MOVE_MARGIN_ENDING', 10, 60, 1),
  hp.quniform('PARAM_NULL_MOVE_RETURN_DEPTH_OPENING', 4, 15, 1),
  hp.quniform('PARAM_NULL_MOVE_RETURN_DEPTH_ENDING', 4, 15, 1),
  hp.quniform('PARAM_PROBCUT_DEPTH_OPENING', 3, 10, 1),
  hp.quniform('PARAM_PROBCUT_DEPTH_ENDING', 3, 10, 1),
  hp.quniform('PARAM_PROBCUT_MARGIN_OPENING', 100, 300, 1),
  hp.quniform('PARAM_PROBCUT_MARGIN_ENDING', 100, 300, 1),
  hp.quniform('PARAM_SINGULAR_EXTENSION_DEPTH_OPENING', 4, 13, 1),
  hp.quniform('PARAM_SINGULAR_EXTENSION_DEPTH_ENDING', 4, 13, 1),
  hp.quniform('PARAM_SINGULAR_MARGIN_OPENING', 128, 400, 1),
  hp.quniform('PARAM_SINGULAR_MARGIN_ENDING', 128, 400, 1),
  hp.quniform('PARAM_SINGULAR_SEARCH_DEPTH_ALPHA_OPENING', 8, 32, 1),
  hp.quniform('PARAM_SINGULAR_SEARCH_DEPTH_ALPHA_ENDING', 8, 32, 1),
  hp.quniform('PARAM_PRUNING_BY_MOVE_COUNT_DEPTH_OPENING', 8, 32, 1),
  hp.quniform('PARAM_PRUNING_BY_MOVE_COUNT_DEPTH_ENDING', 8, 32, 1),
  hp.quniform('PARAM_PRUNING_BY_HISTORY_DEPTH_OPENING', 2, 32, 1),
  hp.quniform('PARAM_PRUNING_BY_HISTORY_DEPTH_ENDING', 2, 32, 1),
  hp.quniform('PARAM_REDUCTION_BY_HISTORY_OPENING', 4000, 15000, 1),
  hp.quniform('PARAM_REDUCTION_BY_HISTORY_ENDING', 4000, 15000, 1),
  hp.quniform('PARAM_IID_MARGIN_ALPHA_OPENING', 128, 384, 1),
  hp.quniform('PARAM_IID_MARGIN_ALPHA_ENDING', 128, 384, 1),
  hp.quniform('PARAM_RAZORING_MARGIN1_OPENING', 400, 700, 1),
  hp.quniform('PARAM_RAZORING_MARGIN1_ENDING', 400, 700, 1),
  hp.quniform('PARAM_RAZORING_MARGIN2_OPENING', 400, 700, 1),
  hp.quniform('PARAM_RAZORING_MARGIN2_ENDING', 400, 700, 1),
  hp.quniform('PARAM_RAZORING_MARGIN3_OPENING', 400, 700, 1),
  hp.quniform('PARAM_RAZORING_MARGIN3_ENDING', 400, 700, 1),
  hp.quniform('PARAM_RAZORING_MARGIN4_OPENING', 400, 700, 1),
  hp.quniform('PARAM_RAZORING_MARGIN4_ENDING', 400, 700, 1),
  hp.quniform('PARAM_REDUCTION_ALPHA_OPENING', 64, 256, 1),
  hp.quniform('PARAM_REDUCTION_ALPHA_ENDING', 64, 256, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_ALPHA0_OPENING', 150, 400, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_ALPHA0_ENDING', 150, 400, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_ALPHA1_OPENING', 150, 400, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_ALPHA1_ENDING', 150, 400, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_BETA0_OPENING', 500, 2000, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_BETA0_ENDING', 500, 2000, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_BETA1_OPENING', 500, 2000, 1),
  hp.quniform('PARAM_FUTILITY_MOVE_COUNT_BETA1_ENDING', 500, 2000, 1),
  hp.quniform('PARAM_QUIET_SEARCH_COUNT_OPENING', 32, 128, 1),
  hp.quniform('PARAM_QUIET_SEARCH_COUNT_ENDING', 32, 128, 1),
  hp.quniform('PARAM_QSEARCH_MATE1_OPENING', 0, 1, 1),
  hp.quniform('PARAM_QSEARCH_MATE1_ENDING', 0, 1, 1),
  hp.quniform('PARAM_SEARCH_MATE1_OPENING', 0, 1, 1),
  hp.quniform('PARAM_SEARCH_MATE1_ENDING', 0, 1, 1),
  hp.quniform('PARAM_WEAK_MATE_PLY_OPENING', 1, 5, 1),
  hp.quniform('PARAM_WEAK_MATE_PLY_ENDING', 1, 5, 1),
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

# pause/resume
class HyperoptState(object):
  def __init__(self):
    self.trials = Trials()
    self.iteration_logs = []

  def get_trials(self): return self.trials

  def get_n_accumulated_iterations(self): return len(self.iteration_logs)

  def record_iteration(self, **kwargs):
    # record any pickle-able object as a dictionary at each iteration.
    # also, len(self.iteration_logs) is used to keep the accumulated number of iterations.
    self.iteration_logs.append(kwargs)

  def calc_max_evals(self, n_additional_evals): return self.get_n_accumulated_iterations() + n_additional_evals

  @staticmethod
  def load(file_path):
    with open(file_path, 'rb') as fi:
      state = pickle.load(fi)
      print('resume from state: {} ({} iteration done)'.format(file_path, state.get_n_accumulated_iterations()))
      return state

  def save(self, file_path):
    try:
      with open(file_path, 'wb') as fo:
        pickle.dump(self, fo, protocol=-1)
        print('saved state to: {}'.format(file_path))
    except:
      print('failed to save state. continue.')

  def dump_log(self):
    for index, entry in enumerate(self.iteration_logs):
      # emulate function()'s output.
      print('-' * 78)
      print(datetime.datetime.today().strftime("%Y-%m-%d %H:%M:%S"))
      print(entry['args'])
      if index:
        remaining = datetime.timedelta(seconds=0)
        print(index, '/', self.get_n_accumulated_iterations(), str(remaining))
      popenargs = ['./YaneuraOu.exe',]
      print(popenargs)
      print(entry['output'])
      lose = entry['lose']
      draw = entry['draw']
      win = entry['win']
      ratio = 0.0
      if lose + draw + win > 0.1:
       ratio = win / (lose + draw + win)
      print ratio

class MSYSBuilder(object):
  def __init__(self):
    pass

  def clean(self):
    popenargs = [
      'make',
      'clean',
    ]
    print(popenargs)
    while True:
      try:
        with open('make-clean.txt', 'w') as file:
          output = subprocess.check_output(popenargs)
          file.write(output)
        break
      except subprocess.CalledProcessError:
        continue

  def build(self, args):
    popenargs = [
      'make',
      '-j4',
      'native',
      'TARGET_PREFIX=tanuki-modified',
      ]
    for key, val in zip(build_argument_names, args):
      popenargs.append('{}={}'.format(key, int(val)))

    print('Executing: make native (MSYS) ...')
    while True:
      try:
        with open('make-native.txt', 'w') as file:
          output = subprocess.check_output(popenargs)
          file.write(output)
        break
      except subprocess.CalledProcessError:
        continue

  def kill(self, process_name):
    subprocess.call(['pkill', process_name])

class MSVCBuilder(object):
  def __init__(self):
    self.devenv_path = 'devenv.exe'
    self.header_path = r'builder_generated.hpp'

  def clean(self):
    log_path = 'make-clean.txt'
    popenargs = [
      self.devenv_path,
      r'..\tanuki-\tanuki-.sln',
      '/Clean',
      'Release|x64',
      '/Project',
      r'..\tanuki-\tanuki-\tanuki-.vcxproj',
      '/Out',
      log_path,
      ]
    print(popenargs)
    while True:
      try:
        if os.path.isfile(log_path):
          os.unlink(log_path)
        subprocess.call(popenargs)
        break
      except subprocess.CalledProcessError:
        print('retry')
        continue

  def update_header(self, args):
    while True:
      try:
        with open(self.header_path, 'w') as file:
          file.write('#ifndef __BUILDER_GENERATED_HPP__\n')
          file.write('#define __BUILDER_GENERATED_HPP__\n')
          file.write('// this file was automatically generated.\n')
          file.write('// generated at {}\n'.format(datetime.datetime.now().strftime('%Y%m%d-%H%M%S')))
          for key, val in zip(build_argument_names, args):
            file.write('#define {} {}\n'.format(key, int(val)))
          file.write('#endif // __BUILDER_GENERATED_HPP__\n')
        break
      except IOError:
        print('retry')
        continue
  
  def clean_header(self):
    while True:
      try:
        with open(self.header_path, 'w') as file:
          file.write('// this file was automatically generated.\n')
          file.write('// generated at {}\n'.format(datetime.datetime.now().strftime('%Y%m%d-%H%M%S')))
        break
      except IOError:
        print('retry')
        continue

  def move_exe(self):
    shutil.copyfile(
        r'..\tanuki-\x64\Release\tanuki-.exe',
        r'tanuki-modified.exe',
        )

  def build(self, args):
    self.update_header(args)

    log_path = 'make-native.txt'
    popenargs = [
      self.devenv_path,
      r'..\tanuki-\tanuki-.sln',
      '/Build',
      'Release|x64',
      '/Project',
      r'..\tanuki-\tanuki-\tanuki-.vcxproj',
      '/Out',
      log_path,
      ]
    print('Executing: make native (MSVC) ...')
    while True:
      try:
        if os.path.isfile(log_path):
          os.unlink(log_path)
        output = subprocess.call(popenargs)
        break
      except subprocess.CalledProcessError:
        print('retry')
        continue

    self.move_exe()
    self.clean_header()

  def kill(self, process_name):
    subprocess.call(['taskkill', '/T', '/F', '/IM', process_name + '.exe'])


class YaneuraouBuilder(object):
  FILENAME = 'param/parameters_slave.h'
  def __init__(self):
    pass

  def clean(self):
    try:
      os.remove(self.FILENAME)
    except WindowsError:
      pass

  def build(self, args):
    with open(self.FILENAME, 'w') as f:
      for key, val in zip(build_argument_names, args):
        f.write('PARAM_DEFINE {0} = {1};\n'.format(key, str(int(val))))

  def kill(self, process_name):
    subprocess.call(['taskkill', '/T', '/F', '/IM', process_name + '.exe'])


def function(args):
  print('-' * 78)

  print(datetime.datetime.today().strftime("%Y-%m-%d %H:%M:%S"))

  global START_COUNTER
  global CURRENT_COUNTER
  print(args)

  if START_COUNTER < CURRENT_COUNTER:
    current_time_sec = time.time()
    delta = current_time_sec - START_TIME_SEC
    sec_per_one = delta / (CURRENT_COUNTER - START_COUNTER)
    remaining = datetime.timedelta(seconds=sec_per_one*(MAX_EVALS-CURRENT_COUNTER))
    print(CURRENT_COUNTER, '/', MAX_EVALS, str(remaining))
  CURRENT_COUNTER += 1

  builder.clean()
  builder.build(args)

  popenargs = ['./YaneuraOu-local-game-server.exe',]
  print(popenargs)
  output = None
  try:
    with open('yaneuraou-config.txt', 'r') as file:
      output = subprocess.check_output(popenargs, stdin=file)
  except subprocess.CalledProcessError:
    pass
  print(output)
  matched = re.compile('GameResult (\\d+) - (\\d+) - (\\d+)').search(output)
  lose = float(matched.group(1))
  draw = float(matched.group(2))
  win = float(matched.group(3))
  ratio = 0.0
  if lose + draw + win > 0.1:
   ratio = win / (lose + draw + win)
  print ratio

  builder.kill('YaneuraOu-2016-mid-engine-master')
  builder.kill('YaneuraOu-2016-mid-engine-slave')

  global state
  state.record_iteration(
      args=args,
      output=output,
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
  parser.add_argument('--builder', type=str, default='Yaneuraou',
      help=u'select building environment. Yaneuraou, MSYS or MSVC.')
  commandline_args = parser.parse_args()
  MAX_EVALS = commandline_args.max_evals

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
  if commandline_args.builder == 'MSVC':
    builder = MSVCBuilder()
  elif commandline_args.builder == 'MSYS':
    builder = MSYSBuilder()
  else:
    builder = YaneuraouBuilder()

  # shutil.copyfile('../tanuki-/x64/Release/tanuki-.exe', 'tanuki-.exe')
  best = fmin(function, space, algo=tpe.suggest, max_evals=state.calc_max_evals(MAX_EVALS), trials=state.get_trials())
  print("best estimate parameters", best)
  for key in sorted(best.keys()):
    print("{0}={1}".format(key, str(int(best[key]))))

