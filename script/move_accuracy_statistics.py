import csv
import statistics
import collections
import enum

class Color(enum.Enum):
    BOTH = 0
    BLACK = 1
    WHITE = 2


Move = collections.namedtuple('Move', ['multi_pv' , 'game_record', 'trial', 'play', 'progress_type', 'match'])
MoveAccuracy = collections.namedtuple('MoveAccuracy', ['multi_pv', 'game_record', 'trial', 'progress_type', 'num_moves', 'num_matched', 'color', 'move_accuracy'])


def main():
    moves = list()
    for multi_pv in range(1, 5 + 1):
        filename = f'MultiPV={multi_pv}.csv'
        with open(filename, newline='') as csvfile:
            csv_reader = csv.reader(csvfile)
            for row in csv_reader:
                game_record = int(row[0])
                trial = int(row[1])
                play = int(row[2])
                progress_type = int(row[3])
                match = int(row[4])
                moves.append(Move(multi_pv, game_record, trial, play, progress_type, match))
    
    move_accuracies = list()
    for multi_pv in range(1, 5 + 1):
        for game_record in range(0, 10):
            for trial in range(0, 10):
                for progress_type in range(0, 3):
                    # 先手後手両方
                    matches = [move.match for move in moves if move.multi_pv == multi_pv and move.game_record == game_record and move.trial == trial and move.progress_type == progress_type]
                    num_moves = len(matches)
                    num_matches = sum(matches)
                    move_accuracy = num_matches / num_moves
                    move_accuracies.append(MoveAccuracy(multi_pv, game_record, trial, progress_type, num_moves, num_matches, Color.BOTH, move_accuracy))
    
                    # 先手
                    matches = [move.match for move in moves if move.multi_pv == multi_pv and move.game_record == game_record and move.trial == trial and move.progress_type == progress_type and move.play % 2 == 1]
                    num_moves = len(matches)
                    num_matches = sum(matches)
                    move_accuracy = num_matches / num_moves
                    move_accuracies.append(MoveAccuracy(multi_pv, game_record, trial, progress_type, num_moves, num_matches, Color.BLACK, move_accuracy))
    
                    # 後手
                    matches = [move.match for move in moves if move.multi_pv == multi_pv and move.game_record == game_record and move.trial == trial and move.progress_type == progress_type and move.play % 2 == 0]
                    num_moves = len(matches)
                    num_matches = sum(matches)
                    move_accuracy = num_matches / num_moves
                    move_accuracies.append(MoveAccuracy(multi_pv, game_record, trial, progress_type, num_moves, num_matches, Color.WHITE, move_accuracy))
    
    with open('move_accuracy_statistics.csv', 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(['棋譜', '序中終盤', '先手後手', 'MultiPV', '局面数', '最小値', '最大値', '平均', '標準偏差', '中央値'])
        for game_record in range(0, 10):
            for progress_type in range(0, 3):
                for color in Color:
                    for multi_pv in range(1, 5 + 1):
                        selected_move_accuracies = [move_accuracy.move_accuracy for move_accuracy in move_accuracies if move_accuracy.multi_pv == multi_pv and move_accuracy.game_record == game_record and move_accuracy.progress_type == progress_type and move_accuracy.color == color]
                        min_value = min(selected_move_accuracies)
                        max_value = max(selected_move_accuracies)
                        mean = statistics.mean(selected_move_accuracies)
                        stdev = statistics.stdev(selected_move_accuracies)
                        median = statistics.median(selected_move_accuracies)
                        num_moves = [move_accuracy.num_moves for move_accuracy in move_accuracies if move_accuracy.multi_pv == multi_pv and move_accuracy.game_record == game_record and move_accuracy.progress_type == progress_type and move_accuracy.color == color][0]
                        writer.writerow([game_record, progress_type, color, multi_pv, num_moves, min_value, max_value, mean, stdev, median])


if __name__ == '__main__':
    main()
