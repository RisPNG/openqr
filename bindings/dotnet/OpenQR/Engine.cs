using System.Runtime.InteropServices;
using System.Text;

namespace OpenQR;

public sealed record Options
{
    public uint Version { get; init; }
    public uint Scale { get; init; } = 12;
    public uint RecoveryMargin { get; init; } = 2;
    public uint? Seed { get; init; }
    public bool Color { get; init; } = true;
    public bool VaryUrl { get; init; }
    public bool RandomizeSeed { get; init; }
    public bool Picture { get; init; }
    public double Contrast { get; init; } = 1;
    public double Brightness { get; init; } = 1;
    public uint BackgroundRgba { get; init; }
    public byte[]? BackgroundImage { get; init; }
    public string BodyShape { get; init; } = "rounded";
    public string EyeFrame { get; init; } = "rounded-dots";
    public string EyeBall { get; init; } = "dots";
}

public sealed record Generated(byte[] Image, byte[] Preview, string ImageFormat, byte[]? Svg, byte[] Payload, uint Version, uint Seed, bool Picture, uint VerifiedFrames, uint TotalFrames, uint ChangedCodewords, uint LayoutAttempts, char ErrorCorrection, double? ArtworkFraction, double ColorExposure, int[] BlockErrors, int[] BlockCapacities, byte[]? TargetPreview);
public sealed record SkippedVersion(uint Version, string Reason);
public sealed record GeneratedBatch(Generated[] Results, SkippedVersion[] Skipped, uint Seed, byte[] Payload, byte[]? ReferencePreview, byte[]? ReferenceInput);

public sealed class OpenQRException : Exception
{
    public int Status { get; }
    internal OpenQRException(string message, int status) : base(message) => Status = status;
}

public static class Engine
{
    public static string Version => Marshal.PtrToStringUTF8(Native.openqr_version())!;

    public static IReadOnlyDictionary<string, string> Styles(int category)
    {
        if (category is < 0 or > 2) throw new ArgumentOutOfRangeException(nameof(category));
        var styles = new Dictionary<string, string>();
        var count = Native.openqr_style_count(category).ToUInt64();
        for (ulong index = 0; index < count; ++index)
            styles.Add(Marshal.PtrToStringUTF8(Native.openqr_style_id(category, (UIntPtr)index))!, Marshal.PtrToStringUTF8(Native.openqr_style_label(category, (UIntPtr)index))!);
        return styles;
    }

    public static byte[] StylePreview(int category, string id)
    {
        var index = Array.IndexOf(Styles(category).Keys.ToArray(), id);
        if (index < 0) throw new ArgumentException("Choose a valid QR style preview.", nameof(id));
        IntPtr buffer = IntPtr.Zero, error = IntPtr.Zero;
        try
        {
            var status = Native.openqr_style_preview(category, (UIntPtr)index, out buffer, out error);
            if (status != 0) throw new OpenQRException(Marshal.PtrToStringUTF8(Native.openqr_error_message(error)) ?? "OpenQR style preview failed.", status);
            var data = Native.openqr_buffer_bytes(buffer);
            var copied = new byte[checked((int)data.Size.ToUInt64())];
            if (copied.Length != 0) Marshal.Copy(data.Data, copied, 0, copied.Length);
            return copied;
        }
        finally
        {
            Native.openqr_buffer_destroy(buffer);
            Native.openqr_error_destroy(error);
        }
    }

    public static GeneratedBatch Generate(string payload, byte[]? reference = null, Options? options = null) => Generate(Encoding.UTF8.GetBytes(payload), reference, options);

    public static GeneratedBatch Generate(byte[] payload, byte[]? reference = null, Options? options = null)
    {
        ArgumentNullException.ThrowIfNull(payload);
        options ??= new Options();
        Native.openqr_request_init(out var request);
        request.Options.Version = options.Version;
        request.Options.Scale = options.Scale;
        request.Options.RecoveryMargin = options.RecoveryMargin;
        request.Options.Seed = options.Seed ?? 0;
        request.Options.Flags = (options.Color ? 1u : 0u) | (options.VaryUrl ? 2u : 0u) | (options.Seed.HasValue ? 4u : 0u) | (options.RandomizeSeed ? 8u : 0u);
        request.Options.Mode = options.Picture ? 1 : 0;
        request.Options.Contrast = options.Contrast;
        request.Options.Brightness = options.Brightness;
        request.Options.BackgroundRgba = options.BackgroundRgba;
        var allocations = new List<IntPtr>();
        var styleAllocations = new List<IntPtr>();
        IntPtr result = IntPtr.Zero, error = IntPtr.Zero;
        try
        {
            var images = new[] { payload, reference, options.BackgroundImage };
            var spans = new Native.Bytes[3];
            for (var index = 0; index < images.Length; ++index)
            {
                if (images[index] is not byte[] data) continue;
                var pointer = Marshal.AllocHGlobal(Math.Max(data.Length, 1));
                allocations.Add(pointer);
                Marshal.Copy(data, 0, pointer, data.Length);
                spans[index] = new Native.Bytes { Data = pointer, Size = (UIntPtr)data.Length };
            }
            request.Payload = spans[0];
            request.Reference = spans[1];
            request.Background = spans[2];
            var styles = new[] { options.BodyShape, options.EyeFrame, options.EyeBall };
            var pointers = new IntPtr[3];
            for (var index = 0; index < styles.Length; ++index)
            {
                if (styles[index].Contains('\0')) throw new ArgumentException("Style identifiers cannot contain NUL.");
                pointers[index] = Marshal.StringToCoTaskMemUTF8(styles[index]);
                styleAllocations.Add(pointers[index]);
            }
            request.Options.BodyShape = pointers[0];
            request.Options.EyeFrame = pointers[1];
            request.Options.EyeBall = pointers[2];
            var generationStatus = Native.openqr_generate(ref request, out result, out error);
            if (generationStatus != 0) throw new OpenQRException(Marshal.PtrToStringUTF8(Native.openqr_error_message(error)) ?? "OpenQR generation failed.", generationStatus);
            var entries = new Generated[checked((int)Native.openqr_result_count(result).ToUInt64())];
            for (var index = 0; index < entries.Length; ++index)
            {
                var status = Native.openqr_result_entry(result, (UIntPtr)index, out var entry);
                if (status != 0) throw new OpenQRException("Unable to read the OpenQR result.", status);
                var nativeBytes = new[] { entry.Image, entry.Preview, entry.Svg, entry.Payload, entry.TargetPreview };
                var copied = new byte[nativeBytes.Length][];
                for (var field = 0; field < copied.Length; ++field)
                {
                    copied[field] = new byte[checked((int)nativeBytes[field].Size.ToUInt64())];
                    if (copied[field].Length != 0) Marshal.Copy(nativeBytes[field].Data, copied[field], 0, copied[field].Length);
                }
                var blockCount = checked((int)entry.BlockCount.ToUInt64());
                var errors = new int[blockCount];
                var capacities = new int[blockCount];
                if (blockCount != 0)
                {
                    Marshal.Copy(entry.BlockErrors, errors, 0, blockCount);
                    Marshal.Copy(entry.BlockCapacities, capacities, 0, blockCount);
                }
                entries[index] = new Generated(copied[0], copied[1], entry.Format == 0 ? "PNG" : "GIF", copied[2].Length == 0 ? null : copied[2], copied[3], entry.Version, entry.Seed, entry.Mode == 1, entry.VerifiedFrames, entry.TotalFrames, entry.ChangedCodewords, entry.LayoutAttempts, (char)entry.ErrorCorrection, entry.ArtworkFraction < 0 ? null : entry.ArtworkFraction, entry.ColorExposure, errors, capacities, copied[4].Length == 0 ? null : copied[4]);
            }
            var skipped = new SkippedVersion[checked((int)Native.openqr_result_skipped_count(result).ToUInt64())];
            for (var index = 0; index < skipped.Length; ++index)
                skipped[index] = new SkippedVersion(Native.openqr_result_skipped_version(result, (UIntPtr)index), Marshal.PtrToStringUTF8(Native.openqr_result_skipped_reason(result, (UIntPtr)index))!);
            var nativeReferences = new[] { Native.openqr_result_reference_preview(result), Native.openqr_result_reference_input(result) };
            var references = new byte[]?[2];
            for (var index = 0; index < references.Length; ++index)
            {
                var length = checked((int)nativeReferences[index].Size.ToUInt64());
                if (length == 0) continue;
                references[index] = new byte[length];
                Marshal.Copy(nativeReferences[index].Data, references[index]!, 0, length);
            }
            return new GeneratedBatch(entries, skipped, Native.openqr_result_seed(result), payload.ToArray(), references[0], references[1]);
        }
        finally
        {
            Native.openqr_result_destroy(result);
            Native.openqr_error_destroy(error);
            foreach (var pointer in styleAllocations) Marshal.FreeCoTaskMem(pointer);
            foreach (var pointer in allocations) Marshal.FreeHGlobal(pointer);
        }
    }
}

internal static class Native
{
    static Native()
    {
        NativeLibrary.SetDllImportResolver(typeof(Native).Assembly, (name, assembly, path) =>
        {
            var library = Environment.GetEnvironmentVariable("OPENQR_LIBRARY");
            return name == "openqr" && !string.IsNullOrEmpty(library) ? NativeLibrary.Load(library) : IntPtr.Zero;
        });
        if (openqr_abi_version() != 1) throw new NotSupportedException("This binding requires OpenQR ABI version 1.");
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct Bytes { internal IntPtr Data; internal UIntPtr Size; }

    [StructLayout(LayoutKind.Sequential)]
    internal struct Options
    {
        internal uint StructSize, AbiVersion, Version, Scale, RecoveryMargin, Seed, Flags;
        internal int Mode;
        internal double Contrast, Brightness;
        internal uint BackgroundRgba;
        internal IntPtr BodyShape, EyeFrame, EyeBall;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct Request
    {
        internal uint StructSize, AbiVersion;
        internal Bytes Payload, Reference, Background;
        internal Options Options;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct Entry
    {
        internal uint Version, Seed;
        internal int Mode, Format;
        internal uint VerifiedFrames, TotalFrames, ChangedCodewords, LayoutAttempts;
        internal byte ErrorCorrection;
        internal double ArtworkFraction, ColorExposure;
        internal Bytes Payload, Image, Preview, Svg, TargetPreview;
        internal IntPtr BlockErrors, BlockCapacities;
        internal UIntPtr BlockCount;
    }

    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern uint openqr_abi_version();
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr openqr_version();
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern void openqr_request_init(out Request request);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern int openqr_generate(ref Request request, out IntPtr result, out IntPtr error);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern void openqr_result_destroy(IntPtr result);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern UIntPtr openqr_result_count(IntPtr result);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern uint openqr_result_seed(IntPtr result);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern int openqr_result_entry(IntPtr result, UIntPtr index, out Entry entry);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern UIntPtr openqr_result_skipped_count(IntPtr result);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern uint openqr_result_skipped_version(IntPtr result, UIntPtr index);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr openqr_result_skipped_reason(IntPtr result, UIntPtr index);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern Bytes openqr_result_reference_preview(IntPtr result);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern Bytes openqr_result_reference_input(IntPtr result);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr openqr_error_message(IntPtr error);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern void openqr_error_destroy(IntPtr error);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern UIntPtr openqr_style_count(int category);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr openqr_style_id(int category, UIntPtr index);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr openqr_style_label(int category, UIntPtr index);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern int openqr_style_preview(int category, UIntPtr index, out IntPtr buffer, out IntPtr error);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern Bytes openqr_buffer_bytes(IntPtr buffer);
    [DllImport("openqr", CallingConvention = CallingConvention.Cdecl)] internal static extern void openqr_buffer_destroy(IntPtr buffer);
}
