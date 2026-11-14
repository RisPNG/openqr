# Third-party notices

OpenQR compiles pinned native components into its shared library. Original copyright and licence notices are retained in vendored source and included in the installed SDK under `share/openqr/licenses`. The revisions and archive hashes in `cmake/Dependencies.cmake`, `cmake/Imaging.cmake`, and vendored origin records identify the sources used by the build.

This software is based in part on the work of the Independent JPEG Group.

| Component | Purpose | Licence notice |
| --- | --- | --- |
| Nayuki QR Code generator | QR encoding and semantic layout | Embedded MIT notices in `third_party/qrcodegen.*` |
| ZXing-C++ | Independent QR verification | Apache-2.0 |
| Blend2D | Raster geometry | zlib-style licence |
| libpng | PNG and APNG | libpng-2.0 |
| zlib | PNG compression | zlib |
| libjpeg-turbo | JPEG decoding | libjpeg-turbo, IJG, and included BSD notices |
| libwebp | WebP decoding and animation | BSD-3-Clause and patent notice |
| giflib | GIF decoding and encoding | MIT |
| Pillow native quantizer | GIF palette generation | MIT-CMU |
| pocketfft | Planner texture scoring | BSD-3-Clause |
| NumPy random support | PCG64 compatibility | BSD-3-Clause and included PCG notices |
| Python and Unicode data | URL character properties and Punycode compatibility | PSF and Unicode data licences |

The Nayuki source is deliberately extended to retain module roles, codeword ownership, block capacities, and payload capacity in its authoritative encoder model. Its post-terminator padding retains Segno's zero-codeword behaviour at an already aligned boundary, so fixed-mask matrices match the existing planner. The Pillow quantizer compiles as native C and does not embed Python. The selected libpng revision includes APNG read and write support and identifies itself as a development revision. Updating any component requires rerunning the behavioural and reference-fixture tests.
