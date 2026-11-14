# OpenQR engine and web application

* **Author:** Ris Peng
* **Date:** 2026-11-14
* **Status:** C11/C++20 native engine, C ABI, language bindings, and separate web repository implemented. Windows x86_64 runtime checks passed. The other five requested OS/CPU combinations have CI definitions and remain unqualified locally.

## Overview

OpenQR is an independently usable QR engine in the openqr repository. The separate Elixir, Phoenix, and Ash application in openqr-web consumes it through the openqr_engine binding. Consumers include C, .NET, Python, Java, Rust, Go, and Elixir.

The requested platform target is Windows, Linux, and macOS on both x86_64 and ARM64, with the same complete feature contract on every supported target. Installation size, execution time, and memory should be low without quietly removing the generation or verification behaviour that makes branded-qr useful.

The selected C/C++ implementation reuses ZXing-C++ verification and native imaging components. OpenQR owns its artwork planner, authoritative QR layout model, and public contract. Language packages consume that implementation. The current component graph, build instructions, contract, and fidelity bounds are recorded in the [implementation guide](C:/Users/SONIC/Documents/Projects/openqr/README.md).

The detailed [source research](C:/Users/SONIC/Documents/Projects/openqr/openqr_research.md) ranks 25 engine/component families across architecture, engineering, maintainability, organisation, platforms, and features, with additional binding-tool and source screens. It records discovery coverage, pinned implementation evidence, skipped or conditional tests, and the limits of the conclusions.

## Analysis

### Product boundary

| Component | Responsibility |
|---|---|
| openqr | Canonical QR construction, authoritative module/codeword/block information, artwork planning, rendering, verification, and public result contracts |
| Language packages | Host-language values, object lifetimes, errors, native loading, and distribution |
| openqr-web | Phoenix UI and HTTP interfaces, Ash resources, authentication, persistence, and application workflows |
| Native executable | Command-line use and optional host process isolation using the same engine |
| Host branding integration | Network retrieval and asset resolution before supplying artwork to the engine |

The author approved this repository split before implementation. The current directory contains the native engine, and the Phoenix application moved intact to sibling openqr-web. Both are independent local Git repositories with no remotes. The web application retains its OpenQR modules and :openqr application name. The engine has no database dependency, and the split did not change the web database schema.

### Selected architecture and reuse

The author selected the C/C++-centred alternative after the source research. Implementation uses Nayuki encoding with retained semantic layout metadata, ZXing-C++ verification, Blend2D geometry, native codecs, and pocketfft scoring. Source revisions, archive hashes, and retained licence notices identify the adopted components. The research does not establish a complete size or performance winner.

ZXing-C++ is the strongest inspected native QR product and distribution reference. CodeGlyphX and styled-qr provide particularly useful per-block artwork models and behavioural tests. Xberg/Alef is the closest inspected shared-core and generated-language-package architecture. pygit2 provides a strong model for testing installed native packages on the requested targets.

The earlier Rust-owned core proposal is not selected for OpenQR. Rust projects remain useful references for planner organisation, ownership, testing, and packaging. Runtime verification now covers the native pipeline and each language binding on Windows x86_64. Size measurements apply to the built artifact, while comparative speed and full platform qualification remain separate work.

Reuse should preserve the existing verifier's exact-payload comparisons, binarisation retries, and rejection of an animation candidate when any frame fails. Imaging components should be selected against the actual preprocessing, colour, alpha, orientation, frame, and export contracts. OpenQR's planner and public model should integrate those components coherently without reproducing their algorithms inside each language package.

The [research report](C:/Users/SONIC/Documents/Projects/openqr/openqr_research.md) keeps compiler support, built binaries, packaged artifacts, runtime tests, and feature parity separate. No inspected family established the full requested language/platform/feature matrix. An OS/CPU label also does not settle libc, CPU instruction requirements, operating-system minimums, host-runtime architecture, or native dependency availability.

### Feature contract

The baseline was extracted from the existing [generation entry point](C:/Users/SONIC/Documents/Projects/branded-qr/branded_qr/generator.py:169), [mosaic planner](C:/Users/SONIC/Documents/Projects/branded-qr/branded_qr/mosaic.py:69), and [export verification](C:/Users/SONIC/Documents/Projects/branded-qr/branded_qr/generator.py:397), then checked against executable source-derived fixtures and independently decoded output.

The baseline includes current QR versions/ECC/mask behaviour, UTF-8/ECI handling, mosaic and picture modes, protected structures, per-block recovery reserves, seeded search, styles, backgrounds, image preparation, animation, exports, previews, per-version failures, and exact-payload verification of every exported frame.

The native image pipeline covers PNG/APNG, JPEG, WebP, GIF, and ICO, with explicit alpha, orientation, resampling, contrast, brightness, timing, looping, and output limits. Tests use Pillow-derived pixel oracles where exact processing matters. Network icon discovery remains a host responsibility. The implementation guide records native raster, palette, automatic mask, and IDNA qualification bounds.

An optional reduced build should make capability changes explicit. It should not return unverified output as verified or allow the complete default build to lose features silently. QR correctness, feature parity, visual equivalence, deterministic replay within a version, and identical output across architectures need separate acceptance criteria.

### Public interfaces and packaging

A versioned C ABI defines pointer-length input spans, borrowed output views, opaque result and SVG buffer ownership, allocator-matched disposal, error statuses, defaults, limits, and discoverable styles. Generation is synchronous. C consumers call that interface directly, and language bindings manage their host values and lifetimes around it.

Python uses ctypes, .NET uses P/Invoke, Java uses its Foreign Function and Memory API, Rust uses owned result lifetimes around the C ABI, and Go uses CGO. Alef and Xberg remain research references. CMake builds the shared library, CLI, installable SDK, and native behavioural tests.

Elixir uses a dirty CPU NIF, with native loading and build integration tested through both the source checkout and installed SDK. The web application compiled and generated a literal payload through that dependency. The CLI remains available for hosts that choose process isolation. Windows ARM64 and the other requested targets still require their CI runtime checks.

## Goals vs. Non-Goals

* **In-Scope Goals:** Separate engine and web application, maintain one algorithm, preserve an agreed feature baseline across the requested platforms, supply usable language packages, reuse suitable implementations, and measure the complete product.
* **Out-of-Scope Non-Goals:** Add a framework or database requirement to the engine, introduce separate engine algorithms for each language, treat package presence as runtime qualification, claim unmeasured size or speed, or silently reduce verification.

Browser, mobile, embedded, additional barcode, minimum platform/runtime, and Linux musl coverage remain explicit scope decisions. They have not been promised or rejected.

## Alternatives Weighed

1. **C/C++-centred native engine with a C ABI:** Selected and implemented. Reuses the existing verifier and native imaging components, with OpenQR owning its planner and public contract. The adopted graph and API are recorded in source and the implementation guide.
2. **Rust-owned native engine with a C ABI:** Not selected for the primary engine after the author chose C/C++. Its organisation, ownership, and testing patterns remain useful references. No performance inferiority is inferred.
3. **Another managed implementation with native compilation:** Not selected for the primary engine. CodeGlyphX remains a useful artwork and engineering reference. The inspection did not establish a ready native C library and complete foreign package system.
4. **Independent implementation in each language:** Not favoured in the working hypothesis because algorithm fixes and feature parity would need repeated coordination.
5. **Worker or HTTP interface as the sole product:** Does not alone establish the intended native dependency contract. It can remain an optional deployment form using the same engine.
6. **Adopt one inspected project wholesale:** No candidate established the full requested combination. Individual components and engineering patterns remain reusable subject to integration and licence decisions.

The C/C++ implementation direction is the author's decision. Remaining assessments describe research findings and open design questions, without inventing rejection reasons or performance results.

## Key Decisions & Staging for Next Steps

The approved structure is implemented: openqr contains the C11/C++20 engine, include/openqr, src, bindings, tests, and CMake. Sibling openqr-web contains the Phoenix/Ash application and consumes the native binding. The C ABI, encoder, image stack, result ownership, numerical input/output limits, and Elixir integration are defined in source. Core project licensing, minimum operating-system versions, and published binary distribution remain release decisions.

The requested research is complete, and the native implementation passes its local runtime checks. The remaining platform qualification must execute the six-target CI matrix rather than infer runtime support from source declarations. Comparative performance claims require a representative benchmark across the complete pipeline.

For example, a .NET application and openqr-web submit a request to the same native planner and receive image bytes and verification metadata. Their interfaces, authentication, persistence, and deployment remain application responsibilities.
