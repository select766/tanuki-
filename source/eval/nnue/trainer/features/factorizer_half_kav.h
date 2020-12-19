// NNUE評価関数の特徴量変換クラステンプレートのHalfKAV用特殊化

#ifndef _NNUE_TRAINER_FEATURES_FACTORIZER_HALF_KA_H_
#define _NNUE_TRAINER_FEATURES_FACTORIZER_HALF_KA_H_

#include "../../../../config.h"

#if defined(EVAL_NNUE)

#include "../../features/half_kav.h"
#include "../../features/av.h"
#include "../../features/half_relative_kav.h"
#include "factorizer.h"

namespace Eval {

namespace NNUE {

namespace Features {

// 入力特徴量を学習用特徴量に変換するクラステンプレート
// HalfKAV用特殊化
template <Side AssociatedKing>
class Factorizer<HalfKAV<AssociatedKing>> {
 private:
  using FeatureType = HalfKAV<AssociatedKing>;

  // 特徴量のうち、同時に値が1となるインデックスの数の最大値
  static constexpr IndexType kMaxActiveDimensions =
      FeatureType::kMaxActiveDimensions;

  // 学習用特徴量の種類
  enum TrainingFeatureType {
    kFeaturesHalfKAV,
    kFeaturesHalfK,
    kFeaturesAV,
    kFeaturesHalfRelativeKAV,
    kNumTrainingFeatureTypes,
  };

  // 学習用特徴量の情報
  static constexpr FeatureProperties kProperties[] = {
    // kFeaturesHalfKAV
    {true, FeatureType::kDimensions},
    // kFeaturesHalfK
    {true, SQ_NB},
    // kFeaturesAV
    {true, Factorizer<AV>::GetDimensions()},
    // kFeaturesHalfRelativeKAV
    {true, Factorizer<HalfRelativeKAV<AssociatedKing>>::GetDimensions()},
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
    // kFeaturesHalfKAV
    IndexType index_offset = AppendBaseFeature<FeatureType>(
        kProperties[kFeaturesHalfKAV], base_index, training_features);

    const auto sq_k = static_cast<Square>(base_index / fe_end3);
    const auto p = static_cast<BonaPiece>(base_index % fe_end3);
    // kFeaturesHalfK
    {
      const auto& properties = kProperties[kFeaturesHalfK];
      if (properties.active) {
        training_features->emplace_back(index_offset + sq_k);
        index_offset += properties.dimensions;
      }
    }
    // kFeaturesAV
    index_offset += InheritFeaturesIfRequired<AV>(
        index_offset, kProperties[kFeaturesAV], p, training_features);
    // kFeaturesHalfRelativeKAV
    if (p >= fe_hand_end) {
      index_offset += InheritFeaturesIfRequired<HalfRelativeKAV<AssociatedKing>>(
          index_offset, kProperties[kFeaturesHalfRelativeKAV],
          HalfRelativeKAV<AssociatedKing>::MakeIndex(sq_k, p),
          training_features);
    } else {
      index_offset += SkipFeatures(kProperties[kFeaturesHalfRelativeKAV]);
    }

    ASSERT_LV5(index_offset == GetDimensions());
  }
};

template <Side AssociatedKing>
constexpr FeatureProperties Factorizer<HalfKAV<AssociatedKing>>::kProperties[];

}  // namespace Features

}  // namespace NNUE

}  // namespace Eval

#endif  // defined(EVAL_NNUE)

#endif
