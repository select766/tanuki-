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

    for index, parameter in enumerate([-500,-400,-300,-200,-100,-2,100,200,300,400,500]):
        thread_id_offset = index * 16 % 128
        numa_node = thread_id_offset // 64

        # Parameterized Build - Jenkins - Jenkins Wiki https://wiki.jenkins.io/display/JENKINS/Parameterized+Build

        # winning_percentage_for_win = fr'{parameter:0.6f}'
        # query = urllib.parse.urlencode({
        #     'EvalDir': fr'D:\hnoda\tanuki-wcsc29-2019-05-06\eval',
        #     'EvalSaveDir': fr'D:\hnoda\shogi\eval\tanuki-wcsc29-2019-05-06.winning_percentage_for_win={winning_percentage_for_win}',
        #     'targetdir': fr'D:\hnoda\shogi\training_data\suisho-wcsoc2020.shuffled',
        #     'eta': fr'1.0',
        #     'validation_set_file_name': fr'D:\hnoda\shogi\validation_data\suisho-wcsoc2020.shuffled\xaa',
        #     'ThreadIdOffset': fr'{index * 16 % 128}',
        #     'winning_percentage_for_win': fr'{winning_percentage_for_win}',
        #     'numa_node': fr'{numa_node}',
        #     'weight_by_progress': '1',
        # })

        # winning_percentage_for_win = fr'{parameter:0.6f}'
        # query = urllib.parse.urlencode({
        #     'eval1': fr'D:\hnoda\shogi\eval\tanuki-wcsc29-2019-05-06.winning_percentage_for_win={winning_percentage_for_win}\final',
        #     'eval2': fr'D:\hnoda\tanuki-denryu-tsec-1\eval',
        # })

        draw_value1 = fr'{parameter}'
        query = urllib.parse.urlencode({
            'eval1': fr'D:\hnoda\tanuki-denryu-tsec-1\eval',
            'eval2': fr'D:\hnoda\tanuki-denryu-tsec-1\eval',
            'draw_value1': fr'{draw_value1}',
        })

        url = f'http://{args.host_name}:8080/job/{args.projet_name}/buildWithParameters?{query}'
        print(url)
        requests.post(url, auth=('hnoda', args.token))


if __name__ == '__main__':
    main()
