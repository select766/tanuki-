// NNUE評価関数の入力特徴量Vの定義

#ifndef _NNUE_FEATURES_V_H_
#define _NNUE_FEATURES_V_H_

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../../../evaluate.h"
#include "features_common.h"

namespace Eval {

namespace NNUE {

namespace Features {

// 特徴量V：駒のBonaPieceと空きマス (A:All Pieces V:Vacant)
class V {
 public:
  // 特徴量名
  static constexpr const char* kName = "V";
  // 評価関数ファイルに埋め込むハッシュ値
  static constexpr std::uint32_t kHashValue = 0x659F8C7Bu;
  // 特徴量の次元数
  static constexpr IndexType kDimensions = SQ_NB;
  // 特徴量のうち、同時に値が1となるインデックスの数の最大値
  static constexpr IndexType kMaxActiveDimensions = SQ_NB;
  // 差分計算の代わりに全計算を行うタイミング
  static constexpr TriggerEvent kRefreshTrigger = TriggerEvent::kNone;

  // 特徴量のうち、値が1であるインデックスのリストを取得する
  static void AppendActiveIndices(const Position& pos, Color perspective,
                                  IndexList* active);

  // 特徴量のうち、一手前から値が変化したインデックスのリストを取得する
  static void AppendChangedIndices(const Position& pos, Color perspective,
                                   IndexList* removed, IndexList* added);
};

}  // namespace Features

}  // namespace NNUE

}  // namespace Eval

#endif  // defined(EVAL_NNUE)

#endif
