import argparse
import requests


def main():
    parser = argparse.ArgumentParser(description='Execute Jenkins Jobs')
    parser.add_argument('--token', type=str, help='Jenkins API Token.')
    parser.add_argument('--host_name', type=str, help='Host name ex) nighthawk')
    parser.add_argument('--projet_name', type=str, help='Project name ex) generate_kifu.2021-08-29')
    args = parser.parse_args()

    for index in range(10, 20 + 1):
        # Parameterized Build - Jenkins - Jenkins Wiki https://wiki.jenkins.io/display/JENKINS/Parameterized+Build
        url = f'http://{args.host_name}:8080/job/{args.projet_name}/buildWithParameters?problem_id={problem_id}&solver_name={args.solver_name}'
        print(url)
        requests.post(url, auth=('hnoda', args.token))


if __name__ == '__main__':
    main()
