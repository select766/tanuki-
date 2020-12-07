// NNUE評価関数の特徴量変換クラステンプレートのHalfKA用特殊化

#ifndef _NNUE_TRAINER_FEATURES_FACTORIZER_HALF_KA_H_
#define _NNUE_TRAINER_FEATURES_FACTORIZER_HALF_KA_H_

#include "../../../../config.h"

#if defined(EVAL_NNUE)

#include "../../features/half_ka.h"
#include "../../features/a.h"
#include "../../features/half_relative_ka.h"
#include "factorizer.h"

namespace Eval {

namespace NNUE {

namespace Features {

// 入力特徴量を学習用特徴量に変換するクラステンプレート
// HalfKA用特殊化
template <Side AssociatedKing>
class Factorizer<HalfKA<AssociatedKing>> {
 private:
  using FeatureType = HalfKA<AssociatedKing>;

  // 特徴量のうち、同時に値が1となるインデックスの数の最大値
  static constexpr IndexType kMaxActiveDimensions =
      FeatureType::kMaxActiveDimensions;

  // 学習用特徴量の種類
  enum TrainingFeatureType {
    kFeaturesHalfKA,
    kFeaturesHalfK,
    kFeaturesA,
    kFeaturesHalfRelativeKA,
    kNumTrainingFeatureTypes,
  };

  // 学習用特徴量の情報
  static constexpr FeatureProperties kProperties[] = {
    // kFeaturesHalfKA
    {true, FeatureType::kDimensions},
    // kFeaturesHalfK
    {true, SQ_NB},
    // kFeaturesA
    {true, Factorizer<A>::GetDimensions()},
    // kFeaturesHalfRelativeKA
    {true, Factorizer<HalfRelativeKA<AssociatedKing>>::GetDimensions()},
  };
  static_assert(GetArrayLength(kProperties) == kNumTrainingFeatureTypes, "");

 public:
  // 学習用特徴量の次元数を取得する
  static constexpr IndexType GetDimensions() {
    return GetActiveDimensions(kProperties);
  }

  // 学習用特徴量のインデックスと学習率のスケールを取得する
  static void AppendTrainingFeatures(
      IndexType base_index, std::vector<TrainingFeature>* training_features) {
    // kFeaturesHalfKA
    IndexType index_offset = AppendBaseFeature<FeatureType>(
        kProperties[kFeaturesHalfKA], base_index, training_features);

    const auto sq_k = static_cast<Square>(base_index / fe_end2);
    const auto p = static_cast<BonaPiece>(base_index % fe_end2);
    // kFeaturesHalfK
    {
      const auto& properties = kProperties[kFeaturesHalfK];
      if (properties.active) {
        training_features->emplace_back(index_offset + sq_k);
        index_offset += properties.dimensions;
      }
    }
    // kFeaturesA
    index_offset += InheritFeaturesIfRequired<A>(
        index_offset, kProperties[kFeaturesA], p, training_features);
    // kFeaturesHalfRelativeKA
    if (p >= fe_hand_end) {
      index_offset += InheritFeaturesIfRequired<HalfRelativeKA<AssociatedKing>>(
          index_offset, kProperties[kFeaturesHalfRelativeKA],
          HalfRelativeKA<AssociatedKing>::MakeIndex(sq_k, p),
          training_features);
    } else {
      index_offset += SkipFeatures(kProperties[kFeaturesHalfRelativeKA]);
    }

    ASSERT_LV5(index_offset == GetDimensions());
  }
};

template <Side AssociatedKing>
constexpr FeatureProperties Factorizer<HalfKA<AssociatedKing>>::kProperties[];

}  // namespace Features

}  // namespace NNUE

}  // namespace Eval

#endif  // defined(EVAL_NNUE)

#endif
