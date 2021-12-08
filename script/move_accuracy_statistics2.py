import csv
import statistics
import collections
import enum

Move = collections.namedtuple('Move', ['day' , 'game_record', 'trial', 'play', 'progress_type', 'match'])


def main():
    moves = list()
    filename = f'baito.csv'
    with open(filename, newline='') as csvfile:
        csv_reader = csv.reader(csvfile)
        for row in csv_reader:
            day = int(row[0])
            game_record = int(row[1])
            trial = int(row[2])
            play = int(row[3])
            progress_type = int(row[4])
            match = int(row[5])
            moves.append(Move(day, game_record, trial, play, progress_type, match))
    
    with open('move_accuracy_statistics2.csv', 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(['day', '棋譜', '試行', '序中終盤', '局面数', '一致回数', '一致率'])
        for day in range(1, 2 + 1):
            for game_record in range(0, 30 + 1):
                for trial in range(0, 10):
                    for progress_type in range(0, 3):
                        matches = [move.match for move in moves if move.day == day and move.game_record == game_record and move.trial == trial and move.progress_type == progress_type]
                        num_moves = len(matches)
                        num_matches = sum(matches)
                        if num_moves == 0:
                            continue
                        move_accuracy = num_matches / num_moves
                        writer.writerow([day, game_record, trial, progress_type, num_moves, num_matches, move_accuracy])


if __name__ == '__main__':
    main()
