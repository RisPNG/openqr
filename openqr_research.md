# OpenQR source research

Date: 14 November 2026

Project decision after this research: the author selected a C/C++-centred native implementation, reusing the existing verifier and suitable imaging components, with OpenQR owning its unique planner and public contract. The [current concept brief](C:/Users/SONIC/Documents/Projects/openqr/concepts_brief.md) records that direction. The comparative findings below remain research evidence, without a claim that the selected implementation has a measured size or performance advantage.

## Findings

The source evidence supports one canonical native engine, a deliberate public contract, and packages tested from their host languages. It does not establish that Rust, C++, or a particular encoder will produce the smallest or fastest complete OpenQR implementation.

The earlier Rust/Nayuki recommendation should remain provisional. ZXing-C++ provides the strongest inspected native QR product and distribution reference. CodeGlyphX and styled-qr provide particularly useful artwork and block-budget models. Xberg, formerly Kreuzberg, and its Alef generator are the closest inspected example of one core exposed to the requested language ecosystems. pygit2 supplies the strongest inspected pattern for testing installed native Python packages across all six requested targets.

No inspected project establishes the complete combination of branded-qr behaviour, native reuse, all requested language packages, and identical feature coverage on Windows, Linux, and macOS on x86_64 and ARM64. The useful result is a set of specific implementation and verification patterns, together with the gaps that OpenQR would still own.

This report is research. It does not approve a design, change the implementation, rename a repository, or record a completed EGM Goldfish review.

## Scope, discovery, and evidence

The target is an independently usable OpenQR engine, with the Phoenix/Ash application in a separate repository provisionally named openqr-web. Intended consumers are C, .NET, Python, Java, Rust, Go, and Elixir. The full default distribution should preserve an agreed branded-qr feature contract on all six OS/CPU combinations. Smaller explicit builds are a separate capability.

The focused pass used English, Chinese, and Japanese searches, GitHub repository searches and tree APIs, GitLab project search, registry searches, dependency traversal, and direct inspection of implementation, tests, build definitions, and package/release automation. The prior source survey supplied additional leads. Registry descriptions, documentation, README files, popularity, marketing, benchmark claims, and reviews were excluded as evidence for quality or feature conclusions.

| Recorded discovery channel | Retrieved scope | Interpretation |
|---|---|---|
| crates.io qrcode and barcode queries | All 295 and 376 returned records, respectively, across their pages. The union contained 607 crate identifiers and 469 distinct declared repository URLs after basic normalisation. | Includes applications, wrappers, forks, unrelated biological barcode tools, and several packages sharing a core. These are discovery records, not independent engines. |
| GitHub seed queries | 8 queries, 138 returned rows, 137 distinct repository names | Includes many forks and applications. Repository trees and source determined which candidates merited inspection. |
| NuGet qr native query | All 47 returned records | Supplemented with broader searches because this filter misses managed/AOT alternatives such as CodeGlyphX. |
| Maven Central zxing query | All 77 returned records | Used to identify wrappers, ports, and canonical implementations. |
| Hex qr query | 23 returned package records | Identified an additional C NIF integration, image_qrcode. |
| npm qrcode/ffi search | 100 sampled records | A broad, noisy lead set, followed through relevant package dependencies. |
| GitLab qrcode search | 60 sampled project records | Supplementary forge coverage, not an exhaustive inventory of GitLab. |
| Codeberg search | API refused the search under load, and web access reported a robots restriction | Search coverage was unavailable. Earlier directly inspected Codeberg sources remain separate evidence. |

The scored shortlist contains 25 engine/component families in three groups. Binding tools and additional source screens are assessed separately. This is an extensive, bounded survey with explicit query coverage. It cannot prove that every public project has been discovered, and it is not a line-by-line audit of every shortlisted repository.

Revisions are pinned in the source links. Large source blobs were checked against tree metadata and fetched through pinned raw endpoints when a connector returned empty content. A successful tool response alone was not treated as a successful source read.

No third-party engine build, benchmark, or complete test suite was executed in this focused pass. Workflow evidence below describes configured execution. No successful CI result is inferred from a workflow file.

## The feature contract that matters

The local reference is [branded-qr's generation entry point](C:/Users/SONIC/Documents/Projects/branded-qr/branded_qr/generator.py:169), its [block-aware mosaic planner](C:/Users/SONIC/Documents/Projects/branded-qr/branded_qr/mosaic.py:69), and its [rendering implementation](C:/Users/SONIC/Documents/Projects/branded-qr/branded_qr/rendering.py:96).

| Area | Source-derived baseline |
|---|---|
| QR construction | Versions 1 to 40, ECC search, fixed or automatic version, mask candidates, UTF-8 and ECI handling, actual encoded payload reporting |
| Artwork | Mosaic and picture modes, per-block codeword budgets, recovery reserve, protected structures, seeded search, artwork fractions, and exposure adjustments |
| Image preparation | Reference/background handling, EXIF orientation, alpha composition, resizing, contrast, brightness, and optional backdrop-aware branding |
| Rendering | Existing body/finder-frame/finder-centre styles, colour and transparency, PNG and SVG where currently provided |
| Animation | Current picture-mode frame handling, timing, loop information, GIF output, and existing restrictions on animated mosaic/background inputs |
| Verification | Decode every exported frame, compare the actual payload bytes, try the current binarisers, reject the whole candidate when any frame fails |
| Results | Artifact bytes, previews, per-version failures, seeds, ECC, attempts, verification counts, and block-budget metadata |

The input payload is currently text, rather than a general binary-payload API. Text can still contain an embedded NUL, so replacing it with a NUL-terminated foreign string changes its representable inputs. URL variation intentionally changes the encoded candidate when enabled.

The [image-loading path](C:/Users/SONIC/Documents/Projects/branded-qr/branded_qr/generator.py:257) delegates to Pillow rather than enforcing a strict format allowlist. Its error message's format examples cannot establish the complete accepted-format contract. The current [frame verification loop](C:/Users/SONIC/Documents/Projects/branded-qr/branded_qr/generator.py:397) is stronger than merely exporting an animation or checking its first frame.

Automatic website-icon retrieval belongs outside deterministic generation, but moving it must preserve the [backdrop-dependent ordering](C:/Users/SONIC/Documents/Projects/branded-qr/branded_qr/generator.py:222). Full product parity must include an available branding integration even if it is packaged separately.

## Ranking method

Scores are ordinal inspection judgments from 1 to 5, not measurements, coverage percentages, or a numeric total.

| Dimension | Evidence considered |
|---|---|
| A: architecture | Ownership, responsibility boundaries, public contracts, and dependency direction |
| E: engineering | Behavioural tests, failure enforcement, fuzzing, error conversion, and checks of shipped packages |
| M: maintainability | Coupling, duplicated algorithms, platform/profile divergence, generated-code discipline, and compatibility burden |
| O: organisation | Cohesion of algorithms, rendering, interfaces, bindings, applications, and tests |
| P: platforms | Explicit OS/CPU builds, native runtime tests, installed-artifact checks, and their skips or conditions |
| F: features | Implemented coverage relevant to the row's stated role |

Rank is reference priority for this OpenQR project. It is not a claim that one engine is universally better than another. For QR candidates, F concerns the clarified OpenQR feature contract. For non-QR engines, F concerns their inspected imaging or native-product role. Cross-domain F values must not be compared as QR coverage.

A high P score is source evidence of a broad configured matrix. It is not independent certification of those platforms. A compiler target, a built binary, an installed package, a runtime test, and full feature parity are separate states.

## QR and barcode candidates

| Rank | Family | A/E/M/O/P/F | Assessment |
|---|---|---|---|
| 1 | ZXing-C++ | 5/5/4/5/4/3 | Strongest native QR product reference: opaque C objects, explicit lengths/freeing, real host bindings, six-target core tests, and native package staging. Artwork planning and public block metadata remain gaps. |
| 2 | CodeGlyphX | 4/5/3/4/3/4 | Strongest newly inspected combined artwork/codec/QR feature reference. Actual block budgets and substantive tests. The inspected product is managed .NET with an AOT consumer, without an established general native C library or seven-language distribution. |
| 3 | Zint | 4/5/4/4/4/3 | Standalone C backend, substantial rendering/encoding, installation machinery, and strong QR tests. Large mutable public structures and incomplete six-target runtime qualification. |
| 4 | styled-qr | 5/4/4/5/2/3 | Coherent symbol/composition/scene/output model and genuine per-block damage accounting. No established C ABI, language-package system, or complete animation pipeline. |
| 5 | anydcode | 5/4/4/5/4/2 | Strong typed symbol model, feature slices, caller-buffer paths, and explicit release targets. Its actual foreign output protocol is wasm32-specific. |
| 6 | Nayuki | 5/3/4/5/1/2 | Small encoding stages, caller buffers, and a no-heap variant. Separate language ports, private/discarded planner metadata, and no inspected platform qualification matrix. |
| 7 | libqrencode | 4/4/4/4/3/2 | Useful C lifecycle, segmented inputs, ECI, and module roles. No established complete artwork, verifier, or host-package product. |
| 8 | rxing | 4/4/3/3/2/2 | Substantial Rust reader/writer and metadata. Overlapping paths, broad defaults, and differing raw-byte meanings complicate reuse. |
| 9 | QuickCodes | 3/2/2/3/3/2 | Real Rust C ABI and language bindings, with coherent owned byte results. Narrower foreign contracts and skipped reader binding tests weaken product readiness. |
| 10 | libqrean | 4/2/3/4/2/2 | Useful exposed block/interleaving implementation and native/WASM machinery. Failure masking and an allocation concern require investigation before adoption. |

### ZXing-C++

Its [six-target core workflow](https://github.com/zxing-cpp/zxing-cpp/blob/f19f7c6e0509fe0eeb822bf35c41b0f637dec290/.github/workflows/ci.yml#L97-L161) names OS/architecture runners, builds, and invokes CTest. Its [.NET staging and package checks](https://github.com/zxing-cpp/zxing-cpp/blob/f19f7c6e0509fe0eeb822bf35c41b0f637dec290/.github/workflows/publish-dotnet.yml#L15-L117) are a concrete template for assembling native assets for all six runtime identifiers.

The important qualification is that [.NET test registration is conditional](https://github.com/zxing-cpp/zxing-cpp/blob/f19f7c6e0509fe0eeb822bf35c41b0f637dec290/wrappers/dotnet/CMakeLists.txt#L3-L36), including a net10+ runtime check. Calling CTest by a test-name expression does not prove that the test was registered or executed. Its installed-NuGet smoke test is specifically [Windows x86](https://github.com/zxing-cpp/zxing-cpp/blob/f19f7c6e0509fe0eeb822bf35c41b0f637dec290/.github/workflows/publish-dotnet.yml#L142-L190).

[Python wheel tests](https://github.com/zxing-cpp/zxing-cpp/blob/f19f7c6e0509fe0eeb822bf35c41b0f637dec290/wrappers/python/pyproject.toml#L53-L77) exercise installed artifacts but skip musllinux. [Go test registration](https://github.com/zxing-cpp/zxing-cpp/blob/f19f7c6e0509fe0eeb822bf35c41b0f637dec290/wrappers/go/CMakeLists.txt#L3-L27) explicitly excludes Windows.

Reuse the [opaque C interface and error conversion](https://github.com/zxing-cpp/zxing-cpp/blob/f19f7c6e0509fe0eeb822bf35c41b0f637dec290/core/src/ZXingC.cpp#L53-L74), concrete byte-oriented verification, and package completeness checks. Do not assume its version string negotiates ABI compatibility or that its [private QR metadata](https://github.com/zxing-cpp/zxing-cpp/blob/f19f7c6e0509fe0eeb822bf35c41b0f637dec290/core/src/qrcode/QRVersion.h#L39-L58) is a supported planner interface.

### CodeGlyphX

Its [block-layout implementation](https://github.com/EvotecIT/CodeGlyphX/blob/ffcc443cb04455100c80a370e3a244a98327bc3f/CodeGlyphX/Qr/QrCodewordLayout.cs#L14-L50) maps placed bits to interleaved blocks. [Logo placement](https://github.com/EvotecIT/CodeGlyphX/blob/ffcc443cb04455100c80a370e3a244a98327bc3f/CodeGlyphX/QrArt.LogoBudget.cs#L36-L95) subtracts a per-block reserve and counts distinct affected words. Automatic placement protects function structures, while an explicit alternative permits covering central alignment.

The [logo tests](https://github.com/EvotecIT/CodeGlyphX/blob/ffcc443cb04455100c80a370e3a244a98327bc3f/CodeGlyphX.Tests/QrLogoBudgetTests.cs#L12-L52) enumerate versions/ECC, flip accepted knockout bits, decode the damaged symbol, and check block layout. [Artwork tests](https://github.com/EvotecIT/CodeGlyphX/blob/ffcc443cb04455100c80a370e3a244a98327bc3f/CodeGlyphX.Tests/QrNativeArtGuardrailTests.cs#L26-L99) include independent decoding, explicit decorative opt-out, and exact binary bytes.

Its [modern framework manifest](https://github.com/EvotecIT/CodeGlyphX/blob/ffcc443cb04455100c80a370e3a244a98327bc3f/CodeGlyphX/CodeGlyphX.csproj#L3-L62) does not declare runtime package dependencies for net8/net10. That does not establish a tiny native library. The [NativeAOT check](https://github.com/EvotecIT/CodeGlyphX/blob/ffcc443cb04455100c80a370e3a244a98327bc3f/.github/workflows/ci.yml#L138-L157) publishes and executes a Linux x64 example application. Neither the word Native in a test name nor AOT compatibility establishes exported C symbols.

Reuse the semantic model, block tests, and output guardrails. Do not equate its [animation timing tests](https://github.com/EvotecIT/CodeGlyphX/blob/ffcc443cb04455100c80a370e3a244a98327bc3f/CodeGlyphX.Tests/GifAnimationRenderTests.cs#L9-L48) with branded-qr's every-frame payload verification.

### The remaining candidates

Zint's [backend builds and installs independently](https://github.com/zint/zint/blob/9dd9791c43d6cde69d27e46f1c39874011aca4c3/backend/CMakeLists.txt#L25-L154), and its [QR tests](https://github.com/zint/zint/blob/9dd9791c43d6cde69d27e46f1c39874011aca4c3/backend/tests/test_qr.c#L81-L161) check exact output. External comparisons are conditional. Its [Windows ARM job](https://github.com/zint/zint/blob/9dd9791c43d6cde69d27e46f1c39874011aca4c3/.github/workflows/build-arm64-select-platform.yml#L56-L152) is build/package evidence, not runtime qualification. Its [public mutable structure](https://github.com/zint/zint/blob/9dd9791c43d6cde69d27e46f1c39874011aca4c3/backend/zint.h#L108-L150) is a larger ABI commitment than opaque handles.

styled-qr's [budget implementation](https://github.com/bipa-app/styled-qr/blob/a63a5bfd34ce7d586d03bf87cd01f8061a28ec0e/src/budget.rs#L12-L45) and [render/decode scenario](https://github.com/bipa-app/styled-qr/blob/a63a5bfd34ce7d586d03bf87cd01f8061a28ec0e/tests/render.rs#L317-L337) are useful. Its [layout reconstruction](https://github.com/bipa-app/styled-qr/blob/a63a5bfd34ce7d586d03bf87cd01f8061a28ec0e/src/symbol.rs#L431-L499) probes an underlying encoder, while key accessors stay private. OpenQR should prefer an intentional authoritative model over repeated reconstruction at several boundaries.

anydcode distinguishes [feature compile checks from executable tests](https://github.com/KarpelesLab/anydcode/blob/0b02edb13e6afdc5d67da5dc510782588210239f/.github/workflows/ci.yml#L16-L116). Its [WASM ownership protocol](https://github.com/KarpelesLab/anydcode/blob/0b02edb13e6afdc5d67da5dc510782588210239f/src/wasm.rs#L25-L49) packs pointer and length into a u64 using 32-bit halves. That cannot be adopted as a native 64-bit pointer contract.

Nayuki's [advanced encoder](https://github.com/nayuki/QR-Code-generator/blob/bcfe1b018cec5f1f91b5705eca5bb4cdd34e9f8d/rust/src/lib.rs#L211-L251) is a component candidate. Its constructor [clears function flags](https://github.com/nayuki/QR-Code-generator/blob/bcfe1b018cec5f1f91b5705eca5bb4cdd34e9f8d/rust/src/lib.rs#L298-L324), and [block interleaving remains private](https://github.com/nayuki/QR-Code-generator/blob/bcfe1b018cec5f1f91b5705eca5bb4cdd34e9f8d/rust/src/lib.rs#L516-L556). Its independent language implementations do not provide the maintenance property of one shared algorithm.

libqrencode provides [segmented/ECI inputs](https://github.com/fukuchi/libqrencode/blob/715e29fd4cd71b6e452ae0f4e36d917b43122ce8/qrencode.h#L187-L213) and actual [Windows CTest execution](https://github.com/fukuchi/libqrencode/blob/715e29fd4cd71b6e452ae0f4e36d917b43122ce8/.github/workflows/cmake-windows.yml#L40-L53). That does not establish packages for every intended host.

rxing's [raw result construction](https://github.com/rxing-core/rxing/blob/69e7a17b297f4e894972f52cc90fb351b8fcaf26/src/qrcode/qr_code_reader.rs#L221-L232) distinguishes corrected codewords from byte segments. Its [feature graph](https://github.com/rxing-core/rxing/blob/69e7a17b297f4e894972f52cc90fb351b8fcaf26/Cargo.toml#L17-L146) includes substantial optional/default infrastructure. A WASM dependency feature is not itself an exported language binding.

QuickCodes has a [real C ABI, boxed output ownership, disposal, and panic conversion](https://github.com/marcioreck/quickcodes/blob/84f4bd2b5228b99f795cbb84da571767797ab6c0/src/c_api.rs#L9-L145). Its in-memory generation entry accepts C strings and fixes PNG output. File generation can select other enabled formats. Reader functions exist, but [.NET round trips](https://github.com/marcioreck/quickcodes/blob/84f4bd2b5228b99f795cbb84da571767797ab6c0/dotnet/QuickCodes.Tests/BarcodeGeneratorTests.cs#L47-L66) and [Go round trips](https://github.com/marcioreck/quickcodes/blob/84f4bd2b5228b99f795cbb84da571767797ab6c0/go/quickcodes/quickcodes_test.go#L39-L66) are skipped. Skip messages cannot prove the exports are absent. Its benchmark command [masks failures](https://github.com/marcioreck/quickcodes/blob/84f4bd2b5228b99f795cbb84da571767797ab6c0/.github/workflows/ci.yml#L140-L141), so it supplies no accepted performance conclusion here.

Its [default reader and rendering feature graph](https://github.com/marcioreck/quickcodes/blob/84f4bd2b5228b99f795cbb84da571767797ab6c0/Cargo.toml#L23-L66) includes several decoder libraries and an SVG parsing/raster stack. That is an integration and maintenance cost to account for, without a measured binary-size conclusion.

libqrean's [block topology](https://github.com/kikuchan/libqrean/blob/b1dd0bdc72c3644593c0a503bd5fe24588f10d76/src/qrpayload.c#L17-L159) is useful. Its [shell harness](https://github.com/kikuchan/libqrean/blob/b1dd0bdc72c3644593c0a503bd5fe24588f10d76/tests/test.sh#L6-L44) can lose parent error state through pipelines. Its [WASM image allocation](https://github.com/kikuchan/libqrean/blob/b1dd0bdc72c3644593c0a503bd5fe24588f10d76/wasm/main.c#L61-L73) also warrants runtime investigation. The allocation finding is static, not a reproduced exploit.

## Imaging, rendering, and codec families

These scores concern the imaging role. The libvips row includes its inspected bindings and distribution, avoiding a second conflicting whole-family score.

| Rank | Family | A/E/M/O/P/F | Assessment |
|---|---|---|---|
| 1 | Imageflow | 5/5/3/4/5/4 | Strong six-target artifact, ABI, output ownership, limits, and cancellation reference. Codec profiles and CPU/libc baselines need close attention. |
| 2 | Blend2D | 5/4/4/5/5/4 | Strong rendering/lifecycle design and six-target configuration. Its built-in codec set does not provide full current input parity. |
| 3 | libvips and bindings | 4/5/3/4/4/5 | Broad pipeline, codecs, buffer ownership, and native distribution. Dependency loading and binding/platform coverage remain uneven. |
| 4 | resvg | 5/4/5/5/3/4 | Narrow implementation/ABI separation. A whole SVG parser is unnecessary unless OpenQR gains a corresponding input requirement. |
| 5 | ThorVG | 4/4/4/4/3/4 | Selectable rendering and loader paths, C API, three-OS tests. Extra GPU/text/media capabilities exceed the current requirement. |
| 6 | image-rs/image | 4/4/4/4/2/4 | Useful codec/frame operations. Broad defaults, unconditional image-processing dependencies, and different adjustment semantics. |
| 7 | tiny-skia | 5/4/5/5/3/3 | Cohesive raster component and premultiplied-pixel model. No complete codec pipeline or inspected native C ABI. |
| 8 | fast_image_resize | 5/4/4/5/3/3 | Narrow resampling responsibility, reusable buffers, alpha processing, and real CPU selection. One component only. |
| 9 | Wuffs | 5/5/4/5/2/3 | Explicit bounded decoder/work-buffer/state contracts and strong malformed-input tests. Current WebP path is single-frame. |
| 10 | stb image family | 3/3/3/3/2/2 | Compact C integration, but codec and animation metadata gaps for this target. No established six-target runtime matrix. |

### Dependency closure and feature semantics

The current image-rs development [manifest](https://github.com/image-rs/image/blob/e70e9fe5073f4b298a33b72993847241a8eaaca6/Cargo.toml#L42-L108) enables parallelism and many formats by default, with unconditional moxcms and pic-scale-safe dependencies. The [resize implementation](https://github.com/image-rs/image/blob/e70e9fe5073f4b298a33b72993847241a8eaaca6/src/imageops/resize.rs#L15-L101) already uses pic-scale-safe and alpha preparation. Adding another resizer automatically would duplicate an established responsibility without evidence that it helps.

Pillow 13 [contrast and brightness](https://github.com/python-pillow/Pillow/blob/293218ad059d994baf1777d882911bed6a49704a/src/PIL/ImageEnhance.py#L62-L96) differ from image-rs [contrast](https://github.com/image-rs/image/blob/e70e9fe5073f4b298a33b72993847241a8eaaca6/src/imageops/colorops.rs#L90-L115) and [brightness](https://github.com/image-rs/image/blob/e70e9fe5073f4b298a33b72993847241a8eaaca6/src/imageops/colorops.rs#L156-L178). Pillow brightness is multiplicative, whereas the inspected image-rs operation adds an integer. A factor/value of 2 therefore has different meaning. Reusing names would silently change results.

The inspected image-rs reader [enables orientation by default](https://github.com/image-rs/image/blob/e70e9fe5073f4b298a33b72993847241a8eaaca6/src/io/image_reader_type.rs#L468-L480) and [applies it during decode](https://github.com/image-rs/image/blob/e70e9fe5073f4b298a33b72993847241a8eaaca6/src/io/image_reader_type.rs#L933-L945). A port must avoid retaining a second EXIF-transpose step unintentionally.

Pillow's native kernels are not already an independent C image library: [Python types](https://github.com/python-pillow/Pillow/blob/293218ad059d994baf1777d882911bed6a49704a/src/libImaging/Imaging.h#L95-L116) and [Python thread hooks](https://github.com/python-pillow/Pillow/blob/293218ad059d994baf1777d882911bed6a49704a/src/_imaging.c#L199-L205) participate in their contract. Either a Rust or C++ core must account for separating or replacing that integration.

Imageflow's [ABI dependency](https://github.com/imazen/imageflow/blob/6bbe6677ab1de76e1e534aaf1c720064263cb816/imageflow_abi/Cargo.toml#L24-L35) retains its core's [default C-codec features](https://github.com/imazen/imageflow/blob/6bbe6677ab1de76e1e534aaf1c720064263cb816/imageflow_core/Cargo.toml#L126-L156). Disabling defaults only on the outer crate does not remove that closure.

### Format support must include frame behaviour

Imageflow's default WebP decoder exposes multiple-frame metadata but [does not iterate additional frames](https://github.com/imazen/imageflow/blob/6bbe6677ab1de76e1e534aaf1c720064263cb816/imageflow_core/src/codecs/webp.rs#L242-L244). Its optional decoder profile has a [separate animation bridge](https://github.com/imazen/imageflow/blob/6bbe6677ab1de76e1e534aaf1c720064263cb816/imageflow_core/src/codecs/zen_decoder.rs#L65-L79). Wuffs' [WebP state machine](https://github.com/google/wuffs/blob/b36a81d3352604fee1fee3661f39909706b1b966/std/webp/decode_webp.wuffs#L439-L471) is also single-frame.

By contrast, image-webp explicitly parses [frame dimensions, timing, blending, and disposal](https://github.com/image-rs/image-webp/blob/e43b2f98a26f4e521a7929f311935819f34196f8/src/decoder.rs#L774-L812). This is stronger capability evidence, without establishing six-platform qualification.

libvips [GIF](https://github.com/libvips/libvips/blob/5e918bd43a5f2fc8cfcd23fddbfc6704a1fcff3f/libvips/foreign/nsgifload.c#L617-L629) and [WebP loading](https://github.com/libvips/libvips/blob/5e918bd43a5f2fc8cfcd23fddbfc6704a1fcff3f/libvips/foreign/webpload.c#L175-L207) require explicit multi-page selection. Blend2D registers [BMP, JPEG, PNG, and QOI](https://github.com/blend2d/blend2d/blob/58ca9460b4138af6e793184682e9324e0a8053b2/blend2d/core/imagecodec.cpp#L343-L351). stb's [decoder dispatcher](https://github.com/nothings/stb/blob/9600522de53973336fa033a289858a4354cbb04c/stb_image.h#L1198-L1247) contains no WebP branch.

### Native boundaries and CPU baselines

Imageflow's [six-target matrix](https://github.com/imazen/imageflow/blob/6bbe6677ab1de76e1e534aaf1c720064263cb816/.github/workflows/ci.yml#L186-L288) and [test execution](https://github.com/imazen/imageflow/blob/6bbe6677ab1de76e1e534aaf1c720064263cb816/.github/workflows/ci.yml#L420-L443) are useful. Intel macOS [compiles with a Haswell CPU baseline](https://github.com/imazen/imageflow/blob/6bbe6677ab1de76e1e534aaf1c720064263cb816/.github/workflows/ci.yml#L360-L365), Linux libc floors differ by architecture, and [musl jobs change the artifact to a static library](https://github.com/imazen/imageflow/blob/6bbe6677ab1de76e1e534aaf1c720064263cb816/.github/workflows/ci.yml#L409-L417). Six OS/CPU labels do not describe all compatibility requirements.

Blend2D's [single/multithread comparison](https://github.com/blend2d/blend2d/blob/58ca9460b4138af6e793184682e9324e0a8053b2/blend2d-testing/tests/bl_test_context_mt.cpp#L81-L140) has meaningful failure behaviour, but the [external driver configuration](https://github.com/blend2d/blend2d/blob/58ca9460b4138af6e793184682e9324e0a8053b2/.github/workflows/build-config.json#L16-L36) marks those programs optional. This corrects the earlier report's inaccurate test-file link.

resvg's [C render entry](https://github.com/linebender/resvg/blob/5bdbae64eefadc4bf0caa56dcf91702037929086/crates/c-api/lib.rs#L882-L899) derives output size from dimensions and trusts the supplied pointer. It lacks an independent buffer-length argument. ThorVG's [picture creator](https://github.com/thorvg/thorvg/blob/2f03fd4bcddc84604db4640d9bf43989af78b0e4/src/bindings/capi/tvgCapi.cpp#L655-L658) is a thin C-to-C++ call. These boundaries do not establish protection from every native failure.

Smaller components provide useful focused contracts: fast_image_resize has [runtime CPU selection](https://github.com/Cykooz/fast_image_resize/blob/ce36aeb20ed4a33805baa3c0ff4caff7cf163e19/src/cpu_extensions.rs#L23-L77), tiny-skia has [explicit features](https://github.com/linebender/tiny-skia/blob/5d4754777746eef0828be166896eaf482c49f8f2/Cargo.toml#L17-L39), and Wuffs has [truncation/frame tests](https://github.com/google/wuffs/blob/b36a81d3352604fee1fee3661f39909706b1b966/test/c/std/gif.c#L69-L162). Their narrow scope is useful, but they do not constitute the full image pipeline.

## Cross-language native-product references

These scores concern the inspected public interfaces, bindings, and distribution, rather than complete audits of PDF, Git, database, or parser algorithms.

| Rank | Family | A/E/M/O/P/F | Assessment |
|---|---|---|---|
| 1 | Xberg, formerly Kreuzberg | 4/5/3/4/3/4 | Closest shared-core/many-language shape, rich native errors, generated handles, and exact-archive qualification. Feature and target profiles differ. |
| 2 | libgit2 with bindings | 5/5/4/4/4/5 | Clear ownership, ABI checks, installed-wheel tests, and native asset packaging. Qualification varies by binding. |
| 3 | DuckDB Java/native family | 4/5/4/5/4/5 | Real bundled-native loader and lifecycle tests. Two packaged target tests are explicitly disabled. |
| 4 | Tree-sitter with bindings | 5/4/4/5/4/4 | One small core and host lifetime mapping. Development and installed-package test coverage differ. |
| 5 | PDF Oxide | 4/4/3/3/3/4 | Real C ABI and requested language packages. Native feature builds are aligned, but some packaging failure checks are weaker. |

### Xberg and Alef

Xberg has [panic/error handling](https://github.com/xberg-io/xberg/blob/235eee254f16dd075847b8b3ff2b71dd7f6a92d7/crates/xberg-ffi/src/lib.rs#L207-L247), a [typed generational handle registry](https://github.com/xberg-io/xberg/blob/235eee254f16dd075847b8b3ff2b71dd7f6a92d7/crates/xberg-ffi/src/lib.rs#L324-L388), and [explicit string/byte disposal](https://github.com/xberg-io/xberg/blob/235eee254f16dd075847b8b3ff2b71dd7f6a92d7/crates/xberg-ffi/src/lib.rs#L545-L598). These are concrete mechanisms for preventing stale-object and ownership mistakes.

Its strongest distribution example qualifies an [exact macOS Elixir archive](https://github.com/xberg-io/xberg/blob/235eee254f16dd075847b8b3ff2b71dd7f6a92d7/.github/workflows/qualify-macos-elixir-nif.yaml#L85-L125), loads it in a clean environment, performs an image round trip, and uses a missing-dependency negative control. This tests the shipped dependency closure.

Its [broad language end-to-end matrix](https://github.com/xberg-io/xberg/blob/235eee254f16dd075847b8b3ff2b71dd7f6a92d7/.github/workflows/ci-e2e.yaml#L143-L212) is principally Linux x64. Its [Elixir native loader targets](https://github.com/xberg-io/xberg/blob/235eee254f16dd075847b8b3ff2b71dd7f6a92d7/packages/elixir/lib/xberg/native.ex#L8-L20) omit Intel macOS and Windows ARM64. [Platform-specific FFI dependencies](https://github.com/xberg-io/xberg/blob/235eee254f16dd075847b8b3ff2b71dd7f6a92d7/crates/xberg-ffi/Cargo.toml#L116-L129) also select different feature profiles. It is a useful architecture reference without proving uniform feature coverage.

Alef's [implemented backends](https://github.com/xberg-io/alef/blob/5466d33fbaa4e88d23f29a31824cda9a98707926/src/backends/mod.rs#L11-L29) cover the requested binding ecosystems more directly than the inspected UniFFI backends. Its handle tests [compile and execute generated code](https://github.com/xberg-io/alef/blob/5466d33fbaa4e88d23f29a31824cda9a98707926/src/backends/ffi/gen_bindings/tests/handle_registry.rs#L27-L104), and a [Java test executes generated error dispatch](https://github.com/xberg-io/alef/blob/5466d33fbaa4e88d23f29a31824cda9a98707926/tests/java_native_error_code_runtime.rs#L60-L104). This merits a representative prototype.

Limits remain: some ownership tests inspect generated source text, its matrix is not all six targets, and a [generated ABI stamp](https://github.com/xberg-io/alef/blob/5466d33fbaa4e88d23f29a31824cda9a98707926/src/backends/ffi/handle_abi_stamp.rs#L13-L38) is a build-time coherence check rather than demonstrated runtime signature negotiation.

The inspected [UniFFI dispatch](https://github.com/mozilla/uniffi-rs/blob/482095c1ba1b76a1393b582e0589e2fdc9c64b14/uniffi_bindgen/src/bindings/mod.rs#L16-L25) covers Kotlin, Python, Ruby, and Swift. It does not establish direct generation of the entire requested set. Alef's development dependencies and Xberg's generated source sizes must not be confused with installed engine size.

### Installed packages, ABI compatibility, and ownership

pygit2 is the strongest installed-Python-package reference: its [native runner matrix](https://github.com/libgit2/pygit2/blob/1c788f345ef4e525d6a6a1a7d0d4da7dbbe8ec6f/.github/workflows/wheels.yml#L60-L128) covers the requested targets, and [cibuildwheel tests built wheels, repaired where applicable](https://github.com/libgit2/pygit2/blob/1c788f345ef4e525d6a6a1a7d0d4da7dbbe8ec6f/pyproject.toml#L22-L46). That is stronger than importing a build-tree extension.

LibGit2Sharp has [owned/non-owned handle management](https://github.com/libgit2/libgit2sharp/blob/eaa698d078941fd5e3cc82b59b885cd35d8cc0f8/LibGit2Sharp/Core/Handles/Libgit2Object.cs#L86-L124). Its native asset builds [disable Unix tests](https://github.com/libgit2/libgit2sharp.nativebinaries/blob/2b95030f15e55dfe70e9fdbb2b3d304a5c95ad23/build.libgit2.sh#L29-L37), so asset presence and wrapper tests remain separate evidence.

git2-rs [generates ABI checks against actual C headers](https://github.com/rust-lang/git2-rs/blob/b9d59b680dcfd5c4960de16a3ab7d6cf91012084/systest/build.rs#L4-L66) and [checks system version/features](https://github.com/rust-lang/git2-rs/blob/b9d59b680dcfd5c4960de16a3ab7d6cf91012084/libgit2-sys/build.rs#L8-L68). This is a better model than assuming a generated declaration matches the loaded library.

NetVips provides a useful [managed/native package split](https://github.com/kleisauke/net-vips/blob/037388a9a76144d101589769813f0453a9adc049/build/native/NetVips.Native.nuspec#L22-L47). Its [test matrix](https://github.com/kleisauke/net-vips/blob/037388a9a76144d101589769813f0453a9adc049/.github/workflows/ci.yml#L15-L38) is less complete than its asset list. pyvips checks [compiled-wrapper compatibility with the loaded native library](https://github.com/libvips/pyvips/blob/9699ee2833785e891456937ade107a8fba8795f7/pyvips/__init__.py#L27-L91).

For Java, vips-ffm has [scoped native buffers and independent copies](https://github.com/lopcode/vips-ffm/blob/686056d87f317ed98c9fcaade7ddaec1a0404542/core/src/main/java/app/photofox/vipsffm/VBlob.java#L71-L127) with [lifetime/thread tests](https://github.com/lopcode/vips-ffm/blob/686056d87f317ed98c9fcaade7ddaec1a0404542/core/src/integrationTest/java/app/photofox/vipsffm/VBlobArenaTest.java#L16-L104). Its [integration task is separate](https://github.com/lopcode/vips-ffm/blob/686056d87f317ed98c9fcaade7ddaec1a0404542/core/build.gradle.kts#L57-L109) from ordinary check, so test presence does not establish CI execution.

One vips-ffm Windows [hash comparison](https://github.com/lopcode/vips-ffm/blob/686056d87f317ed98c9fcaade7ddaec1a0404542/.github/workflows/checks.yml#L79-L88) does not enforce failure on mismatch. A small PowerShell reproduction returned False and continued. This supports inspecting the failure path, without claiming a compromised artifact or an executed repository workflow.

DuckDB Java's [loader](https://github.com/duckdb/duckdb-java/blob/89fe8d9b913a9a9f8b0440aec110c9b907347074/src/main/java/org/duckdb/DuckDBNative.java#L32-L120) selects and loads packaged native resources. Its [Linux ARM64 job configures native execution](https://github.com/duckdb/duckdb-java/blob/89fe8d9b913a9a9f8b0440aec110c9b907347074/.github/workflows/Java.yml#L539-L590), while its [Windows ARM64 test](https://github.com/duckdb/duckdb-java/blob/89fe8d9b913a9a9f8b0440aec110c9b907347074/.github/workflows/Java.yml#L829-L864) and [Intel macOS test](https://github.com/duckdb/duckdb-java/blob/89fe8d9b913a9a9f8b0440aec110c9b907347074/.github/workflows/Java.yml#L1002-L1020) are explicitly disabled despite real artifacts.

Tree-sitter Python has [six-target development tests](https://github.com/tree-sitter/py-tree-sitter/blob/cb92a454132d6dd1cea315e660b8a5b6027561a6/.github/workflows/ci.yml#L24-L52), while [Windows wheel tests are skipped](https://github.com/tree-sitter/py-tree-sitter/blob/cb92a454132d6dd1cea315e660b8a5b6027561a6/.github/workflows/pypi.yml#L30-L58). Its Java binding maps [parser lifetime to native cleanup](https://github.com/tree-sitter/java-tree-sitter/blob/fb0a1a7739e7d40c33f39cbf79558575a1f75341/src/main/java/io/github/treesitter/jtreesitter/Parser.java#L19-L40), with a concrete JDK floor.

PDF Oxide has [real native ownership operations](https://github.com/yfedoseev/pdf_oxide/blob/08accb2e0be8252309083a33fafc74015269594c/src/ffi.rs#L418-L481), a [six-target native matrix](https://github.com/yfedoseev/pdf_oxide/blob/08accb2e0be8252309083a33fafc74015269594c/.github/workflows/release.yml#L355-L398), and a common extended feature build. However, [NuGet copy failures are suppressed](https://github.com/yfedoseev/pdf_oxide/blob/08accb2e0be8252309083a33fafc74015269594c/.github/workflows/release.yml#L1166-L1190), while [asset inclusion is conditional](https://github.com/yfedoseev/pdf_oxide/blob/08accb2e0be8252309083a33fafc74015269594c/csharp/PdfOxide/PdfOxide.csproj#L147-L156). Packing alone therefore cannot prove the promised native payload is complete.

## Six-target evidence, without collapsing the states

T below means an explicit configured runtime test on a named native target. B means build/package evidence. D means a declared target in package configuration. C means conditional test registration. S means explicitly skipped. A question mark means the inspected path does not establish that exact target. None of these symbols implies an independently observed passing run.

| Assessed path | Win x64 | Win ARM64 | Linux x64 | Linux ARM64 | Mac Intel | Mac ARM64 |
|---|---|---|---|---|---|---|
| ZXing-C++ core | T | T | T | T | T | T |
| ZXing Python installed wheels | T | T | T | T | T | T |
| ZXing .NET release path | B/C | B/C | B/C | B/C | B/C | B/C |
| pygit2 installed CPython wheels | T | T | T | T | T | T |
| DuckDB Java | T | B/S | T | T | B/S | T |
| Tree-sitter Python development binding | T | T | T | T | T | T |
| Tree-sitter Python installed wheel | B/S | B/S | T | T | T | T |
| Xberg Elixir target configuration | D | ? | D | D | ? | D |
| image_qrcode precompiled target configuration | ? | ? | B | B | B | B |

The Linux columns concern glibc unless stated otherwise. Tree-sitter's installed-wheel configuration skips ARM64 musllinux builds, and ZXing skips musllinux wheels. The Xberg and image_qrcode rows concern configured target sets, not a promise that every archive is published or exercised. They cannot be promoted to T based on other jobs.

Additional platform constraints include operating-system minimums, libc, compiler/runtime linkage, CPU instructions, host-runtime architecture, native dependencies, and artifact type. An ARM64 operating system running an x64 host under emulation does not establish that an ARM64 native library loads in that host.

OTP has [positive native Windows ARM64 build/boot evidence](https://github.com/erlang/otp/blob/147d42aaa206bb934487458070cc260d78c8ce31/.github/workflows/reusable-build-windows.yaml#L219-L255). RustlerPrecompiled nevertheless [omits Windows ARM64 from its default targets](https://github.com/philss/rustler_precompiled/blob/d8618368e81c12aac8dff4a9cefc444ca49233ab/lib/rustler_precompiled/config.ex#L21-L35), and its [Windows fallback](https://github.com/philss/rustler_precompiled/blob/d8618368e81c12aac8dff4a9cefc444ca49233ab/lib/rustler_precompiled.ex#L456-L507) requires care. This is a packaging/detection gap to qualify, without asserting that custom support is impossible.

## Additional source screens and dispositions

| Project | Useful source evidence | Why it does not settle the engine choice |
|---|---|---|
| furious-qr | [Public Rust construction stages](https://github.com/zhengkyl/furious-qr/blob/456a598a3630bb0b1d74421c1f9f0c769b542608/rust/src/lib.rs#L384-L558), [module/codeword/block blueprint](https://github.com/zhengkyl/furious-qr/blob/456a598a3630bb0b1d74421c1f9f0c769b542608/typescript/src/extras/blueprint.ts#L19-L69) | Separate TS/Rust implementations, no established native ABI/platform matrix, ECI gap in inspected encoders, and logo clearing needs protected-module review |
| houseme/qrcode-rs | [Actual ECI/byte decode assertions](https://github.com/houseme/qrcode-rs/blob/b6d96eda54dc7e6aab3787f09f4854f0523ad945/tests/scan_decode.rs#L130-L175), multiple core/render/decode crates | No established requested-language package system. [Maximum error count aggregates blocks](https://github.com/houseme/qrcode-rs/blob/b6d96eda54dc7e6aab3787f09f4854f0523ad945/crates/qrcode-core/src/ec.rs#L412-L427). The pic crate emits PIC diagram syntax, not artistic picture mode. |
| fast_qr | [Typed module roles](https://github.com/erwanvivien/fast_qr/blob/53e8c9930e985ca7f23c1500e887552de085b2cc/src/module.rs#L1-L50), actual WASM export | Native cdylib alone is not a C ABI. [Raster conversion reparses generated SVG](https://github.com/erwanvivien/fast_qr/blob/53e8c9930e985ca7f23c1500e887552de085b2cc/src/convert/image.rs#L154-L175), adding another pipeline stage. |
| qrc | [Centre-dot artwork blend](https://github.com/sebastienrousseau/qrc/blob/7ebb7b196cc753a5d6164f568526a133b1e9eb72/src/art.rs#L86-L138) and selected image formats | Finder-specific preservation, no per-block planner in that path, and [art tests focus on dimensions](https://github.com/sebastienrousseau/qrc/blob/7ebb7b196cc753a5d6164f568526a133b1e9eb72/tests/art_qr.rs#L6-L45) |
| image_qrcode | [Actual dirty-CPU C NIF](https://github.com/elixir-image/image_qrcode/blob/5afacb596d0a822351c4f60e7d21b8580df9bb0f/c_src/qrcode_nif.c#L228-L233), [contention round trips](https://github.com/elixir-image/image_qrcode/blob/5afacb596d0a822351c4f60e7d21b8580df9bb0f/test/concurrency_test.exs#L25-L45) | Useful Elixir integration, but [Windows precompiler map is empty](https://github.com/elixir-image/image_qrcode/blob/5afacb596d0a822351c4f60e7d21b8580df9bb0f/mix.exs#L117-L143) and its text entry copies into a C string |
| Aspose.BarCode FOSS for Python | [Capability model](https://github.com/aspose-barcode-foss/Aspose.BarCode-FOSS-for-Python/blob/06eca5c01e13ed6d59a640f1cf330c1c5a57d151/src/aspose_barcode_foss/_internal/models/capabilities.py#L9-L22), [golden QR modules](https://github.com/aspose-barcode-foss/Aspose.BarCode-FOSS-for-Python/blob/06eca5c01e13ed6d59a640f1cf330c1c5a57d151/tests/encoding/test_qr_golden_vectors.py#L20-L67) | Source-level Python encoder/API reference, without an established Python-free native boundary |
| taeyunekim-chris/barcode-engine | [Actual browser/WASM decoder code](https://github.com/taeyunekim-chris/barcode-engine/blob/61b948ac15ef7c0240c9107aa100c1642bbf95b7/packages/core/src/decoder/zxing.ts#L80-L89) | A browser scanner wrapper around other engines, not an independent native engine for these consumers |

Other sources from the earlier survey remain useful orientation: Segno/qrcode-artistic as the current Python behaviour reference, Flutter artistic_qr, qr-code-styling-rs, QRCoder, QRtsy, Go QR writers, QArt, Java/.NET ZXing ports, BoofCV, OpenCV, and libdmtx. Their earlier role-specific findings are not promoted here into complete native-product qualification.

Skia/CanvasKit, Pillow, RustFFT, and pocketfft received supplementary component screens. The inspected [RustFFT planner](https://github.com/ejmahler/RustFFT/blob/a161dacfbb9d5dba981ad792eb8f2e2c9463f41a/src/plan.rs#L75-L86) and [pocketfft options](https://github.com/mreineck/pocketfft/blob/4d865cba24359b05b37da4e75aee24498c3b78e7/pocketfft_hdronly.h#L64-L103) show different numerical/CPU choices. Neither establishes identical artwork selection after a NumPy port.

## What to take, and what to avoid

| Concern | Pattern worth taking | Pattern to avoid |
|---|---|---|
| Canonical implementation | One engine owning QR construction, authoritative module/codeword/block information, artwork planning, rendering, and verification | Reimplementing the algorithm independently for each language or reconstructing private metadata in several adapters |
| Public interface | Explicit lengths, independently owned results, allocator-matched disposal, typed errors, and compatibility checks | C strings as a substitute for arbitrary text, raw host objects crossing the boundary, or assuming a version label proves ABI compatibility |
| Bindings | Language-appropriate lifetime handling, generated declarations where suitable, and real foreign execution | Treating export count, directory count, cdylib, or a WASM feature as proof of a shipped language interface |
| Packaging | Test the repaired wheel, packed NuGet/Maven/Hex artifact, and complete dependency closure in a clean environment | Testing only the build tree, conditionally omitting native assets, or masking copy/load failures |
| Platforms | Separate build, package, runtime, and feature-parity states for each target | Promoting universal compilation, emulation, or a generic OS label into native execution evidence |
| Features | Full default profile, explicit capability reporting for reduced builds, and the same qualified contract on every supported target | Different platform profiles silently losing features or disabling verification to meet a size target |
| Images | Explicit colour, alpha, orientation, resize, enhancement, frame, timing, disposal, and loop semantics | Assuming equivalent names or format checkboxes imply equivalent behaviour |
| Performance | Compare the complete installed dependency set and realistic end-to-end workloads | Ranking languages by reputation, comparing encoder-only memory with full generation, or counting code-generation dependencies as runtime size |
| Reliability | Exact payload checks, every-frame verification, malformed-input tests, cancellation, and actual failure enforcement | Snapshot-only correctness, optional tests that quietly skip the substantive path, or panic conversion being presented as process isolation |

## Recommendation after the focused research

Retain the shared native-engine direction and the deliberate C ABI as leading architectural candidates. Reopen the implementation-language and encoder decisions until a representative prototype measures the whole pipeline.

The two leading implementation shapes are:

1. A C/C++-centred native implementation reusing the existing verifier and suitable imaging components, with OpenQR owning its unique planner and public contract.
2. A Rust-owned planner/rendering implementation with the same native verifier or a qualified alternative, an intentional semantic QR model, and a C ABI for foreign callers.

The first can reduce mixed-toolchain integration when the reused components are already C/C++. The second has concrete organisation examples in styled-qr/resvg and generated-binding examples in Xberg/Alef. These are architectural inferences from the inspected graphs. Neither is a measured size, latency, or memory winner.

Do not select Nayuki automatically. Compare encoder foundations on UTF-8/ECI, version/ECC/mask control, segmentation behaviour, authoritative layout metadata, licence terms, and integration cost. Do not import an entire graphics runtime merely to write SVG geometry that the current renderer already constructs.

For a Rust prototype, evaluate Alef against a small handwritten C boundary and host adapters. Its actual backends cover this language set more closely than the inspected UniFFI implementation, but it still needs tests of the generated packages, ABI/version handling, lifetimes, errors, and every requested target. A generator can reduce repetitive interface work without removing ownership or distribution responsibilities.

For Elixir, compare a dirty CPU NIF with a supervised native executable built from the same core. Scheduling and process isolation solve different problems. The choice should account for image/decoder failures, cancellation, concurrent jobs, memory, call overhead, and target-package availability. This is an application integration choice rather than a second QR algorithm.

Before selecting the final graph, the prototype should cover small/large symbols, Unicode and embedded NUL text, block reserves, multiple styles, alpha/orientation, real image adjustments, animation, and exact exported-frame decoding. It should measure complete installed size, cold/repeated-call latency, throughput, and peak memory with the same inputs and verification policy.

Compatibility needs separate acceptance criteria for QR correctness, feature parity, visual equivalence, replay within an engine version, and identical output across architectures. A different valid QR, changed pixel values, and different compressed PNG bytes are different outcomes. The source audit does not justify promising all of them as one equivalence guarantee.

## Verification and remaining limits

The focused pass executed small independent shell/PowerShell experiments that reproduced failure masking. It did not execute the third-party workflows or benchmark their native products. The prior 43 targeted branded-qr tests passed in the earlier research phase, covering the selected version/block/verification paths. They were not rerun here and are not a full-suite or six-platform result.

Public source availability and licence artifacts were considered as selection constraints. No combined licence-compatibility decision, code-copy approval, or selected dependency lockfile results from this report. Source file sizes, dependency declarations, and feature flags are not installed binary measurements.

No engine implementation, web implementation, Git remote, or database configuration was changed by this research.

A simple intended usage is the same QR request being sent from a .NET application and from openqr-web through their respective packages. Both should receive the same contracted generation behaviour and verification metadata. Their UI, database, authentication, and deployment remain application responsibilities. The research identifies how other projects implement and test those boundaries, while the exact OpenQR design remains to be agreed.

