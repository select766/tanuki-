# -*- coding: utf-8 -*-
# [(特徴インデックス,頻度)]のCSVを解析
import os
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

# experimental_learner.cpp *IndexToRawIndex()参照 
SQ_NB = 81
fe_end = 90 + 18 * 81
N_KKT = SQ_NB * SQ_NB * 2
N_KPPT = SQ_NB * fe_end * fe_end * 2
N_KKPT = SQ_NB * SQ_NB * fe_end * 2
def get_KPP(arr): return arr[:N_KPPT]
def get_KKP(arr): return arr[N_KPPT:N_KPPT+N_KKPT]
def get_KK(arr): return arr[N_KPPT+N_KKPT:]

print('feature size = {}'.format(N_KKT+N_KPPT+N_KKPT))

def plot_CSV(fig, axs, file_path):
    df = pd.read_csv(file_path, header=None)
    print('CSV loaded')
    arr = np.array(df, dtype=np.int64)
    del df

    def plot_array(axs, name, arr):
        arr = np.array(arr)
        N = len(arr)
        print('N = {}'.format(N))
        if N == 0: return
        arr.sort(axis=0)
        print('sorted')
        arr = arr[::-1]
        max_vote = arr[0]

        cdf = np.cumsum(arr)
        total_vote = cdf[-1]
        cdf = cdf.astype(np.float32) / cdf[-1]

        # log scale indices
        indices = [0, 1]
        while indices[-1] < N:
            indices += [indices[-1] * i for i in range(2, 11)]
        indices += [N - 1]
        indices = [i for i in indices if i < N]

        line = axs[0].plot(indices, arr[indices], '.-', label=os.path.basename(file_path))[0]
        axs[0].set_title('{} #dim={} #count={}'.format(name, N, total_vote), fontsize=9)
        axs[0].set_yscale('log')
        axs[0].set_xscale('log')
        axs[0].set_ylabel('# appearance', fontsize=9)
        axs[0].set_xlabel('# feature', fontsize=9)
        level = 1.0
        while level < max_vote:
            level *= 10
            axs[0].axhline(y=level, color='gray', alpha=0.5)
        axs[0].axvline(x=N - 1, color=line.get_color(), linestyle='--', linewidth=2, alpha=0.5)

        axs[1].plot(indices, cdf[indices], '.-')
        axs[1].set_xscale('log')
        axs[1].set_ylabel('CDF', fontsize=9)
        axs[1].set_xlabel('# feature', fontsize=9)
        axs[1].set_ylim((0.0, 1.0))
        level = 0.0
        while level < 1.0:
            level += 0.1
            axs[1].axhline(y=level, color='gray', alpha=0.5)
        axs[1].axvline(x=N - 1, color=line.get_color(), linestyle='--', linewidth=2, alpha=0.5)

        del arr
        del cdf

    # 全部
    plot_array([axs[0, 0], axs[1, 0]], 'all', arr)
    # KPP
    plot_array([axs[0, 1], axs[1, 1]], 'KPP', get_KPP(arr))
    # KKP
    plot_array([axs[0, 2], axs[1, 2]], 'KKP', get_KKP(arr))
    # KK
    plot_array([axs[0, 3], axs[1, 3]], 'KK', get_KK(arr))

if __name__ == '__main__':
    fig, axs = plt.subplots(2, 4)
    for f in sys.argv[1:]:
        plot_CSV(fig, axs, f)
    axs[0, 0].legend(loc='best', fontsize=9)
    axs[0, 1].legend(loc='best', fontsize=9)
    axs[0, 2].legend(loc='best', fontsize=9)
    axs[0, 3].legend(loc='best', fontsize=9)
    plt.show()

