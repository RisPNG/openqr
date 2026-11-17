package org.openqr;

import java.nio.file.Path;
import java.util.Arrays;

public final class Smoke {
    public static void main(String[] arguments) {
        var engine = new OpenQR(Path.of(arguments[0]));
        var options = new OpenQR.Options();
        options.version = 3;
        options.scale = 3;
        options.seed = 0xffffffffL;
        byte[] payload = {32, 65, 0, 66, 10, 32};
        var batch = engine.generate(payload, null, options);
        var entry = batch.results().getFirst();
        if (batch.results().size() != 1 || batch.seed() != 0xffffffffL || !Arrays.equals(payload, entry.payload()) || entry.verifiedFrames() != 1 || entry.totalFrames() != 1 || entry.version() != 3 || entry.image().length < 8 || !Arrays.equals(Arrays.copyOf(entry.image(), 8), new byte[] {(byte)137, 80, 78, 71, 13, 10, 26, 10}))
            throw new AssertionError("The Java/native result does not match the request.");
        for (int category = 0; category < 3; ++category) {
            var styles = engine.styles(category);
            if (styles.isEmpty()) throw new AssertionError("Missing native style catalog.");
            var preview = new String(engine.stylePreview(category, styles.keySet().iterator().next()), java.nio.charset.StandardCharsets.UTF_8);
            if (!preview.contains("<svg ")) throw new AssertionError("Invalid native style preview.");
        }
        for (char correction : new char[] {'L', 'M', 'Q', 'H'}) {
            options.errorCorrection = correction;
            var corrected = engine.generate(payload, null, options);
            if (corrected.results().getFirst().errorCorrection() != correction)
                throw new AssertionError("The requested error correction was changed.");
        }
        options.errorCorrection = 'A';
        try {
            engine.generate(payload, null, options);
            throw new AssertionError("Invalid error correction was accepted.");
        } catch (IllegalArgumentException error) { }
        options.errorCorrection = null;
        options.scale = 4;
        try {
            engine.generate(payload, null, options);
            throw new AssertionError("The native option error was lost.");
        } catch (OpenQR.OpenQRException error) {
            if (error.status() != 1) throw error;
        }
        System.out.println("OpenQR " + engine.version() + ": Java ABI smoke passed");
    }
}
