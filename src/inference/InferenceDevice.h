#pragma once

/**
 * @brief Backend di inferenza richiesto per i modelli ONNX.
 *
 * Il valore viene passato a detector e recognizer. Le classi di inferenza
 * provano a usare il device richiesto e mantengono anche il device effettivo,
 * perche CUDA puo non essere disponibile a runtime o in fase di build.
 */
enum class InferenceDevice {
    /** Inferenza eseguita su CPU. */
    CPU,
    /** Inferenza richiesta su CUDA, con fallback interno a CPU se non disponibile. */
    CUDA
};

inline const char* inferenceDeviceName(InferenceDevice device) {
    return device == InferenceDevice::CUDA ? "CUDA" : "CPU";
}
