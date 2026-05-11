# Overview

A lightweight NLoS fire detection system based on Wi-Fi sensing, designed for early fire warning in high-rise buildings. It jointly uses CSI and RSS from commercial Wi-Fi to perceive fire-induced signal variations across walls and floors, and runs entirely on a resource-constrained STM32 MCU. An SNR-based subcarrier selection strategy and a subcarrier differencing correlation feature are introduced to suppress temperature drift and reduce computational cost.

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

# Experimrntal Setup

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


