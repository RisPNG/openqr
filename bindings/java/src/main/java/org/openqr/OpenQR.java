package org.openqr;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.StructLayout;
import java.lang.foreign.SymbolLookup;
import java.lang.invoke.MethodHandle;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import static java.lang.foreign.MemoryLayout.PathElement.groupElement;
import static java.lang.foreign.ValueLayout.*;

public final class OpenQR {
    private static final StructLayout BYTES = MemoryLayout.structLayout(ADDRESS.withName("data"), JAVA_LONG.withName("size"));
    private static final StructLayout OPTIONS = MemoryLayout.structLayout(
        JAVA_INT.withName("struct_size"), JAVA_INT.withName("abi_version"), JAVA_INT.withName("version"), JAVA_INT.withName("scale"),
        JAVA_INT.withName("recovery_margin"), JAVA_INT.withName("seed"), JAVA_INT.withName("flags"), JAVA_INT.withName("mode"),
        JAVA_DOUBLE.withName("contrast"), JAVA_DOUBLE.withName("brightness"), JAVA_INT.withName("background_rgba"), MemoryLayout.paddingLayout(4),
        ADDRESS.withName("body_shape"), ADDRESS.withName("eye_frame"), ADDRESS.withName("eye_ball"));
    private static final StructLayout REQUEST = MemoryLayout.structLayout(
        JAVA_INT.withName("struct_size"), JAVA_INT.withName("abi_version"), BYTES.withName("payload"), BYTES.withName("reference"), BYTES.withName("background"), OPTIONS.withName("options"));
    private static final StructLayout ENTRY = MemoryLayout.structLayout(
        JAVA_INT.withName("version"), JAVA_INT.withName("seed"), JAVA_INT.withName("mode"), JAVA_INT.withName("format"),
        JAVA_INT.withName("verified_frames"), JAVA_INT.withName("total_frames"), JAVA_INT.withName("changed_codewords"), JAVA_INT.withName("layout_attempts"),
        JAVA_BYTE.withName("error_correction"), MemoryLayout.paddingLayout(7), JAVA_DOUBLE.withName("artwork_fraction"), JAVA_DOUBLE.withName("color_exposure"),
        BYTES.withName("payload"), BYTES.withName("image"), BYTES.withName("preview"), BYTES.withName("svg"), BYTES.withName("target_preview"),
        ADDRESS.withName("block_errors"), ADDRESS.withName("block_capacities"), JAVA_LONG.withName("block_count"));

    public static final class Options {
        public int version = 0;
        public int scale = 12;
        public int recoveryMargin = 2;
        public Long seed = null;
        public Character errorCorrection = null;
        public boolean color = true;
        public boolean varyUrl = false;
        public boolean randomizeSeed = false;
        public boolean picture = false;
        public double contrast = 1;
        public double brightness = 1;
        public int backgroundRgba = 0;
        public byte[] backgroundImage = null;
        public String bodyShape = "rounded";
        public String eyeFrame = "rounded-dots";
        public String eyeBall = "dots";
    }

    public record Generated(byte[] image, byte[] preview, String imageFormat, byte[] svg, byte[] payload, int version, long seed,
                            boolean picture, int verifiedFrames, int totalFrames, int changedCodewords, int layoutAttempts,
                            char errorCorrection, Double artworkFraction, double colorExposure, int[] blockErrors, int[] blockCapacities, byte[] targetPreview) {}
    public record SkippedVersion(int version, String reason) {}
    public record GeneratedBatch(List<Generated> results, List<SkippedVersion> skipped, long seed, byte[] payload, byte[] referencePreview, byte[] referenceInput) {}

    public static final class OpenQRException extends RuntimeException {
        private final int status;
        public OpenQRException(String message, int status) { super(message); this.status = status; }
        public int status() { return status; }
    }

    private final Map<String, MethodHandle> functions;

    public OpenQR(Path library) {
        if (ADDRESS.byteSize() != 8) throw new UnsupportedOperationException("OpenQR Java bindings require a 64-bit process.");
        var linker = Linker.nativeLinker();
        var symbols = SymbolLookup.libraryLookup(library.toAbsolutePath(), Arena.global());
        var descriptors = new LinkedHashMap<String, FunctionDescriptor>();
        descriptors.put("openqr_abi_version", FunctionDescriptor.of(JAVA_INT));
        descriptors.put("openqr_version", FunctionDescriptor.of(ADDRESS));
        descriptors.put("openqr_request_init", FunctionDescriptor.ofVoid(ADDRESS));
        descriptors.put("openqr_generate", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, ADDRESS));
        descriptors.put("openqr_result_destroy", FunctionDescriptor.ofVoid(ADDRESS));
        descriptors.put("openqr_result_count", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        descriptors.put("openqr_result_seed", FunctionDescriptor.of(JAVA_INT, ADDRESS));
        descriptors.put("openqr_result_entry", FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS));
        descriptors.put("openqr_result_skipped_count", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        descriptors.put("openqr_result_skipped_version", FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_LONG));
        descriptors.put("openqr_result_skipped_reason", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG));
        descriptors.put("openqr_result_reference_preview", FunctionDescriptor.of(BYTES, ADDRESS));
        descriptors.put("openqr_result_reference_input", FunctionDescriptor.of(BYTES, ADDRESS));
        descriptors.put("openqr_error_message", FunctionDescriptor.of(ADDRESS, ADDRESS));
        descriptors.put("openqr_error_destroy", FunctionDescriptor.ofVoid(ADDRESS));
        descriptors.put("openqr_style_count", FunctionDescriptor.of(JAVA_LONG, JAVA_INT));
        descriptors.put("openqr_style_id", FunctionDescriptor.of(ADDRESS, JAVA_INT, JAVA_LONG));
        descriptors.put("openqr_style_label", FunctionDescriptor.of(ADDRESS, JAVA_INT, JAVA_LONG));
        descriptors.put("openqr_style_preview", FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_LONG, ADDRESS, ADDRESS));
        descriptors.put("openqr_buffer_bytes", FunctionDescriptor.of(BYTES, ADDRESS));
        descriptors.put("openqr_buffer_destroy", FunctionDescriptor.ofVoid(ADDRESS));
        var handles = new LinkedHashMap<String, MethodHandle>();
        for (var descriptor : descriptors.entrySet())
            handles.put(descriptor.getKey(), linker.downcallHandle(symbols.find(descriptor.getKey()).orElseThrow(), descriptor.getValue()));
        functions = Map.copyOf(handles);
        try {
            int abi = (int) functions.get("openqr_abi_version").invokeExact();
            if (abi != 1) throw new UnsupportedOperationException("This binding requires OpenQR ABI version 1.");
        } catch (RuntimeException | Error failure) { throw failure; }
        catch (Throwable failure) { throw new IllegalStateException("Unable to initialize the OpenQR ABI.", failure); }
    }

    public String version() {
        try {
            var text = (MemorySegment) functions.get("openqr_version").invokeExact();
            return text.reinterpret(65536).getString(0);
        } catch (RuntimeException | Error failure) { throw failure; }
        catch (Throwable failure) { throw new IllegalStateException("Unable to read the OpenQR version.", failure); }
    }

    public Map<String, String> styles(int category) {
        if (category < 0 || category > 2) throw new IllegalArgumentException("Style category must be 0 (body), 1 (eye frame), or 2 (eye ball).");
        try {
            long count = (long) functions.get("openqr_style_count").invokeExact(category);
            var result = new LinkedHashMap<String, String>();
            for (long index = 0; index < count; ++index) {
                var id = (MemorySegment) functions.get("openqr_style_id").invokeExact(category, index);
                var label = (MemorySegment) functions.get("openqr_style_label").invokeExact(category, index);
                result.put(id.reinterpret(65536).getString(0), label.reinterpret(65536).getString(0));
            }
            return Collections.unmodifiableMap(result);
        } catch (RuntimeException | Error failure) { throw failure; }
        catch (Throwable failure) { throw new IllegalStateException("Unable to read the OpenQR styles.", failure); }
    }

    public GeneratedBatch generate(String payload, byte[] reference, Options options) {
        return generate(payload.getBytes(StandardCharsets.UTF_8), reference, options);
    }

    public byte[] stylePreview(int category, String id) {
        long index = new ArrayList<>(styles(category).keySet()).indexOf(id);
        if (index < 0) throw new IllegalArgumentException("Choose a valid QR style preview.");
        try (var arena = Arena.ofConfined()) {
            var bufferOut = arena.allocate(ADDRESS);
            var errorOut = arena.allocate(ADDRESS);
            try {
                int status = (int) functions.get("openqr_style_preview").invokeExact(category, index, bufferOut, errorOut);
                if (status != 0) {
                    var error = errorOut.get(ADDRESS, 0);
                    var message = (MemorySegment) functions.get("openqr_error_message").invokeExact(error);
                    throw new OpenQRException(message.address() == 0 ? "OpenQR style preview failed." : message.reinterpret(65536).getString(0), status);
                }
                var data = (MemorySegment) functions.get("openqr_buffer_bytes").invokeExact((java.lang.foreign.SegmentAllocator)arena, bufferOut.get(ADDRESS, 0));
                long length = data.get(JAVA_LONG, BYTES.byteOffset(groupElement("size")));
                return length == 0 ? new byte[0] : data.get(ADDRESS, BYTES.byteOffset(groupElement("data"))).reinterpret(length).toArray(JAVA_BYTE);
            } finally {
                functions.get("openqr_buffer_destroy").invokeExact(bufferOut.get(ADDRESS, 0));
                functions.get("openqr_error_destroy").invokeExact(errorOut.get(ADDRESS, 0));
            }
        } catch (RuntimeException | Error failure) { throw failure; }
        catch (Throwable failure) { throw new IllegalStateException("Unable to render the OpenQR style preview.", failure); }
    }

    public GeneratedBatch generate(byte[] payload, byte[] reference, Options options) {
        if (payload == null) throw new NullPointerException("payload");
        if (options == null) options = new Options();
        if (options.seed != null && (options.seed < 0 || options.seed > 0xffffffffL)) throw new IllegalArgumentException("Seed must be an unsigned 32-bit integer.");
        try (var arena = Arena.ofConfined()) {
            var request = arena.allocate(REQUEST);
            functions.get("openqr_request_init").invokeExact(request);
            if (request.get(JAVA_INT, REQUEST.byteOffset(groupElement("struct_size"))) != REQUEST.byteSize()) throw new UnsupportedOperationException("The native request layout differs from this binding.");
            var nativeOptions = request.asSlice(REQUEST.byteOffset(groupElement("options")), OPTIONS.byteSize());
            if (nativeOptions.get(JAVA_INT, OPTIONS.byteOffset(groupElement("struct_size"))) != OPTIONS.byteSize()) throw new UnsupportedOperationException("The native options layout differs from this binding.");
            String[] dataFields = {"payload", "reference", "background"};
            byte[][] inputs = {payload, reference, options.backgroundImage};
            for (int index = 0; index < dataFields.length; ++index) {
                if (inputs[index] == null) continue;
                var data = inputs[index].length == 0 ? arena.allocate(1) : arena.allocateFrom(JAVA_BYTE, inputs[index]);
                long offset = REQUEST.byteOffset(groupElement(dataFields[index]));
                request.set(ADDRESS, offset + BYTES.byteOffset(groupElement("data")), data);
                request.set(JAVA_LONG, offset + BYTES.byteOffset(groupElement("size")), inputs[index].length);
            }
            int flags = (options.color ? 1 : 0) | (options.varyUrl ? 2 : 0) | (options.seed != null ? 4 : 0) | (options.randomizeSeed ? 8 : 0);
            flags |= options.errorCorrection == null ? 0 : switch (options.errorCorrection) {
                case 'L' -> 16;
                case 'M' -> 32;
                case 'Q' -> 64;
                case 'H' -> 128;
                default -> throw new IllegalArgumentException("Error correction must be L, M, Q, H, or null for automatic selection.");
            };
            nativeOptions.set(JAVA_INT, OPTIONS.byteOffset(groupElement("version")), options.version);
            nativeOptions.set(JAVA_INT, OPTIONS.byteOffset(groupElement("scale")), options.scale);
            nativeOptions.set(JAVA_INT, OPTIONS.byteOffset(groupElement("recovery_margin")), options.recoveryMargin);
            nativeOptions.set(JAVA_INT, OPTIONS.byteOffset(groupElement("seed")), options.seed == null ? 0 : options.seed.intValue());
            nativeOptions.set(JAVA_INT, OPTIONS.byteOffset(groupElement("flags")), flags);
            nativeOptions.set(JAVA_INT, OPTIONS.byteOffset(groupElement("mode")), options.picture ? 1 : 0);
            nativeOptions.set(JAVA_DOUBLE, OPTIONS.byteOffset(groupElement("contrast")), options.contrast);
            nativeOptions.set(JAVA_DOUBLE, OPTIONS.byteOffset(groupElement("brightness")), options.brightness);
            nativeOptions.set(JAVA_INT, OPTIONS.byteOffset(groupElement("background_rgba")), options.backgroundRgba);
            String[] styleFields = {"body_shape", "eye_frame", "eye_ball"};
            String[] styles = {options.bodyShape, options.eyeFrame, options.eyeBall};
            for (int index = 0; index < styleFields.length; ++index) {
                if (styles[index].indexOf('\0') >= 0) throw new IllegalArgumentException("Style identifiers cannot contain NUL.");
                nativeOptions.set(ADDRESS, OPTIONS.byteOffset(groupElement(styleFields[index])), arena.allocateFrom(styles[index]));
            }
            var resultOut = arena.allocate(ADDRESS);
            var errorOut = arena.allocate(ADDRESS);
            try {
                int status = (int) functions.get("openqr_generate").invokeExact(request, resultOut, errorOut);
                var result = resultOut.get(ADDRESS, 0);
                if (status != 0) {
                    var error = errorOut.get(ADDRESS, 0);
                    var message = (MemorySegment) functions.get("openqr_error_message").invokeExact(error);
                    throw new OpenQRException(message.address() == 0 ? "OpenQR generation failed." : message.reinterpret(65536).getString(0), status);
                }
                long count = (long) functions.get("openqr_result_count").invokeExact(result);
                var entries = new ArrayList<Generated>();
                var entry = arena.allocate(ENTRY);
                for (long index = 0; index < count; ++index) {
                    status = (int) functions.get("openqr_result_entry").invokeExact(result, index, entry);
                    if (status != 0) throw new OpenQRException("Unable to read the OpenQR result.", status);
                    var copied = new LinkedHashMap<String, byte[]>();
                    for (var field : List.of("image", "preview", "payload", "svg", "target_preview")) {
                        long offset = ENTRY.byteOffset(groupElement(field));
                        long length = entry.get(JAVA_LONG, offset + BYTES.byteOffset(groupElement("size")));
                        var pointer = entry.get(ADDRESS, offset + BYTES.byteOffset(groupElement("data")));
                        copied.put(field, length == 0 ? new byte[0] : pointer.reinterpret(length).toArray(JAVA_BYTE));
                    }
                    int blockCount = Math.toIntExact(entry.get(JAVA_LONG, ENTRY.byteOffset(groupElement("block_count"))));
                    int[] errors = new int[blockCount], capacities = new int[blockCount];
                    if (blockCount != 0) {
                        errors = entry.get(ADDRESS, ENTRY.byteOffset(groupElement("block_errors"))).reinterpret(blockCount * 4L).toArray(JAVA_INT);
                        capacities = entry.get(ADDRESS, ENTRY.byteOffset(groupElement("block_capacities"))).reinterpret(blockCount * 4L).toArray(JAVA_INT);
                    }
                    double fraction = entry.get(JAVA_DOUBLE, ENTRY.byteOffset(groupElement("artwork_fraction")));
                    entries.add(new Generated(copied.get("image"), copied.get("preview"), entry.get(JAVA_INT, ENTRY.byteOffset(groupElement("format"))) == 0 ? "PNG" : "GIF",
                        copied.get("svg").length == 0 ? null : copied.get("svg"), copied.get("payload"), entry.get(JAVA_INT, ENTRY.byteOffset(groupElement("version"))), Integer.toUnsignedLong(entry.get(JAVA_INT, ENTRY.byteOffset(groupElement("seed")))),
                        entry.get(JAVA_INT, ENTRY.byteOffset(groupElement("mode"))) == 1, entry.get(JAVA_INT, ENTRY.byteOffset(groupElement("verified_frames"))), entry.get(JAVA_INT, ENTRY.byteOffset(groupElement("total_frames"))),
                        entry.get(JAVA_INT, ENTRY.byteOffset(groupElement("changed_codewords"))), entry.get(JAVA_INT, ENTRY.byteOffset(groupElement("layout_attempts"))), (char)entry.get(JAVA_BYTE, ENTRY.byteOffset(groupElement("error_correction"))),
                        fraction < 0 ? null : fraction, entry.get(JAVA_DOUBLE, ENTRY.byteOffset(groupElement("color_exposure"))), errors, capacities, copied.get("target_preview").length == 0 ? null : copied.get("target_preview")));
                }
                long skippedCount = (long) functions.get("openqr_result_skipped_count").invokeExact(result);
                var skipped = new ArrayList<SkippedVersion>();
                for (long index = 0; index < skippedCount; ++index) {
                    int version = (int) functions.get("openqr_result_skipped_version").invokeExact(result, index);
                    var reason = (MemorySegment) functions.get("openqr_result_skipped_reason").invokeExact(result, index);
                    skipped.add(new SkippedVersion(version, reason.reinterpret(65536).getString(0)));
                }
                var references = new LinkedHashMap<String, byte[]>();
                for (var field : List.of("preview", "input")) {
                    var data = (MemorySegment) functions.get("openqr_result_reference_" + field).invokeExact((java.lang.foreign.SegmentAllocator)arena, result);
                    long length = data.get(JAVA_LONG, BYTES.byteOffset(groupElement("size")));
                    var pointer = data.get(ADDRESS, BYTES.byteOffset(groupElement("data")));
                    references.put(field, length == 0 ? null : pointer.reinterpret(length).toArray(JAVA_BYTE));
                }
                int seed = (int) functions.get("openqr_result_seed").invokeExact(result);
                return new GeneratedBatch(List.copyOf(entries), List.copyOf(skipped), Integer.toUnsignedLong(seed), payload.clone(), references.get("preview"), references.get("input"));
            } finally {
                functions.get("openqr_result_destroy").invokeExact(resultOut.get(ADDRESS, 0));
                functions.get("openqr_error_destroy").invokeExact(errorOut.get(ADDRESS, 0));
            }
        } catch (RuntimeException | Error failure) { throw failure; }
        catch (Throwable failure) { throw new IllegalStateException("Unable to call the OpenQR native engine.", failure); }
    }
}
