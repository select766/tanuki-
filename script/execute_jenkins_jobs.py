import argparse
import requests
import urllib.parse


def main():
    parser = argparse.ArgumentParser(description='Execute Jenkins Jobs')
    parser.add_argument('--token', type=str,
                        help='Jenkins API Token.', required=True)
    parser.add_argument('--host_name', type=str,
                        help='Host name ex) nighthawk', required=True)
    parser.add_argument('--projet_name', type=str,
                        help='Project name ex) generate_kifu.2021-08-29', required=True)
    parser.add_argument('--user_name', type=str,
                        help='User name ex) nodchip', required=True)
    args = parser.parse_args()

    for index in range(0, 7):
        thread_id_offset = index * 16 % 128
        numa_node = thread_id_offset // 64

        # Parameterized Build - Jenkins - Jenkins Wiki https://wiki.jenkins.io/display/JENKINS/Parameterized+Build
        # query = urllib.parse.urlencode({
        #     'KifuDir': fr'D:\hnoda\shogi\training_data\tanuki-wcsc28.depth={depth}',
        #     'ShuffledKifuDir': fr'D:\hnoda\shogi\training_data\tanuki-wcsc28.depth={depth}.shuffled',
        #     'KifuDirForTest': fr'D:\hnoda\shogi\validation_data\tanuki-wcsc28.depth={depth}',
        #     'ShuffledKifuDirForTest': fr'D:\hnoda\shogi\validation_data\tanuki-wcsc28.depth={depth}.shuffled',
        # })

        # query = urllib.parse.urlencode({
        #     'EvalDir': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
        #     'EvalSaveDir': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.depth={depth}',
        #     'targetdir': fr'D:\hnoda\shogi\training_data\tanuki-wcsc28.depth={depth}.shuffled',
        #     'eta': fr'0.01',
        #     'validation_set_file_name': fr'D:\hnoda\shogi\validation_data\tanuki-wcsc28.depth={depth}.shuffled\xaa',
        #     'ThreadIdOffset': fr'{index * 16 % 128}',
        # })

        # query = urllib.parse.urlencode({
        #     'eval1': fr'D:\hnoda\tanuki-denryu-tsec-1\eval',
        #     'eval2': fr'D:\hnoda\tanuki-denryu-tsec-1\eval',
        #     'slow_mover1': fr'{slow_mover}',
        # })

        # query = urllib.parse.urlencode({
        #     'eval1': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.depth={depth}\final',
        #     'eval2': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
        # })

        # l = f'{index/10.0:.1f}'
        # query = urllib.parse.urlencode({
        #     'EvalDir': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
        #     'EvalSaveDir': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.depth=9.lambda={l}',
        #     'targetdir': fr'D:\hnoda\shogi\training_data\tanuki-wcsc28.depth=9.shuffled',
        #     'eta': fr'0.01',
        #     'validation_set_file_name': fr'D:\hnoda\shogi\validation_data\tanuki-wcsc28.depth=9.shuffled\xaa',
        #     'ThreadIdOffset': fr'{index * 16 % 128}',
        #     'lambda': l,
        # })

        # l = f'{index/10.0:.1f}'
        # query = urllib.parse.urlencode({
        #     'eval1': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.depth=9.lambda={l}\final',
        #     'eval2': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
        # })

        # p = index * 0.5 + 9.0
        # eval_limit = int(2.0 ** p)
        # query = urllib.parse.urlencode({
        #     'EvalDir': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
        #     'EvalSaveDir': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.depth=9.lambda=0.4.eval_limit={eval_limit}',
        #     'targetdir': fr'D:\hnoda\shogi\training_data\tanuki-wcsc28.depth=9.shuffled',
        #     'eta': fr'0.01',
        #     'validation_set_file_name': fr'D:\hnoda\shogi\validation_data\tanuki-wcsc28.depth=9.shuffled\xaa',
        #     'ThreadIdOffset': fr'{128 - 16 - index * 16 % 128}',
        #     'lambda': fr'0.4',
        #     'eval_limit': fr'{eval_limit}'
        # })

        # p = index * 0.5 + 9.0
        # eval_limit = int(2.0 ** p)
        # query = urllib.parse.urlencode({
        #     'eval1': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.depth=9.lambda=0.4.eval_limit={eval_limit}\final',
        #     'eval2': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
        # })

        # winning_percentage_for_win = fr'{1.0 - 2.0 ** (-(index + 2)):0.6f}'
        # query = urllib.parse.urlencode({
        #     'EvalDir': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
        #     'EvalSaveDir': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.winning_percentage_for_win={winning_percentage_for_win}',
        #     'targetdir': fr'D:\hnoda\shogi\training_data\tanuki-wcsc28.depth=9.shuffled',
        #     'eta': fr'0.01',
        #     'validation_set_file_name': fr'D:\hnoda\shogi\validation_data\tanuki-wcsc28.depth=9.shuffled\xaa',
        #     'ThreadIdOffset': fr'{index * 16 % 128}',
        #     'winning_percentage_for_win': fr'{winning_percentage_for_win}'
        # })

        # winning_percentage_for_win = fr'{1.0 - 2.0 ** (-(index + 2)):0.6f}'
        # if index == 10:
        #     winning_percentage_for_win = fr'0.999999'
        # query = urllib.parse.urlencode({
        #     'eval1': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.winning_percentage_for_win={winning_percentage_for_win}\final',
        #     'eval2': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
        # })

        # l2_regularization_parameter = fr'{10.0 ** (-(index + 1)):0.8f}'
        # query = urllib.parse.urlencode({
        #     'EvalDir': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
        #     'EvalSaveDir': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.l2_regularization_parameter={l2_regularization_parameter}',
        #     'targetdir': fr'D:\hnoda\shogi\training_data\tanuki-wcsc28.depth=9.shuffled',
        #     'eta': fr'0.01',
        #     'validation_set_file_name': fr'D:\hnoda\shogi\validation_data\tanuki-wcsc28.depth=9.shuffled\xaa',
        #     'ThreadIdOffset': fr'{index * 16 % 128}',
        #     'l2_regularization_parameter': fr'{l2_regularization_parameter}'
        # })

        # l2_regularization_parameter = fr'{10.0 ** (-(index + 1)):0.8f}'
        # query = urllib.parse.urlencode({
        #     'eval1': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.l2_regularization_parameter={l2_regularization_parameter}\final',
        #     'eval2': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
        # })

        nn_batch_size = int(10.0 ** ((index / 2.0) + 2.0))
        eta = 10.0 ** ((index / 2.0) - 3.0)
        query = urllib.parse.urlencode({
            'EvalDir': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
            'EvalSaveDir': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.nn_batch_size={nn_batch_size}',
            'targetdir': fr'D:\hnoda\shogi\training_data\tanuki-wcsc28.depth=9.shuffled',
            'eta': fr'{eta}',
            'validation_set_file_name': fr'D:\hnoda\shogi\validation_data\tanuki-wcsc28.depth=9.shuffled\xaa',
            'ThreadIdOffset': fr'{thread_id_offset}',
            'numa_node': fr'{numa_node}',
            'nn_batch_size': fr'{nn_batch_size}'
        })

        # nn_batch_size = int(10.0 ** ((index / 2.0) + 2.0))
        # query = urllib.parse.urlencode({
        #     'eval1': fr'D:\hnoda\shogi\eval\tanuki-wcsc28.nn_batch_size={nn_batch_size}\final',
        #     'eval2': fr'D:\hnoda\tnk-wcsc28-2018-05-05\eval',
        # })

        url = f'http://{args.host_name}:8080/job/{args.projet_name}/buildWithParameters?{query}'
        print(url)
        requests.post(url, auth=('hnoda', args.token))


if __name__ == '__main__':
    main()
