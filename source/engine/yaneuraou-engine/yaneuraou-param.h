#ifndef __PARAMETERS_GENERATED_HPP__
#define __PARAMETERS_GENERATED_HPP__
// Created at: 2023-05-27 09:35:49
// Log: D:\hnoda\hakubishin-private\source\optimize_parameters.hyperopt_state.20230524_001642.pickle
// Parameters CSV: D:\hnoda\hakubishin-private\source\engine\yaneuraou-engine\yaneuraou-param.h
// minima_method = Data
// n_iterations_to_use = 1000

// |-----@------------| raw=163.9999999968, min=100, max=300 default=168
PARAM_DEFINE PARAM_FUTILITY_MARGIN_ALPHA1 = 164;

// |-@--+-------------| raw=108.99999999935714, min=100, max=240 default=138
PARAM_DEFINE PARAM_FUTILITY_MARGIN_BETA = 109;

// |-------@----------| raw=115.99999999559999, min=50, max=200 default=118
PARAM_DEFINE PARAM_FUTILITY_MARGIN_QUIET = 116;

// |-@---+------------| raw=5.999999999, min=5, max=15 default=8
PARAM_DEFINE PARAM_FUTILITY_RETURN_DEPTH = 6;

// |------+------@----| raw=16.999999992, min=5, max=20 default=11
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_DEPTH = 17;

// |-+----@-----------| raw=221.9999999959333, min=100, max=400 default=122
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1 = 222;

// |----+----------@--| raw=45.99999999114286, min=15, max=50 default=25
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1 = 46;

// |--------@---------| raw=19.999999995, min=10, max=30 default=20
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2 = 20;

// |-----------+--@---| raw=260.9999999913, min=0, max=300 default=203
PARAM_DEFINE PARAM_LMR_SEE_MARGIN1 = 261;

// |---@-------------+| raw=236.99999999768326, min=0, max=1024 default=1463
PARAM_DEFINE PARAM_REDUCTION_ALPHA = 237;

// |-------+-@--------| raw=1100.9999999944334, min=600, max=1500 default=1010
PARAM_DEFINE PARAM_REDUCTION_BETA = 1101;

// |--------+-@-------| raw=1131.99999999368, min=500, max=1500 default=1024
PARAM_DEFINE PARAM_NULL_MOVE_DYNAMIC_ALPHA = 1132;

// |-----@--+---------| raw=73.99999999657143, min=50, max=120 default=85
PARAM_DEFINE PARAM_NULL_MOVE_DYNAMIC_BETA = 74;

// |----+------@------| raw=296.9999999929429, min=50, max=400 default=147
PARAM_DEFINE PARAM_NULL_MOVE_DYNAMIC_GAMMA = 297;

// |----+----@--------| raw=26879.99999999463, min=0, max=50000 default=14695
PARAM_DEFINE PARAM_NULL_MOVE_MARGIN0 = 26880;

// |-+--@-------------| raw=23.9999999972, min=10, max=60 default=15
PARAM_DEFINE PARAM_NULL_MOVE_MARGIN1 = 24;

// |----+----@--------| raw=23.99999999457143, min=5, max=40 default=15
PARAM_DEFINE PARAM_NULL_MOVE_MARGIN3 = 24;

// |-@------+---------| raw=25.99999999935, min=0, max=400 default=198
PARAM_DEFINE PARAM_NULL_MOVE_MARGIN4 = 26;

// |--------@---+-----| raw=9.999999995, min=4, max=16 default=13
PARAM_DEFINE PARAM_NULL_MOVE_RETURN_DEPTH = 10;

// |--+-@-------------| raw=4.999999997142857, min=3, max=10 default=4
PARAM_DEFINE PARAM_PROBCUT_DEPTH = 5;

// |------+--------@--| raw=285.99999999069996, min=100, max=300 default=179
PARAM_DEFINE PARAM_PROBCUT_MARGIN1 = 286;

// |-------+-------@--| raw=73.99999999100001, min=20, max=80 default=46
PARAM_DEFINE PARAM_PROBCUT_MARGIN2 = 74;

// |@--+--------------| raw=2.0, min=2, max=13 default=4
PARAM_DEFINE PARAM_SINGULAR_EXTENSION_DEPTH = 2;

// |-@----------+-----| raw=78.99999999922852, min=0, max=1024 default=768
PARAM_DEFINE PARAM_SINGULAR_MARGIN = 79;

// |---+-------------@| raw=15.99999999, min=2, max=16 default=5
PARAM_DEFINE PARAM_PRUNING_BY_HISTORY_DEPTH = 16;

// |------+@----------| raw=4661.999999995566, min=2000, max=8000 default=4334
PARAM_DEFINE PARAM_REDUCTION_BY_HISTORY = 4662;

// |+----------------@| raw=0.9999999900000002, min=0, max=1 default=0
PARAM_DEFINE PARAM_QSEARCH_FORCE_EVAL = 1;

// |--+------------@--| raw=36.999999991071434, min=12, max=40 default=16
PARAM_DEFINE PARAM_ASPIRATION_SEARCH_DELTA = 37;

// 元の値 = 22 , step = 2
// [PARAM] min:10,max:60,step:2,interval:1,time_rate:1,fixed
PARAM_DEFINE PARAM_NULL_MOVE_MARGIN2 = 22;

// 静止探索での1手詰め
// 元の値 = 1
// →　1スレ2秒で対技巧だと有りのほうが強かったので固定しておく。
// NNUEだと、これ無しのほうが良い可能性がある。
// いったん無しでやって最後に有りに変更して有効か見る。
// 2スレ1,2秒程度だと無しと有意差がなかったが、4秒～8秒では、有りのほうが+R30ぐらい強い。
// [PARAM] min:0,max:1,step:1,interval:1,time_rate:1,fixed
PARAM_DEFINE PARAM_QSEARCH_MATE1 = 1;

// 通常探索での1手詰め
// →　よくわからないが1スレ2秒で対技巧だと無しのほうが強かった。
//     1スレ3秒にすると有りのほうが強かった。やはり有りのほうが良いのでは..
// 元の値 = 1
// [PARAM] min:0,max:1,step:1,interval:1,time_rate:1,fixed
PARAM_DEFINE PARAM_SEARCH_MATE1 = 1;

// 1手詰めではなくN手詰めを用いる
// ※　3手,5手はコストに見合わないようだ。
// 元の値 = 1
// [PARAM] min:1,max:5,step:2,interval:1,time_rate:1,fixed
PARAM_DEFINE PARAM_WEAK_MATE_PLY = 1;

// 元の値 = 32 , step = 8
// これだけ " 2 * " と係数が掛かっているので倍にして考える必要がある。
// 32は大きすぎるのではないかと…。
// [PARAM] min:10,max:50,step:4,interval:1,time_rate:1,fixed
PARAM_DEFINE MOVE_PICKER_Q_PARAM1 = 32;

// 元の値 = 32 , step = 8
// [PARAM] min:10,max:50,step:8,interval:1,time_rate:1,fixed
PARAM_DEFINE MOVE_PICKER_Q_PARAM2 = 32;

// 元の値 = 32 , step = 8
// [PARAM] min:10,max:50,step:4,interval:1,time_rate:1,fixed
PARAM_DEFINE MOVE_PICKER_Q_PARAM3 = 32;

// 元の値 = 16 , step = 4
// [PARAM] min:10,max:50,step:4,interval:1,time_rate:1,fixed
PARAM_DEFINE MOVE_PICKER_Q_PARAM4 = 16;

// 元の値 = 16 , step = 4
// [PARAM] min:10,max:50,step:2,interval:1,time_rate:1,fixed
PARAM_DEFINE MOVE_PICKER_Q_PARAM5 = 16;


// ABテスト用
// [PARAM] min:0,max:1,step:1,interval:1,time_rate:1,fixed
PARAM_DEFINE AB_TEST1 = 1;

// ABテスト用
// [PARAM] min:0,max:1,step:1,interval:1,time_rate:1,fixed
PARAM_DEFINE AB_TEST2 = 1;

#endif // __PARAMETERS_GENERATED_HPP__
