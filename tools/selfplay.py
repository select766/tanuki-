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
ENGINE_CONFIG_TXT_TEMPLATE = '''YaneuraOu-2016-late.exe
go byoyomi 1000
setoption name Threads value 1
setoption name Hash value 256
setoption name EvalDir value {0}
setoption name NetworkDelay value 0
setoption name NetworkDelay2 value 0
setoption name MinimumThinkingTime value 1000
'''


def GetSubfolders(folder_path):
    return [x for x in os.listdir(folder_path) if os.path.isdir(os.path.join(folder_path, x))]


def GetDateTimeString():
    now = datetime.datetime.now()
    return now.strftime("%Y-%m-%d-%H-%M-%S")


def AdoptSubfolder(subfolder):
    num_positions = int(subfolder)
    return num_positions % 1000000000 == 0


def CalculateWinningPercentage(base_eval_folder_path, learner_output_folder_path):
    print("CalculateWinningPercentage()", flush=True)
    subfolders = GetSubfolders(learner_output_folder_path)
    output_cvs_file_path = os.path.join(learner_output_folder_path, 'winning_percentage.' + GetDateTimeString() + '.csv')
    with open(output_cvs_file_path, 'w', newline='') as output_file:
        csvwriter = csv.writer(output_file)
        for subfolder in subfolders:
            if not AdoptSubfolder(subfolder):
                continue
            print(subfolder, flush=True)

            with open('engine-config1.txt', 'w') as engine_config1_file:
                engine_config1_file.write(ENGINE_CONFIG_TXT_TEMPLATE.format(base_eval_folder_path))
            with open('engine-config2.txt', 'w') as engine_config2_file:
                engine_config2_file.write(ENGINE_CONFIG_TXT_TEMPLATE.format(os.path.join(learner_output_folder_path, subfolder)))
            input = '''usi
setoption name Threads value 48
isready
usinewgame
go btime 1000 byoyomi 1
'''.encode('utf-8')
            completed_process = subprocess.run(['YaneuraOu-local-server.exe'], input=input, stdout=subprocess.PIPE, check=True)
            output = completed_process.stdout.decode('utf-8')
            print(output, flush=True)
            matched = re.compile('GameResult (\\d+) - (\\d+) - (\\d+)').search(output)
            lose = float(matched.group(1))
            draw = float(matched.group(2))
            win = float(matched.group(3))
            winning_percentage = 0.0
            if lose + draw + win > 0.0:
             winning_percentage = win / (lose + draw + win)
            csvwriter.writerow([subfolder, winning_percentage])


def CalculateError(learner_output_folder_path, learner_exe_file_path):
    print("CalculateError()", flush=True)
    subfolders = GetSubfolders(learner_output_folder_path)
    output_cvs_file_path = os.path.join(learner_output_folder_path, 'error.' + GetDateTimeString() + '.csv')
    with open(output_cvs_file_path, 'w', newline='') as output_file:
        csvwriter = csv.writer(output_file)
        csvwriter.writerow(['', 'rmse_value', 'rmse_winning_percentage', 'mean_cross_entropy', 'norm'])
        for subfolder in subfolders:
            print(subfolder, flush=True)
            input = '''setoption name EvalDir value {0}
setoption name KifuDir value kifu_for_test
setoption name Threads value 48
error_measurement
'''.format(os.path.join(learner_output_folder_path, subfolder)).encode('utf-8')
            completed_process = subprocess.run([learner_exe_file_path], input=input, stdout=subprocess.PIPE, check=True)
            output = completed_process.stdout.decode('utf-8')
            print(output, flush=True)
            matched = re.compile('info string rmse_value=(.+) rmse_winning_percentage=(.+) mean_cross_entropy=(.+) norm=(.+)').search(output)
            rmse_value = float(matched.group(1))
            rmse_winning_percentage = float(matched.group(2))
            mean_cross_entropy = float(matched.group(3))
            norm = float(matched.group(4))
            csvwriter.writerow([subfolder, rmse_value, rmse_winning_percentage, mean_cross_entropy, norm])


def main():
    parser = argparse.ArgumentParser(description='selfplay')
    parser.add_argument(
        '--base_eval_folder_path',
        action='store',
        dest='base_eval_folder_path',
        required=True,
        help='Folder path of the base eval files. ex) eval/apery_wcsc26')
    parser.add_argument(
        '--learner_output_folder_path',
        action='store',
        dest='learner_output_folder_path',
        required=True,
        help='Folder path of the learner output. ex) learner_output/2016-06-28')
    parser.add_argument(
        '--calculate_winning_percentage',
        action='store_true',
        dest='calculate_winning_percentage',
        help='Calculate the winning percentage.')
    parser.add_argument(
        '--calculate_error',
        action='store_true',
        dest='calculate_error',
        help='Calculate the errors.')
    parser.add_argument(
        '--learner_exe_file_path',
        action='store',
        dest='learner_exe_file_path',
        help='Exe file name of the learner. ex) YaneuraOu.2016-06-23.qsearch.0.1.exe',
        required=True)
    args = parser.parse_args()

    if args.calculate_winning_percentage:
        CalculateWinningPercentage(args.base_eval_folder_path, args.learner_output_folder_path)

    if args.calculate_error:
        CalculateError(args.learner_output_folder_path, args.learner_exe_file_path)


if __name__ == '__main__':
    main()
