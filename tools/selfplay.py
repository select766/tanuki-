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


def GetSubfolders(folder_path):
  return [x for x in os.listdir(folder_path) if os.path.isdir(os.path.join(folder_path, x))]


def GetWinningPercentage():
  popenargs = ['./YaneuraOu-local-game-server.exe',]
  print(popenargs)
  output = None
  try:
    with open('Yaneuraou-local-game-server.txt', 'r') as file:
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
  return ratio


def GetDateTimeString():
  now = datetime.datetime.now()
  return now.strftime("%Y-%m-%d-%H-%M-%S")


def main():
  parser = argparse.ArgumentParser(description='selfplay')
  parser.add_argument(
    '--learner_output_folder_path',
    action='store',
    dest='learner_output_folder_path',
    help='Ignored unit test labels. Multiple file names can be specified with separating by comma(s). ex) ignore,benchmark',
    default=EVAL_FOLDER_PATH)
  args = parser.parse_args()

  subfolders = GetSubfolders(args.learner_output_folder_path)
  output_cvs_file_path = os.path.join(args.learner_output_folder_path, GetDateTimeString() + '.csv')
  with open(output_cvs_file_path, 'wb') as output_file:
    csvwriter = csv.writer(output_file)
    for subfolder in subfolders:
      print(subfolder)
      for src_filename, dst_filename in [
        ('KPP_synthesized.bin', 'KPP_synthesized.learn.bin'),
        ('KKP_synthesized.bin', 'KKP_synthesized.learn.bin'),
        ('KK_synthesized.bin', 'KK_synthesized.learn.bin')]:
        src = os.path.join(args.learner_output_folder_path, subfolder, src_filename)
        dst = os.path.join(EVAL_FOLDER_PATH, dst_filename)
        shutil.copyfile(src, dst)
      winning_percentage = GetWinningPercentage()
      csvwriter.writerow([subfolder, winning_percentage])


if __name__ == '__main__':
  main()
