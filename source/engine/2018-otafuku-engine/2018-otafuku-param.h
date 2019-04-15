#ifndef __PARAMETERS_GENERATED_HPP__
#define __PARAMETERS_GENERATED_HPP__
// Created at: 2019-04-15 11:02:11
// Log: optimize_parameters.hyperopt_state.20190313_173749.pickle
// Parameters CSV: param/2018-otafuku-param.original.h
// minima_method = Data
// n_iterations_to_use = 1000

// |--------@---------| raw=169.999999995, min=100, max=240 default=172
PARAM_DEFINE PARAM_FUTILITY_MARGIN_ALPHA1 = 170;

// |-----+-@----------| raw=57.9999999956, min=25, max=100 default=50
PARAM_DEFINE PARAM_FUTILITY_MARGIN_ALPHA2 = 58;

// |--------@--+------| raw=165.9999999952857, min=100, max=240 default=195
PARAM_DEFINE PARAM_FUTILITY_MARGIN_BETA = 166;

// |--------------@---| raw=146.99999999118182, min=50, max=160 default=145
PARAM_DEFINE PARAM_FUTILITY_MARGIN_QUIET = 147;

// |------@-----------| raw=8.999999996, min=5, max=15 default=9
PARAM_DEFINE PARAM_FUTILITY_RETURN_DEPTH = 9;

// |--@----+----------| raw=6.999999998666667, min=5, max=20 default=12
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_DEPTH = 7;

// |----@--------+----| raw=151.99999999738694, min=100, max=300 default=256
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_MARGIN1 = 152;

// |---@----------+---| raw=56.999999998099995, min=0, max=300 default=248
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_MARGIN2 = 57;

// |-----------@------| raw=40.999999993, min=20, max=50 default=40
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_GAMMA1 = 41;

// |--------@----+----| raw=40.99999999475, min=20, max=60 default=51
PARAM_DEFINE PARAM_FUTILITY_AT_PARENT_NODE_GAMMA2 = 41;

// |--@--+------------| raw=663.99999999836, min=500, max=1500 default=818
PARAM_DEFINE PARAM_NULL_MOVE_DYNAMIC_ALPHA = 664;

// |-@---+------------| raw=54.999999998999996, min=50, max=100 default=67
PARAM_DEFINE PARAM_NULL_MOVE_DYNAMIC_BETA = 55;

// |-------+---@------| raw=44.999999992999996, min=10, max=60 default=31
PARAM_DEFINE PARAM_NULL_MOVE_MARGIN = 45;

// |--@-----------+---| raw=5.999999998333333, min=4, max=16 default=14
PARAM_DEFINE PARAM_NULL_MOVE_RETURN_DEPTH = 6;

// |--@-+-------------| raw=3.9999999985714285, min=3, max=10 default=5
PARAM_DEFINE PARAM_PROBCUT_DEPTH = 4;

// |--------+-@-------| raw=218.9999999940909, min=100, max=300 default=202
PARAM_DEFINE PARAM_PROBCUT_MARGIN1 = 219;

// |@+----------------| raw=21.999999999666667, min=20, max=80 default=24
PARAM_DEFINE PARAM_PROBCUT_MARGIN2 = 22;

// |-----+-@----------| raw=7.999999995555555, min=4, max=13 default=7
PARAM_DEFINE PARAM_SINGULAR_EXTENSION_DEPTH = 8;

// |----+------@------| raw=303.9999999935294, min=128, max=400 default=194
PARAM_DEFINE PARAM_SINGULAR_MARGIN = 304;

// |--------+@--------| raw=20.999999994583334, min=8, max=32 default=20
PARAM_DEFINE PARAM_SINGULAR_SEARCH_DEPTH_ALPHA = 21;

// |-@---+------------| raw=9.999999999166667, min=8, max=32 default=16
PARAM_DEFINE PARAM_PRUNING_BY_MOVE_COUNT_DEPTH = 10;

// |--@+--------------| raw=6.999999998333333, min=2, max=32 default=9
PARAM_DEFINE PARAM_PRUNING_BY_HISTORY_DEPTH = 7;

// |-----+-@----------| raw=4523.999999995796, min=2000, max=8000 default=4000
PARAM_DEFINE PARAM_REDUCTION_BY_HISTORY = 4524;

// |@---------+-------| raw=406.99999999976666, min=400, max=700 default=590
PARAM_DEFINE PARAM_RAZORING_MARGIN2 = 407;

// |------@----+------| raw=515.9999999961952, min=400, max=700 default=604
PARAM_DEFINE PARAM_RAZORING_MARGIN3 = 516;

// |----@-+-----------| raw=117.99999999717278, min=64, max=256 default=135
PARAM_DEFINE PARAM_REDUCTION_ALPHA = 118;

// |------+@----------| raw=261.999999995502, min=150, max=400 default=240
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_ALPHA0 = 262;

// |---------@+-------| raw=459.99999999466667, min=300, max=600 default=492
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_ALPHA1 = 460;

// |--+----@----------| raw=1141.9999999957172, min=500, max=2000 default=740
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_BETA0 = 1142;

// |-----+@-----------| raw=1047.9999999963593, min=500, max=2000 default=1000
PARAM_DEFINE PARAM_FUTILITY_MOVE_COUNT_BETA1 = 1048;

// |-----+----@-------| raw=90.99999999385417, min=32, max=128 default=64
PARAM_DEFINE PARAM_QUIET_SEARCH_COUNT = 91;

// |--+@--------------| raw=17.99999999785714, min=12, max=40 default=16
PARAM_DEFINE PARAM_ASPIRATION_SEARCH_DELTA = 18;

// |----+---@---------| raw=30.99999999475, min=10, max=50 default=20
PARAM_DEFINE PARAM_EVAL_TEMPO = 31;

// |----@----+--------| raw=8.9999999971875, min=0, max=32 default=17
PARAM_DEFINE STAT_BONUS_DEPTH = 9;

// |--------+-----@---| raw=53.999999991562504, min=0, max=64 default=32
PARAM_DEFINE STAT_BONUS_COEFFICIENT2 = 54;

// |--------+--@------| raw=85.99999999328125, min=0, max=128 default=64
PARAM_DEFINE STAT_BONUS_COEFFICIENT1 = 86;

// |--------+--@------| raw=86.99999999320312, min=0, max=128 default=64
PARAM_DEFINE STAT_BONUS_COEFFICIENT0 = 87;


#endif // __PARAMETERS_GENERATED_HPP__
