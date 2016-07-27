#!/usr/bin/python
# coding:utf-8

import argparse
import csv
import datetime
import os
import re
import shutil
import subprocess


EVAL_FOLDER_PATH = 'eval'
ENGINE_CONFIG_TXT_TEMPLATE = '''YaneuraOu-2016-mid_gcc.exe
go rtime 100
setoption name Threads value 4
setoption name Hash value 256
setoption name EvalDir value {0}
'''
LOCAL_GAME_SERVER_TXT_FILE_NAME = 'Yaneuraou-local-game-server.txt'
LOCAL_GAME_SERVER_TXT_TEMPLATE = '''setoption name Threads value 1
go btime 1
'''
ERROR_MEASUREMENT_TXT_FILE_NAME = 'Yaneuraou-error-measurement.txt'
ERROR_MEASUREMENT_TXT_TEMPLATE = '''setoption name EvalDir value {0}
setoption name KifuDir value kifu_for_test
setoption name Threads value 48
error_measurement
'''


def GetSubfolders(folder_path):
  return [x for x in os.listdir(folder_path) if os.path.isdir(os.path.join(folder_path, x))]


def GetDateTimeString():
  now = datetime.datetime.now()
  return now.strftime("%Y-%m-%d-%H-%M-%S")


def AdoptSubfolder(subfolder):
  num_positions = int(subfolder)
  return num_positions % 1000000000 == 0 or num_positions % 10000 != 0


def CalculateWinningPercentage(learner_output_folder_path):
  subfolders = GetSubfolders(learner_output_folder_path)
  output_cvs_file_path = os.path.join(learner_output_folder_path, 'winning_percentage.' + GetDateTimeString() + '.csv')
  with open(output_cvs_file_path, 'wb') as output_file:
    csvwriter = csv.writer(output_file)
    for subfolder in subfolders:
      if not AdoptSubfolder(subfolder):
        continue
      print(subfolder)

      with open('engine-config1.txt', 'w') as engine_config1_file:
        engine_config1_file.write(ENGINE_CONFIG_TXT_TEMPLATE.format(EVAL_FOLDER_PATH))
      with open('engine-config2.txt', 'w') as engine_config2_file:
        engine_config2_file.write(ENGINE_CONFIG_TXT_TEMPLATE.format(os.path.join(learner_output_folder_path, subfolder)))
      with open(LOCAL_GAME_SERVER_TXT_FILE_NAME, 'w') as local_game_server_file:
        local_game_server_file.write(LOCAL_GAME_SERVER_TXT_TEMPLATE)
      popenargs = ['./YaneuraOu-local-game-server.exe',]
      print(popenargs)
      output = None
      try:
        with open(LOCAL_GAME_SERVER_TXT_FILE_NAME, 'r') as file:
          output = subprocess.check_output(popenargs, stdin=file)
      except subprocess.CalledProcessError:
        pass
      print(output)
      matched = re.compile('GameResult (\\d+) - (\\d+) - (\\d+)').search(output)
      lose = float(matched.group(1))
      draw = float(matched.group(2))
      win = float(matched.group(3))
      winning_percentage = 0.0
      if lose + draw + win > 0.0:
       winning_percentage = win / (lose + draw + win)
      csvwriter.writerow([subfolder, winning_percentage])


def CalculateMeanSquaredError(learner_output_folder_path, learner_exe_file_path):
  subfolders = GetSubfolders(learner_output_folder_path)
  output_cvs_file_path = os.path.join(learner_output_folder_path, 'mean_squared_error.' + GetDateTimeString() + '.csv')
  with open(output_cvs_file_path, 'wb') as output_file:
    csvwriter = csv.writer(output_file)
    csvwriter.writerow(['subfolder', 'rmse_value', 'rmse_winning_percentage', 'mean_cross_entropy', 'norm'])
    for subfolder in subfolders:
      print(subfolder)

      with open(ERROR_MEASUREMENT_TXT_FILE_NAME, 'w') as error_measurement_txt:
        error_measurement_txt.write(ERROR_MEASUREMENT_TXT_TEMPLATE.format(os.path.join(learner_output_folder_path, subfolder)))

      popenargs = [learner_exe_file_path,]
      print(popenargs)
      output = None
      try:
        with open(ERROR_MEASUREMENT_TXT_FILE_NAME, 'r') as file:
          output = subprocess.check_output(popenargs, stdin=file)
      except subprocess.CalledProcessError:
        pass
      print(output)
      matched = re.compile('info string rmse_value=(.+) rmse_winning_percentage=(.+) mean_cross_entropy=(.+) norm=(.+)').search(output)
      rmse_value = float(matched.group(1))
      rmse_winning_percentage = float(matched.group(2))
      mean_cross_entropy = float(matched.group(3))
      norm = float(matched.group(4))
      csvwriter.writerow([subfolder, rmse_value, rmse_winning_percentage, mean_cross_entropy, norm])


def main():
  parser = argparse.ArgumentParser(description='selfplay')
  parser.add_argument(
    '--learner_output_folder_path',
    action='store',
    dest='learner_output_folder_path',
    help='Output the folder path of the learner. ex) learner_output/2016-06-28',
    default=EVAL_FOLDER_PATH)
  parser.add_argument(
    '--calculate_winning_percentage',
    action='store_true',
    dest='calculate_winning_percentage',
    help='Calculate the winning percentage.')
  parser.add_argument(
    '--calculate_mean_squared_error',
    action='store_true',
    dest='calculate_mean_squared_error',
    help='Calculate the mean squared error.')
  parser.add_argument(
    '--learner_exe_file_path',
    action='store',
    dest='learner_exe_file_path',
    help='Exe file name of the learner. ex) YaneuraOu.2016-06-23.qsearch.0.1.exe',
    default=EVAL_FOLDER_PATH)
  args = parser.parse_args()

  if args.calculate_winning_percentage:
    CalculateWinningPercentage(args.learner_output_folder_path)

  if args.calculate_mean_squared_error:
    CalculateMeanSquaredError(args.learner_output_folder_path, args.learner_exe_file_path)


if __name__ == '__main__':
  main()
