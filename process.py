import numpy as np
import pandas as pd


LABEL_MAP = ["NFN", "NFH", "FNH", "FAH"]


def load_rss_file(rss_file_path):
    data = pd.read_csv(rss_file_path)

    if "rss" not in data.columns:
        raise ValueError("RSS file must contain a column named 'rss'.")

    return data["rss"].values


def load_csi_file(csi_file_path):
    data = pd.read_csv(csi_file_path)

    csi_columns = [col for col in data.columns if col.startswith("csi_")]

    if len(csi_columns) == 0:
        raise ValueError("CSI file must contain columns such as csi_0, csi_1, ...")

    return data[csi_columns].values


class KalmanFilter1D:
    def __init__(self, process_noise=1e-5, measurement_noise=1e-2):
        self.process_noise = process_noise
        self.measurement_noise = measurement_noise

    def filter(self, data):
        data = np.asarray(data, dtype=float)

        estimated_value = data[0]
        estimation_error = 1.0
        filtered_data = []

        for measurement in data:
            prediction = estimated_value
            prediction_error = estimation_error + self.process_noise

            kalman_gain = prediction_error / (
                prediction_error + self.measurement_noise
            )

            estimated_value = prediction + kalman_gain * (
                measurement - prediction
            )

            estimation_error = (1 - kalman_gain) * prediction_error
            filtered_data.append(estimated_value)

        return np.array(filtered_data)


class SubcarrierSelection:
    def __init__(self, M, N):
        self.M = M
        self.N = N

    def calculate_snr(self, data):
        mean_values = np.mean(data, axis=0)
        std_values = np.std(data, axis=0) + 1e-8

        return 20 * np.log10(np.abs(mean_values) / std_values)

    def select_amplitude_subcarriers(self, csi_amplitude):
        snr_values = self.calculate_snr(csi_amplitude)

        selected_indices = np.argsort(snr_values)[-self.M:]
        selected_indices = np.sort(selected_indices)

        selected_amplitude = csi_amplitude[:, selected_indices]

        return selected_amplitude, selected_indices

    def generate_correlation_data(self, selected_amplitude):
        correlation_data = []
        pair_indices = []

        num_subcarriers = selected_amplitude.shape[1]

        for i in range(num_subcarriers):
            for j in range(i + 1, num_subcarriers):
                difference = selected_amplitude[:, i] - selected_amplitude[:, j]
                correlation_data.append(difference)
                pair_indices.append((i, j))

        correlation_data = np.array(correlation_data).T

        return correlation_data, pair_indices

    def select_correlation_data(self, correlation_data, pair_indices):
        snr_values = self.calculate_snr(correlation_data)

        selected_indices = np.argsort(snr_values)[-self.N:]
        selected_indices = np.sort(selected_indices)

        selected_correlation = correlation_data[:, selected_indices]
        selected_pairs = [pair_indices[index] for index in selected_indices]

        return selected_correlation, selected_pairs


class AttentionFusion:
    def feature_score(self, feature):
        return np.mean(np.abs(feature))

    def compute_attention_weights(self, rss_feature, amp_feature, corr_feature):
        rss_score = self.feature_score(rss_feature)
        amp_score = self.feature_score(amp_feature)
        corr_score = self.feature_score(corr_feature)

        scores = np.array([rss_score, amp_score, corr_score])

        exp_scores = np.exp(scores - np.max(scores))
        weights = exp_scores / np.sum(exp_scores)

        return weights

    def fuse(self, rss_feature, amp_feature, corr_feature):
        weights = self.compute_attention_weights(
            rss_feature,
            amp_feature,
            corr_feature
        )

        weighted_rss = weights[0] * rss_feature
        weighted_amp = weights[1] * amp_feature
        weighted_corr = weights[2] * corr_feature

        fused_feature = np.concatenate([
            weighted_rss.flatten(),
            weighted_amp.flatten(),
            weighted_corr.flatten()
        ])

        return fused_feature, weights


def extract_feature_vector(rss_data, csi_amplitude, M, N):
    if len(rss_data) != csi_amplitude.shape[0]:
        raise ValueError("RSS data length must match CSI data sample length.")

    kalman_filter = KalmanFilter1D()

    filtered_rss = kalman_filter.filter(rss_data)

    filtered_csi = np.zeros_like(csi_amplitude, dtype=float)

    for i in range(csi_amplitude.shape[1]):
        filtered_csi[:, i] = kalman_filter.filter(csi_amplitude[:, i])

    selector = SubcarrierSelection(M=M, N=N)

    selected_amplitude, selected_amp_indices = selector.select_amplitude_subcarriers(
        filtered_csi
    )

    correlation_data, pair_indices = selector.generate_correlation_data(
        selected_amplitude
    )

    if N > correlation_data.shape[1]:
        raise ValueError("N is larger than the available number of correlation pairs.")

    filtered_correlation = np.zeros_like(correlation_data, dtype=float)

    for i in range(correlation_data.shape[1]):
        filtered_correlation[:, i] = kalman_filter.filter(
            correlation_data[:, i]
        )

    selected_correlation, selected_corr_pairs = selector.select_correlation_data(
        filtered_correlation,
        pair_indices
    )

    rss_feature = np.array([
        np.mean(filtered_rss),
        np.std(filtered_rss)
    ])

    amp_feature = np.concatenate([
        np.mean(selected_amplitude, axis=0),
        np.std(selected_amplitude, axis=0)
    ])

    corr_feature = np.concatenate([
        np.mean(selected_correlation, axis=0),
        np.std(selected_correlation, axis=0)
    ])

    fusion_model = AttentionFusion()

    fused_feature, attention_weights = fusion_model.fuse(
        rss_feature,
        amp_feature,
        corr_feature
    )

    return fused_feature, attention_weights, selected_amp_indices, selected_corr_pairs


def process_four_classes(M, N):
    all_features = []

    for label in LABEL_MAP:
        print(f"\nInput files for class: {label}")

        rss_file_path = input("Enter RSS CSV file path: ")
        csi_file_path = input("Enter CSI amplitude CSV file path: ")

        rss_data = load_rss_file(rss_file_path)
        csi_amplitude = load_csi_file(csi_file_path)

        fused_feature, attention_weights, amp_indices, corr_pairs = extract_feature_vector(
            rss_data=rss_data,
            csi_amplitude=csi_amplitude,
            M=M,
            N=N
        )

        feature_row = {"label": label}

        for i, value in enumerate(fused_feature):
            feature_row[f"f_{i:03d}"] = value

        all_features.append(feature_row)

        print(f"Finished processing class: {label}")
        print("Attention weights:", attention_weights)
        print("Selected CSI subcarriers:", amp_indices)
        print("Selected correlation pairs:", corr_pairs)

    output_data = pd.DataFrame(all_features)
    output_data.to_csv("edge_impulse_fused_features.csv", index=False)

    print("\nOutput file saved as: edge_impulse_fused_features.csv")


if __name__ == "__main__":
    M = int(input("Enter M: "))
    N = int(input("Enter N: "))

    if M <= 1:
        raise ValueError("M must be greater than 1.")

    max_correlation_pairs = M * (M - 1) // 2

    if N > max_correlation_pairs:
        raise ValueError(
            f"N cannot be larger than the maximum number of correlation pairs: {max_correlation_pairs}"
        )

    process_four_classes(M, N)
