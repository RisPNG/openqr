# OpenQR development

OpenQR is a C11/C++20 QR artwork engine with a versioned C API. [OpenQR Web](https://github.com/rispng/openqr-web) is the separate Phoenix and Ash application. The engine has no database, HTTP server, Python runtime, or BEAM dependency.

The engine owns QR planning, Reed-Solomon correction budgets, artwork scoring, rendering, image preparation, and output verification. It reuses Nayuki for encoding, ZXing-C++ for independent decoding, Blend2D for raster geometry, and native image codecs. Dependencies are pinned to source revisions and archive hashes. See [third-party notices](../THIRD_PARTY.md).

## Build

Install a C++20 compiler, mise, and the tools in `mise.toml`. Windows requires Visual Studio Build Tools with the C++ workload. Linux and macOS use their native C/C++ toolchains.

```sh
mise install
mise exec -- cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
mise exec -- cmake --build build --config Release --parallel 1
mise exec -- ctest --test-dir build -C Release --output-on-failure
mise exec -- cmake --install build --config Release --prefix dist --component OpenQR
```

The installed SDK contains the shared library, CLI, C header, CMake package, and third-party licence notices. Native dependencies are linked into OpenQR. The platform C/C++ runtime remains a system requirement. A first build downloads the pinned component sources.

The CI matrix builds and executes native tests on Windows, Linux, and macOS for x86_64 and ARM64. A matrix definition alone does not establish passing platform qualification. Local verification results should be reported separately from CI results.

## Use

```c
#include <openqr/openqr.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *payload = "https://example.com";
    openqr_request request;
    openqr_request_init(&request);
    request.payload = (openqr_bytes){(const uint8_t *)payload, strlen(payload)};
    request.options.version = 5;
    openqr_result *result = NULL;
    openqr_error *error = NULL;
    if (openqr_generate(&request, &result, &error) != OPENQR_OK) {
        fprintf(stderr, "%s\n", openqr_error_message(error));
        openqr_error_destroy(error);
        return 1;
    }
    openqr_entry entry;
    openqr_result_entry(result, 0, &entry);
    FILE *file = fopen("qr.png", "wb");
    if (file) {
        fwrite(entry.image.data, 1, entry.image.size, file);
        fclose(file);
    }
    openqr_result_destroy(result);
    return file ? 0 : 1;
}
```

Use `find_package(OpenQR CONFIG REQUIRED)` and link `OpenQR::openqr` from CMake. The CLI accepts `--payload`, `--reference`, `--mode`, `--version`, `--seed`, and rendering options. `openqr --help` lists the full contract. Results include image files, SVG where applicable, previews, and a JSON manifest.

Bindings live under `bindings/`: Python uses ctypes, .NET uses P/Invoke, Java uses the Foreign Function and Memory API, Rust uses the C ABI with owned result lifetimes, Go uses CGO, and Elixir uses a dirty CPU NIF. Bindings use the same native planner and verifier.

| Binding | Consumer setup |
| --- | --- |
| C or C++ | Install the SDK and link `OpenQR::openqr`, or include `openqr/openqr.h` and link the shared library. |
| Python 3.10+ | Install `bindings/python` and set `OPENQR_LIBRARY` to the SDK shared library when required. The binding has no runtime package dependencies. |
| .NET 8+ | Reference `bindings/dotnet/OpenQR/OpenQR.csproj`, use `OpenQR.Engine.Generate`, and set `OPENQR_LIBRARY` to the SDK shared library. |
| Java 22+ | Compile `bindings/java/src/main/java/org/openqr/OpenQR.java`, construct `new OpenQR(Path.of(libraryPath))`, and run with `--enable-native-access=ALL-UNNAMED`. |
| Rust 1.85+ | Use `bindings/rust` as a path dependency and set `OPENQR_LIB_DIR` to the SDK library directory. The platform loader must also find the shared library at runtime. |
| Go | Use the module in `bindings/go`, enable CGO, and set `CGO_CFLAGS` and `CGO_LDFLAGS` to the SDK include and library directories. Use a C compiler compatible with the target SDK. |
| Elixir 1.18+ | Add `{:openqr_engine, path: "../openqr/bindings/elixir"}`. Mix builds the native binding. Set `OPENQR_SDK_DIR` to an installed SDK to compile only the binding, or `OPENQR_SOURCE_DIR` to use another complete engine checkout. |

Use the style catalog and SVG preview APIs for selectors in a host application. Those previews come from the same geometry used to render QR output. Rust borrows result buffers until its batch is dropped. The managed bindings copy buffers before releasing the native result.

```python
from openqr import generate

batch = generate("https://example.com", version=5, seed=42)
with open("qr.png", "wb") as output:
    output.write(batch.results[0].image)
```

```elixir
{:ok, batch} = OpenQR.Engine.generate("https://example.com", version: 5, seed: 42)
File.write!("qr.png", hd(batch.results).image)
```

## Public contract

Initialise requests and options with their API functions. `version = 0` searches all 40 versions and returns successful entries with reasons for skipped versions. Payloads are pointer-length byte spans, so embedded NUL bytes and leading or trailing whitespace are preserved. Style IDs are discoverable through the API. Seed zero is valid when `OPENQR_SEED_SET` is present.

Error correction searches H, Q, M, then L by default. Set exactly one `OPENQR_ERROR_CORRECTION_L`, `OPENQR_ERROR_CORRECTION_M`, `OPENQR_ERROR_CORRECTION_Q`, or `OPENQR_ERROR_CORRECTION_H` flag to restrict generation to that level. Explicit levels never fall back or boost to another level. The CLI accepts `--error-correction L|M|Q|H`. Python and Elixir accept `error_correction="M"` and `error_correction: "M"`, respectively. Recovery reserves still apply to every block, so a selected level may not fit a chosen version. The flags preserve ABI version 1 and its request and options layouts.

Input spans are borrowed only for the synchronous generation call. A result owns its images, previews, SVG, payloads, block arrays, and text views. Those views remain valid until `openqr_result_destroy`. Errors have their own `openqr_error_destroy`. Allocations are freed by the library that created them. Independent requests can run concurrently. No C++ exception crosses the C boundary.

`openqr_inspect_image` decodes a still image into unchanged RGBA pixels and checks its QR content against the expected payload bytes with the same independent decoder used during generation. Inspection returns dimensions and a verification flag. A readable image with different or unreadable QR content returns `verified = 0`. The pixel buffer is owned by the caller, remains valid until `openqr_buffer_destroy`, and is read through `openqr_buffer_bytes`. The Elixir binding exposes this as `OpenQR.Engine.inspect_image(encoded, expected_payload)`, returning `{:ok, %{pixels: pixels, width: width, height: height, verified: verified}}` or `{:error, reason}`. Image input limits apply, and animated images are rejected.

Reference input supports PNG/APNG, JPEG, WebP, GIF, and ICO. Mosaic mode accepts still artwork. Picture mode accepts animation and retains timing and looping in GIF output. PNG candidates are checked before lossless compression so unreadable rasters can be rejected early. Each result is accepted only after decoding every serialized frame and matching its actual payload bytes. Mosaic SVG uses the same prepared geometry and is written only after raster verification. Recovery reserves apply independently to each Reed-Solomon block. The quiet zone is white and four modules wide. With colour enabled, Mosaic uses the local reference colour for each dark data module, including modules in pale image regions. It compresses those tones to keep contrast against the white canvas and leaves finder and other fixed patterns black. The existing exposure search can darken the artwork further if needed to decode the result.

References are limited to 10 MiB, 16 million pixels per source frame, and 60 frames. Animated references are normalised one frame at a time. Picture output is limited to 100 million aggregate pixels. The reusable reference output contains normalised artwork before contrast and brightness, with alpha retained. Backgrounds, contrast, brightness, colour, and artistic styles are explicit inputs.

Website discovery, icon downloading, authentication, and persistence belong to the host application. A host can fetch an icon once and pass its bytes as artwork. This keeps network policy and credentials outside the processing library.

## Reference fidelity

The implementation retains the ordered correction-level search, mask and rotation candidates, per-block codeword budgets, artwork fractions, exposure retries, and verified-frame acceptance. Source-derived native tests cover fixed-mask encoding across all QR versions, block ownership, seeded random sequences, planner edits, scoring, and image processing. Integration tests exercise the public C API through Python and decode outputs with an independent ZXing installation.

The native rasterizer, automatic encoder mask selection, and GIF palette generation can produce different pixels or accepted candidates from branded-qr. Reusable input is consistently normalised across callers. URL variation covers the existing website-input fixtures, while full IDNA2003 normalisation, Nameprep, and bidirectional round-trip equivalence have not been established. These are explicit native contract choices. A passing native test suite does not establish byte-for-byte equivalence with every original image.

```sh
mise exec -- uv venv build/test-env
mise exec -- uv pip sync --python build/test-env/bin/python --require-hashes tests/requirements.txt
mise exec -- build/test-env/bin/python -m pytest -q -p no:cacheprovider --basetemp=build/test-tmp tests
```

On Windows, use `build/test-env/Scripts/python.exe`. Set `OPENQR_LIBRARY` when the native library is outside the standard build or system library paths. The CLI uses an embedded UTF-8 process manifest, supported on Windows 10 version 1903 and later, so payloads and file paths retain Unicode without changing the system locale.
