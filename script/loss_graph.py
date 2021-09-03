import collections
import datetime
import matplotlib.pyplot as plt
import numpy as np
import re
import urllib
import urllib.request
import time


Pattern = collections.namedtuple('Pattern', ['label', 'regex_pattern'])


URL = 'url'
LABEL = 'label'
DATA_LIST = [
]
PATTERNS = [
    [
        # Pattern('learn_cross_entropy', r', ([\d]+) sfens, .+ , learn_cross_entropy = ([.\d]+) ,'),
        Pattern('test cross entropy',
                r', ([\d]+) sfens, .+ , test_cross_entropy = ([.\d]+) ,'),
    ],
    [
        Pattern('move accuracy',
                r', ([\d]+) sfens, .+ , move accuracy = ([.\d]+)% , '),
    ],
    [
        Pattern('eta', r', ([\d]+) sfens, .+, eta = ([.\d]+),'),
    ],
    [
        Pattern('hirate eval',
                r', ([\d]+) sfens, .+, hirate eval = ([.\d]+) ,'),
    ],
]
MIN_SFENS = -1


def Show():
    start_time = time.time()

    fig, axes = plt.subplots(
        nrows=2, ncols=2, sharex=False, figsize=(1920 / 100.0, 1080 / 100.0))

    for data in DATA_LIST:
        print(data[URL])
        with urllib.request.urlopen(data[URL]) as response:
            html = response.read().decode('cp932')

        for index, patterns in enumerate(PATTERNS):
            for pattern in patterns:
                xs = list()
                ys = list()
                for m in re.finditer(pattern.regex_pattern, html):
                    x = m.group(1)
                    x = float(x)
                    if x < MIN_SFENS:
                        continue
                    xs.append(x)
                    y = m.group(2)
                    y = float(y)
                    ys.append(y)
                if not xs:
                    continue
                axes[index % 2, index //
                     2].plot(xs, ys, label=data[LABEL], linewidth=1.0)

    for index in range(len(PATTERNS)):
        axe = axes[index % 2, index // 2]
        axe.legend()
        axe.grid()
        axe.set_title(PATTERNS[index][0].label)

    elapsed_time = time.time() - start_time
    print("elapsed_time:{0}".format(elapsed_time) + "[sec]")

    fig.tight_layout()

    # plt.show()

    now = datetime.datetime.now()
    filename = 'image.' + now.strftime("%Y-%m-%d-%H-%M-%S") + ".png"
    plt.savefig(filename)


def main():
    Show()


if __name__ == '__main__':
    main()
