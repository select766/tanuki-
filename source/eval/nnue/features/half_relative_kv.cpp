// NNUE評価関数の入力特徴量HalfRelativeKVの定義

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "half_relative_kv.h"
#include "index_list.h"

namespace Eval {

namespace NNUE {

namespace Features {

// 玉の位置とBonaPieceから特徴量のインデックスを求める
template <Side AssociatedKing>
inline IndexType HalfRelativeKV<AssociatedKing>::MakeIndex(
    Square sq_k, Square square_vacant) {
  constexpr IndexType W = kBoardWidth;
  constexpr IndexType H = kBoardHeight;
  const IndexType relative_file = file_of(square_vacant) - file_of(sq_k) + (W / 2);
  const IndexType relative_rank = rank_of(square_vacant) - rank_of(sq_k) + (H / 2);
  return H * relative_file + relative_rank;
}

// 駒の情報を取得する
template <Side AssociatedKing>
inline void HalfRelativeKV<AssociatedKing>::GetPieces(
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
void HalfRelativeKV<AssociatedKing>::AppendActiveIndices(
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
          active->push_back(MakeIndex(
              sq_target_k, Inv(sq)));
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
void HalfRelativeKV<AssociatedKing>::AppendChangedIndices(
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

template class HalfRelativeKV<Side::kFriend>;
template class HalfRelativeKV<Side::kEnemy>;

}  // namespace Features

}  // namespace NNUE

}  // namespace Eval

#endif  // defined(EVAL_NNUE)
