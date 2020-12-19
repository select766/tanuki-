// NNUE評価関数の入力特徴量HalfRelativeKAVの定義

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "half_relative_kav.h"
#include "index_list.h"

namespace Eval {

namespace NNUE {

namespace Features {

// 玉の位置とBonaPieceから特徴量のインデックスを求める
template <Side AssociatedKing>
inline IndexType HalfRelativeKAV<AssociatedKing>::MakeIndex(
    Square sq_k, BonaPiece p) {
  constexpr IndexType W = kBoardWidth;
  constexpr IndexType H = kBoardHeight;
  const IndexType piece_index = (p - fe_hand_end) / SQ_NB;
  const Square sq_p = static_cast<Square>((p - fe_hand_end) % SQ_NB);
  const IndexType relative_file = file_of(sq_p) - file_of(sq_k) + (W / 2);
  const IndexType relative_rank = rank_of(sq_p) - rank_of(sq_k) + (H / 2);
  return H * W * piece_index + H * relative_file + relative_rank;
}

// 駒の情報を取得する
template <Side AssociatedKing>
inline void HalfRelativeKAV<AssociatedKing>::GetPieces(
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
void HalfRelativeKAV<AssociatedKing>::AppendActiveIndices(
    const Position& pos, Color perspective, IndexList* active) {
  // コンパイラの警告を回避するため、配列サイズが小さい場合は何もしない
  if (RawFeatures::kMaxActiveDimensions < kMaxActiveDimensions) return;

  BonaPiece* pieces;
  Square sq_target_k;
  GetPieces(pos, perspective, &pieces, &sq_target_k);
  for (PieceNumber i = PIECE_NUMBER_ZERO; i < PIECE_NUMBER_NB; ++i) {
    if (pieces[i] >= fe_hand_end) {
      active->push_back(MakeIndex(sq_target_k, pieces[i]));
    }
  }
  for (Square sq = SQ_11; sq < SQ_NB; ++sq) {
      if (pos.piece_on(sq) != Piece::NO_PIECE) {
          continue;
      }
      if (perspective == BLACK) {
          active->push_back(MakeIndex(
              sq_target_k, static_cast<BonaPiece>(fe_end2 + sq)));
      }
      else {
          active->push_back(MakeIndex(
              sq_target_k, static_cast<BonaPiece>(fe_end2 + Inv(sq))));
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

    /// <summary>
    /// 与えられたマスに対応するBonaPieceを返す。
    /// </summary>
    /// <param name="sq">マス</param>
    /// <returns>BonaPiece</returns>
    BonaPiece ToBonaPiece(Square sq) {
        return static_cast<BonaPiece>(fe_end2 + sq);
    }
}

// 特徴量のうち、一手前から値が変化したインデックスのリストを取得する
template <Side AssociatedKing>
void HalfRelativeKAV<AssociatedKing>::AppendChangedIndices(
    const Position& pos, Color perspective,
    IndexList* removed, IndexList* added) {
  BonaPiece* pieces;
  Square sq_target_k;
  GetPieces(pos, perspective, &pieces, &sq_target_k);
  const auto& dp = pos.state()->dirtyPiece;
  for (int i = 0; i < dp.dirty_num; ++i) {
    const auto old_p = static_cast<BonaPiece>(
        dp.changed_piece[i].old_piece.from[perspective]);
    if (old_p >= fe_hand_end) {
      removed->push_back(MakeIndex(sq_target_k, old_p));
    }
    const auto new_p = static_cast<BonaPiece>(
        dp.changed_piece[i].new_piece.from[perspective]);
    if (new_p >= fe_hand_end) {
      added->push_back(MakeIndex(sq_target_k, new_p));
    }
  }

  if (dp.dirty_num == 1) {
      if (dp.changed_piece[0].old_piece.from[perspective] >= fe_hand_end) {
          // 駒を移動する手
          const auto old_p = static_cast<BonaPiece>(
              dp.changed_piece[0].old_piece.from[perspective]);
          const auto new_p = static_cast<BonaPiece>(
              dp.changed_piece[0].new_piece.from[perspective]);

          // 移動元のマスが空く
          added->push_back(MakeIndex(sq_target_k, ToBonaPiece(ToSquare(old_p))));

          // 移動先のマスが占領される
          removed->push_back(MakeIndex(sq_target_k, ToBonaPiece(ToSquare(new_p))));
      }
      else {
          // 駒を打つ手
          const auto new_p = static_cast<BonaPiece>(
              dp.changed_piece[0].new_piece.from[perspective]);

          // 移動先のマスが占領される
          removed->push_back(MakeIndex(sq_target_k, ToBonaPiece(ToSquare(new_p))));
      }
  }
  else if (dp.dirty_num == 2) {
      // 駒を取る手
      const auto old_p = static_cast<BonaPiece>(
          dp.changed_piece[0].old_piece.from[perspective]);

      // 移動元のマスが空く
      added->push_back(MakeIndex(sq_target_k, ToBonaPiece(ToSquare(old_p))));
  }
}

template class HalfRelativeKAV<Side::kFriend>;
template class HalfRelativeKAV<Side::kEnemy>;

}  // namespace Features

}  // namespace NNUE

}  // namespace Eval

#endif  // defined(EVAL_NNUE)
