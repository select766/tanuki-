// NNUE評価関数の特徴量変換クラステンプレートのHalfKV用特殊化

#ifndef _NNUE_TRAINER_FEATURES_FACTORIZER_HALF_KV_H_
#define _NNUE_TRAINER_FEATURES_FACTORIZER_HALF_KV_H_

#include "../../../../config.h"

#if defined(EVAL_NNUE)

#include "../../features/half_kv.h"
#include "../../features/v.h"
#include "../../features/half_relative_kv.h"
#include "factorizer.h"

namespace Eval {

namespace NNUE {

namespace Features {

// 入力特徴量を学習用特徴量に変換するクラステンプレート
// HalfKV用特殊化
template <Side AssociatedKing>
class Factorizer<HalfKV<AssociatedKing>> {
 private:
  using FeatureType = HalfKV<AssociatedKing>;

  // 特徴量のうち、同時に値が1となるインデックスの数の最大値
  static constexpr IndexType kMaxActiveDimensions =
      FeatureType::kMaxActiveDimensions;

  // 学習用特徴量の種類
  enum TrainingFeatureType {
    kFeaturesHalfKV,
    kFeaturesHalfK,
    kFeaturesV,
    kFeaturesHalfRelativeKV,
    kNumTrainingFeatureTypes,
  };

  // 学習用特徴量の情報
  static constexpr FeatureProperties kProperties[] = {
    // kFeaturesHalfKV
    {true, FeatureType::kDimensions},
    // kFeaturesHalfK
    {true, SQ_NB},
    // kFeaturesV
    {true, Factorizer<V>::GetDimensions()},
    // kFeaturesHalfRelativeKV
    {true, Factorizer<HalfRelativeKV<AssociatedKing>>::GetDimensions()},
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
    // kFeaturesHalfKV
    IndexType index_offset = AppendBaseFeature<FeatureType>(
        kProperties[kFeaturesHalfKV], base_index, training_features);

    const auto sq_k = static_cast<Square>(base_index / SQ_NB);
    const auto sq_vacant = static_cast<Square>(base_index % SQ_NB);
    // kFeaturesHalfK
    {
      const auto& properties = kProperties[kFeaturesHalfK];
      if (properties.active) {
        training_features->emplace_back(index_offset + sq_k);
        index_offset += properties.dimensions;
      }
    }
    // kFeaturesV
    index_offset += InheritFeaturesIfRequired<V>(
        index_offset, kProperties[kFeaturesV], sq_vacant, training_features);
    // kFeaturesHalfRelativeKV
    index_offset += InheritFeaturesIfRequired<HalfRelativeKV<AssociatedKing>>(
        index_offset, kProperties[kFeaturesHalfRelativeKV],
        HalfRelativeKV<AssociatedKing>::MakeIndex(sq_k, sq_vacant),
        training_features);

    ASSERT_LV5(index_offset == GetDimensions());
  }
};

template <Side AssociatedKing>
constexpr FeatureProperties Factorizer<HalfKV<AssociatedKing>>::kProperties[];

}  // namespace Features

}  // namespace NNUE

}  // namespace Eval

#endif  // defined(EVAL_NNUE)

#endif
