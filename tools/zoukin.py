#!/usr/bin/python
# coding:utf-8

import argparse
import csv
import datetime
import os
import re
import shutil
import subprocess
import sys

OUTPUT_EVAL_FOLDER_NAME = '5000000000'
#OUTPUT_EVAL_FOLDER_NAME = '99999999999'
ENGINE_CONFIG_TXT_TEMPLATE = '''YaneuraOu-2016-mid.exe
go byoyomi 1000
setoption name Threads value 1
setoption name Hash value 256
>>>>>>> 46afdf59cd78304873cf7e7304565f718f0e7484
setoption name EvalDir value {0}
setoption name NetworkDelay value 0
setoption name NetworkDelay2 value 0
'''
YANEURAOU_LOCAL_GAME_SERVER_EXE = 'YaneuraOu-local-game-server.exe'


def GetSubfolders(folder_path):
  return [x for x in os.listdir(folder_path) if os.path.isdir(os.path.join(folder_path, x))]


def GetDateTimeString():
  now = datetime.datetime.now()
  return now.strftime("%Y-%m-%d-%H-%M-%S")


def AdoptSubfolder(subfolder):
  num_positions = int(subfolder)
  return num_positions % 1000000000 == 0 or num_positions % 10000 != 0


def GenerateKifu(eval_folder_path, kifu_output_folder_path_base, generate_kifu_exe_file_path, num_threads):
  print('-' * 80)
  print('GenerateKifu')
  kifu_folder_path = os.path.join(kifu_output_folder_path_base, GetDateTimeString())
  input = '''usi
setoption name EvalDir value {0}
setoption name KifuDir value {1}
setoption name Threads value {2}
isready
usinewgame
generate_kifu
'''.format(eval_folder_path, kifu_folder_path, num_threads).encode('utf-8')
  print(input.decode('utf-8'))
  print(flush=True)
  subprocess.run([generate_kifu_exe_file_path], input=input, check=True)
  return kifu_folder_path


def Learn(eval_folder_path, kifu_folder_path, learner_output_folder_path_base, learner_exe_file_path, num_threads):
  print('-' * 80)
  print('Learn')
  eval_folder_path_base = os.path.join(learner_output_folder_path_base, GetDateTimeString())
  input = '''usi
setoption name EvalDir value {0}
setoption name KifuDir value {1}
setoption name Threads value {2}
setoption name MaxMovesToDraw value 300
isready
usinewgame
learn output_folder_path_base {3}
'''.format(eval_folder_path, kifu_folder_path, num_threads, eval_folder_path_base).encode('utf-8')
  print(input.decode('utf-8'))
  print(flush=True)
  subprocess.run([learner_exe_file_path], input=input, check=True)
  return eval_folder_path_base


def SelfPlay(old_eval_folder_path, new_eval_folder_path_base, result_file_path, local_game_server_exe_file_path, num_threads, num_games):
  print('-' * 80)
  print('SelfPlay')
  with open(result_file_path, 'a') as output_file:
    csvwriter = csv.writer(output_file)
    subfolder = OUTPUT_EVAL_FOLDER_NAME
    print(subfolder)

    with open('engine-config1.txt', 'w') as engine_config1_file:
      engine_config1_file.write(ENGINE_CONFIG_TXT_TEMPLATE.format(old_eval_folder_path))
    with open('engine-config2.txt', 'w') as engine_config2_file:
      engine_config2_file.write(ENGINE_CONFIG_TXT_TEMPLATE.format(os.path.join(new_eval_folder_path_base, subfolder)))
    input = '''setoption name Threads value {0}
go btime {1} byoyomi 1
quit
'''.format(num_threads, num_games).encode('utf-8')
    print(input.decode('utf-8'))
    print(flush=True)
    completed_process = subprocess.run([local_game_server_exe_file_path], input=input, stdout=subprocess.PIPE, check=True)
    stdout = completed_process.stdout.decode('utf-8')
    print(stdout)
    matched = re.compile('GameResult (\\d+) - (\\d+) - (\\d+)').search(stdout)
    lose = float(matched.group(1))
    draw = float(matched.group(2))
    win = float(matched.group(3))
    winning_percentage = 0.0
    if lose + draw + win > 0.0:
      winning_percentage = win / (lose + draw + win)
    csvwriter.writerow([new_eval_folder_path_base, subfolder, winning_percentage])


def main():
  parser = argparse.ArgumentParser(description='iteration')
  parser.add_argument(
    '--learner_output_folder_path_base',
    action='store',
    dest='learner_output_folder_path_base',
    required=True,
    help='Folder path baseof the output folders. ex) learner_output')
  parser.add_argument(
    '--kifu_output_folder_path_base',
    action='store',
    dest='kifu_output_folder_path_base',
    required=True,
    help='Folder path baseof the output kifu. ex) kifu')
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
    '--initial_new_eval_folder_path_base',
    action='store',
    dest='initial_new_eval_folder_path_base',
    required=True,
    help='Folder path base of the inintial new eval files. ex) eval')
  parser.add_argument(
    '--skip_first_kifu_generation',
    action='store_true',
    dest='skip_first_kifu_generation',
    help='Skip the first kifu generation')
  parser.add_argument(
    '--skip_first_learn',
    action='store_true',
    dest='skip_first_learn',
    help='Skip the first learn')
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
    '--num_threads',
    action='store',
    type=int,
    dest='num_threads',
    required=True,
    help='Number of the threads. ex) 8')
  parser.add_argument(
    '--num_games',
    action='store',
    type=int,
    dest='num_games',
    required=True,
    help='Number of the games to play. ex) 100')
  args = parser.parse_args()

  learner_output_folder_path_base = args.learner_output_folder_path_base
  kifu_output_folder_path_base = args.kifu_output_folder_path_base
  initial_eval_folder_path = args.initial_eval_folder_path
  initial_kifu_folder_path = args.initial_kifu_folder_path
  initial_new_eval_folder_path_base = args.initial_new_eval_folder_path_base
  skip_first_kifu_generation = args.skip_first_kifu_generation
  skip_first_learn = args.skip_first_learn
  original_eval_folder_path = args.original_eval_folder_path
  generate_kifu_exe_file_path = args.generate_kifu_exe_file_path
  learner_exe_file_path = args.learner_exe_file_path
  local_game_server_exe_file_path = args.local_game_server_exe_file_path
  vs_original_result_file_path = args.vs_original_result_file_path
  vs_base_result_file_path = args.vs_base_result_file_path
  num_threads = args.num_threads
  num_games = args.num_games

  skip_kifu_generation = skip_first_kifu_generation
  kifu_folder_path = initial_kifu_folder_path
  eval_folder_path = initial_eval_folder_path
  previous_eval_folder_path = eval_folder_path
  skip_learn = skip_first_learn
  while True:
    if not skip_kifu_generation:
      kifu_folder_path = GenerateKifu(eval_folder_path, kifu_output_folder_path_base, generate_kifu_exe_file_path, num_threads)
      previous_eval_folder_path = eval_folder_path
    skip_kifu_generation = False

    new_eval_folder_path_base = None
    if skip_learn:
      new_eval_folder_path_base = initial_new_eval_folder_path_base
    else:
      new_eval_folder_path_base = Learn(eval_folder_path, kifu_folder_path, learner_output_folder_path_base, learner_exe_file_path, num_threads)
    skip_learn = False
    SelfPlay(original_eval_folder_path, new_eval_folder_path_base, vs_original_result_file_path, local_game_server_exe_file_path, num_threads, num_games)
    SelfPlay(previous_eval_folder_path, new_eval_folder_path_base, vs_base_result_file_path, local_game_server_exe_file_path, num_threads, num_games)

    eval_folder_path = os.path.join(new_eval_folder_path_base, OUTPUT_EVAL_FOLDER_NAME)
    #break


if __name__ == '__main__':
  main()
