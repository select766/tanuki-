// NNUE評価関数の入力特徴量HalfRelativeKAVの定義

#ifndef _NNUE_FEATURES_HALF_RELATIVE_KAV_H_
#define _NNUE_FEATURES_HALF_RELATIVE_KAV_H_

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../../../evaluate.h"
#include "features_common.h"

namespace Eval {

namespace NNUE {

namespace Features {

// 特徴量HalfRelativeKAV：自玉または敵玉を基準とした、玉以外の各駒の相対位置
template <Side AssociatedKing>
class HalfRelativeKAV {
 public:
  // 特徴量名
  static constexpr const char* kName = (AssociatedKing == Side::kFriend) ?
      "HalfRelativeKAV(Friend)" : "HalfRelativeKAV(Enemy)";
  // 評価関数ファイルに埋め込むハッシュ値
  static constexpr std::uint32_t kHashValue =
      0x26420D73u ^ (AssociatedKing == Side::kFriend);
  // 駒種
  static constexpr IndexType kNumPieceKinds = (fe_end3 - fe_hand_end) / SQ_NB;
  // 玉を中央に置いた仮想的な盤の幅
  static constexpr IndexType kBoardWidth = FILE_NB * 2 - 1;
  // 玉を中央に置いた仮想的な盤の高さ
  static constexpr IndexType kBoardHeight = RANK_NB * 2 - 1;
  // 特徴量の次元数
  static constexpr IndexType kDimensions =
      kNumPieceKinds * kBoardHeight * kBoardWidth;
  // 特徴量のうち、同時に値が1となるインデックスの数の最大値
  static constexpr IndexType kMaxActiveDimensions = PIECE_NUMBER_NB + SQ_NB;
  // 差分計算の代わりに全計算を行うタイミング
  static constexpr TriggerEvent kRefreshTrigger =
      (AssociatedKing == Side::kFriend) ?
      TriggerEvent::kFriendKingMoved : TriggerEvent::kEnemyKingMoved;

  // 特徴量のうち、値が1であるインデックスのリストを取得する
  static void AppendActiveIndices(const Position& pos, Color perspective,
                                  IndexList* active);

  // 特徴量のうち、一手前から値が変化したインデックスのリストを取得する
  static void AppendChangedIndices(const Position& pos, Color perspective,
                                   IndexList* removed, IndexList* added);

  // 玉の位置とBonaPieceから特徴量のインデックスを求める
  static IndexType MakeIndex(Square sq_k, BonaPiece p);

 private:
  // 駒の情報を取得する
  static void GetPieces(const Position& pos, Color perspective,
                        BonaPiece** pieces, Square* sq_target_k);
};

}  // namespace Features

}  // namespace NNUE

}  // namespace Eval

#endif  // defined(EVAL_NNUE)

#endif
