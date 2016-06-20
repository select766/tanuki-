#!/usr/bin/python
# coding:utf-8

import csv
import datetime
import os
import re
import shutil
import subprocess


EVAL_FOLDER_NAME = 'eval'


def GetEvalSubfolders():
  return [x for x in os.listdir(EVAL_FOLDER_NAME) if os.path.isdir(os.path.join(EVAL_FOLDER_NAME, x)) and x != 'book']


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
  eval_subfolders = GetEvalSubfolders()
  with open(GetDateTimeString() + '.csv', 'wb') as output_file:
    csvwriter = csv.writer(output_file)
    for eval_subfolder in eval_subfolders:
      print(eval_subfolder)
      for src_filename, dst_filename in [
        ('KPP_synthesized.bin', 'KPP_synthesized.learn.bin'),
        ('KKP_synthesized.bin', 'KKP_synthesized.learn.bin'),
        ('KK_synthesized.bin', 'KK_synthesized.learn.bin')]:
        src = os.path.join(EVAL_FOLDER_NAME, eval_subfolder, src_filename)
        dst = os.path.join(EVAL_FOLDER_NAME, dst_filename)
        shutil.copyfile(src, dst)
      winning_percentage = GetWinningPercentage()
      csvwriter.writerow([eval_subfolder, winning_percentage])


if __name__ == '__main__':
  main()
