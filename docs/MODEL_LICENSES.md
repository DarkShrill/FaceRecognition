# License Report For Models In `models/`

Verification date: 2026-06-22.

## Summary

The files in `models/` have names and sizes compatible with InsightFace model
zoo packs:

- `det_500m.onnx` and `face_detection.onnx` have the same size: likely duplicate
  copies of the SCRFD 500M detector.
- `w600k_mbf.onnx` and `face_recognition.onnx` have the same size: likely
  duplicate copies of the MobileFaceNet/WebFace600K recognizer.
- `2d106det.onnx` and `face_landmark_2d_106.onnx` have the same size: likely
  duplicate copies of the 2D 106-landmark model.
- `1k3d68.onnx` and `genderage.onnx` match names found in InsightFace packs.

The practical consequence is important:

- InsightFace code is declared MIT;
- models and training/annotation data are declared usable only for
  non-commercial research unless a specific license exists;
- for commercial use or product distribution, replace the models with assets
  that have an explicit license, or contact InsightFace for licensing.

Sources checked:

- InsightFace README, License section: https://github.com/deepinsight/insightface
- InsightFace model zoo: https://github.com/deepinsight/insightface/tree/master/model_zoo

## Local File Map

| Local file | Project use | Likely origin | Prudent license status |
| --- | --- | --- | --- |
| `det_500m.onnx` | detector used by `FaceRecognitionController` | SCRFD 500M / InsightFace pack | Non-commercial/research unless explicitly licensed |
| `face_detection.onnx` | not used directly, duplicate by size | SCRFD 500M / InsightFace pack | Non-commercial/research unless explicitly licensed |
| `w600k_mbf.onnx` | recognizer used by `FaceRecognitionController` | MobileFaceNet WebFace600K / buffalo_s or related pack | Non-commercial/research unless explicitly licensed |
| `face_recognition.onnx` | not used directly, duplicate by size | MobileFaceNet WebFace600K / buffalo_s or related pack | Non-commercial/research unless explicitly licensed |
| `face_landmark_2d_106.onnx` | 106-landmark model used by the pipeline when requested | 2D 106 landmark / InsightFace | Non-commercial/research unless explicitly licensed |
| `2d106det.onnx` | not used directly, duplicate by size | 2D 106 landmark / InsightFace | Non-commercial/research unless explicitly licensed |
| `1k3d68.onnx` | 3D 68-landmark model used by the pipeline when requested | 3D 68 landmark / InsightFace pack | Non-commercial/research unless explicitly licensed |
| `genderage.onnx` | not used directly | gender/age attributes / InsightFace pack | Non-commercial/research unless explicitly licensed |

## What The Upstream Source Says

The main InsightFace README distinguishes between:

- code: MIT, without academic or commercial limitations;
- training data with annotations and models trained on that data: non-commercial
  research only;
- models downloaded manually from the repository or automatically through the
  Python library: same non-commercial policy.

The model zoo also states that all models are available only for non-commercial
research.

## Recommendation For `models/`

To turn this project into a distributable library, use this rule:

1. Do not include these `.onnx` files in commercial releases without an explicit
   license.
2. Keep `models/` out of the public package, or make it a folder configurable by
   the host application.
3. Add a `MODEL_MANIFEST.md` or `models.json` next to the models with:
   - file name;
   - origin/download URL;
   - license;
   - allowed use;
   - download date;
   - SHA256 hash.
4. If you want to support multiple providers, pass paths to
   `FaceRecognitionEngine::initialize(...)` instead of hardcoding them.
5. For commercial use, choose models with a clear commercial license or obtain a
   license from InsightFace.

## Unused Model Status

Today the code uses only:

- `models/det_500m.onnx`;
- `models/face_landmark_2d_106.onnx`;
- `models/1k3d68.onnx`;
- `models/w600k_mbf.onnx`.
