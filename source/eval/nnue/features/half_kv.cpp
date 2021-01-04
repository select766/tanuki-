// NNUE評価関数の入力特徴量HalfKVの定義

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "half_kv.h"
#include "index_list.h"

namespace Eval {

namespace NNUE {

namespace Features {

// 玉の位置とBonaPieceから特徴量のインデックスを求める
template <Side AssociatedKing>
inline IndexType HalfKV<AssociatedKing>::MakeIndex(Square sq_k, Square sq_vacant) {
  return static_cast<IndexType>(SQ_NB) * static_cast<IndexType>(sq_k) + sq_vacant;
}

// 駒の情報を取得する
template <Side AssociatedKing>
inline void HalfKV<AssociatedKing>::GetPieces(
    const Position& pos, Color perspective,
    BonaPiece** pieces, Square* sq_target_k) {
  *pieces = (perspective == BLACK) ?
      pos.eval_list()->piece_list_fb() :
      pos.eval_list()->piece_list_fw();
  const PieceNumber target = (AssociatedKing == Side::kFriend) ?
      static_cast<PieceNumber>(PIECE_NUMBER_KING + perspective) :
      static_cast<PieceNumber>(PIECE_NUMBER_KING + ~perspective);
  *sq_target_k = static_cast<Square>(((*pieces)[target] - f_king) % SQ_NB);
}

// 特徴量のうち、値が1であるインデックスのリストを取得する
template <Side AssociatedKing>
void HalfKV<AssociatedKing>::AppendActiveIndices(
    const Position& pos, Color perspective, IndexList* active) {
  // コンパイラの警告を回避するため、配列サイズが小さい場合は何もしない
  if (RawFeatures::kMaxActiveDimensions < kMaxActiveDimensions) return;

  BonaPiece* pieces;
  Square sq_target_k;
  GetPieces(pos, perspective, &pieces, &sq_target_k);

  for (Square sq = SQ_11; sq < SQ_NB; ++sq) {
      if (pos.piece_on(sq) != Piece::NO_PIECE) {
          continue;
      }
      if (perspective == BLACK) {
          active->push_back(MakeIndex(sq_target_k, sq));
      }
      else {
          active->push_back(MakeIndex(sq_target_k, Inv(sq)));
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
template <Side AssociatedKing>
void HalfKV<AssociatedKing>::AppendChangedIndices(
    const Position& pos, Color perspective,
    IndexList* removed, IndexList* added) {
  BonaPiece* pieces;
  Square sq_target_k;
  GetPieces(pos, perspective, &pieces, &sq_target_k);
  const auto& dp = pos.state()->dirtyPiece;

  if (dp.dirty_num == 1) {
      if (dp.changed_piece[0].old_piece.from[perspective] >= fe_hand_end) {
          // 駒を移動する手
          const auto old_p = static_cast<BonaPiece>(
              dp.changed_piece[0].old_piece.from[perspective]);
          const auto new_p = static_cast<BonaPiece>(
              dp.changed_piece[0].new_piece.from[perspective]);

          // 移動元のマスが空く
          added->push_back(MakeIndex(sq_target_k, ToSquare(old_p)));

          // 移動先のマスが占領される
          removed->push_back(MakeIndex(sq_target_k, ToSquare(new_p)));
      }
      else {
          // 駒を打つ手
          const auto new_p = static_cast<BonaPiece>(
              dp.changed_piece[0].new_piece.from[perspective]);

          // 移動先のマスが占領される
          removed->push_back(MakeIndex(sq_target_k, ToSquare(new_p)));
      }
  }
  else if (dp.dirty_num == 2) {
      // 駒を取る手
      const auto old_p = static_cast<BonaPiece>(
          dp.changed_piece[0].old_piece.from[perspective]);

      // 移動元のマスが空く
      added->push_back(MakeIndex(sq_target_k, ToSquare(old_p)));
  }
}

template class HalfKV<Side::kFriend>;
template class HalfKV<Side::kEnemy>;

}  // namespace Features

}  // namespace NNUE

}  // namespace Eval

#endif  // defined(EVAL_NNUE)
