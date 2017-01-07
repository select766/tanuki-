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
  self_play_with_base = 5


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
          kif_for_test_folder_path, output_folder_path_base, learner_exe_file_path, learning_rate):
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
setoption name MiniBatchSize value 1000000
setoption name ReadBatchSize value {num_positions_to_learn}
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
  learning_rate=learning_rate).encode('utf-8')
  print(input.decode('utf-8'), flush=True)
  subprocess.run([learner_exe_file_path], input=input, check=True)


def SelfPlay(old_eval_folder_path, new_eval_folder_path, local_game_server_exe_file_path,
             num_threads, num_games):
  print(locals(), flush=True)
  args = [
    'C:\\Python27\\python.exe',
    '..\script\engine_invoker5.py',
    # hakubishin-private\exe以下から実行していると仮定する
    'home:{0}'.format(os.getcwd()),
    # {home}\exeからの相対パスに変換する
    'engine1:{0}'.format(os.path.relpath(os.path.abspath(local_game_server_exe_file_path), os.path.join(os.getcwd(), 'exe'))),
    # {home}\evalからの相対パスに変換する
    'eval1:{0}'.format(os.path.relpath(os.path.abspath(old_eval_folder_path), os.path.join(os.getcwd(), 'eval'))),
    'engine2:{0}'.format(os.path.relpath(os.path.abspath(local_game_server_exe_file_path), os.path.join(os.getcwd(), 'exe'))),
    'eval2:{0}'.format(os.path.relpath(os.path.abspath(new_eval_folder_path), os.path.join(os.getcwd(), 'eval'))),
    'cores:{0}'.format(num_threads),
    'loop:{0}'.format(num_games),
    'cpu:1',
    'engine_threads:1',
    'hash1:256',
    'hash2:256',
    'time:b1000',
    ]
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
    dest='learner_output_folder_path_base',
    required=True,
    help='Folder path baseof the output folders. ex) eval')
  parser.add_argument(
    '--kifu_output_folder_path_base',
    action='store',
    dest='kifu_output_folder_path_base',
    required=True,
    help='Folder path baseof the output kifu. ex) kifu')
  parser.add_argument(
    '--kifu_for_test_output_folder_path_base',
    action='store',
    dest='kifu_for_test_output_folder_path_base',
    required=True,
    help='Folder path baseof the output kifu for test. ex) kifu for test')
  parser.add_argument(
    '--initial_eval_folder_path',
    action='store',
    dest='initial_eval_folder_path',
    required=True,
    help='Folder path of the inintial eval files. ex) eval')
  parser.add_argument(
    '--initial_kifu_folder_path',
    action='store',
    dest='initial_kifu_folder_path',
    required=True,
    help='Folder path of the inintial kifu files. ex) kifu')
  parser.add_argument(
    '--initial_kifu_for_test_folder_path',
    action='store',
    dest='initial_kifu_for_test_folder_path',
    required=True,
    help='Folder path of the inintial kifu files for test. ex) kifu_for_test')
  parser.add_argument(
    '--initial_new_eval_folder_path_base',
    action='store',
    dest='initial_new_eval_folder_path_base',
    required=True,
    help='Folder path base of the inintial new eval files. ex) eval')
  parser.add_argument(
    '--initial_state',
    action='store',
    dest='initial_state',
    required=True,
    help='Initial state. [' + ', '.join([state.name for state in State]) + ']')
  parser.add_argument(
    '--original_eval_folder_path',
    action='store',
    dest='original_eval_folder_path',
    required=True,
    help='Folder path of the original eval files. ex) eval/apery_wcsc26')
  parser.add_argument(
    '--generate_kifu_exe_file_path',
    action='store',
    dest='generate_kifu_exe_file_path',
    required=True,
    help='Exe file name of the kifu generator. ex) YaneuraOu.2016-08-05.learn.exe')
  parser.add_argument(
    '--learner_exe_file_path',
    action='store',
    dest='learner_exe_file_path',
    required=True,
    help='Exe file name of the learner. ex) YaneuraOu.2016-08-05.generate_kifu.exe')
  parser.add_argument(
    '--local_game_server_exe_file_path',
    action='store',
    dest='local_game_server_exe_file_path',
    required=True,
    help='Exe file name of the local game server. ex) YaneuraOu-local-game-server.exe')
  parser.add_argument(
    '--vs_original_result_file_path',
    action='store',
    dest='vs_original_result_file_path',
    required=True,
    help='Result file path for VS original eval. ex) vs_original.2016-08-05.csv')
  parser.add_argument(
    '--vs_base_result_file_path',
    action='store',
    dest='vs_base_result_file_path',
    required=True,
    help='Result file path for VS base eval. ex) vs_base.2016-08-05.csv')
  parser.add_argument(
    '--num_threads_to_generate_kifu',
    action='store',
    type=int,
    dest='num_threads_to_generate_kifu',
    required=True,
    help='Number of the threads used for learning. ex) 8')
  parser.add_argument(
    '--num_threads_to_learn',
    action='store',
    type=int,
    dest='num_threads_to_learn',
    required=True,
    help='Number of the threads used for learning. ex) 8')
  parser.add_argument(
    '--num_threads_to_selfplay',
    action='store',
    type=int,
    dest='num_threads_to_selfplay',
    required=True,
    help='Number of the threads used for selfplay. ex) 8')
  parser.add_argument(
    '--num_games_to_selfplay',
    action='store',
    type=int,
    dest='num_games_to_selfplay',
    required=True,
    help='Number of the games to play for selfplay. ex) 100')
  parser.add_argument(
    '--num_positions_to_generator_train',
    action='store',
    type=int,
    dest='num_positions_to_generator_train',
    required=True,
    help='Number of the games to play for generator. ex) 100')
  parser.add_argument(
    '--num_positions_to_generator_test',
    action='store',
    type=int,
    dest='num_positions_to_generator_test',
    required=True,
    help='Number of the games to play for generator. ex) 100')
  parser.add_argument(
    '--num_positions_to_learn',
    action='store',
    type=int,
    dest='num_positions_to_learn',
    required=True,
    help='Number of the positions for learning. ex) 10000')
  parser.add_argument(
    '--num_iterations',
    action='store',
    type=int,
    dest='num_iterations',
    required=True,
    help='Number of the iterations. ex) 100')
  parser.add_argument(
    '--search_depth',
    action='store',
    type=int,
    dest='search_depth',
    required=True,
    help='Search depth. ex) 8')
  parser.add_argument(
    '--learning_rate',
    action='store',
    type=float,
    dest='learning_rate',
    required=True,
    help='Learning rate. ex) 2.0')
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
      GenerateKifu(old_eval_folder_path, kifu_folder_path, num_threads_to_generate_kifu,
                   num_positions_to_generator_train, search_depth, 'train',
                   generate_kifu_exe_file_path)
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
            learning_rate)
      new_eval_folder_path = os.path.join(new_eval_folder_path_base, str(num_positions_to_learn))
      state = State.self_play_with_original

    elif state == State.self_play_with_original:
      SelfPlay(original_eval_folder_path, new_eval_folder_path, local_game_server_exe_file_path,
               num_threads_to_selfplay, num_games_to_selfplay)
      state = State.self_play_with_base

    elif state == State.self_play_with_base:
      SelfPlay(old_eval_folder_path, new_eval_folder_path, local_game_server_exe_file_path,
               num_threads_to_selfplay, num_games_to_selfplay)
      state = State.generate_kifu
      iteration += 1
      old_eval_folder_path = new_eval_folder_path

    else:
      sys.exit('Invalid state: state=%s' % state)


if __name__ == '__main__':
  main()
