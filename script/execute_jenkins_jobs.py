import argparse
import requests
import urllib.parse


def main():
    parser = argparse.ArgumentParser(description='Execute Jenkins Jobs')
    parser.add_argument('--token', type=str, help='Jenkins API Token.', required=True)
    parser.add_argument('--host_name', type=str, help='Host name ex) nighthawk', required=True)
    parser.add_argument('--projet_name', type=str, help='Project name ex) generate_kifu.2021-08-29', required=True)
    parser.add_argument('--user_name', type=str, help='User name ex) nodchip', required=True)
    args = parser.parse_args()

    for depth in range(9, 20 + 1):
        # Parameterized Build - Jenkins - Jenkins Wiki https://wiki.jenkins.io/display/JENKINS/Parameterized+Build
        query = urllib.parse.urlencode({
            'KifuDir': fr'D:\hnoda\shogi\training_data\tanuki-wcsc28.depth={depth}',
            'ShuffledKifuDir': fr'D:\hnoda\shogi\training_data\tanuki-wcsc28.depth={depth}.shuffled',
            'KifuDirForTest': fr'D:\hnoda\shogi\validation_data\tanuki-wcsc28.depth={depth}',
            'ShuffledKifuDirForTest': fr'D:\hnoda\shogi\validation_data\tanuki-wcsc28.depth={depth}.shuffled',
        })
        url = f'http://{args.host_name}:8080/job/{args.projet_name}/buildWithParameters?{query}'
        print(url)
        requests.post(url, auth=('hnoda', args.token))


if __name__ == '__main__':
    main()
