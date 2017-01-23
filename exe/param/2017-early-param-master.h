#ifndef __PARAMETERS_GENERATED_HPP__
#define __PARAMETERS_GENERATED_HPP__
// Created at: 2017-01-24 08:25:55
// Log: optimize_parameters.hyperopt_state.20170123_102223.pickle
// Parameters CSV: param\parameters.csv
// n_iterations_to_use = 1000
// minima_method = Data

// |@-----+-----------| raw=62.9999999965, min=100, max=240 default=150
PARAM_DEFINE PARAM_FUTILITY_MARGIN_ALPHA_OPENING = 63;

// |------@-----------| raw=150.0, min=100, max=240 default=150
PARAM_DEFINE PARAM_FUTILITY_MARGIN_ALPHA_ENDING = 150;

// |-------@----+-----| raw=163.999999996, min=100, max=240 default=200
PARAM_DEFINE PARAM_FUTILITY_MARGIN_BETA_OPENING = 164;

// |------------@-----| raw=200.0, min=100, max=240 default=200
PARAM_DEFINE PARAM_FUTILITY_MARGIN_BETA_ENDING = 200;

// |------------+----@| raw=206.999999992, min=50, max=160 default=128
PARAM_DEFINE PARAM_FUTILITY_MARGIN_QUIET_OPENING = 207;

// |------------@-----| raw=128.0, min=50, max=160 default=128
PARAM_DEFINE PARAM_FUTILITY_MARGIN_QUIET_ENDING = 128;

// |----@-------------| raw=6.999999995, min=5, max=13 default=7
PARAM_DEFINE PARAM_FUTILITY_RETURN_DEPTH_OPENING = 7;

// |----@-------------| raw=7.0, min=5, max=13 default=7
PARAM_DEFINE PARAM_FUTILITY_RETURN_DEPTH_ENDING = 7;

// |----+-----@-------| raw=9.999999994, min=5, max=13 default=7
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_DEPTH_OPENING = 10;

// |----@-------------| raw=7.0, min=5, max=13 default=7
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_DEPTH_ENDING = 7;

// |-------------+---@| raw=343.999999996, min=100, max=300 default=256
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1_OPENING = 344;

// |-------------@----| raw=256.0, min=100, max=300 default=256
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1_ENDING = 256;

// |----------@+------| raw=179.999999996, min=0, max=300 default=200
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2_OPENING = 180;

// |-----------@------| raw=200.0, min=0, max=300 default=200
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2_ENDING = 200;

// |--------+@--------| raw=36.9999999929, min=20, max=50 default=35
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1_OPENING = 37;

// |--------@---------| raw=35.0, min=20, max=50 default=35
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1_ENDING = 35;

// |------+@----------| raw=37.9999999934, min=20, max=60 default=35
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2_OPENING = 38;

// |------@-----------| raw=35.0, min=20, max=60 default=35
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2_ENDING = 35;

// |----@+------------| raw=772.999999996, min=500, max=1500 default=823
PARAM_DEFINE PARAM_NULL_MOVE_DYNAMIC_ALPHA_OPENING = 773;

// |-----@------------| raw=823.0, min=500, max=1500 default=823
PARAM_DEFINE PARAM_NULL_MOVE_DYNAMIC_ALPHA_ENDING = 823;

// |---@-+------------| raw=59.9999999955, min=50, max=100 default=67
PARAM_DEFINE PARAM_NULL_MOVE_DYNAMIC_BETA_OPENING = 60;

// |-----@------------| raw=67.0, min=50, max=100 default=67
PARAM_DEFINE PARAM_NULL_MOVE_DYNAMIC_BETA_ENDING = 67;

// |--------@---------| raw=34.9999999954, min=10, max=60 default=35
PARAM_DEFINE PARAM_NULL_MOVE_MARGIN_OPENING = 35;

// |--------@---------| raw=35.0, min=10, max=60 default=35
PARAM_DEFINE PARAM_NULL_MOVE_MARGIN_ENDING = 35;

// |------------+@----| raw=12.9999999944, min=4, max=15 default=12
PARAM_DEFINE PARAM_NULL_MOVE_RETURN_DEPTH_OPENING = 13;

// |------------@-----| raw=12.0, min=4, max=15 default=12
PARAM_DEFINE PARAM_NULL_MOVE_RETURN_DEPTH_ENDING = 12;

// |----+--@----------| raw=5.99999999375, min=3, max=10 default=5
PARAM_DEFINE PARAM_PROBCUT_DEPTH_OPENING = 6;

// |----@-------------| raw=5.0, min=3, max=10 default=5
PARAM_DEFINE PARAM_PROBCUT_DEPTH_ENDING = 5;

// |--------@---------| raw=196.999999995, min=100, max=300 default=200
PARAM_DEFINE PARAM_PROBCUT_MARGIN_OPENING = 197;

// |--------@---------| raw=200.0, min=100, max=300 default=200
PARAM_DEFINE PARAM_PROBCUT_MARGIN_ENDING = 200;

// |-----@-+----------| raw=6.99999999444, min=4, max=13 default=8
PARAM_DEFINE PARAM_SINGULAR_EXTENSION_DEPTH_OPENING = 7;

// |-------@----------| raw=8.0, min=4, max=13 default=8
PARAM_DEFINE PARAM_SINGULAR_EXTENSION_DEPTH_ENDING = 8;

// |--------+--------@| raw=460.999999994, min=128, max=400 default=256
PARAM_DEFINE PARAM_SINGULAR_MARGIN_OPENING = 461;

// |--------@---------| raw=256.0, min=128, max=400 default=256
PARAM_DEFINE PARAM_SINGULAR_MARGIN_ENDING = 256;

// |-----+-@----------| raw=18.9999999932, min=8, max=32 default=16
PARAM_DEFINE PARAM_SINGULAR_SEARCH_DEPTH_ALPHA_OPENING = 19;

// |-----@------------| raw=16.0, min=8, max=32 default=16
PARAM_DEFINE PARAM_SINGULAR_SEARCH_DEPTH_ALPHA_ENDING = 16;

// |-----@------------| raw=15.9999999955, min=8, max=32 default=16
PARAM_DEFINE PARAM_PRUNING_BY_MOVE_COUNT_DEPTH_OPENING = 16;

// |-----@------------| raw=16.0, min=8, max=32 default=16
PARAM_DEFINE PARAM_PRUNING_BY_MOVE_COUNT_DEPTH_ENDING = 16;

// |+-@---------------| raw=5.99999999385, min=2, max=32 default=3
PARAM_DEFINE PARAM_PRUNING_BY_HISTORY_DEPTH_OPENING = 6;

// |@-----------------| raw=3.0, min=2, max=32 default=3
PARAM_DEFINE PARAM_PRUNING_BY_HISTORY_DEPTH_ENDING = 3;

// |------+-@---------| raw=9283.99999999, min=4000, max=15000 default=8000
PARAM_DEFINE PARAM_REDUCTION_BY_HISTORY_OPENING = 9284;

// |------@-----------| raw=8000.0, min=4000, max=15000 default=8000
PARAM_DEFINE PARAM_REDUCTION_BY_HISTORY_ENDING = 8000;

// |------@-+---------| raw=231.999999996, min=128, max=384 default=256
PARAM_DEFINE PARAM_IID_MARGIN_ALPHA_OPENING = 232;

// |--------@---------| raw=256.0, min=128, max=384 default=256
PARAM_DEFINE PARAM_IID_MARGIN_ALPHA_ENDING = 256;

// |--@-+-------------| raw=447.999999995, min=400, max=700 default=483
PARAM_DEFINE PARAM_RAZORING_MARGIN1_OPENING = 448;

// |----@-------------| raw=483.0, min=400, max=700 default=483
PARAM_DEFINE PARAM_RAZORING_MARGIN1_ENDING = 483;

// |---------+--@-----| raw=619.999999994, min=400, max=700 default=570
PARAM_DEFINE PARAM_RAZORING_MARGIN2_OPENING = 620;

// |---------@--------| raw=570.0, min=400, max=700 default=570
PARAM_DEFINE PARAM_RAZORING_MARGIN2_ENDING = 570;

// |---------@-+------| raw=575.999999996, min=400, max=700 default=603
PARAM_DEFINE PARAM_RAZORING_MARGIN3_OPENING = 576;

// |-----------@------| raw=603.0, min=400, max=700 default=603
PARAM_DEFINE PARAM_RAZORING_MARGIN3_ENDING = 603;

// |--------+-@-------| raw=593.999999993, min=400, max=700 default=554
PARAM_DEFINE PARAM_RAZORING_MARGIN4_OPENING = 594;

// |--------@---------| raw=554.0, min=400, max=700 default=554
PARAM_DEFINE PARAM_RAZORING_MARGIN4_ENDING = 554;

// |-----+--@---------| raw=159.999999994, min=64, max=256 default=128
PARAM_DEFINE PARAM_REDUCTION_ALPHA_OPENING = 160;

// |-----@------------| raw=128.0, min=64, max=256 default=128
PARAM_DEFINE PARAM_REDUCTION_ALPHA_ENDING = 128;

// |------+-@---------| raw=274.999999994, min=150, max=400 default=240
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_ALPHA0_OPENING = 275;

// |------@-----------| raw=240.0, min=150, max=400 default=240
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_ALPHA0_ENDING = 240;

// |---------+---@----| raw=344.999999993, min=150, max=400 default=290
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_ALPHA1_OPENING = 345;

// |---------@--------| raw=290.0, min=150, max=400 default=290
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_ALPHA1_ENDING = 290;

// |-@-+--------------| raw=625.999999997, min=500, max=2000 default=773
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_BETA0_OPENING = 626;

// |---@--------------| raw=773.0, min=500, max=2000 default=773
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_BETA0_ENDING = 773;

// |-----@+-----------| raw=1003.0, min=500, max=2000 default=1045
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_BETA1_OPENING = 1003;

// |------@-----------| raw=1045.0, min=500, max=2000 default=1045
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_BETA1_ENDING = 1045;

// |-----+@-----------| raw=65.9999999947, min=32, max=128 default=64
PARAM_DEFINE PARAM_QUIET_SEARCH_COUNT_OPENING = 66;

// |-----@------------| raw=64.0, min=32, max=128 default=64
PARAM_DEFINE PARAM_QUIET_SEARCH_COUNT_ENDING = 64;

// |-----------------@| raw=0.999999995, min=0, max=1 default=1
PARAM_DEFINE PARAM_QSEARCH_MATE1_OPENING = 1;

// |-----------------@| raw=1.0, min=0, max=1 default=1
PARAM_DEFINE PARAM_QSEARCH_MATE1_ENDING = 1;

// |-----------------@| raw=0.999999995, min=0, max=1 default=1
PARAM_DEFINE PARAM_SEARCH_MATE1_OPENING = 1;

// |-----------------@| raw=1.0, min=0, max=1 default=1
PARAM_DEFINE PARAM_SEARCH_MATE1_ENDING = 1;

// |@-----------------| raw=-2.00000000167, min=1, max=5 default=1
PARAM_DEFINE PARAM_WEAK_MATE_PLY_OPENING = -2;

// |@-----------------| raw=1.0, min=1, max=5 default=1
PARAM_DEFINE PARAM_WEAK_MATE_PLY_ENDING = 1;


#endif // __PARAMETERS_GENERATED_HPP__
