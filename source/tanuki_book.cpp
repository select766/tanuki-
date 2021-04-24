#include "tanuki_book.h"
#include "config.h"

#ifdef EVAL_LEARN

#include <atomic>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <queue>
#include <random>
#include <set>
#include <sstream>

#include <omp.h>

#include "sqlite/sqlite3.h"

#include "book/book.h"
#include "csa.h"
#include "evaluate.h"
#include "learn/learn.h"
#include "misc.h"
#include "position.h"
#include "tanuki_progress_report.h"
#include "thread.h"
#include "tt.h"

using Book::BookMoveSelector;
using Book::BookMove;
using Book::MemoryBook;
using USI::Option;

namespace {
	constexpr const char* kBookSfenFile = "BookSfenFile";
	constexpr const char* kBookMaxMoves = "BookMaxMoves";
	constexpr const char* kBookFile = "BookFile";
	constexpr const char* kBookSearchDepth = "BookSearchDepth";
	constexpr const char* kBookSearchNodes = "BookSearchNodes";
	constexpr const char* kBookInputFile = "BookInputFile";
	constexpr const char* kBookInputFolder = "BookInputFolder";
	constexpr const char* kBookOutputFile = "BookOutputFile";
	constexpr const char* kThreads = "Threads";
	constexpr const char* kMultiPV = "MultiPV";
	constexpr const char* kBookOverwriteExistingPositions = "OverwriteExistingPositions";
	constexpr const char* kBookNarrowBook = "NarrowBook";
	constexpr const char* kBookTargetSfensFile = "BookTargetSfensFile";
	constexpr const char* kBookCsaFolder = "BookCsaFolder";
	constexpr const char* kBookTanukiColiseumLogFolder = "BookTanukiColiseumLogFolder";
	constexpr const char* kBookMinimumWinningPercentage = "BookMinimumWinningPercentage";
	constexpr const char* kBookBlackMinimumValue = "BookBlackMinimumValue";
	constexpr const char* kBookWhiteMinimumValue = "BookWhiteMinimumValue";
	constexpr int kShowProgressPerAtMostSec = 1 * 60 * 60;	// 1時間
	constexpr time_t kSavePerAtMostSec = 6 * 60 * 60;		// 6時間

	const std::vector<std::string> kStrongPlayers = {
		"Hinatsuru_Ai", // 4607
		"Suisho3test_TR3990X", // 4576
		"BURNING_BRIDGES", // 4556
		"NEEDLED-24.7", // 4535
		"Suisho4_TR3990X", // 4530
		"NEEDLED-35.8kai0151_TR-3990X", // 4476
		"FROZEN_BRIDGE", // 4465
		"kabuto", // 4454
		"Suisho3kai_TR3990X", // 4453
		"AAV", // 4443
		"gcttest_x6_RTX2080ti", // 4440
		"LUNA", // 4436
		"Marulk", // 4433
		"gct_v100x8_model-0000195", // 4427
		"COMEON", // 4393
		"Yashajin_Ai", // 4390
		"BLUETRANSPARENCY", // 4385
		"Hainaken_Corei9-7980XE_18c", // 4381
		"QueenAI_210222_i9-7920x", // 4377
		"BURNING_BRIDGE_210404z", // 4361
		"xg_kpe9", // 4344
		"ECLIPSE_RyzenTR3990X", // 4344
		"BURNING_BRIDGE_210401", // 4338
		"QueenInLove_test201224", // 4314
		"DG_test210401", // 4302
		"xeon_gold", // 4278
		"xeon_w", // 4278
		"fist", // 4272
		"Karimero", // 4272
		"xgs", // 4267
		"Mariel", // 4266
		"kame", // 4258
		"RINGO", // 4256
		"NEEDLED-35.8kai1050_R7-4800H", // 4251
		"daigo8", // 4250
		"ANESIS", // 4247
		"DG_test210307_24C_ubuntu", // 4246
		"BB_ry4500U", // 4239
		"FukauraOu_RTX3090-2080S", // 4218
		"Frozen", // 4216
		"dlshogi_resnet20_swish_447", // 4211
		"Cendrillon", // 4210
		"Kamuy_s009_HKPE9", // 4209
		"mbk_3340m", // 4208
		"RUN", // 4207
		"ECLIPSE_20210117_i9-9960X", // 4206
		"BB210302_RTX3070", // 4205
		"KASHMIR", // 4203
		"TK045", // 4202
		"mytest0426", // 4201
		"Procyon", // 4199
		"DaigorillaEX_test1_E5_2698v4", // 4189
		"Nashi", // 4186
		"Kristffersen", // 4177
		"DLSuishoFO6.02_RTX3090", // 4176
		"YO600-HalfKPE9-20210124-8t", // 4175
		"yuatan", // 4174
		"uy88", // 4171
		"daigoEX", // 4167
		"koron", // 4159
		"Method", // 4156
		"19W", // 4154
		"Amaeta_Denryu1_2950X", // 4154
		"BB_RTX3070", // 4153
		"dlshogitest", // 4150
		"Lladro", // 4148
		"PLX", // 4147
		"Supercell", // 4143
		"Nao.", // 4142
		"sui3k_ry4500U", // 4140
		"QueenAI_210317_i9-7920x", // 4140
		"ry4500U", // 4138
		"Kristallweizen_4800H", // 4129
		"gcttest_RTX2080ti", // 4126
		"QueenInLove-j088_Ryzen7-4800H", // 4125
		"Tora_Cat", // 4123
		"ECLIPSE_test210205", // 4121
		"test_e-4800H", // 4118
		"Peach", // 4117
		"NEEDLED-24.7kai0447_16t", // 4116
		"Unagi", // 4115
		"Rinne_p003_Ryzen7-4800H", // 4113
		"gcttest_x5_RTX2080ti", // 4112
		"15b_n001_3070", // 4107
		"40b_n008_RTX3070", // 4107
		"Qhapaq_WCSC28_Mizar_4790k", // 4106
		"n3k1177", // 4102
		"GGG", // 4102
		"YKT", // 4101
		"Kristallweizen-E5-2620", // 4099
		"10W", // 4099
		"YO6.00_HKPE9_8t", // 4096
		"PANDRABOX", // 4096
		"Venus_210327", // 4094
		"test20210114", // 4092
		"Suisho3test_i5-10210U", // 4091
		"gcttest_x3_RTX2080ti", // 4090
		"40b_n007_RTX3070", // 4090
		"gcttest_x4_RTX2070_Max-Q", // 4089
		"Rinne_m009", // 4089
		"Suisho3kai_YO6.01_i5-6300U", // 4088
		"Fx13_2", // 4088
		"Suisho2test_i5-10210U", // 4086
		"Sagittarius", // 4083
		"Takeshi2_Ryzen7-4800H", // 4076
		"Venus_210324", // 4074
		"40b_n002", // 4074
		"40b_n003", // 4074
		"ECLIPSE_20210326", // 4073
		"40b_n009_RTX3070", // 4073
		"Rinne_w041", // 4071
		"gcttest_x4_RTX2080ti", // 4070
		"Suisho3-i5-6300U", // 4069
		"Suisho3kai_i5-10210U", // 4069
		"Titanda_L", // 4066
		"Rinne_x022", // 4063
		"40b_n006_RTX3090", // 4063
		"N100k", // 4059
		"ECLIPSE_test_i7-6700HQ", // 4056
		"VIVI006", // 4055
		"d_x15_n001_2080Ti", // 4051
		"BB2021_i5-6300U", // 4051
		"Ultima", // 4046
		"d_x15_n002_2080Ti", // 4046
		"ELLE", // 4046
		"Mike_Cat", // 4044
		"reflect.RTX2070.MAX-Q", // 4037
		"Pacman_4", // 4035
		"Amid", // 4032
		"DG_DGbook_i7_4720HQ", // 4031
		"40-05-70", // 4030
		"DG_test_210202_i7_4720HQ", // 4029
		"Aquarius", // 4027
		"Takeshi_ry4500U", // 4016
		"YaOu_V540_nnue_1227", // 4009
		"sui3k_2c", // 4001
		"cobra_denryu_6c", // 4000
		"slmv100_3c", // 3998
		"DLSuishoFu6.02_RTX3090", // 3998
		"ddp", // 3993
		"FukauraOu_RTX2080ti", // 3992
		"slmv115_2c", // 3989
		"slmv100_2c", // 3985
		"sui3_2c", // 3985
		"melt", // 3985
		"OTUKARE_SAYURI_4C", // 3977
		"Fairy-Stockfish-Suisho3kai-8t", // 3973
		"sui2_2c", // 3972
		"Venus_210406", // 3970
		"Rinne_v055_test", // 3969
		"aqua.RTX2070.MAX-Q", // 3968
		"40-06-70", // 3968
		"MentalistDaiGo", // 3964
		"deg", // 3963
		"slmv85_2c", // 3954
		"dlshogi_blend_gtx1060", // 3954
		"Vx_Cvk_tm1_YO620_4415Y", // 3950
		"gcttest_x5_RTX2070_Max-Q", // 3947
		"usa2xkai", // 3947
		"Kristallweizen-i7-4578U", // 3947
		"slmv145_2c", // 3944
		"40-04-70", // 3939
		"Soho_Amano", // 3938
		"Daigorilla_i7_4720HQ", // 3938
		"BB210214_RTX3070", // 3935
		"slmv130_2c", // 3931
		"Takeshi_2c", // 3930
		"nibanshibori", // 3928
		"DLSuishoFO6.02_GTX1650", // 3928
		"flcl", // 3928
		"YaOu_V540_nnue_1222", // 3921
		"Krist_483_473stb_16t_100m", // 3920
		"goto2200last3", // 3915
		"jky", // 3907
		"nnn_0406", // 3906
		"d_x20_0008", // 3902
		"d_x20_0009", // 3900
		"ilq6_ilqshock201202_tm2_YO_4415Y", // 3899
		"FBK_Fbook", // 3894
		"CBTest01", // 3888
		"az_w2245_n_p100k_UctTreeDeclLose", // 3887
		"YaOu_V600_nnue_0103", // 3883
		"Fairy-Stockfish-Furibisha-8t", // 3879
		"az_w2425_n_s10_DlM1", // 3870
		"JK_FGbook_1c", // 3869
		"MBP2014", // 3863
		"Q006_1t_tn_tn_N800kD14P3_M3kSM90", // 3857
		"Cute_2c", // 3855
		"Cool_2c", // 3849
		"az_w2425_n_s10_DlPM3", // 3848
		"b20_n012_1080Ti", // 3845
		"omiss", // 3842
		"Miacis_2080ti", // 3839
		"QMP006_1_TNtn_N800kD16P3_M3kS90", // 3838
		"d_x20_0007", // 3835
		"Siri", // 3834
		"15b2_2060", // 3832
		"az_w2305_n_s10_Mate1", // 3828
		"YaOu_V600_nnue_0130", // 3824
		"JKishi18gou_m_0814", // 3822
		"siege.RTX2070.MAX-Q", // 3818
		"slmv145", // 3812
		"nnn_210324", // 3807
		"ICHIGO", // 3806
		"aziu_w3303_n_s10_M1", // 3804
		"JKishi18gou_1112", // 3801
		"slmv115", // 3793
		"nnn_210214", // 3793
		"vaio", // 3793
		"slmv100", // 3792
		"BBFtest_RTX3070", // 3792
		"dbga", // 3788
		"Sui3NB_i5-10210U", // 3787
		"YaOu_V600_nnue_0406", // 3784
		"craft_Percy_MT2020_CB_F", // 3782
		"slmv130", // 3780
		"40b_n008_RX580", // 3780
		"Unagi_hkpe9", // 3774
		"Kristallweizen-Core2Duo-P7450", // 3774
		"YaOu_V540_nnue_1211", // 3772
		"fku0", // 3770
		"slmv70", // 3767
		"aziu_w3303_n_s10_PM3", // 3767
		"Sui3_illqhashock_i5-10210U", // 3766
		"slmv85", // 3764
		"AYM8__Suisho3_8000k", // 3762
		"c2d-2c", // 3761
		"YaOu_V533_nnue_1201", // 3760
		"Q_2t_tn_tn_N800kD14P3_MT3k_SM90", // 3759
		"YaOu_V600_nnue_0106", // 3759
		"ViVi005", // 3751
		"dl40b2_2060", // 3749
		"15b_n004_1060", // 3748
		"nnn_210305", // 3746
		"aziu_w2905_n_s10_PM3", // 3740
		"GCT_GTX1660Ti", // 3736
		"15b_n001_1060", // 3734
		"Suisho-1T-D22-YO5.40", // 3732
		"az_w2245_n_p20k_UctTreeDeclLose", // 3729
		"test_g4dn.xlarge", // 3729
		"superusa2x", // 3719
		"ChocoCake", // 3717
		"Kamuy_vi05_1c", // 3709
		"nnn_tt0402", // 3708
		"sankazero0007", // 3706
		"b20_n011_1080Ti", // 3706
		"GCT_RX580", // 3700
		"ForDen_1", // 3698
		"kwe0.4_ym_Cortex-A17_4c", // 3697
		"10be_n006_1080Ti", // 3696
		"10be_n008_1080Ti", // 3695
		"Macbook2010", // 3693
		"GCT_1060", // 3676
		"bbs-nnue-02.1", // 3675
		"AobaZero-1.6-radeonvii-10sec", // 3674
		"emmg_Suisho3_4000k", // 3672
		"10be_n007_1080Ti", // 3671
		"moon", // 3664
		"goto2200last6", // 3664
		"mac_5i_F", // 3663
		"suiseihuman", // 3661
		"10be_n009_1080Ti", // 3661
		"goto2200last5", // 3653
		"nnn_210202", // 3648
		"tx-nnue", // 3647
		"GCT_GTX-1060", // 3646
		"20-10-70", // 3643
		"5b_n007_1080Ti", // 3642
		"craft_xeon_489_XKW_2t", // 3641
		"40b_n005_RX580", // 3636
		"JeSuisMoi", // 3627
		"AobaZero_w2215_RTX2070", // 3627
		"ViVi004", // 3624
		"shotgun_i7-6700", // 3623
		"VIVI003", // 3613
		"5b_n007_1060", // 3609
		"aziu_w2185_n_p20k", // 3605
		"10be_n005_1080Ti", // 3603
		"40b_n007_1050Ti", // 3603
		"nSRU_Suisho3_2000k", // 3600
		"raizen21062004", // 3599
		"pp3", // 3598
		"Suisho-1T-D20-YO5.40", // 3592
		"elmo_WCSC27_479_16t_100m", // 3588
		"AP2_R2500U", // 3583
		"20b_n007_RX580", // 3568
		"nnn_tt0403", // 3555
		"sankazero008", // 3555
		"bbs-nnue-02.0.8", // 3551
		"t0-nnue", // 3548
		"AobaZero-1.9-radeonvii-10sec", // 3543
		"AobaZero_GTX1660Ti", // 3543
		"3be_n005_1050Ti", // 3534
		"AobaZero_w3287_1080Ti", // 3527
		"sz_ym_Cortex-A53_4c", // 3523
		"40b_n005_1050Ti", // 3521
		"nnn_20210113", // 3520
		"Suisho-1T-D18-YO5.40", // 3505
		"5b2_2060", // 3489
		"hYPY_Suisho3_1000k", // 3484
		"emmg_Suisho3_D17", // 3482
		"3be_n005_1060", // 3482
		"VIVi00", // 3477
		"10b_n003_1060", // 3469
		"dts-nnue-0.0", // 3451
		"amant_VII", // 3446
		"TEMPVSFVGIT_4", // 3425
		"afzgnnue005", // 3425
		"SSFv2", // 3410
		"Suika-VAIOtypeG", // 3401
		"Ayame_4t", // 3401
		"b20_n009_1080Ti", // 3371
		"elmo_WCSC27_479_4t_10m", // 3366
		"40b_n006_1050Ti", // 3364
		"AobaZero_w3129_n_RTX3090x2", // 3357
		"SSFv1", // 3324
		"nnn", // 3323
		"model6apu", // 3313
		"gikou2_1c", // 3300
		//"ayame", // 3295
		//"TEMPVSFVGIT_2", // 3294
		//"AobaZero_w3190_kld_ave_p704", // 3284
		//"AobaZero_w2525_n_p800", // 3278
		//"TY-test_i7_0427", // 3275
		//"F_f12", // 3271
		//"AobaZero_w3053_n_p800", // 3261
		//"F_f8", // 3255
		//"Krist_483_473stb_1000k", // 3254
		//"F_f10", // 3252
		//"WHn3_Suisho3_500k", // 3251
		//"AobaZero_w2865_n_p800", // 3245
		//"AobaZero_w2885_n_p800", // 3243
		//"512x", // 3241
		//"AobaZero_w2925_n_p800", // 3235
		//"AobaZero_w3203_n_p800", // 3215
		//"AobaZero_w3263_n_p800", // 3212
		//"AobaZero_w3163_n_p800", // 3205
		//"AobaZero_w2805_n_p800", // 3201
		//"AobaZero_w3083_n_p800", // 3199
		//"AobaZero_w3143_n_p800", // 3190
		//"az_w2305_n_p800_Mate1", // 3189
		//"F_g1", // 3187
		//"AobaZero_w2825_n_p800", // 3186
		//"DLSuisho_FO6.02_MX250", // 3177
		//"AobaZero_w2245_n_p800", // 3177
		//"AobaZero_w3133_n_p800", // 3176
		//"AobaZero_w3183_n_p800", // 3175
		//"AobaZero_w2665_n_p800", // 3171
		//"AobaZero_w2565_n_p800", // 3170
		//"nSRU_Suisho3_D15", // 3167
		//"AobaZero_w3005_n_p800", // 3167
		//"AobaZero_w2785_n_p800", // 3166
		//"TEMPVSFVGIT_3", // 3164
		//"F_f6", // 3161
		//"AobaZero_w2945_n_p800", // 3161
		//"AobaZero_w3043_n_p800", // 3159
		//"afzgnnue002", // 3156
		//"F_g2", // 3156
		//"AisakaTaiga", // 3155
		//"AobaZero_w3283_n_p800", // 3153
		//"AobaZero_w2845_n_p800", // 3151
		//"F_e4", // 3150
		//"AobaZero_w3393_n_p800", // 3150
		//"AobaZero_w3333_n_p800", // 3146
		//"F_f4", // 3143
		//"AobaZero_w2545_n_p800", // 3139
		//"AobaZero_w3173_n_p800", // 3138
		//"AobaZero_w3253_n_p800", // 3135
		//"AobaZero_w3223_n_p800", // 3133
		//"AobaZero_w2965_n_p800", // 3132
		//"AobaZero_w3113_n_p800", // 3131
		//"AobaZero_w3273_n_p800", // 3131
		//"F_e33", // 3130
		//"AobaZero_w3293_n_p800", // 3130
		//"AobaZero_w3303_n_p800", // 3130
		//"AobaZero_w3233_n_p800", // 3130
		//"AobaZero_w3323_n_p800", // 3128
		//"g001", // 3127
		//"AobaZero_w2985_n_p800", // 3126
		//"AobaZero_w3025_n_p800", // 3112
		//"AobaZero_w3243_n_p800", // 3111
		//"AobaZero_w3153_n_p800", // 3108
		//"AobaZero_w3073_n_p800", // 3107
		//"AobaZero_w3343_n_p800", // 3106
		//"AobaZero_w3373_n_p800", // 3104
		//"AobaZero_w2905_n_p800", // 3103
		//"AobaZero_w3123_n_p800", // 3096
		//"AobaZero_w3193_n_p800", // 3096
		//"AobaZero_w3063_n_p800", // 3092
		//"AobaZero_w3103_n_p800", // 3088
		//"AobaZero_w3353_n_p800", // 3087
		//"AobaZero_w3093_n_p800", // 3085
		//"256x", // 3080
		//"AobaZero_w3213_n_p800", // 3079
		//"FukauraOu-MKL-i7-6700HQ", // 3078
		//"apery_WCSC25_E5-2687W_8t", // 3076
		//"rezeroX1_cl2_2_YO_tm2_4415Y", // 3073
		//"AobaZero_w2765_n_p800", // 3071
		//"AobaZero_w3313_n_p800", // 3060
		//"TEMPVS_FVGIT_1", // 3040
		//"maxmove256wwwwww", // 3039
		//"AobaZero_w2856_n_t4", // 3031
		//"elmo_WCSC27_479_1000k", // 3029
		//"AobaZero_w3383_n_p800", // 3026
		//"Jcus_Suisho3_250k", // 3016
		//"AobaZero_w2035_n_t4", // 3008
		//"Genesis1_2_YO_v1_1c", // 2992
		//"AobaZero_w2289_n_t4", // 2986
		//"AobaZero_w2705_n_p800", // 2982
		//"AobaZero_w3086_n_t4", // 2979
		//"AobaZero_w3028_n_t4", // 2979
		//"AobaZero_w3363_n_p800", // 2966
		//"AobaZero_w2077_n_t4", // 2966
		//"AobaZero_w2462_n_t4", // 2964
		//"AobaZero_w2681_n_t4", // 2962
		//"AobaZero_w3317_n_t4", // 2961
		//"AobaZero_w1819_n_t4", // 2958
		//"AobaZero_w1944_n_t4", // 2945
		//"AobaZero_w2515_n_t4", // 2939
		//"AobaZero_w1993_n_t4", // 2934
		//"AobaZero_w2762_n_t4", // 2932
		//"AobaZero_w2943_n_t4", // 2927
		//"uezero-p05.8", // 2926
		//"AobaZero_w1854_n_t4", // 2922
		//"AobaZero_w3333_n_t4", // 2922
		//"128x", // 2909
		//"TakeWarabe_i9-9960X", // 2906
		//"AobaZero_w3296_n_t4", // 2898
		//"AobaZero_w3175_n_t4", // 2896
		//"hYPY_Suisho3_D13", // 2895
		//"AobaZero_w1880_n_t4", // 2893
		//"YaneuraOuMiniV223_stdbk_16t", // 2884
		//"AobaZero_w3139_n_t4", // 2877
		//"AobaZero_w3235_n_t4", // 2876
		//"Krist_483_473stb_100k", // 2829
		//"AobaZero_w3351_n_t4", // 2824
		//"AobaZero_w2515_n_p400_t8", // 2799
		//"nanashogi", // 2786
		//"AobaZero_w1537_n_p200_t4", // 2766
		//"Gikou2_D11", // 2762
		//"AobaZero_w1671_n_p200_t4", // 2739
		//"AobaZero_w1618_n_p200_t4", // 2731
		//"WHn3_Suisho3_D11", // 2730
		//"AobaZero_192x10b_1207_n_p800", // 2700
		//"64x", // 2697
		//"Yss1000k", // 2688
		//"AobaZero_w1310_n_p200_t4", // 2683
		//"elmo_WCSC27_479_100k", // 2668
		//"TEMPVSFVGIT_1", // 2618
		//"32x", // 2614
		//"bona6_D10", // 2613
		//"TakeWarabe_i5-7200U", // 2596
		//"test4_test", // 2509
		//"YSS", // 2504
		//"John", // 2488
		//"go_test10c", // 2472
		//"Jcus_Suisho3_D9", // 2449
		//"frenzy_human", // 2446
		//"Emily", // 2416
		//"16x", // 2413
		//"coduck_pi2_600MHz_1c", // 2247
		//"8x", // 2218
		//"AobaZero_w3392_p1", // 2178
		//"4x", // 2011
		//"tenuki", // 1967
		//"2x", // 1836
		//"komadokun_depth10", // 1833
		//"komadokun_depth9", // 1792
		//"1x", // 1766
		//"Poppo-poppo_WCSC30_Kw", // 1597
		//"Ino-shishi_WCSC25_Kw", // 1508
		//"policy_23ver2", // 1430
		//"Gao-gao_WCSC28_Kw", // 1335
	};

	struct SfenAndMove {
		std::string sfen;
		Move best_move;
		Move next_move;
	};

	void WriteBook(Book::MemoryBook& book, const std::string output_book_file_path) {
		std::string backup_file_path = output_book_file_path + ".bak";

		if (std::filesystem::exists(backup_file_path)) {
			sync_cout << "Removing the backup file. backup_file_path=" << backup_file_path << sync_endl;
			std::filesystem::remove(backup_file_path);
		}

		if (std::filesystem::exists(output_book_file_path)) {
			sync_cout << "Renaming the output file. output_book_file_path=" << output_book_file_path << " backup_file_path=" << backup_file_path << sync_endl;
			std::filesystem::rename(output_book_file_path, backup_file_path);
		}

		book.write_book(output_book_file_path);
		sync_cout << "|output_book_file_path|=" << book.get_body().size() << sync_endl;
	}

	void WriteBook(BookMoveSelector& book, const std::string output_book_file_path) {
		WriteBook(book.get_body(), output_book_file_path);
	}

	std::mutex UPSERT_BOOK_MOVE_MUTEX;

	/// <summary>
	/// 定跡データベースに指し手を登録する。
	/// すでに指し手が登録されている場合、情報を上書きする。
	/// 内部的に大域的なロックを取って処理する。
	/// 指し手情報は16ビットに変換してから保存する。
	/// </summary>
	/// <param name="book"></param>
	/// <param name="sfen"></param>
	/// <param name="best_move"></param>
	/// <param name="next_move"></param>
	/// <param name="value"></param>
	/// <param name="depth"></param>
	/// <param name="num"></param>
	void UpsertBookMove(MemoryBook& book, const std::string& sfen, Move best_move, Move next_move, int value, int depth, uint64_t num)
	{
		std::lock_guard<std::mutex> lock(UPSERT_BOOK_MOVE_MUTEX);

		// MemoryBook::insert()に処理を移譲する。
		book.insert(sfen, BookMove(Move16(best_move), Move16(next_move), value, depth, num));
	}
}

bool Tanuki::InitializeBook(USI::OptionsMap& o) {
	o[kBookSfenFile] << Option("merged.sfen");
	o[kBookMaxMoves] << Option(32, 0, 256);
	o[kBookSearchDepth] << Option(64, 0, 256);
	o[kBookSearchNodes] << Option(500000 * 60, 0, INT_MAX);
	o[kBookInputFile] << Option("user_book1.db");
	o[kBookInputFolder] << Option("");
	o[kBookOutputFile] << Option("user_book2.db");
	o[kBookOverwriteExistingPositions] << Option(false);
	o[kBookTargetSfensFile] << Option("");
	o[kBookCsaFolder] << Option("");
	o[kBookTanukiColiseumLogFolder] << Option("");
	o[kBookMinimumWinningPercentage] << Option(0, 0, 100);
	o[kBookBlackMinimumValue] << Option(-VALUE_MATE, -VALUE_MATE, VALUE_MATE);
	o[kBookWhiteMinimumValue] << Option(-VALUE_MATE, -VALUE_MATE, VALUE_MATE);
	return true;
}

// sfen棋譜ファイルから定跡データベースを作成する
bool Tanuki::CreateRawBook() {
	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	std::string sfen_file = Options[kBookSfenFile];
	int max_moves = (int)Options[kBookMaxMoves];
	bool narrow_book = static_cast<bool>(Options[kBookNarrowBook]);

	std::ifstream ifs(sfen_file);
	if (!ifs) {
		sync_cout << "info string Failed to open the sfen file." << sync_endl;
		std::exit(-1);
	}
	std::string line;

	MemoryBook memory_book;
	std::vector<StateInfo> state_info(1024);
	int num_records = 0;
	while (std::getline(ifs, line)) {
		std::istringstream iss(line);
		Position& pos = Threads[0]->rootPos;
		pos.set_hirate(&state_info[0], Threads[0]);

		std::string token;
		while (pos.game_ply() < max_moves && iss >> token) {
			if (token == "startpos" || token == "moves") {
				continue;
			}
			Move move = USI::to_move(pos, token);
			if (!pos.pseudo_legal(move) || !pos.legal(move)) {
				continue;
			}

			UpsertBookMove(memory_book, pos.sfen(), move, Move::MOVE_NONE, 0, 0, 1);

			pos.do_move(move, state_info[pos.game_ply()]);
		}

		++num_records;
		if (num_records % 10000 == 0) {
			sync_cout << "info string " << num_records << sync_endl;
		}
	}

	WriteBook(memory_book, "book/" + Options[kBookFile]);

	return true;
}

bool Tanuki::CreateScoredBook() {
	int num_threads = (int)Options[kThreads];
	std::string input_book_file = Options[kBookInputFile];
	int search_depth = (int)Options[kBookSearchDepth];
	int search_nodes = (int)Options[kBookSearchNodes];
	int multi_pv = (int)Options[kMultiPV];
	std::string output_book_file = Options[kBookOutputFile];
	bool overwrite_existing_positions = static_cast<bool>(Options[kBookOverwriteExistingPositions]);

	omp_set_num_threads(num_threads);

	sync_cout << "info string num_threads=" << num_threads << sync_endl;
	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string search_depth=" << search_depth << sync_endl;
	sync_cout << "info string search_nodes=" << search_nodes << sync_endl;
	sync_cout << "info string multi_pv=" << multi_pv << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;
	sync_cout << "info string overwrite_existing_positions=" << overwrite_existing_positions
		<< sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	MemoryBook input_book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	input_book.read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book|=" << input_book.get_body().size() << sync_endl;

	MemoryBook output_book;
	output_book_file = "book/" + output_book_file;
	sync_cout << "Reading output book file: " << output_book_file << sync_endl;
	output_book.read_book(output_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.get_body().size() << sync_endl;

	std::vector<std::string> sfens;
	for (const auto& sfen_and_count : input_book.get_body()) {
		if (!overwrite_existing_positions &&
			output_book.get_body().find(sfen_and_count.first) != output_book.get_body().end()) {
			continue;
		}
		sfens.push_back(sfen_and_count.first);
	}
	int num_sfens = static_cast<int>(sfens.size());
	sync_cout << "Number of the positions to be processed: " << num_sfens << sync_endl;

	time_t start_time = 0;
	std::time(&start_time);

	std::atomic_int global_position_index;
	global_position_index = 0;
	ProgressReport progress_report(num_sfens, kShowProgressPerAtMostSec);
	time_t last_save_time_sec = std::time(nullptr);

	std::atomic<bool> need_wait = false;
	std::atomic_int global_pos_index;
	global_pos_index = 0;
	std::atomic_int global_num_processed_positions;
	global_num_processed_positions = 0;
#pragma omp parallel
	{
		int thread_index = ::omp_get_thread_num();
		WinProcGroup::bindThisThread(thread_index);

		for (int position_index = global_pos_index++; position_index < num_sfens;
			position_index = global_pos_index++) {
			const std::string& sfen = sfens[position_index];
			Thread& thread = *Threads[thread_index];
			StateInfo state_info = {};
			Position& pos = thread.rootPos;
			pos.set(sfen, &state_info, &thread);

			if (pos.is_mated()) {
				continue;
			}

			Learner::search(pos, search_depth, multi_pv, search_nodes);

			int num_pv = std::min(multi_pv, static_cast<int>(thread.rootMoves.size()));
			for (int pv_index = 0; pv_index < num_pv; ++pv_index) {
				const auto& root_move = thread.rootMoves[pv_index];
				Move best = Move::MOVE_NONE;
				if (root_move.pv.size() >= 1) {
					best = root_move.pv[0];
				}
				Move next = Move::MOVE_NONE;
				if (root_move.pv.size() >= 2) {
					next = root_move.pv[1];
				}
				int value = root_move.score;
				UpsertBookMove(output_book, sfen, best, next, value, thread.completedDepth, 1);
			}

			int num_processed_positions = ++global_num_processed_positions;
			// 念のため、I/Oはマスタースレッドでのみ行う
#pragma omp master
			{
				// 進捗状況を表示する
				progress_report.Show(num_processed_positions);

				// 一定時間ごとに保存する
				{
					std::lock_guard<std::mutex> lock(UPSERT_BOOK_MOVE_MUTEX);
					if (last_save_time_sec + kSavePerAtMostSec < std::time(nullptr)) {
						WriteBook(output_book, output_book_file);
						last_save_time_sec = std::time(nullptr);
					}
				}
			}

			need_wait = need_wait ||
				(progress_report.HasDataPerTime() &&
					progress_report.GetDataPerTime() * 2 < progress_report.GetMaxDataPerTime());

			if (need_wait) {
				// 処理速度が低下してきている。
				// 全てのスレッドを待機する。
#pragma omp barrier

				// マスタースレッドでしばらく待機する。
#pragma omp master
				{
					sync_cout << "Speed is down. Waiting for a while. GetDataPerTime()=" <<
						progress_report.GetDataPerTime() << " GetMaxDataPerTime()=" <<
						progress_report.GetMaxDataPerTime() << sync_endl;

					std::this_thread::sleep_for(std::chrono::minutes(10));
					progress_report.Reset();
					need_wait = false;
				}

				// マスタースレッドの待機が終わるまで、再度全てのスレッドを待機する。
#pragma omp barrier
			}
		}
	}

	WriteBook(output_book, output_book_file);

	return true;
}

// 複数の定跡をマージする
// BookInputFileには「;」区切りで定跡データベースの古パースを指定する
// BookOutputFileにはbook以下のファイル名を指定する
bool Tanuki::MergeBook() {
	std::string input_file_list = Options[kBookInputFile];
	std::string output_file = Options[kBookOutputFile];

	sync_cout << "info string input_file_list=" << input_file_list << sync_endl;
	sync_cout << "info string output_file=" << output_file << sync_endl;

	BookMoveSelector output_book;
	sync_cout << "Reading output book file: " << output_file << sync_endl;
	output_book.get_body().read_book("book/" + output_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.get_body().get_body().size() << sync_endl;

	std::vector<std::string> input_files;
	{
		std::istringstream iss(input_file_list);
		std::string input_file;
		while (std::getline(iss, input_file, ';')) {
			input_files.push_back(input_file);
		}
	}

	for (int input_file_index = 0; input_file_index < input_files.size(); ++input_file_index) {
		sync_cout << (input_file_index + 1) << " / " << input_files.size() << sync_endl;

		const auto& input_file = input_files[input_file_index];
		BookMoveSelector input_book;
		sync_cout << "Reading input book file: " << input_file << sync_endl;
		input_book.get_body().read_book(input_file);
		sync_cout << "done..." << sync_endl;
		sync_cout << "|input_book|=" << input_book.get_body().get_body().size() << sync_endl;

		for (const auto& book_type : input_book.get_body().get_body()) {
			const auto& sfen = book_type.first;
			const auto& pos_move_list = book_type.second;

			uint64_t max_move_count = 0;
			for (const auto& pos_move : *pos_move_list) {
				max_move_count = std::max(max_move_count, pos_move.move_count);
			}

			for (const auto& pos_move : *pos_move_list) {
				if (max_move_count > 0 && pos_move.move_count == 0) {
					// 採択回数が設定されており、この手の採択回数が0の場合、
					// 手動でこの手を指さないよう調整されている。
					// そのような手はスキップする。
					continue;
				}
				output_book.get_body().insert(sfen, pos_move, false);
			}
		}
	}

	WriteBook(output_book, "book/" + output_file);

	return true;
}

// 定跡の各指し手に評価値を付ける
// 相手の応手が設定されていない場合、読み筋から設定する。
bool Tanuki::SetScoreToMove() {
	int num_threads = (int)Options[kThreads];
	std::string input_book_file = Options[kBookInputFile];
	int search_depth = (int)Options[kBookSearchDepth];
	int search_nodes = (int)Options[kBookSearchNodes];
	std::string output_book_file = Options[kBookOutputFile];

	omp_set_num_threads(num_threads);

	sync_cout << "info string num_threads=" << num_threads << sync_endl;
	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string search_depth=" << search_depth << sync_endl;
	sync_cout << "info string search_nodes=" << search_nodes << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	MemoryBook input_book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	input_book.read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book|=" << input_book.get_body().size() << sync_endl;

	MemoryBook output_book;
	output_book_file = "book/" + output_book_file;
	sync_cout << "Reading output book file: " << output_book_file << sync_endl;
	output_book.read_book(output_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.get_body().size() << sync_endl;

	std::vector<SfenAndMove> sfen_and_moves;

	// 未処理の局面をキューに登録する
	for (const auto& input_sfen_and_pos_move_list : input_book.get_body()) {
		const auto& input_sfen = input_sfen_and_pos_move_list.first;

		Position& position = Threads[0]->rootPos;
		StateInfo state_info;
		position.set(input_sfen, &state_info, Threads[0]);

		auto output_book_moves_it = output_book.get_body().find(input_sfen);
		if (output_book_moves_it == output_book.get_body().end()) {
			// 出力定跡データベースに局面が登録されていなかった場合
			for (const auto& input_book_move : *input_sfen_and_pos_move_list.second) {
				sfen_and_moves.push_back({
					input_sfen,
					position.to_move(input_book_move.move),
					position.to_move(input_book_move.ponder)
					});
			}
			continue;
		}

		auto& output_book_moves = *output_book_moves_it->second;
		for (const auto& input_book_move : *input_sfen_and_pos_move_list.second) {
			if (std::find_if(output_book_moves.begin(), output_book_moves.end(),
				[&input_book_move](const auto& x) {
					return input_book_move.move == x.move;
				}) != output_book_moves.end()) {
				continue;
			}

			// 出力定跡データベースに指し手が登録されていなかった場合
			sfen_and_moves.push_back({
				input_sfen,
				position.to_move(input_book_move.move),
				position.to_move(input_book_move.ponder)
				});
		}
	}
	int num_sfen_and_moves = static_cast<int>(sfen_and_moves.size());
	sync_cout << "Number of the moves to be processed: " << num_sfen_and_moves << sync_endl;

	// マルチスレッド処理準備のため、インデックスを初期化する
	std::atomic_int global_sfen_and_move_index;
	global_sfen_and_move_index = 0;

	// 進捗状況表示の準備
	ProgressReport progress_report(num_sfen_and_moves, kShowProgressPerAtMostSec);

	// 定跡を定期的に保存するための変数
	time_t last_save_time_sec = std::time(nullptr);

	// Apery式マルチスレッド処理
	std::atomic<bool> need_wait = false;
	std::atomic_int global_num_processed_positions;
	global_num_processed_positions = 0;
#pragma omp parallel
	{
		int thread_index = ::omp_get_thread_num();
		WinProcGroup::bindThisThread(thread_index);

		for (int position_index = global_sfen_and_move_index++; position_index < num_sfen_and_moves;
			position_index = global_sfen_and_move_index++) {
			const auto& sfen_and_move = sfen_and_moves[position_index];
			const auto& sfen = sfen_and_move.sfen;
			Thread& thread = *Threads[thread_index];
			StateInfo state_info = {};
			Position& pos = thread.rootPos;
			pos.set(sfen, &state_info, &thread);
			Move best_move = sfen_and_move.best_move;
			Move next_move = MOVE_NONE;

			if (pos.is_mated()) {
				continue;
			}

			if (!pos.pseudo_legal(best_move) || !pos.legal(best_move)) {
				sync_cout << "Illegal move. sfen=" << sfen << " best_move=" <<
					USI::move(best_move) << " next_move=" << USI::move(next_move) << sync_endl;
				continue;
			}

			StateInfo state_info0;
			pos.do_move(best_move, state_info0);
			Eval::evaluate_with_no_return(pos);

			// この局面について探索する
			auto value_and_pv = Learner::search(pos, search_depth, 1, search_nodes);

			// ひとつ前の局面から見た評価値を代入する必要があるので、符号を反転する。
			Value value = -value_and_pv.first;

			auto pv = value_and_pv.second;
			if (next_move == MOVE_NONE && pv.size() >= 1) {
				// 定跡の手を指した次の局面なので、nextMoveにはpv[0]を代入する。
				// ただし、もともとnextMoveが設定されている場合、それを優先する。
				next_move = pv[0];
			}

			Depth depth = pos.this_thread()->completedDepth;

			// 指し手を出力先の定跡に登録する
			UpsertBookMove(output_book, sfen, best_move, next_move, value, depth, 1);

			int num_processed_positions = ++global_num_processed_positions;
			// 念のため、I/Oはマスタースレッドでのみ行う
#pragma omp master
			{
				// 進捗状況を表示する
				progress_report.Show(num_processed_positions);

				// 定跡をストレージに書き出す。
				if (last_save_time_sec + kSavePerAtMostSec < std::time(nullptr)) {
					std::lock_guard<std::mutex> lock(UPSERT_BOOK_MOVE_MUTEX);
					WriteBook(output_book, output_book_file);
					last_save_time_sec = std::time(nullptr);
				}
			}

			need_wait = need_wait ||
				(progress_report.HasDataPerTime() &&
					progress_report.GetDataPerTime() * 2 < progress_report.GetMaxDataPerTime());

			if (need_wait) {
				// 処理速度が低下してきている。
				// 全てのスレッドを待機する。
#pragma omp barrier

			// マスタースレッドでしばらく待機する。
#pragma omp master
				{
					sync_cout << "Speed is down. Waiting for a while. GetDataPerTime()=" <<
						progress_report.GetDataPerTime() << " GetMaxDataPerTime()=" <<
						progress_report.GetMaxDataPerTime() << sync_endl;

					std::this_thread::sleep_for(std::chrono::minutes(10));
					progress_report.Reset();
					need_wait = false;
				}

				// マスタースレッドの待機が終わるまで、再度全てのスレッドを待機する。
#pragma omp barrier
			}
		}
	}

	WriteBook(output_book, output_book_file);

	return true;
}

namespace {
	struct ValueMoveDepth {
		// 評価値
		Value value = VALUE_NONE;
		// この局面における指し手
		// 定跡の指し手に対する応手を定跡データベースに登録するため、指し手も返せるようにしておく
		Move move = MOVE_NONE;
		// 探索深さ
		Depth depth = DEPTH_NONE;
	};

	// (sfen, ValueAndDepth)
	// やねうら王本家では手数の部分を削除しているが、
	// 実装を容易にするため手数も含めて格納する。
	// 本来であればこれによる副作用を熟考する必要があるが、
	// よく分からなかったので適当に実装する。
	std::unordered_map<std::string, ValueMoveDepth> memo;

	// IgnoreBookPlyオプションがtrueの場合、
	// 与えられたsfen文字列からplyを取り除く
	std::string RemovePlyIfIgnoreBookPly(const std::string& sfen) {
		if (Options["IgnoreBookPly"]) {
			return StringExtension::trim_number(sfen);
		}
		else {
			return sfen;
		}
	}

	// Nega-Max法で末端局面の評価値をroot局面に向けて伝搬する
	// book 定跡データベース
	// pos 現在の局面
	// value_and_depth_without_nega_max_parent SetScoreToMove()で設定した評価値と探索深さ
	//                                         現局面から見た評価値になるよう、符号を反転してから渡すこと
	ValueMoveDepth NegaMax(MemoryBook& book, Position& pos, int& counter) {
		if (++counter % 1000000 == 0) {
			sync_cout << counter << " |memo|=" << memo.size() << sync_endl;
		}

		// この局面に対してのキーとして使用するsfen文字列。
		// 必要に応じて末尾のplyを取り除いておく。
		std::string sfen = RemovePlyIfIgnoreBookPly(pos.sfen());

		// NegaMax()は定跡データベースに登録されている局面についてのみ処理を行う。
		ASSERT_LV3(book.get_body().count(sfen));

		auto& vmd = memo[sfen];
		if (vmd.depth != DEPTH_NONE) {
			// キャッシュにヒットした場合、その値を返す。
			return vmd;
		}

		if (pos.is_mated()) {
			// 詰んでいる場合
			vmd.value = mated_in(0);
			vmd.depth = 0;
			return vmd;
		}

		if (pos.DeclarationWin() != MOVE_NONE) {
			// 宣言勝ちできる場合
			vmd.value = mate_in(1);
			vmd.depth = 0;
			return vmd;
		}

		auto repetition_state = pos.is_repetition();
		switch (repetition_state) {
		case REPETITION_WIN:
			// 連続王手の千日手により勝ちの場合
			vmd.value = mate_in(MAX_PLY);
			vmd.depth = 0;
			return vmd;

		case REPETITION_LOSE:
			// 連続王手の千日手により負けの場合
			vmd.value = mated_in(MAX_PLY);
			vmd.depth = 0;
			return vmd;

		case REPETITION_DRAW:
			// 引き分けの場合
			// やねうら王では先手後手に別々の評価値を付加している。
			// ここでは簡単のため、同じ評価値を付加する。
			vmd.value = static_cast<Value>(Options["Contempt"] * Eval::PawnValue / 100);
			vmd.depth = 0;
			return vmd;

		case REPETITION_SUPERIOR:
			// 優等局面の場合
			vmd.value = VALUE_SUPERIOR;
			vmd.depth = 0;
			return vmd;

		case REPETITION_INFERIOR:
			// 劣等局面の場合
			vmd.value = -VALUE_SUPERIOR;
			vmd.depth = 0;
			return vmd;

		default:
			break;
		}

		vmd.value = mated_in(0);
		vmd.depth = 0;
		// 全合法手について調べる
		for (const auto& move : MoveList<LEGAL_ALL>(pos)) {
			StateInfo state_info = {};
			pos.do_move(move, state_info);
			if (book.get_body().find(pos.sfen()) != book.get_body().end()) {
				// 子局面が存在する場合のみ処理する。
				ValueMoveDepth vmd_child = NegaMax(book, pos, counter);

				// 指し手情報に探索の結果を格納する。
				// 返ってきた評価値は次の局面から見た評価値なので、符号を反転する。
				// また、探索深さを+1する。
				UpsertBookMove(book, sfen, move, vmd_child.move, -vmd_child.value, vmd_child.depth + 1, 1);
			}
			pos.undo_move(move);
		}

		// 現局面について、定跡データベースに子局面への指し手が登録された。
		// 定跡データベースを調べ、この局面における最適な指し手を調べて返す。
		auto book_moves = book.get_body().find(sfen);
		ASSERT_LV3(book_moves != book.get_body().end());
		for (const auto& book_move : *book_moves->second) {
			if (vmd.value < book_move.value) {
				vmd.value = static_cast<Value>(book_move.value);
				vmd.move = pos.to_move(book_move.move);
				vmd.depth = book_move.depth;
			}
		}

		return vmd;
	}
}

// 定跡データベースの末端局面の評価値をroot局面に向けて伝搬する
bool Tanuki::PropagateLeafNodeValuesToRoot() {
	std::string input_book_file = Options[kBookInputFile];
	std::string output_book_file = Options[kBookOutputFile];

	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	MemoryBook book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	book.read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book_file|=" << book.get_body().size() << sync_endl;

	// 平手の局面からたどれる局面について処理する
	// メモをクリアするのを忘れない
	memo.clear();
	auto& pos = Threads[0]->rootPos;
	StateInfo state_info = {};
	pos.set_hirate(&state_info, Threads[0]);
	int counter = 0;
	NegaMax(book, pos, counter);

	// 平手の局面から辿れなかった局面を処理する
	for (const auto& book_entry : book.get_body()) {
		auto& pos = Threads[0]->rootPos;
		StateInfo state_info = {};
		pos.set(book_entry.first, &state_info, Threads[0]);
		NegaMax(book, pos, counter);
	}

	WriteBook(book, "book/" + output_book_file);
	sync_cout << "|output_book|=" << book.get_body().size() << sync_endl;

	return true;
}

namespace {
	// 与えられた局面における、与えられた指し手が定跡データベースに含まれているかどうかを返す。
	// 含まれている場合は、その指し手へのポインターを返す。
	// 含まれていない場合は、nullptrを返す。
	BookMove* IsBookMoveExist(BookMoveSelector& book, Position& position, Move move) {
		auto book_moves = book.get_body().find(position);
		if (book_moves == nullptr) {
			return nullptr;
		}

		auto book_move = std::find_if(book_moves->begin(), book_moves->end(),
			[move](auto& m) {
				return m.move == Move16(move);
			});
		if (book_move == book_moves->end()) {
			// 定跡データベースに、この指し手が登録されていない場合。
			return nullptr;
		}
		else {
			return &*book_move;
		}
	}

	// 与えられた局面における、与えられた指し手が、展開すべき対象かどうかを返す。
	// 展開する条件は、
	// - 定跡データベースに指し手が含まれている、かつ評価値が閾値以上
	// - 定跡データベースに指し手が含まれていない、かつ次の局面が含まれている
	bool IsTargetMove(BookMoveSelector& book, Position& position, Move move32, int book_eval_black_limit, int book_eval_white_limit) {
		auto book_pos = IsBookMoveExist(book, position, move32);
		if (book_pos != nullptr) {
			// 定跡データベースに指し手が含まれている
			int book_eval_limit = position.side_to_move() == BLACK ? book_eval_black_limit : book_eval_white_limit;
			return book_pos->value > book_eval_limit;
		}
		else {
			StateInfo state_info = {};
			position.do_move(move32, state_info);
			bool exist = book.get_body().get_body().find(position.sfen()) != book.get_body().get_body().end();
			position.undo_move(move32);
			return exist;
		}
	}

	// 与えられた局面を延長すべきかどうか判断する
	bool IsTargetPosition(BookMoveSelector& book, Position& position, int multi_pv) {
		auto book_moves = book.get_body().find(position);
		if (book_moves == nullptr) {
			// 定跡データベースに、この局面が登録されていない場合、延長する。
			return true;
		}

		// 登録されている指し手の数が、MultiPVより少ない場合、延長する。
		return book_moves->size() < multi_pv;
	}
}

// 定跡の延長の対象となる局面を抽出する。
// MultiPVが指定された値より低い局面、および定跡データベースに含まれていない局面が対象となる。
// あらかじめ、SetScoreToMove()を用い、定跡データベースの各指し手に評価値を付け、
// PropagateLeafNodeValuesToRoot()を用い、Leaf局面の評価値をRoot局面に伝搬してから使用する。
bool Tanuki::ExtractTargetPositions() {
	int multi_pv = (int)Options[kMultiPV];
	std::string input_book_file = Options[kBookInputFile];
	int book_eval_black_limit = (int)Options["BookEvalBlackLimit"];
	int book_eval_white_limit = (int)Options["BookEvalWhiteLimit"];
	std::string target_sfens_file = Options[kBookTargetSfensFile];

	sync_cout << "info string multi_pv=" << multi_pv << sync_endl;
	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string book_eval_black_limit=" << book_eval_black_limit << sync_endl;
	sync_cout << "info string book_eval_white_limit=" << book_eval_white_limit << sync_endl;
	sync_cout << "info string target_sfens_file=" << sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	BookMoveSelector book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	book.get_body().read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book_file|=" << book.get_body().get_body().size() << sync_endl;

	std::set<std::string> explorered;
	explorered.insert(SFEN_HIRATE);
	// 千日手の処理等のため、平手局面からの指し手として保持する
	// Moveは32ビット版とする
	std::deque<std::vector<Move>> frontier;
	frontier.push_back({});

	std::ofstream ofs(target_sfens_file);

	int counter = 0;
	std::vector<StateInfo> state_info(1024);
	while (!frontier.empty()) {
		if (++counter % 1000 == 0) {
			sync_cout << counter << sync_endl;
		}

		auto moves = frontier.front();
		Position& position = Threads[0]->rootPos;
		position.set_hirate(&state_info[0], Threads[0]);
		// 現局面まで指し手を進める
		for (auto move : moves) {
			position.do_move(move, state_info[position.game_ply()]);
		}
		frontier.pop_front();

		// 千日手の局面は処理しない
		auto draw_type = position.is_repetition(MAX_PLY);
		if (draw_type == REPETITION_DRAW) {
			continue;
		}

		// 詰み、宣言勝ちの局面も処理しない
		if (position.is_mated() || position.DeclarationWin() != MOVE_NONE) {
			continue;
		}

		if (IsTargetPosition(book, position, multi_pv)) {
			// 対象の局面を書き出す。
			// 形式はmoveをスペース区切りで並べたものとする。
			// これは、千日手等を認識させるため。

			for (auto m : moves) {
				ofs << m << " ";
			}
			ofs << std::endl;
		}

		// 子局面を展開する
		for (const auto& move : MoveList<LEGAL_ALL>(position)) {
			if (!position.pseudo_legal(move) || !position.legal(move)) {
				// 不正な手の場合は処理しない
				continue;
			}

			if (!IsTargetMove(book, position, move, book_eval_black_limit, book_eval_white_limit)) {
				// この指し手の先の局面は処理しない。
				continue;
			}

			position.do_move(move, state_info[position.game_ply()]);

			// undo_move()を呼び出す必要があるので、continueとbreakを禁止する。
			if (!explorered.count(position.sfen())) {
				explorered.insert(position.sfen());

				moves.push_back(move);
				frontier.push_back(moves);
				moves.pop_back();
			}

			position.undo_move(move);
		}
	}

	sync_cout << "done..." << sync_endl;

	return true;
}

bool Tanuki::AddTargetPositions() {
	int num_threads = (int)Options[kThreads];
	std::string input_book_file = Options[kBookInputFile];
	int search_depth = (int)Options[kBookSearchDepth];
	int search_nodes = (int)Options[kBookSearchNodes];
	int multi_pv = (int)Options[kMultiPV];
	std::string output_book_file = Options[kBookOutputFile];
	std::string target_sfens_file = Options[kBookTargetSfensFile];

	omp_set_num_threads(num_threads);

	sync_cout << "info string num_threads=" << num_threads << sync_endl;
	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string search_depth=" << search_depth << sync_endl;
	sync_cout << "info string search_nodes=" << search_nodes << sync_endl;
	sync_cout << "info string multi_pv=" << multi_pv << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;
	sync_cout << "info string target_sfens_file=" << sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	MemoryBook input_book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	input_book.read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book|=" << input_book.get_body().size() << sync_endl;

	MemoryBook output_book;
	output_book_file = "book/" + output_book_file;
	sync_cout << "Reading output book file: " << output_book_file << sync_endl;
	output_book.read_book(output_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.get_body().size() << sync_endl;

	// 対象の局面を読み込む
	sync_cout << "Reading target positions: target_sfens_file=" << target_sfens_file << sync_endl;
	std::vector<std::string> lines;
	std::ifstream ifs(target_sfens_file);
	std::string line;
	while (std::getline(ifs, line)) {
		lines.push_back(line);
	}
	int num_positions = static_cast<int>(lines.size());
	sync_cout << "done..." << sync_endl;
	sync_cout << "|lines|=" << num_positions << sync_endl;

	ProgressReport progress_report(num_positions, kShowProgressPerAtMostSec);
	time_t last_save_time_sec = std::time(nullptr);

	std::atomic<bool> need_wait = false;
	std::atomic_int global_pos_index;
	global_pos_index = 0;
	std::atomic_int global_num_processed_positions;
	global_num_processed_positions = 0;

#pragma omp parallel
	{
		int thread_index = ::omp_get_thread_num();
		WinProcGroup::bindThisThread(thread_index);

		for (int position_index = global_pos_index++; position_index < num_positions;
			position_index = global_pos_index++) {
			Thread& thread = *Threads[thread_index];
			std::vector<StateInfo> state_info(1024);
			Position& pos = thread.rootPos;
			pos.set_hirate(&state_info[0], &thread);

			std::istringstream iss(lines[position_index]);
			std::string move_string;
			while (iss >> move_string) {
				Move16 move16 = USI::to_move16(move_string);
				Move move = pos.to_move(move16);
				pos.do_move(move, state_info[pos.game_ply()]);
			}

			if (pos.is_mated()) {
				continue;
			}

			Learner::search(pos, search_depth, multi_pv, search_nodes);

			int num_pv = std::min(multi_pv, static_cast<int>(thread.rootMoves.size()));
			for (int pv_index = 0; pv_index < num_pv; ++pv_index) {
				const auto& root_move = thread.rootMoves[pv_index];
				Move best = Move::MOVE_NONE;
				if (root_move.pv.size() >= 1) {
					best = root_move.pv[0];
				}
				Move next = Move::MOVE_NONE;
				if (root_move.pv.size() >= 2) {
					next = root_move.pv[1];
				}
				int value = root_move.score;
				UpsertBookMove(output_book, pos.sfen(), best, next, value, thread.completedDepth, 1);
			}

			int num_processed_positions = ++global_num_processed_positions;
			// 念のため、I/Oはマスタースレッドでのみ行う
#pragma omp master
			{
				// 進捗状況を表示する
				progress_report.Show(num_processed_positions);

				// 一定時間ごとに保存する
				{
					std::lock_guard<std::mutex> lock(UPSERT_BOOK_MOVE_MUTEX);
					if (last_save_time_sec + kSavePerAtMostSec < std::time(nullptr)) {
						WriteBook(output_book, output_book_file);
						last_save_time_sec = std::time(nullptr);
					}
				}
			}

			need_wait = need_wait ||
				(progress_report.HasDataPerTime() &&
					progress_report.GetDataPerTime() * 2 < progress_report.GetMaxDataPerTime());

			if (need_wait) {
				// 処理速度が低下してきている。
				// 全てのスレッドを待機する。
#pragma omp barrier

				// マスタースレッドでしばらく待機する。
#pragma omp master
				{
					sync_cout << "Speed is down. Waiting for a while. GetDataPerTime()=" <<
						progress_report.GetDataPerTime() << " GetMaxDataPerTime()=" <<
						progress_report.GetMaxDataPerTime() << sync_endl;

					std::this_thread::sleep_for(std::chrono::minutes(10));
					progress_report.Reset();
					need_wait = false;
				}

				// マスタースレッドの待機が終わるまで、再度全てのスレッドを待機する。
#pragma omp barrier
			}

			// 置換表の世代を進める
			Threads[thread_index]->tt.new_search();
		}
	}

	WriteBook(output_book, output_book_file);

	return true;
}

bool Tanuki::CreateFromTanukiColiseum()
{
	std::string input_book_file = Options[kBookInputFile];
	std::string input_book_folder = Options[kBookInputFolder];
	std::string output_book_file = Options[kBookOutputFile];

	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string input_book_folder=" << input_book_folder << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	MemoryBook input_book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	input_book.read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book|=" << input_book.get_body().size() << sync_endl;

	MemoryBook output_book;
	output_book_file = "book/" + output_book_file;
	sync_cout << "Reading output book file: " << output_book_file << sync_endl;
	output_book.read_book(output_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.get_body().size() << sync_endl;

	// TanukiColiseu
	for (const auto& entry : std::filesystem::recursive_directory_iterator(input_book_folder)) {
		const auto& file_name = entry.path().filename();
		if (file_name != "sfen.txt") {
			continue;
		}

		std::cout << entry.path() << std::endl;

		std::fstream iss(entry.path());
		std::string line;
		while (std::getline(iss, line)) {
			auto& pos = Threads[0]->rootPos;
			std::vector<StateInfo> state_info(1024);
			pos.set_hirate(&state_info[0], Threads[0]);

			std::istringstream iss_line(line);
			std::string move_string;
			std::vector<std::pair<std::string, Move16>> moves;
			// 先手が勝った場合は0、後手が勝った場合は1
			// 引き分けの場合は-1
			int winner_offset = -1;
			while (iss_line >> move_string) {
				if (move_string == "startpos" || move_string == "moves") {
					continue;
				}

				auto sfen = pos.sfen();
				auto move = USI::to_move(pos, move_string);
				pos.do_move(move, state_info[pos.game_ply()]);

				auto move16 = USI::to_move16(move_string);
				moves.emplace_back(sfen, move16);

				auto repetition = pos.is_repetition();
				if (pos.is_mated()) {
					if (moves.size() % 2 == 0) {
						// 後手勝ち
						winner_offset = 1;
					}
					else {
						// 先手勝ち
						winner_offset = 0;
					}
					break;
				}
				else if (pos.DeclarationWin()) {
					if (moves.size() % 2 == 0) {
						// 先手勝ち
						winner_offset = 0;
					}
					else {
						// 後手勝ち
						winner_offset = 1;
					}
					break;
				}
				else if (repetition == REPETITION_WIN) {
					// 連続王手の千日手による勝ち
					if (moves.size() % 2 == 0) {
						// 先手勝ち
						winner_offset = 0;
					}
					else {
						// 後手勝ち
						winner_offset = 1;
					}
					break;
				}
				else if (repetition == REPETITION_LOSE) {
					// 連続王手の千日手による負け
					if (moves.size() % 2 == 0) {
						// 後手勝ち
						winner_offset = 1;
					}
					else {
						// 先手勝ち
						winner_offset = 0;
					}
					break;
				}
				else if (repetition == REPETITION_DRAW) {
					// 連続王手ではない普通の千日手
					break;
				}
				else if (repetition == REPETITION_SUPERIOR) {
					// 優等局面(盤上の駒が同じで手駒が相手より優れている)
					if (moves.size() % 2 == 0) {
						// 先手勝ち
						winner_offset = 0;
					}
					else {
						// 後手勝ち
						winner_offset = 1;
					}
					break;
				}
				else if (repetition == REPETITION_INFERIOR) {
					// 劣等局面(盤上の駒が同じで手駒が相手より優れている)
					if (moves.size() % 2 == 0) {
						// 後手勝ち
						winner_offset = 1;
					}
					else {
						// 先手勝ち
						winner_offset = 0;
					}
					break;
				}
			}

			if (winner_offset == -1) {
				continue;
			}

			for (int move_index = winner_offset; move_index < moves.size(); move_index += 2) {
				const auto& sfen = moves[move_index].first;
				Move16 best = moves[move_index].second;
				Move16 next = MOVE_NONE;
				if (move_index + 1 < moves.size()) {
					next = moves[move_index + 1].second;
				}

				output_book.insert(sfen, BookMove(best, next, 0, 0, 1));
			}
		}
	}

	WriteBook(output_book, output_book_file);

	return true;
}

bool Tanuki::Create18Book() {
	std::string csa_folder = Options[kBookCsaFolder];
	std::string output_book_file = Options[kBookOutputFile];

	MemoryBook output_book;
	output_book_file = "book/" + output_book_file;
	sync_cout << "Reading output book file: " << output_book_file << sync_endl;
	output_book.read_book(output_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.get_body().size() << sync_endl;

	int num_records = 0;
	for (const std::filesystem::directory_entry& entry :
		std::filesystem::recursive_directory_iterator(csa_folder)) {
		auto& file_path = entry.path();
		if (file_path.extension() != ".csa") {
			// 拡張子が .csa でなかったらスキップする。
			continue;
		}

		if (std::count_if(kStrongPlayers.begin(), kStrongPlayers.end(),
			[&file_path](const auto& strong_player) {
				return file_path.string().find("+" + strong_player + "+") != std::string::npos;
			}) != 2) {
			// 強いプレイヤー同士の対局でなかったらスキップする。
			continue;
		}

		if (++num_records % 1000 == 0) {
			sync_cout << num_records << sync_endl;
		}

		auto& pos = Threads[0]->rootPos;
		StateInfo state_info[512];
		pos.set_hirate(&state_info[0], Threads[0]);

		FILE* file = std::fopen(file_path.string().c_str(), "r");

		if (file == nullptr) {
			std::cout << "!!! Failed to open the input file: filepath=" << file_path << std::endl;
			continue;
		}

		bool toryo = false;
		std::string black_player_name;
		std::string white_player_name;
		std::vector<Move> moves;
		int winner_offset = 0;
		char buffer[1024];
		while (std::fgets(buffer, sizeof(buffer) - 1, file)) {
			std::string line = buffer;
			// std::fgets()の出力は行末の改行を含むため、ここで削除する。
			while (!line.empty() && std::isspace(line.back())) {
				line.pop_back();
			}

			auto offset = line.find(',');
			if (offset != std::string::npos) {
				// 将棋所の出力するCSAの指し手の末尾に",T1"などとつくため
				// ","以降を削除する
				line = line.substr(0, offset);
			}

			if (line.find("N+") == 0) {
				black_player_name = line.substr(2);
			}
			else if (line.find("N-") == 0) {
				white_player_name = line.substr(2);
			}
			else if (line.size() == 7 && (line[0] == '+' || line[0] == '-')) {
				Move move = CSA::to_move(pos, line.substr(1));

				if (!pos.pseudo_legal(move) || !pos.legal(move)) {
					std::cout << "!!! Found an illegal move." << std::endl;
					break;
				}

				pos.do_move(move, state_info[pos.game_ply()]);

				moves.push_back(move);
			}
			else if (line.find(black_player_name + " win") != std::string::npos) {
				winner_offset = 0;
			}
			else if (line.find(white_player_name + " win") != std::string::npos) {
				winner_offset = 1;
			}

			if (line.find("toryo") != std::string::npos) {
				toryo = true;
			}
		}

		std::fclose(file);
		file = nullptr;

		// 投了以外の棋譜はスキップする
		if (!toryo) {
			continue;
		}

		pos.set_hirate(&state_info[0], Threads[0]);

		for (int play = 0; play < moves.size(); ++play) {
			Move move = moves[play];
			if (play % 2 == winner_offset) {
				Move ponder = (play + 1 < moves.size()) ? moves[play + 1] : Move::MOVE_NONE;
				output_book.insert(pos.sfen(), Book::BookMove(move, ponder, 0, 0, 1));
			}
			pos.do_move(move, state_info[pos.game_ply()]);
		}
	}

	WriteBook(output_book, output_book_file);

	return true;
}

namespace {
	struct InternalBookMove {
		Move16 move = Move::MOVE_NONE;   // この局面での指し手
		Move16 ponder = Move::MOVE_NONE; // その指し手を指したときの予想される相手の指し手(指し手が無いときはnoneと書くことになっているので、このとMOVE_NONEになっている)
		// この手を指した場合の勝利回数
		int num_win = 0;
		// この手を指した場合の敗北回数
		int num_lose = 0;
		// 評価値が付いていた指し手について、評価値の総和
		s64 sum_values = 0;
		// 評価値が付いていた指し手の数
		int num_values = 0;
	};
	using InternalBook = std::map<std::pair<std::string, u16 /* Move16 */>, InternalBookMove>;

	void ParseFloodgateCsaFiles(const std::string& csa_folder_path, InternalBook& internal_book) {
		int num_records = 0;
		for (const std::filesystem::directory_entry& entry :
			std::filesystem::recursive_directory_iterator(csa_folder_path)) {
			auto& file_path = entry.path();
			if (file_path.extension() != ".csa") {
				// 拡張子が .csa でなかったらスキップする。
				continue;
			}

			if (std::count_if(kStrongPlayers.begin(), kStrongPlayers.end(),
				[&file_path](const auto& strong_player) {
					return file_path.string().find("+" + strong_player + "+") != std::string::npos;
				}) != 2) {
				// 強いプレイヤー同士の対局でなかったらスキップする。
				continue;
			}

			if (++num_records % 1000 == 0) {
				sync_cout << num_records << sync_endl;
			}

			auto& pos = Threads[0]->rootPos;
			StateInfo state_info[512];
			pos.set_hirate(&state_info[0], Threads[0]);

			FILE* file = std::fopen(file_path.string().c_str(), "r");

			if (file == nullptr) {
				std::cout << "!!! Failed to open the input file: filepath=" << file_path << std::endl;
				continue;
			}

			bool toryo = false;
			std::string black_player_name;
			std::string white_player_name;
			std::vector<Move> moves;
			int winner_offset = 0;
			char buffer[1024];
			while (std::fgets(buffer, sizeof(buffer) - 1, file)) {
				std::string line = buffer;
				// std::fgets()の出力は行末の改行を含むため、ここで削除する。
				while (!line.empty() && std::isspace(line.back())) {
					line.pop_back();
				}

				auto offset = line.find(',');
				if (offset != std::string::npos) {
					// 将棋所の出力するCSAの指し手の末尾に",T1"などとつくため
					// ","以降を削除する
					line = line.substr(0, offset);
				}

				if (line.find("N+") == 0) {
					black_player_name = line.substr(2);
				}
				else if (line.find("N-") == 0) {
					white_player_name = line.substr(2);
				}
				else if (line.size() == 7 && (line[0] == '+' || line[0] == '-')) {
					Move move = CSA::to_move(pos, line.substr(1));

					if (!pos.pseudo_legal(move) || !pos.legal(move)) {
						std::cout << "!!! Found an illegal move." << std::endl;
						break;
					}

					pos.do_move(move, state_info[pos.game_ply()]);

					moves.push_back(move);
				}
				else if (line.find(black_player_name + " win") != std::string::npos) {
					winner_offset = 0;
				}
				else if (line.find(white_player_name + " win") != std::string::npos) {
					winner_offset = 1;
				}

				if (line.find("toryo") != std::string::npos) {
					toryo = true;
				}
			}

			std::fclose(file);
			file = nullptr;

			// 投了以外の棋譜はスキップする
			if (!toryo) {
				continue;
			}

			pos.set_hirate(&state_info[0], Threads[0]);

			for (int play = 0; play < moves.size(); ++play) {
				auto move = moves[play];
				if (!pos.pseudo_legal(move) || !pos.legal(move)) {
					std::cout << "Illegal move. sfen=" << pos.sfen() << " move=" << move << std::endl;
					break;
				}

				auto& internal_book_move = internal_book[std::make_pair(pos.sfen(), static_cast<u16>(move))];
				internal_book_move.move = move;
				internal_book_move.ponder = (play + 1 < moves.size()) ? moves[play + 1] : Move::MOVE_NONE;
				if (play % 2 == winner_offset) {
					++internal_book_move.num_win;
				}
				else {
					++internal_book_move.num_lose;
				}

				pos.do_move(move, state_info[pos.game_ply()]);
			}
		}
	}

	void GetGameIdsAndWinners(sqlite3* database, std::vector<std::pair<int, int>>& game_ids_and_winners) {
		sync_cout << "GetGameIdsAndWinners()" << sync_endl;

		sqlite3_stmt* stmt = NULL;

		// ゲームIDと勝者の取得。
		int result = sqlite3_prepare(database, "SELECT id, winner FROM game ORDER BY id", -1, &stmt, nullptr);
		if (result != SQLITE_OK) {
			sync_cout << "Faile to call sqlite3_prepare(). errmsg=" << sqlite3_errmsg(database) << sync_endl;
			return;
		}

		if (stmt == nullptr) {
			// コメント等、有効なSQLステートメントでないと、戻り値はOKだがstmはNULLになる。
			return;
		}

		int num_columns = sqlite3_column_count(stmt);
		int column_id = -1;
		int column_winner = -1;
		for (int column = 0; column < num_columns; ++column) {
			std::string column_name = sqlite3_column_name(stmt, column);
			if (column_name == "id") {
				column_id = column;
			}
			else if (column_name == "winner") {
				column_winner = column;
			}
			else {
				sync_cout << "Invalid column name. column_name=" << column_name << sync_endl;
			}
		}

		while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
			int id = -1;
			int winner = -1;
			for (int column = 0; column < num_columns; ++column) {
				if (column_id == column) {
					id = sqlite3_column_int(stmt, column);
				}
				else if (column_winner == column) {
					winner = sqlite3_column_int(stmt, column);
				}
				else {
					sync_cout << "Unknown column. column=" << column << sync_endl;
				}
			}

			if (id == -1) {
				sync_cout << "id is not contained." << sync_endl;
				continue;
			}

			if (winner == -1) {
				sync_cout << "winner is not contained." << sync_endl;
				continue;
			}

			game_ids_and_winners.emplace_back(id, winner);
		}

		sqlite3_finalize(stmt);
		stmt = nullptr;

		if (result == SQLITE_ERROR) {
			sync_cout << "Faile to call sqlite3_step(). errmsg=" << sqlite3_errmsg(database) << sync_endl;
			sqlite3_finalize(stmt);
			return;
		}
	}

	u16 to_move16(const std::string& str) {
		if (str == "none") {
			return static_cast<u16>(Move::MOVE_NONE);
		}
		return USI::to_move16(str).to_u16();
	}

	struct InternalMove {
		u16 best;
		u16 next;
		int value;
		int book;
	};

	void GetRecord(sqlite3* database, std::map<int, std::map<int, InternalMove>>& internal_moves) {
		sync_cout << "GetRecord()" << sync_endl;

		sqlite3_stmt* stmt = NULL;

		int result = sqlite3_prepare(database, "SELECT game_id, play, best, next, value, book FROM move", -1, &stmt, nullptr);
		if (result != SQLITE_OK) {
			sync_cout << "Faile to call sqlite3_prepare(). errmsg=" << sqlite3_errmsg(database) << sync_endl;
			return;
		}

		if (stmt == nullptr) {
			// コメント等、有効なSQLステートメントでないと、戻り値はOKだがstmはNULLになる。
			return;
		}

		int column_game_id = -1;
		int column_play = -1;
		int column_best = -1;
		int column_next = -1;
		int column_value = -1;
		int column_book = -1;
		int num_columns = sqlite3_column_count(stmt);
		for (int column = 0; column < num_columns; ++column) {
			std::string column_name = sqlite3_column_name(stmt, column);
			if (column_name == "game_id") {
				column_game_id = column;
			}
			else if (column_name == "play") {
				column_play = column;
			}
			else if (column_name == "best") {
				column_best = column;
			}
			else if (column_name == "next") {
				column_next = column;
			}
			else if (column_name == "value") {
				column_value = column;
			}
			else if (column_name == "book") {
				column_book = column;
			}
			else {
				sync_cout << "Invalid column name. column_name=" << column_name << sync_endl;
			}
		}

		// カラム数の取得
		while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
			int game_id = -1;
			int play = -1;
			std::string best;
			std::string next;
			int value = Value::VALUE_NONE;
			int book = -1;
			for (int column = 0; column < num_columns; ++column) {
				if (column_game_id == column) {
					game_id = sqlite3_column_int(stmt, column);
				}
				else if (column_play == column) {
					play = sqlite3_column_int(stmt, column);
				}
				else if (column_best == column) {
					best = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
				}
				else if (column_next == column) {
					next = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
				}
				else if (column_value == column) {
					value = sqlite3_column_int(stmt, column);
				}
				else if (column_book == column) {
					book = sqlite3_column_int(stmt, column);
				}
				else {
					sync_cout << "Invalid column. column=" << column << sync_endl;
				}
			}

			if (game_id == -1) {
				sync_cout << "game_id is not recorded." << sync_endl;
				continue;
			}

			if (play == -1) {
				sync_cout << "play is not recorded." << sync_endl;
				continue;
			}

			if (best.empty()) {
				sync_cout << "best is not recorded." << sync_endl;
				continue;
			}

			if (next.empty()) {
				sync_cout << "next is not recorded." << sync_endl;
				continue;
			}

			if (value == Value::VALUE_NONE) {
				sync_cout << "value is not recorded." << sync_endl;
				continue;
			}

			if (book == -1) {
				sync_cout << "book is not recorded." << sync_endl;
				continue;
			}

			internal_moves[game_id][play] = { to_move16(best), to_move16(next), value, book, };
		}

		sqlite3_finalize(stmt);
		stmt = nullptr;

		if (result == SQLITE_ERROR) {
			sync_cout << "Faile to call sqlite3_step(). errmsg=" << sqlite3_errmsg(database) << sync_endl;
			sqlite3_finalize(stmt);
			return;
		}
	}

	void ParseTanukiColiseumResultFiles(const std::string& tanuki_coliseum_folder_path, InternalBook& internal_book) {
		std::vector<StateInfo> state_infos(512);
		int num_records = 0;
		for (const std::filesystem::directory_entry& entry :
			std::filesystem::recursive_directory_iterator(tanuki_coliseum_folder_path)) {
			auto& file_path = entry.path();
			if (file_path.extension() != ".sqlite3") {
				// 拡張子が .csa でなかったらスキップする。
				continue;
			}

			// C++からSQLite3を使ってみる。 - seraphyの日記 https://seraphy.hatenablog.com/entry/20061031/p1
			sqlite3* database = nullptr;
			int result = 0;

			result = sqlite3_open(file_path.string().c_str(), &database);
			if (result != SQLITE_OK) {
				sqlite3_close(database);
				database = nullptr;

				sync_cout << "Failed to open an sqlite file. file_path=" << file_path << sync_endl;
				continue;
			}

			std::vector<std::pair<int, int>> game_ids_and_winners;
			GetGameIdsAndWinners(database, game_ids_and_winners);

			std::map<int, std::map<int, InternalMove>> internal_moves;
			GetRecord(database, internal_moves);

			sqlite3_close(database);
			database = nullptr;

			sync_cout << "Constructing an internal book." << sync_endl;

			for (auto& [game_id, winner] : game_ids_and_winners) {
				if (winner > 1) {
					// 引き分けは取り除く
					continue;
				}

				auto& position = Threads[0]->rootPos;
				position.set_hirate(&state_infos[0], Threads[0]);

				for (auto& [play, move] : internal_moves[game_id]) {
					auto move32 = position.to_move(move.best);
					if (!position.pseudo_legal(move32) || !position.legal(move32)) {
						sync_cout << "Illegal move. sfen=" << position.sfen() << " move=" << move32 << sync_endl;
						break;
					}

					auto& internal_book_move = internal_book[std::make_pair(position.sfen(), move.best)];
					internal_book_move.move = move.best;
					internal_book_move.ponder = move.next;
					if (play % 2 == winner) {
						++internal_book_move.num_win;
					}
					else {
						++internal_book_move.num_lose;
					}

					if (!move.book) {
						++internal_book_move.num_values;
						internal_book_move.sum_values += move.value;
					}

					position.do_move(move32, state_infos[position.game_ply()]);
				}
			}
		}
	}
}

bool Tanuki::CreateTayayanBook() {
	std::string csa_folder = Options[kBookCsaFolder];
	std::string tanuki_coliseum_log_folder = Options[kBookTanukiColiseumLogFolder];
	std::string output_book_file = Options[kBookOutputFile];
	int minimum_winning_percentage = Options[kBookMinimumWinningPercentage];

	MemoryBook output_book;
	output_book_file = "book/" + output_book_file;
	sync_cout << "Reading output book file: " << output_book_file << sync_endl;
	output_book.read_book(output_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.get_body().size() << sync_endl;

	InternalBook internal_book;
	ParseFloodgateCsaFiles(csa_folder, internal_book);
	ParseTanukiColiseumResultFiles(tanuki_coliseum_log_folder, internal_book);

	for (auto& [sfen_and_best16, book_move] : internal_book) {
		const auto& sfen = sfen_and_best16.first;
		auto move = book_move.move;
		auto ponder = book_move.ponder;
		int value = book_move.num_values ? (book_move.sum_values / book_move.num_values) : 0;
		int count = book_move.num_win + book_move.num_lose;

		// book_move.num_win / count < minimum_winning_percentage / 100
		if (book_move.num_win * 100 < minimum_winning_percentage * count) {
			continue;
		}

		auto& position = Threads[0]->rootPos;
		StateInfo state_info;
		position.set(sfen, &state_info, Threads[0]);
		auto move32 = position.to_move(move);
		if (!position.pseudo_legal(move32) || !position.legal(move32)) {
			sync_cout << "Illegal move. sfen=" << position.sfen() << " move=" << move32 << sync_endl;
			continue;
		}

		output_book.insert(sfen, Book::BookMove(move, ponder, value, 0, count));
	}

	WriteBook(output_book, output_book_file);

	return true;
}

bool Tanuki::CreateTayayanBook2() {
	std::string csa_folder = Options[kBookCsaFolder];
	std::string tanuki_coliseum_log_folder = Options[kBookTanukiColiseumLogFolder];
	std::string output_book_file = Options[kBookOutputFile];
	int minimum_winning_percentage = Options[kBookMinimumWinningPercentage];
	int black_minimum_value = Options[kBookBlackMinimumValue];
	int white_minimum_value = Options[kBookWhiteMinimumValue];

	MemoryBook output_book;
	output_book_file = "book/" + output_book_file;
	sync_cout << "Reading output book file: " << output_book_file << sync_endl;
	output_book.read_book(output_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.get_body().size() << sync_endl;

	InternalBook internal_book;
	ParseFloodgateCsaFiles(csa_folder, internal_book);
	ParseTanukiColiseumResultFiles(tanuki_coliseum_log_folder, internal_book);

	for (auto& [sfen_and_best16, book_move] : internal_book) {
		const auto& sfen = sfen_and_best16.first;
		auto move = book_move.move;
		auto ponder = book_move.ponder;
		int value = book_move.num_values ? (book_move.sum_values / book_move.num_values) : 0;
		int count = book_move.num_win + book_move.num_lose;
		auto color = (sfen.find(" b ") != std::string::npos ? BLACK : WHITE);

		// 勝率が行って一以下の指し手を削除する。
		// book_move.num_win / count < minimum_winning_percentage / 100
		if (book_move.num_win * 100 < minimum_winning_percentage * count) {
			continue;
		}

		// 評価値が行って一以下の指し手を削除する。
		if (count > 0 && ((color == BLACK && value < black_minimum_value) || (color == WHITE && value < white_minimum_value))) {
			continue;
		}

		auto& position = Threads[0]->rootPos;
		StateInfo state_info;
		position.set(sfen, &state_info, Threads[0]);
		auto move32 = position.to_move(move);
		if (!position.pseudo_legal(move32) || !position.legal(move32)) {
			sync_cout << "Illegal move. sfen=" << position.sfen() << " move=" << move32 << sync_endl;
			continue;
		}

		output_book.insert(sfen, Book::BookMove(move, ponder, value, 0, count));
	}

	WriteBook(output_book, output_book_file);

	return true;
}

#endif
