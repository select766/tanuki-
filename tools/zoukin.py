#!/usr/bin/python
# coding:utf-8

import argparse
import csv
import datetime
import enum
import os
import re
import shutil
import subprocess
import sys


class State(enum.Enum):
  generate_kifu = 1
  generate_kifu_for_test = 2
  learn = 3
  self_play_with_original = 4
  self_play_with_tanuki_wcsc27 = 5
  self_play_with_base = 6


def GetSubfolders(folder_path):
  return [x for x in os.listdir(folder_path) if os.path.isdir(os.path.join(folder_path, x))]


def GetDateTimeString():
  now = datetime.datetime.now()
  return now.strftime("%Y-%m-%d-%H-%M-%S")


def AdoptSubfolder(subfolder):
  num_positions = int(subfolder)
  return num_positions % 1000000000 == 0 or num_positions % 10000 != 0


def GenerateKifu(eval_folder_path, kifu_folder_path, num_threads, num_positions, search_depth,
                 kifu_tag, generate_kifu_exe_file_path):
  print(locals(), flush=True)
  input = '''usi
setoption name EvalDir value {eval_folder_path}
setoption name KifuDir value {kifu_folder_path}
setoption name Threads value {num_threads}
setoption name Hash value 16384
setoption name GeneratorNumPositions value {num_positions}
setoption name GeneratorMinSearchDepth value {search_depth}
setoption name GeneratorMaxSearchDepth value {search_depth}
setoption name GeneratorKifuTag value {kifu_tag}
setoption name GeneratorStartposFileName value startpos.sfen
setoption name GeneratorMinBookMove value 0
setoption name GeneratorValueThreshold value 3000
setoption name GeneratorDoRandomKingMoveProbability value 0
setoption name GeneratorSwapTwoPiecesProbability value 0
setoption name GeneratorDoRandomMoveProbability value 0.1
setoption name GeneratorDoRandomMoveAfterBook value 0.1
isready
usinewgame
generate_kifu
'''.format(
  eval_folder_path=eval_folder_path,
  kifu_folder_path=kifu_folder_path,
  num_threads=num_threads,
  num_positions=num_positions,
  search_depth=search_depth,
  kifu_tag=kifu_tag).encode('utf-8')
  print(input.decode('utf-8'), flush=True)
  subprocess.run([generate_kifu_exe_file_path], input=input, check=True)


def Learn(num_threads, eval_folder_path, kifu_folder_path, num_positions_to_learn,
          kif_for_test_folder_path, output_folder_path_base, learner_exe_file_path, learning_rate,
          num_actual_positions, mini_batch_size, fobos_l1_parameter, fobos_l2_parameter):
  print(locals(), flush=True)
  input = '''usi
setoption name Threads value {num_threads}
setoption name MaxMovesToDraw value 300
setoption name EvalDir value {eval_folder_path}
setoption name KifuDir value {kifu_folder_path}
setoption name LearnerNumPositions value {num_positions_to_learn}
setoption name LearningRate value {learning_rate}
setoption name KifuForTestDir value {kif_for_test_folder_path}
setoption name LearnerNumPositionsForTest value 1000000
setoption name MiniBatchSize value {mini_batch_size}
setoption name ReadBatchSize value {num_actual_positions}
setoption name FobosL1Parameter value {fobos_l1_parameter}
setoption name FobosL2Parameter value {fobos_l2_parameter}
isready
usinewgame
learn output_folder_path_base {output_folder_path_base}
'''.format(
  num_threads=num_threads,
  eval_folder_path=eval_folder_path,
  kifu_folder_path=kifu_folder_path,
  num_positions_to_learn=num_positions_to_learn,
  kif_for_test_folder_path=kif_for_test_folder_path,
  output_folder_path_base=output_folder_path_base,
  learning_rate=learning_rate,
  num_actual_positions=num_actual_positions,
  mini_batch_size=mini_batch_size,
  fobos_l1_parameter=fobos_l1_parameter,
  fobos_l2_parameter=fobos_l2_parameter).encode('utf-8')
  print(input.decode('utf-8'), flush=True)
  subprocess.run([learner_exe_file_path], input=input, check=True)


def SelfPlay(old_eval_folder_path, new_eval_folder_path, num_threads, num_games, num_numa_nodes):
  print(locals(), flush=True)
  args = [
    'TanukiColiseum.exe',
    '--engine1', 'YaneuraOu.2017-04-28.exe',
    '--engine2', 'YaneuraOu.2017-04-28.exe',
    '--eval1', new_eval_folder_path,
    '--eval2', old_eval_folder_path,
    '--num_concurrent_games', str(num_threads),
    '--num_games', str(num_games),
    '--hash', '256',
    '--time', '1000',
    '--num_numa_nodes', str(num_numa_nodes)]
  print(args, flush=True)
  if subprocess.run(args).returncode:
    sys.exit('Failed to calculate the winning rate...');


def GetSubfolders(folder_path):
  return [x for x in os.listdir(folder_path) if os.path.isdir(os.path.join(folder_path, x))]


def main():
  parser = argparse.ArgumentParser(description='iteration')
  parser.add_argument(
    '--learner_output_folder_path_base',
    action='store',
    required=True,
    help='Folder path baseof the output folders. ex) eval')
  parser.add_argument(
    '--kifu_output_folder_path_base',
    action='store',
    required=True,
    help='Folder path baseof the output kifu. ex) kifu')
  parser.add_argument(
    '--kifu_for_test_output_folder_path_base',
    action='store',
    required=True,
    help='Folder path baseof the output kifu for test. ex) kifu for test')
  parser.add_argument(
    '--initial_eval_folder_path',
    action='store',
    required=True,
    help='Folder path of the inintial eval files. ex) eval')
  parser.add_argument(
    '--initial_kifu_folder_path',
    action='store',
    required=True,
    help='Folder path of the inintial kifu files. ex) kifu')
  parser.add_argument(
    '--initial_kifu_for_test_folder_path',
    action='store',
    required=True,
    help='Folder path of the inintial kifu files for test. ex) kifu_for_test')
  parser.add_argument(
    '--initial_new_eval_folder_path_base',
    action='store',
    required=True,
    help='Folder path base of the inintial new eval files. ex) eval')
  parser.add_argument(
    '--initial_state',
    action='store',
    required=True,
    help='Initial state. [' + ', '.join([state.name for state in State]) + ']')
  parser.add_argument(
    '--original_eval_folder_path',
    action='store',
    required=True,
    help='Folder path of the original eval files. ex) eval/apery_wcsc26')
  parser.add_argument(
    '--tanuki_wcsc27_eval_folder_path',
    action='store',
    required=True,
    help='Folder path of the tanuki-wcsc27 eval files. ex) eval/apery_sdt4')
  parser.add_argument(
    '--generate_kifu_exe_file_path',
    action='store',
    required=True,
    help='Exe file name of the kifu generator. ex) YaneuraOu.2016-08-05.learn.exe')
  parser.add_argument(
    '--learner_exe_file_path',
    action='store',
    required=True,
    help='Exe file name of the learner. ex) YaneuraOu.2016-08-05.generate_kifu.exe')
  parser.add_argument(
    '--local_game_server_exe_file_path',
    action='store',
    required=True,
    help='Exe file name of the local game server. ex) YaneuraOu-local-game-server.exe')
  parser.add_argument(
    '--vs_original_result_file_path',
    action='store',
    required=True,
    help='Result file path for VS original eval. ex) vs_original.2016-08-05.csv')
  parser.add_argument(
    '--vs_base_result_file_path',
    action='store',
    required=True,
    help='Result file path for VS base eval. ex) vs_base.2016-08-05.csv')
  parser.add_argument(
    '--num_threads_to_generate_kifu',
    action='store',
    type=int,
    required=True,
    help='Number of the threads used for learning. ex) 8')
  parser.add_argument(
    '--num_threads_to_learn',
    action='store',
    type=int,
    required=True,
    help='Number of the threads used for learning. ex) 8')
  parser.add_argument(
    '--num_threads_to_selfplay',
    action='store',
    type=int,
    required=True,
    help='Number of the threads used for selfplay. ex) 8')
  parser.add_argument(
    '--num_games_to_selfplay',
    action='store',
    type=int,
    required=True,
    help='Number of the games to play for selfplay. ex) 100')
  parser.add_argument(
    '--num_positions_to_generator_train',
    action='store',
    type=int,
    required=True,
    help='Number of the games to play for generator. ex) 100')
  parser.add_argument(
    '--num_positions_to_generator_test',
    action='store',
    type=int,
    required=True,
    help='Number of the games to play for generator. ex) 100')
  parser.add_argument(
    '--num_positions_to_learn',
    action='store',
    type=int,
    required=True,
    help='Number of the positions for learning. ex) 10000')
  parser.add_argument(
    '--num_iterations',
    action='store',
    type=int,
    required=True,
    help='Number of the iterations. ex) 100')
  parser.add_argument(
    '--search_depth',
    action='store',
    type=int,
    required=True,
    help='Search depth. ex) 8')
  parser.add_argument(
    '--learning_rate',
    action='store',
    type=float,
    required=True,
    help='Learning rate. ex) 2.0')
  parser.add_argument(
    '--mini_batch_size',
    action='store',
    type=int,
    required=True,
    help='Learning rate. ex) 2.0')
  parser.add_argument(
    '--fobos_l1_parameter',
    action='store',
    type=float,
    required=True,
    help='Learning rate. ex) 0.0')
  parser.add_argument(
    '--fobos_l2_parameter',
    action='store',
    type=float,
    required=True,
    help='Learning rate. ex) 0.99989464503')
  parser.add_argument(
    '--num_numa_nodes',
    action='store',
    type=int,
    required=True,
    help='Number of the NUMA nodes. ex) 2')
  parser.add_argument(
    '--num_divisions_to_generator_train',
    action='store',
    type=int,
    required=True,
    help='Number of the divisions to generate train data. ex) 10')
  parser.add_argument(
    '--initial_division_to_generator_train',
    action='store',
    type=int,
    default=0,
    help='Initial division to generate train data. ex) 2')
  args = parser.parse_args()

  learner_output_folder_path_base = args.learner_output_folder_path_base
  kifu_output_folder_path_base = args.kifu_output_folder_path_base
  kifu_for_test_output_folder_path_base = args.kifu_for_test_output_folder_path_base
  initial_eval_folder_path = args.initial_eval_folder_path
  initial_kifu_folder_path = args.initial_kifu_folder_path
  initial_kifu_for_test_folder_path = args.initial_kifu_for_test_folder_path
  initial_new_eval_folder_path_base = args.initial_new_eval_folder_path_base
  initial_state = State[args.initial_state]
  if not initial_state:
    sys.exit('Unknown initial state: %s' % args.initial_state)
  original_eval_folder_path = args.original_eval_folder_path
  tanuki_wcsc27_eval_folder_path = args.tanuki_wcsc27_eval_folder_path
  generate_kifu_exe_file_path = args.generate_kifu_exe_file_path
  learner_exe_file_path = args.learner_exe_file_path
  local_game_server_exe_file_path = args.local_game_server_exe_file_path
  vs_original_result_file_path = args.vs_original_result_file_path
  vs_base_result_file_path = args.vs_base_result_file_path
  num_threads_to_generate_kifu = args.num_threads_to_generate_kifu
  num_threads_to_learn = args.num_threads_to_learn
  num_threads_to_selfplay = args.num_threads_to_selfplay
  num_games_to_selfplay = args.num_games_to_selfplay
  num_positions_to_generator_train = args.num_positions_to_generator_train
  num_positions_to_generator_test = args.num_positions_to_generator_test
  num_positions_to_learn = args.num_positions_to_learn
  num_iterations = args.num_iterations
  search_depth = args.search_depth
  learning_rate = args.learning_rate
  mini_batch_size = args.mini_batch_size
  fobos_l1_parameter = args.fobos_l1_parameter
  fobos_l2_parameter = args.fobos_l2_parameter
  num_numa_nodes = args.num_numa_nodes
  num_divisions_to_generator_train = args.num_divisions_to_generator_train
  initial_division_to_generator_train = args.initial_division_to_generator_train

  kifu_folder_path = initial_kifu_folder_path
  kifu_for_test_folder_path = initial_kifu_for_test_folder_path
  old_eval_folder_path = initial_eval_folder_path
  new_eval_folder_path = os.path.join(initial_new_eval_folder_path_base, str(num_positions_to_learn))
  state = initial_state

  iteration = 0
  while iteration < num_iterations:
    print('-' * 80)
    print('- %s' % state)
    print('-' * 80, flush=True)

    if state == State.generate_kifu:
      kifu_folder_path = os.path.join(kifu_output_folder_path_base, GetDateTimeString())
      for division in range(initial_division_to_generator_train, num_divisions_to_generator_train):
          GenerateKifu(old_eval_folder_path, kifu_folder_path, num_threads_to_generate_kifu,
                       num_positions_to_generator_train / num_divisions_to_generator_train,
                       search_depth, 'train.{0}'.format(division), generate_kifu_exe_file_path)
      state = State.generate_kifu_for_test

    elif state == State.generate_kifu_for_test:
      kifu_for_test_folder_path = os.path.join(kifu_for_test_output_folder_path_base,
                                               GetDateTimeString())
      GenerateKifu(old_eval_folder_path, kifu_for_test_folder_path, num_threads_to_generate_kifu,
                   num_positions_to_generator_test, search_depth, 'test',
                   generate_kifu_exe_file_path)
      state = State.learn

    elif state == State.learn:
      new_eval_folder_path_base = os.path.join(learner_output_folder_path_base, GetDateTimeString())
      Learn(num_threads_to_learn, old_eval_folder_path, kifu_folder_path, num_positions_to_learn,
            kifu_for_test_folder_path, new_eval_folder_path_base, learner_exe_file_path,
            learning_rate, num_positions_to_generator_train, mini_batch_size, fobos_l1_parameter,
            fobos_l2_parameter)
      new_eval_folder_path = os.path.join(new_eval_folder_path_base, str(num_positions_to_learn))
      state = State.self_play_with_original

    elif state == State.self_play_with_original:
      SelfPlay(original_eval_folder_path, new_eval_folder_path, num_threads_to_selfplay,
               num_games_to_selfplay, num_numa_nodes)
      state = State.self_play_with_tanuki_wcsc27

    elif state == State.self_play_with_tanuki_wcsc27:
      SelfPlay(tanuki_wcsc27_eval_folder_path, new_eval_folder_path, num_threads_to_selfplay,
               num_games_to_selfplay, num_numa_nodes)
      state = State.self_play_with_base

    elif state == State.self_play_with_base:
      SelfPlay(old_eval_folder_path, new_eval_folder_path, num_threads_to_selfplay,
               num_games_to_selfplay, num_numa_nodes)
      state = State.generate_kifu
      iteration += 1
      old_eval_folder_path = new_eval_folder_path

    else:
      sys.exit('Invalid state: state=%s' % state)


if __name__ == '__main__':
  main()
