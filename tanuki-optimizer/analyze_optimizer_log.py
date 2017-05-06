#!env python
# -*- coding:utf-8 -*-
# analyze a log file produced by optimize_parameters.py to calculate posterior of the fitness ~ hyperparameter model.
# usage:
#  analyze_optimizer_log.py parameters_reordered.csv parameters-xxxx.txt 

import os
import sys
import re
import csv
import datetime

import pandas as pd
import numpy as np
import scipy
import sklearn.gaussian_process
#import sklearn.kernel_ridge
import matplotlib
matplotlib.use('tkagg')
import matplotlib.pyplot as plt
import pickle
from hyperopt_state import HyperoptState

def score_from_game_result(game_result):
    u'''calculate score (lower is better, [0.0, 1.0] or None (ignore)) from game_result'''
    lose, draw, win = map(int, game_result)
    n_games = lose + draw + win

    # if an error occured during matches, n_games might be zero.
    if n_games == 0:
        return None # ignore.

    return (win * (0.0) + draw * (0.5) + lose * (1.0)) / float(n_games)

def winning_rate_from_score(score, n_matches):
    u'''do inverse of score_from_game_result()'''
    wins = (1.0 - score) * n_matches
    return wins / n_matches

def transform_score_to_target(ys):
    u'''score [0, 1] -> [-4, 4] (logit)'''
    return np.maximum(-4.0, np.minimum(4.0, np.log(ys) - np.log(1.0 - ys)))

def transform_target_to_score(ys):
    u'''[-inf, inf] -> score [0, 1] (logistic)'''
    return 1.0 / (1.0 + np.exp(-ys))

def stringify_value_in_bounds(value, default_value, min_value, max_value, n_chars=20):
    u'''create a string like |---+---@----| for visualization. +: default'''
    n_chars = max(1, n_chars - 2)
    res = ['-'] * n_chars
    vidx = max(0, min(n_chars - 1, int((n_chars - 1) * (value - min_value) / (max_value - min_value))))
    didx = max(0, min(n_chars - 1, int((n_chars - 1) * (default_value - min_value) / (max_value - min_value))))
    res[didx] = '+'
    res[vidx] = '@'
    return '|{}|'.format(''.join(res))

def create_header_file(file_path, analyzer, x_opt, additional_dict={}):
    u'''create a C++ header to define (optimal) parameter x_opt'''
    with open(file_path, 'wb') as fo:
        guard = os.path.basename(file_path).replace('.', '_').upper()
        now = datetime.datetime.now()
        fo.write('#ifndef __{}__\n'.format(guard))
        fo.write('#define __{}__\n'.format(guard))
        fo.write('// Created at: {}\n'.format(now.strftime('%Y-%m-%d %H:%M:%S')))
        fo.write('// Log: {}\n'.format(analyzer.log_path))
        fo.write('// Parameters CSV: {}\n'.format(analyzer.parameters_csv_path))
        for k, v in additional_dict.items():
            fo.write('// {} = {}\n'.format(k, v))
        fo.write('\n')
        for index, (name, (default_value, min_value, max_value), best_value) in enumerate(zip(analyzer.index2name, analyzer.index2bounds, x_opt)):
            rounded_value = int(round(best_value))
            option_string = '{} raw={}, min={}, max={} default={}'.format(
                stringify_value_in_bounds(rounded_value, default_value, min_value, max_value),
                best_value, min_value, max_value, default_value)
            fo.write('// {}\n'.format(option_string))
            fo.write('PARAM_DEFINE {} = {};\n'.format(name, rounded_value))
            fo.write('\n')
        fo.write('\n')
        fo.write('#endif // __{}__\n'.format(guard))
    print('Wrote header file to: {}'.format(file_path))

class LogAnalyzer(object):
    def __init__(self, log_file_path, parameters_csv_file_path):
        self.log_path = None
        self.parameters_csv_path = None

        self.parse_parameters_csv(parameters_csv_file_path)
        self.parse_file(log_file_path)

    def parse_file(self, path):
        with open(path, 'rb') as fi:
            if self.parse_file_object(fi) is not None:
                self.log_path = path

    def parse_file_object(self, fi):
        print('Loading the pickle file...')
        state = pickle.load(fi)
        print('Reading values...')
        log_entries = [] # (parameters, GameResult, score)
        values = []
        for name in self.index2name:
            values.append(state.trials.vals[name])
            sys.stdout.write('.')
        print ''
        print('Generating log entries...')
        for index, result in enumerate(state.trials.results):
            if result['status'] != 'ok':
                continue
            # lower is better
            win = state.iteration_logs[index]['win']
            draw = state.iteration_logs[index]['draw']
            lose = state.iteration_logs[index]['lose']
            score = None
            if win + draw + lose > 0.0:
              score = (win * 0.0 + draw * 0.5 + lose * 1.0) / (win + draw + lose)
            parameters = []
            for row in values:
                parameters.append(row[index])
            log_entries.append((tuple(parameters), (lose, draw, win), score))

        self.log_entries = log_entries
        return log_entries

    def parse_parameters_csv(self, csv_path):
        index2name = []
        index2bounds = []
        with open(csv_path, 'rb') as fi:
            reader = csv.reader(fi)
            for index, row in enumerate(reader):
                name, default_value, min_value, max_value = row
                index2name.append(name)
                index2bounds.append(map(int, (default_value, min_value, max_value))) # all parameters are integer.

        self.parameters_csv_path = csv_path
        self.index2name = index2name
        self.index2bounds = index2bounds

    def all_entries(self):
        return list(self.log_entries)

    def effective_entries(self):
        temp = [(p, (i, p, r, s)) for i, (p, r, s) in enumerate(self.log_entries) if s is not None]
        n_entry_before = len(temp)
        temp = dict(temp) # remove duplicated "p".
        temp = [(p, r, s) for i, p, r, s in sorted(temp.values())] # reorder.
        n_entry_after = len(temp)
        print('Effective entry: {} -> {}'.format(n_entry_before, n_entry_after))
        return temp

    def _number_of_matches(self):
        matches = []
        for p, result, s in self.effective_entries():
            loses, draws, wins = result
            matches.append(wins + draws + loses)
        return matches

    def maximum_matches_per_iteration(self):
        return max(self._number_of_matches())

    def total_number_of_matches(self):
        return sum(self._number_of_matches())

    def n_dim(self):
        return len(self.index2name)

    def numpy_objects(self, normalize=False, expand=False):
        entries = self.effective_entries()
        if not expand:
            # ys: score [0, 1]
            xs = np.array([p for (p, r, s) in entries]) # X[trialindex, paramindex]: parameters.
            ys = np.array([s for (p, r, s) in entries]) # y[trialindex]: score.
        else:
            # ys: 0 (win) or -1 (lose)
            xs = []
            ys = []
            for p, r, s in entries:
                loses, draws, wins = r
                for i in range(wins):
                    xs.append(p)
                    ys.append(0.0)
                for i in range(loses):
                    xs.append(p)
                    ys.append(1.0)
        if normalize:
            xs = np.normalize_xs(xs)
        return xs, ys

    def dataframe(self, normalize=False, expand=False):
        xs, ys = self.numpy_objects(normalize, expand)
        df = pd.DataFrame(
                np.hstack([np.atleast_2d(ys).T, xs]),
                columns=['score'] + ['param{}'.format(i) for i in range(self.n_dim())]
                )
        return df

    def normalize_xs(self, xs):
        xs = np.array(xs)
        if False:
            # use bounds
            mins = []
            maxes = []
            # res := (xs - min) / (max - min)
            for i, (default_value, min_value, max_value) in enumerate(self.index2bounds):
                mins.append(min_value)
                maxes.append(max_value)
            self.mins = np.atleast_2d(mins)
            self.maxes = np.atleast_2d(maxes)
        else:
            # use empirical values
            self.mins = xs.min(axis=0)
            self.maxes = xs.max(axis=0)
        return (xs - self.mins) / (self.maxes - self.mins + 1e-8)

    def unnormalize_xs(self, xs_norm):
        return xs_norm * (self.maxes - self.mins) + self.mins

    def summarize(self):
        print('Now: {}'.format(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')))
        if self.log_path:
            print('Log file path: {}'.format(self.log_path))
        print('Total iterations: {}'.format(len(self.all_entries())))
        print('Total effective iterations: {}'.format(len(self.effective_entries())))
        print('Maximum matches per iteration: {}'.format(self.maximum_matches_per_iteration()))
        print('Total matches: {}'.format(self.total_number_of_matches()))
        if len(self.effective_entries()) > 0:
            params, result, score = self.effective_entries()[0]
            print('Parameters in log: {}'.format(len(params)))
        if self.parameters_csv_path:
            print('Parameter file path: {}'.format(self.parameters_csv_path))
            print('Parameters in CSV: {}'.format(len(self.index2name)))
        if self.parameters_csv_path:
            xs, ys = self.numpy_objects()
            for i in range(len(self.index2name)):
                default_value, min_value, max_value = self.index2bounds[i]
                empirical_min_value = xs[:, i].min()
                empirical_max_value = xs[:, i].max()
                print('Param #{:3}: {} (min={}, max={}, default={}) empirical statistics: (min={}, max={})'.format(
                    i, self.index2name[i],
                    min_value, max_value, default_value,
                    empirical_min_value, empirical_max_value))
                if min_value <= empirical_min_value and empirical_max_value <= max_value:
                    pass
                else:
                    print(' !! out of range !! confirm parameters csv file is reordered to optimization log file.')

        #print('log:')
        #for i, (p, r, s) in enumerate(self.effective_entries()):
        #    print('{:3}: {} ({})'.format(i, s, r))

    def save_log_as_csv(self, file_path):
        with open(file_path, 'wb') as fo:
            writer = csv.writer(fo)
            dim = len(self.index2name)
            writer.writerow(['index'] + ['param{}'.format(i) for i in range(dim)] + ['score'])
            for i, (p, r, s) in enumerate(self.effective_entries()):
                writer.writerow([i] + list(p) + [s])

# --------------------------------------------------------------------------------
# calculate parameters from log analyzer ...
# --------------------------------------------------------------------------------

def analyze_correlation_20160315(analyzer):
    print('-'*80)
    print('Calculate per-parameter correlation to the score')
    xs, ys = analyzer.numpy_objects(expand=False)
    xs_norm = analyzer.normalize_xs(xs)

    # per-parameter correlation
    index2correlation = {}
    for i in range(xs_norm.shape[1]):
        R = np.corrcoef(xs_norm[:, i], ys)[0, 1]
        index2correlation[i] = R

    # descending order of absolute R.
    index2correlation_sorted = sorted(index2correlation.items(), key=lambda(i,R): -np.abs(R))

    for i, R in index2correlation_sorted:
        print('Param #{:3d} : R = {:.10f} ({})'.format(i, R, analyzer.index2name[i]))

    if False:
        # plot correlations (since socre values do not have sufficient resolution, visualization tells less)
        nrow, ncol = 6, 8
        fig, axs = plt.subplots(nrow, ncol)
        for irow in range(nrow):
            for icol in range(ncol):
                idx = irow*ncol + icol
                if 0 <= idx < xs_norm.shape[1]:
                    n = xs_norm.shape[0]
                    axs[irow][icol].plot(xs_norm[:, idx] + np.random.randn(n)*0.05, ys + np.random.randn(n)*0.05, '+', color='blue', alpha=0.1)
                    axs[irow][icol].plot([0, 1], [0, 1], '-', color='gray', alpha=0.5)
                    axs[irow][icol].set_aspect(1.0)
                    axs[irow][icol].set_xlim(-0.1, 1.1)
                    axs[irow][icol].set_ylim(-0.1, 1.1)
                    axs[irow][icol].set_title('#{} R={:.7f}\n{}'.format(idx, index2correlation[idx], analyzer.index2name[idx][:20]), fontsize=8)
        plt.show()

def calc_optimal_parameters_20160315(analyzer, n_iterations_to_use=-1):
    print('-'*80)
    print('Calculate optimal parameters with Gaussian Process')
    print('using {} iterations (-1: all)'.format(n_iterations_to_use))

    xs, ys = analyzer.numpy_objects()
    effective_entries = analyzer.effective_entries()
    xs = xs[:n_iterations_to_use]
    ys = ys[:n_iterations_to_use]
    effective_entries = effective_entries[:n_iterations_to_use]
    xs_norm = analyzer.normalize_xs(xs)
    ys = transform_score_to_target(ys)
    n_matches = analyzer.maximum_matches_per_iteration()

    # feature selection: use all features.
    n_dim = xs_norm.shape[1]
    xs_norm = xs_norm[:, :n_dim]

    # fit a model.
    regr_method = 'GP'
    #regr_method = 'KernelRidge'

    if regr_method == 'GP':
        seed = np.random.RandomState(1)
        regr = sklearn.gaussian_process.GaussianProcess(
                regr='constant', corr='absolute_exponential',
                verbose=True,
                optimizer='Welch',
                random_state=seed,
                random_start=4,
                nugget=2.0,
                #nugget=(0.25/(1.0e-3 + ys))**2,
                theta0=1e-1, thetaL=1e-3, thetaU=1e3)
        regr.fit(xs_norm, ys)

    elif regr_method == 'KernelRidge':
        regr = sklearn.kernel_ridge.KernelRidge(
                alpha=1.0e1,
                kernel='rbf')
        regr.fit(xs_norm, ys)

    if True:
        print('Data mean score: {}'.format(ys.mean()))
        for i in range(10):
            print('Score at random point: {}'.format(regr.predict(np.atleast_2d(np.random.randn(n_dim)))))

    def func(x_norm):
        y = float(regr.predict(np.atleast_2d(x_norm)))
        #print x_norm, y
        return y

    bounds = [(0.0, 1.0)] * xs_norm.shape[1]
    minima_method = 'Data'
    #minima_method = 'DescendOnData'
    if minima_method == 'Raw':
        # just return the best point in the input data.
        assert xs_norm.shape[0] == len(effective_entries)
        best_idx = np.argmin([s for _, _, s in effective_entries])
        x_opt_norm = xs_norm[best_idx]
        _, result, score = effective_entries[best_idx]
        print('Naive minimum .. found index: {} (was result: {}, score: {})'.format(
            best_idx, result, score))

    elif minima_method == 'Data':
        # find the best scoring point in the input data.
        assert xs_norm.shape[0] == len(effective_entries)
        predicts = regr.predict(xs_norm)
        best_idx = np.argmin(predicts)
        x_opt_norm = xs_norm[best_idx]
        _, result, score = effective_entries[best_idx]
        print('Exhausitive search on data points.. found index: {} (was result: {}, score: {}) -> {}'.format(
            best_idx, result, score, predicts[best_idx]))

    elif minima_method == 'DescendOnData':
        # find the best scoring point in the input data and then apply local optimization on the point.
        assert xs_norm.shape[0] == len(effective_entries)
        best_idxs = []
        best_score = 1e9
        for i in range(xs_norm.shape[0]):
            _, result, score = effective_entries[i]
            if score < best_score:
                best_score = score
        for i in range(xs_norm.shape[0]):
            _, result, score = effective_entries[i]
            if score <= best_score:
                best_idxs.append(i)
        best_score = 1e9
        for i,best_idx in enumerate(best_idxs):
            x_test_norm = xs_norm[best_idx]
            _, result, score = effective_entries[best_idx]
            x_test_norm, f, d = scipy.optimize.fmin_l_bfgs_b(func, x0=xs_norm[best_idx], bounds=bounds, approx_grad=True)
            print('Local optimization search on data points.. index: {} ({}/{}) (was result: {}, score: {}) -> {}'.format(
                best_idx, i, len(best_idxs), result, score, f))
            if f < best_score:
                best_score = f
                x_opt_norm = x_test_norm

    else:
        # since no analytical minimizer to the posterior is possible,
        # optimize again for the minimizer of the posterior model.
        print('Optimization on posterior function..')
        opt_n_iter = 5
        seed = np.random.RandomState(1)
        x0_norm = np.random.rand(xs_norm.shape[1])
        #x_opt_norm = scipy.optimize.differential_evolution(func, bounds=bounds, maxiter=opt_n_iter, seed=seed, disp=True).x
        x_opt_norm = scipy.optimize.basinhopping(func, x0_norm, minimizer_kwargs=dict(method='L-BFGS-B'), niter=opt_n_iter, disp=True).x

    x_opt = analyzer.unnormalize_xs(x_opt_norm)


    # print the parameters.
    print('Best parameters: (+: default, @: best)')
    for index, (name, (default_value, min_value, max_value), best_value) in enumerate(zip(analyzer.index2name, analyzer.index2bounds, x_opt)):
        rounded_value = int(round(best_value))
        print('{:70} = {:7} {} (raw={}, min={}, max={} default={})'.format(
            name, rounded_value,
            stringify_value_in_bounds(rounded_value, default_value, min_value, max_value),
            best_value, min_value, max_value, default_value))

    score = func(x_opt_norm)
    score = transform_target_to_score(score)
    print('Estimated score: {}, winning rate: {}%'.format(
        score,
        100.0*winning_rate_from_score(score, n_matches)))

    config_dict = dict(
            minima_method=minima_method,
            )
    return x_opt, config_dict


def main():
    # SETUP
    if len(sys.argv) < 3:
        print('usage: analyze_optimizer_log.py parameters_reordered.csv parameters-xxxx.pickle')
        return
    log_file = sys.argv[2]
    param_file = sys.argv[1]

    analyzer = LogAnalyzer(log_file, param_file)
    analyzer.summarize()
    #analyzer.save_log_as_csv('log.csv')

    # ANALYZE
    #analyze_correlation_20160315(analyzer)
    n_iterations_to_use = 1000
    x_opt = None
    x_opt, config_dict = calc_optimal_parameters_20160315(analyzer, n_iterations_to_use=n_iterations_to_use)

    # SAVE
    config_dict.update(
            n_iterations_to_use=n_iterations_to_use,
            )
    if x_opt is not None:
        create_header_file('parameters_generated.hpp', analyzer, x_opt, config_dict)


if __name__=='__main__':
    main()

