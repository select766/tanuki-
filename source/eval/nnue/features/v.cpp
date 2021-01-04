// NNUE評価関数の入力特徴量Pの定義

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "v.h"
#include "index_list.h"

namespace Eval {

namespace NNUE {

namespace Features {

// 特徴量のうち、値が1であるインデックスのリストを取得する
void V::AppendActiveIndices(
    const Position& pos, Color perspective, IndexList* active) {
  // コンパイラの警告を回避するため、配列サイズが小さい場合は何もしない
  if (RawFeatures::kMaxActiveDimensions < kMaxActiveDimensions) return;

  for (Square sq = SQ_11; sq < SQ_NB; ++sq) {
	  if (pos.piece_on(sq) != Piece::NO_PIECE) {
		  continue;
	  }
	  if (perspective == BLACK) {
          active->push_back(sq);
      } else {
          active->push_back(Inv(sq));
      }
  }
}

namespace {
    /// <summary>
    /// 与えられたBonaPieceで表される駒が置かれているマスを返す。
    /// </summary>
    /// <param name="p">BonaPiece</param>
    /// <returns>マス</returns>
    Square ToSquare(BonaPiece p) {
        ASSERT_LV3(p >= fe_hand_end);
        return static_cast<Square>((p - fe_hand_end) % SQ_NB);
    }
}

// 特徴量のうち、一手前から値が変化したインデックスのリストを取得する
void V::AppendChangedIndices(
    const Position& pos, Color perspective,
    IndexList* removed, IndexList* added) {
  const auto& dp = pos.state()->dirtyPiece;

  if (dp.dirty_num == 1) {
      if (dp.changed_piece[0].old_piece.from[perspective] >= fe_hand_end) {
          // 駒を移動する手
          const auto old_p = static_cast<BonaPiece>(
              dp.changed_piece[0].old_piece.from[perspective]);
          const auto new_p = static_cast<BonaPiece>(
              dp.changed_piece[0].new_piece.from[perspective]);

          // 移動元のマスが空く
          added->push_back(ToSquare(old_p));

          // 移動先のマスが占領される
          removed->push_back(ToSquare(new_p));
      }
      else {
          // 駒を打つ手
          const auto new_p = static_cast<BonaPiece>(
              dp.changed_piece[0].new_piece.from[perspective]);

          // 移動先のマスが占領される
          removed->push_back(ToSquare(new_p));
      }
  }
  else if (dp.dirty_num == 2) {
      // 駒を取る手
      const auto old_p = static_cast<BonaPiece>(
          dp.changed_piece[0].old_piece.from[perspective]);

      // 移動元のマスが空く
      added->push_back(ToSquare(old_p));
  }
}

}  // namespace Features

}  // namespace NNUE

}  // namespace Eval

#endif  // defined(EVAL_NNUE)
