import numpy as np
import pandas as pd


LABEL_MAP = ["NFN", "NFH", "FNH", "FAH"]


def load_rss_file(rss_file_path):
    data = pd.read_csv(rss_file_path)

    if "rss" not in data.columns:
        raise ValueError("RSS file must contain a column named 'rss'.")

    return data["rss"].values


def load_feature_file(file_path, prefix):
    data = pd.read_csv(file_path)

    feature_columns = [col for col in data.columns if col.startswith(prefix)]

    if len(feature_columns) == 0:
        raise ValueError(f"The file must contain columns starting with '{prefix}'.")

    return data[feature_columns].values


class AttentionFusion:
    def feature_score(self, feature):
        return np.mean(np.abs(feature))

    def compute_attention_weights(self, rss_feature, csi_feature, corr_feature):
        rss_score = self.feature_score(rss_feature)
        csi_score = self.feature_score(csi_feature)
        corr_score = self.feature_score(corr_feature)

        scores = np.array([rss_score, csi_score, corr_score])

        exp_scores = np.exp(scores - np.max(scores))
        weights = exp_scores / np.sum(exp_scores)

        return weights

    def fuse(self, rss_feature, csi_feature, corr_feature):
        weights = self.compute_attention_weights(
            rss_feature,
            csi_feature,
            corr_feature
        )

        weighted_rss = weights[0] * rss_feature
        weighted_csi = weights[1] * csi_feature
        weighted_corr = weights[2] * corr_feature

        fused_feature = np.concatenate([
            weighted_rss.flatten(),
            weighted_csi.flatten(),
            weighted_corr.flatten()
        ])

        return fused_feature, weights


def extract_statistical_feature(data):
    data = np.asarray(data, dtype=float)

    if data.ndim == 1:
        return np.array([
            np.mean(data),
            np.std(data),
            np.max(data),
            np.min(data)
        ])

    return np.concatenate([
        np.mean(data, axis=0),
        np.std(data, axis=0),
        np.max(data, axis=0),
        np.min(data, axis=0)
    ])


def extract_attention_feature(rss_data, csi_data, corr_data):
    rss_feature = extract_statistical_feature(rss_data)
    csi_feature = extract_statistical_feature(csi_data)
    corr_feature = extract_statistical_feature(corr_data)

    fusion_model = AttentionFusion()

    fused_feature, attention_weights = fusion_model.fuse(
        rss_feature,
        csi_feature,
        corr_feature
    )

    return fused_feature, attention_weights


def process_four_classes():
    all_features = []

    for label in LABEL_MAP:
        print(f"\nInput files for class: {label}")

        rss_file_path = input("Enter RSS CSV file path: ")
        csi_file_path = input("Enter CSI amplitude CSV file path: ")
        corr_file_path = input("Enter Correlation CSV file path: ")

        rss_data = load_rss_file(rss_file_path)
        csi_data = load_feature_file(csi_file_path, "csi_")
        corr_data = load_feature_file(corr_file_path, "corr_")

        fused_feature, attention_weights = extract_attention_feature(
            rss_data,
            csi_data,
            corr_data
        )

        feature_row = {"label": label}

        for i, value in enumerate(fused_feature):
            feature_row[f"f_{i:03d}"] = value

        all_features.append(feature_row)

        print(f"Finished processing class: {label}")
        print("Attention weights:", attention_weights)

    output_data = pd.DataFrame(all_features)
    output_data.to_csv("edge_impulse_attention_features.csv", index=False)

    print("\nOutput file saved as: edge_impulse_attention_features.csv")


if __name__ == "__main__":
    process_four_classes()
