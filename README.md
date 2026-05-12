# Overview
  The proposed method is a lightweight NLoS fire detection system based on Wi-Fi sensing, designed for early fire warning in high-rise buildings. It jointly utilizes CSI and RSS from commercial Wi-Fi devices to capture fire-induced signal variations across walls and floors, while running entirely on a resource-constrained STM32 MCU. To improve robustness and efficiency, an SNR-based subcarrier selection strategy and a subcarrier-differencing correlation feature are introduced to suppress temperature drift and reduce computational cost. The overall processing framework of the proposed method is illustrated in the figure.
<img width="1635" height="786" alt="image" src="https://github.com/user-attachments/assets/98bd6695-ca60-4678-bf4c-c9297d26ed50" />
  As shown in the figure, this method mainly consists of three stages: data preprocessing, attention-based feature fusion, and edge deployment. First, an ESP32 device is used to collect RSS and CSI amplitude data under different environmental and operational states. Differential operations are then performed among CSI subcarriers to extract inter-subcarrier correlation features, thereby enhancing the representation of wireless channel variations. Subsequently, Kalman filtering is applied to the RSS, CSI amplitude, and correlation features to suppress environmental noise and random fluctuations. A subcarrier selection algorithm is further employed to retain the features that are more sensitive to target state recognition, reducing the impact of redundant information on model training.
  In the feature fusion stage, an attention mechanism is introduced to dynamically assign weights to RSS, CSI amplitude, and correlation features, enabling effective fusion of multidimensional wireless signal characteristics. Finally, the fused feature vectors are imported into the Edge Impulse platform, where an MLP model is deployed on the STM32 target platform to perform a four-class classification task. The model performance is evaluated in terms of classification accuracy, inference time, RAM usage, and Flash memory usage, thereby verifying the feasibility and real-time capability of this method on embedded edge devices.

# Highlights

- **Through-Obstacle Sensing**: Detects fires through walls, corridors, and across floors.
- **Strong Performance**: High accuracy, recall, and F1-score on a four-class joint fire-and-human classification task.
- **Drift-Resistant**: Maintains stable performance across varying temperature conditions via subcarrier differencing.
- **MCU-Friendly**: Compact feature design enables deployment on STM32.
- **Fire + Human Joint Recognition**: Distinguishes NFN, NFH, FNH, and FAH scenarios.

# Method

- **Dual-Signal Sensing**: Captures RSS and CSI amplitude from Wi-Fi signals.
- **Kalman Filtering**: Suppresses noise in raw signals.
- **SNR-Based Subcarrier Selection**: Ranks subcarriers by their SNR and keeps the most informative ones.
- **Drift Suppression**: Subcarrier differencing cancels the common drift term shared across subcarriers.
- **Attention Fusion + MLP**: An attention module adaptively weights RSS, amplitude, and correlation features; an MLP outputs the four classes via Softmax.

# Experimental Setup

- **Platform**: ESP32 + STM32, connected via UART.
- **Scenarios**: Through-wall, corridor, and cross-floor NLoS environments.
- **Baselines**: Outperforms representative traditional and lightweight deep learning baselines under the same MCU deployment.
- **Temperature Robustness**: Stable recognition performance under low-, normal-, and high-temperature conditions, with noticeable degradation when correlation features are removed.

# Advantages

- **Cross-Obstacle Coverage** beyond LoS sensors.
- **Lower Cost** by reusing existing Wi-Fi infrastructure.
- **Privacy-Preserving** device-free sensing.
- **Robust to Hardware Drift** under temperature changes.

# System Architecture

- **Acquisition**: ESP32 streams RSS + CSI to STM32.
- **Preprocessing**: Kalman filter + two-stage subcarrier/correlation selection.
- **Inference**: Attention-weighted feature fusion → MLP → four-class output, displayed on the onboard LCD.

# Conclusion

  The system delivers practical, low-cost, and robust early fire warning under NLoS conditions on a single MCU. Future work will focus on energy efficiency, distributed multi-node collaborative sensing, and integration with existing fire alarm and IoT safety platforms.


