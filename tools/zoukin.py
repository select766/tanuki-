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


SHUFFLED_BIN_FILE_NAME = 'shuffled.bin'


class State(enum.Enum):
    generate_kifu = 1
    shuffle_kifu = 2
    learn = 3
    self_play = 4


def GetSubfolders(folder_path):
    return [x for x in os.listdir(folder_path) if os.path.isdir(os.path.join(folder_path, x))]


def GetDateTimeString():
    now = datetime.datetime.now()
    return now.strftime("%Y-%m-%d-%H-%M-%S")


def AdoptSubfolder(subfolder):
    num_positions = int(subfolder)
    return num_positions % 1000000000 == 0 or num_positions % 10000 != 0


def ToOptionValueString(b):
    if b:
        return 'True'
    else:
        return 'False'


def generate_kifu(args):
    print(locals(), flush=True)
    os.makedirs(args.kifu_folder_path, exist_ok=True)
    input = '''usi
EvalDir {eval_dir}
KifuDir {kifu_dir}
Threads {threads}
Hash {hash}
GeneratorNumPositions {generator_num_positions}
GeneratorSearchDepth {generator_search_depth}
GeneratorKifuTag {generator_kifu_tag}
GeneratorStartposFileName {generator_startpos_file_name}
GeneratorValueThreshold {generator_value_threshold}
GeneratorOptimumNodesSearched {generator_optimum_nodes_searched}
isready
usinewgame
generate_kifu
quit
'''.format(eval_dir=args.eval_folder_path,
    kifu_dir=args.kifu_folder_path,
    threads=str(args.threads_to_generate_kifu),
    hash=str(args.hash_to_generate_kifu),
    generator_num_positions=str(args.generator_num_positions),
    generator_search_depth=str(args.generator_search_depth),
    generator_kifu_tag=args.generator_kifu_tag,
    generator_startpos_file_name=args.generator_startpos_file_name,
    generator_value_threshold=str(args.generator_value_threshold),
    generator_optimum_nodes_searched=str(args.generator_optimum_nodes_searched))
    print(input, flush=True)
    subprocess.run([args.generate_kifu_exe_file_path], input=input.encode('utf-8'), check=True)


def shuffle_kifu(args, kifu_dir, shuffled_kifu_dir, split=False):
    print(locals(), flush=True)
    shutil.rmtree(shuffled_kifu_dir, ignore_errors=True)
    os.makedirs(shuffled_kifu_dir, exist_ok=True)
    input = '''usi
SkipLoadingEval true
KifuDir {kifu_dir}
ShuffledKifuDir {shuffled_kifu_dir}
isready
usinewgame
shuffle_kifu
quit
'''.format(kifu_dir=kifu_dir,
           shuffled_kifu_dir=shuffled_kifu_dir)
    print(input, flush=True)
    subprocess.run([args.shuffle_kifu_exe_file_path], input=input.encode('utf-8'), check=True)

    if not split:
        return

    with open(os.path.join(shuffled_kifu_dir, SHUFFLED_BIN_FILE_NAME), 'wb') as output:
        remained = args.validation_set_file_size
        for file_name in os.listdir(shuffled_kifu_folder_path):
            with open(os.path.join(shuffled_kifu_folder_path, file_name), 'rb') as input:
                data = input.read(remained)
            remained -= len(data)
            output.write(data)


def Learn(args):
    print(locals(), flush=True)
    input = '''EvalDir {eval_dir}
Threads {threads}
Hash {hash}
EvalSaveDir {eval_save_dir}
learn targetdir {targetdir} loop {loop} batchsize {batchsize} lambda {elmo_lambda} eta {eta} newbob_decay {newbob_decay} eval_save_interval {eval_save_interval} loss_output_interval {loss_output_interval} mirror_percentage {mirror_percentage} validation_set_file_name {validation_set_file_name} nn_batch_size {nn_batch_size} eval_limit {eval_limit}
'''.format(eval_dir=args.eval_folder_path,
           threads=str(args.threads_to_learn),
           hash=str(args.hash_to_learn),
           eval_save_dir=args.eval_save_folder_path,
           targetdir=args.shuffled_kifu_folder_path,
           loop=str(args.loop),
           batchsize=str(args.batchsize),
           elmo_lambda=str(args.elmo_lambda),
           eta=str(args.eta),
           newbob_decay=str(args.newbob_decay),
           eval_save_interval=str(args.eval_save_interval),
           loss_output_interval=str(args.loss_output_interval),
           mirror_percentage=str(args.mirror_percentage),
           validation_set_file_name=os.path.join(args.shuffled_kifu_for_test_folder_path, SHUFFLED_BIN_FILE_NAME),
           nn_batch_size=str(args.nn_batch_size),
           eval_limit=str(args.eval_limit))
    print(input, flush=True)
    subprocess.run([args.learner_exe_file_path], input=input.encode('utf-8'), check=True)


def SelfPlay(args, engine1, eval1, engine2, eval2):
    print(locals(), flush=True)
    args = ['TanukiColiseum.exe',
        '--engine1', engine1,
        '--engine2', engine2,
        '--eval1', eval1,
        '--eval2', eval2,
        '--num_concurrent_games', str(args.threads_to_selfplay),
        '--num_games', str(args.num_games_to_selfplay),
        '--hash', str(args.hash_to_selfplay),
        '--time', str(args.thinking_time_ms),
        '--num_numa_nodes', str(args.num_numa_nodes),
        '--num_book_moves1', '0',
        '--num_book_moves2', '0',
        '--book_file_name1', 'no_book',
        '--book_file_name2', 'no_book',
        '--num_book_moves', '24',
        '--no_gui',
        '--sfen_file_name', args.startpos_file_name_to_selfplay,]
    print(args, flush=True)
    if subprocess.run(args).returncode:
        sys.exit('Failed to calculate the winning rate...')


def GetSubfolders(folder_path):
    return [x for x in os.listdir(folder_path) if os.path.isdir(os.path.join(folder_path, x))]


def main():
    parser = argparse.ArgumentParser(description='iteration')
    parser.add_argument('--generate_kifu_exe_file_path',
        action='store',
        required=True,
        help='Exe file path to generate kifu.')
    parser.add_argument('--shuffle_kifu_exe_file_path',
        action='store',
        required=True,
        help='Exe file path to shuffle kifu.')
    parser.add_argument('--learner_exe_file_path',
        action='store',
        required=True,
        help='Exe file path to learn.')
    parser.add_argument('--eval_folder_path',
        action='store',
        required=True,
        help='Folder path containing evaluation function files.')
    parser.add_argument('--kifu_folder_path',
        action='store',
        required=True,
        help='Folder path to save kifu.')
    parser.add_argument('--kifu_for_test_folder_path',
        action='store',
        required=True,
        help='Folder path to save kifu for test.')
    parser.add_argument('--threads_to_generate_kifu',
        action='store',
        type=int,
        required=True,
        help='Number of threads to generate kifu.')
    parser.add_argument('--hash_to_generate_kifu',
        action='store',
        type=int,
        required=True,
        help='Hash size in mega bytes to generate kifu.')
    parser.add_argument('--generator_num_positions',
        action='store',
        type=int,
        required=True,
        help='Number of positions to generate.')
    parser.add_argument('--generator_search_depth',
        action='store',
        type=int,
        required=True,
        help='Search depth to generate kifu.')
    parser.add_argument('--generator_kifu_tag',
        action='store',
        required=True,
        help='Kifu tag to generate kifu.')
    parser.add_argument('--generator_startpos_file_name',
        action='store',
        type=int,
        required=True,
        help='Start position file path to generate kifu.')
    parser.add_argument('--generator_value_threshold',
        action='store',
        type=int,
        required=True,
        help='Value threshold to generate kifu.')
    parser.add_argument('--generator_optimum_nodes_searched',
        action='store',
        type=int,
        required=True,
        help='Number of optimum nodes searched to generate kifu.')
    parser.add_argument('--validation_set_file_size',
        action='store',
        type=int,
        required=True,
        help='File size of the validation set.')
    parser.add_argument('--threads_to_learn',
        action='store',
        type=int,
        required=True,
        help='Number of threads to learn')
    parser.add_argument('--hash_to_learn',
        action='store',
        type=int,
        required=True,
        help='Hash size in mega bytes to learn.')
    parser.add_argument('--eval_save_folder_path',
        action='store',
        required=True,
        help='Folder path to save the output evaluation function files.')
    parser.add_argument('--shuffled_kifu_folder_path',
        action='store',
        required=True,
        help='Folder path to save shuffled kifu.')
    parser.add_argument('--shuffled_kifu_for_test_folder_path',
        action='store',
        required=True,
        help='Folder path to save shuffled kifu for test.')
    parser.add_argument('--loop',
        action='store',
        type=int,
        required=True,
        help='loop parameter in the NNUE machine learning routine.')
    parser.add_argument('--batchsize',
        action='store',
        type=int,
        required=True,
        help='batchsize parameter in the NNUE machine learning routine.')
    parser.add_argument('--elmo_lambda',
        action='store',
        type=float,
        required=True,
        help='lambda parameter in the NNUE machine learning routine.')
    parser.add_argument('--eta',
        action='store',
        type=float,
        required=True,
        help='eta parameter in the NNUE machine learning routine.')
    parser.add_argument('--newbob_decay',
        action='store',
        type=float,
        required=True,
        help='newbob_decay parameter in the NNUE machine learning routine.')
    parser.add_argument('--eval_save_interval',
        action='store',
        type=int,
        required=True,
        help='eval_save_interval parameter in the NNUE machine learning routine.')
    parser.add_argument('--loss_output_interval',
        action='store',
        type=int,
        required=True,
        help='loss_output_interval parameter in the NNUE machine learning routine.')
    parser.add_argument('--mirror_percentage',
        action='store',
        type=int,
        required=True,
        help='mirror_percentage parameter in the NNUE machine learning routine.')
    parser.add_argument('--nn_batch_size',
        action='store',
        type=int,
        required=True,
        help='nn_batch_size parameter in the NNUE machine learning routine.')
    parser.add_argument('--eval_limit',
        action='store',
        type=int,
        required=True,
        help='eval_limit parameter in the NNUE machine learning routine.')
    parser.add_argument('--threads_to_selfplay',
        action='store',
        type=int,
        required=True,
        help='Number of threads to selfplay.')
    parser.add_argument('--num_games_to_selfplay',
        action='store',
        type=int,
        required=True,
        help='Number of games to selfplay.')
    parser.add_argument('--hash_to_selfplay',
        action='store',
        type=int,
        required=True,
        help='Hash size in mega bytes to selfplay.')
    parser.add_argument('--thinking_time_ms',
        action='store',
        type=int,
        required=True,
        help='Thinking time in milliseconds to selfplay.')
    parser.add_argument('--num_numa_nodes',
        action='store',
        type=int,
        required=True,
        help='Number of NUMA nodes to selfplay.')
    parser.add_argument('--startpos_file_name_to_selfplay',
        action='store',
        required=True,
        help='Start positions file path to selfplay.')
    parser.add_argument('--reference_engine_file_path',
        action='append',
        required=True,
        help='File path of a reference engine. User needs to specify the same number of the parameters to --reference_engine_file_path and --reference_eval_folder_path.')
    parser.add_argument('--reference_eval_folder_path',
        action='append',
        required=True,
        help='Folder path of an evaluation function files. User needs to specify the same number of the parameters to --reference_engine_file_path and --reference_eval_folder_path.')
    parser.add_argument('--my_engine_file_path',
        action='store',
        required=True,
        help='File path of the engine to work with the output evaluation function file.')

    args = parser.parse_args()

    print('-' * 80)
    print('- %s' % state)
    print('-' * 80, flush=True)

    stop_on_this_state = (state == final_state)

    if state == State.generate_kifu:
        kifu_folder_path = os.path.join(kifu_output_folder_path_base, GetDateTimeString())
        for division in range(initial_division_to_generator_train, num_divisions_to_generator_train):
            GenerateKifu(args, old_eval_folder_path, kifu_folder_path,
                        int(num_positions_to_generator_train / num_divisions_to_generator_train),
                        'train.{0}'.format(division))
        state = State.generate_kifu_for_test

    if state == State.generate_kifu_for_test:
        kifu_for_test_folder_path = os.path.join(kifu_for_test_output_folder_path_base,
                                                GetDateTimeString())
        GenerateKifu(args, old_eval_folder_path, kifu_for_test_folder_path,
                    num_positions_to_generator_test, 'test')
        state = State.shuffle_kifu

    if state == State.shuffle_kifu:
        shuffled_kifu_folder_path = kifu_folder_path + '-shuffled'
        ShuffleKifu(args, kifu_folder_path, shuffled_kifu_folder_path)
        state = State.learn

    if state == State.learn:
        new_eval_folder_path = os.path.join(learner_output_folder_path_base,
                                            GetDateTimeString())
        Learn(args, old_eval_folder_path, shuffled_kifu_folder_path, kifu_for_test_folder_path,
            new_eval_folder_path)
        state = State.self_play

    if state == State.self_play:
        for reference_eval_folder_path in [old_eval_folder_path] + reference_eval_folder_paths:
            SelfPlay(args, reference_eval_folder_path, new_eval_folder_path)
            state = State.generate_kifu
            iteration += 1
            old_eval_folder_path = new_eval_folder_path


if __name__ == '__main__':
  main()
