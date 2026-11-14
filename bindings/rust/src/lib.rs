use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fmt;
use std::mem::MaybeUninit;
use std::ptr::{self, NonNull};
use std::slice;

#[repr(C)]
#[derive(Clone, Copy)]
struct NativeBytes {
    data: *const u8,
    size: usize,
}

#[repr(C)]
struct NativeOptions {
    struct_size: u32,
    abi_version: u32,
    version: u32,
    scale: u32,
    recovery_margin: u32,
    seed: u32,
    flags: u32,
    mode: c_int,
    contrast: f64,
    brightness: f64,
    background_rgba: u32,
    body_shape: *const c_char,
    eye_frame: *const c_char,
    eye_ball: *const c_char,
}

#[repr(C)]
struct NativeRequest {
    struct_size: u32,
    abi_version: u32,
    payload: NativeBytes,
    reference: NativeBytes,
    background: NativeBytes,
    options: NativeOptions,
}

#[repr(C)]
struct NativeEntry {
    version: u32,
    seed: u32,
    mode: c_int,
    format: c_int,
    verified_frames: u32,
    total_frames: u32,
    changed_codewords: u32,
    layout_attempts: u32,
    error_correction: c_char,
    artwork_fraction: f64,
    color_exposure: f64,
    payload: NativeBytes,
    image: NativeBytes,
    preview: NativeBytes,
    svg: NativeBytes,
    target_preview: NativeBytes,
    block_errors: *const i32,
    block_capacities: *const i32,
    block_count: usize,
}

unsafe extern "C" {
    fn openqr_abi_version() -> u32;
    fn openqr_version() -> *const c_char;
    fn openqr_options_init(options: *mut NativeOptions);
    fn openqr_request_init(request: *mut NativeRequest);
    fn openqr_generate(
        request: *const NativeRequest,
        result: *mut *mut c_void,
        error: *mut *mut c_void,
    ) -> c_int;
    fn openqr_result_destroy(result: *mut c_void);
    fn openqr_result_count(result: *const c_void) -> usize;
    fn openqr_result_seed(result: *const c_void) -> u32;
    fn openqr_result_entry(result: *const c_void, index: usize, entry: *mut NativeEntry) -> c_int;
    fn openqr_result_skipped_count(result: *const c_void) -> usize;
    fn openqr_result_skipped_version(result: *const c_void, index: usize) -> u32;
    fn openqr_result_skipped_reason(result: *const c_void, index: usize) -> *const c_char;
    fn openqr_result_reference_preview(result: *const c_void) -> NativeBytes;
    fn openqr_result_reference_input(result: *const c_void) -> NativeBytes;
    fn openqr_error_message(error: *const c_void) -> *const c_char;
    fn openqr_error_destroy(error: *mut c_void);
    fn openqr_style_count(category: c_int) -> usize;
    fn openqr_style_id(category: c_int, index: usize) -> *const c_char;
    fn openqr_style_label(category: c_int, index: usize) -> *const c_char;
    fn openqr_style_preview(
        category: c_int,
        index: usize,
        buffer: *mut *mut c_void,
        error: *mut *mut c_void,
    ) -> c_int;
    fn openqr_buffer_bytes(buffer: *const c_void) -> NativeBytes;
    fn openqr_buffer_destroy(buffer: *mut c_void);
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Mode {
    Mosaic = 0,
    Picture = 1,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ImageFormat {
    Png,
    Gif,
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum StyleCategory {
    Body = 0,
    EyeFrame = 1,
    EyeBall = 2,
}

#[derive(Clone, Debug)]
pub struct Options {
    pub version: u32,
    pub scale: u32,
    pub recovery_margin: u32,
    pub seed: Option<u32>,
    pub color: bool,
    pub vary_url: bool,
    pub randomize_seed: bool,
    pub mode: Mode,
    pub contrast: f64,
    pub brightness: f64,
    pub background_rgba: u32,
    pub body_shape: String,
    pub eye_frame: String,
    pub eye_ball: String,
}

impl Default for Options {
    fn default() -> Self {
        assert_eq!(
            unsafe { openqr_abi_version() },
            1,
            "The loaded OpenQR library does not support ABI version 1."
        );
        let mut raw = MaybeUninit::<NativeOptions>::uninit();
        unsafe {
            openqr_options_init(raw.as_mut_ptr());
            let raw = raw.assume_init();
            Self {
                version: raw.version,
                scale: raw.scale,
                recovery_margin: raw.recovery_margin,
                seed: (raw.flags & 4 != 0).then_some(raw.seed),
                color: raw.flags & 1 != 0,
                vary_url: raw.flags & 2 != 0,
                randomize_seed: raw.flags & 8 != 0,
                mode: if raw.mode == 0 {
                    Mode::Mosaic
                } else {
                    Mode::Picture
                },
                contrast: raw.contrast,
                brightness: raw.brightness,
                background_rgba: raw.background_rgba,
                body_shape: CStr::from_ptr(raw.body_shape)
                    .to_string_lossy()
                    .into_owned(),
                eye_frame: CStr::from_ptr(raw.eye_frame).to_string_lossy().into_owned(),
                eye_ball: CStr::from_ptr(raw.eye_ball).to_string_lossy().into_owned(),
            }
        }
    }
}

#[derive(Debug)]
pub struct Error {
    pub status: i32,
    pub message: String,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.message)
    }
}

impl std::error::Error for Error {}

struct NativeError(*mut c_void);

impl Drop for NativeError {
    fn drop(&mut self) {
        unsafe { openqr_error_destroy(self.0) }
    }
}

struct NativeBuffer(NonNull<c_void>);

impl Drop for NativeBuffer {
    fn drop(&mut self) {
        unsafe { openqr_buffer_destroy(self.0.as_ptr()) }
    }
}

#[derive(Debug)]
pub struct Generated<'a> {
    pub version: u32,
    pub seed: u32,
    pub mode: Mode,
    pub image_format: ImageFormat,
    pub verified_frames: u32,
    pub total_frames: u32,
    pub changed_codewords: u32,
    pub layout_attempts: u32,
    pub error_correction: char,
    pub artwork_fraction: Option<f64>,
    pub color_exposure: f64,
    pub payload: &'a [u8],
    pub image: &'a [u8],
    pub preview: &'a [u8],
    pub svg: Option<&'a [u8]>,
    pub target_preview: Option<&'a [u8]>,
    pub block_errors: &'a [i32],
    pub block_capacities: &'a [i32],
}

#[derive(Debug)]
pub struct SkippedVersion<'a> {
    pub version: u32,
    pub reason: &'a str,
}

pub struct GeneratedBatch {
    native: NonNull<c_void>,
}

// The C ABI result is immutable after generation, and its allocator has no thread affinity.
unsafe impl Send for GeneratedBatch {}
unsafe impl Sync for GeneratedBatch {}

impl Drop for GeneratedBatch {
    fn drop(&mut self) {
        unsafe { openqr_result_destroy(self.native.as_ptr()) }
    }
}

impl GeneratedBatch {
    pub fn len(&self) -> usize {
        unsafe { openqr_result_count(self.native.as_ptr()) }
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    pub fn seed(&self) -> u32 {
        unsafe { openqr_result_seed(self.native.as_ptr()) }
    }

    pub fn get(&self, index: usize) -> Result<Generated<'_>, Error> {
        let mut raw = MaybeUninit::<NativeEntry>::uninit();
        let status = unsafe { openqr_result_entry(self.native.as_ptr(), index, raw.as_mut_ptr()) };
        if status != 0 {
            return Err(Error {
                status,
                message: "The requested QR result does not exist.".into(),
            });
        }
        unsafe {
            let raw = raw.assume_init();
            Ok(Generated {
                version: raw.version,
                seed: raw.seed,
                mode: if raw.mode == 0 {
                    Mode::Mosaic
                } else {
                    Mode::Picture
                },
                image_format: if raw.format == 0 {
                    ImageFormat::Png
                } else {
                    ImageFormat::Gif
                },
                verified_frames: raw.verified_frames,
                total_frames: raw.total_frames,
                changed_codewords: raw.changed_codewords,
                layout_attempts: raw.layout_attempts,
                error_correction: raw.error_correction as u8 as char,
                artwork_fraction: (raw.artwork_fraction >= 0.0).then_some(raw.artwork_fraction),
                color_exposure: raw.color_exposure,
                payload: if raw.payload.size == 0 {
                    &[]
                } else {
                    slice::from_raw_parts(raw.payload.data, raw.payload.size)
                },
                image: if raw.image.size == 0 {
                    &[]
                } else {
                    slice::from_raw_parts(raw.image.data, raw.image.size)
                },
                preview: if raw.preview.size == 0 {
                    &[]
                } else {
                    slice::from_raw_parts(raw.preview.data, raw.preview.size)
                },
                svg: (raw.svg.size != 0).then(|| slice::from_raw_parts(raw.svg.data, raw.svg.size)),
                target_preview: (raw.target_preview.size != 0).then(|| {
                    slice::from_raw_parts(raw.target_preview.data, raw.target_preview.size)
                }),
                block_errors: if raw.block_count == 0 {
                    &[]
                } else {
                    slice::from_raw_parts(raw.block_errors, raw.block_count)
                },
                block_capacities: if raw.block_count == 0 {
                    &[]
                } else {
                    slice::from_raw_parts(raw.block_capacities, raw.block_count)
                },
            })
        }
    }

    pub fn entries(&self) -> impl ExactSizeIterator<Item = Result<Generated<'_>, Error>> + '_ {
        (0..self.len()).map(|index| self.get(index))
    }

    pub fn skipped(&self) -> Vec<SkippedVersion<'_>> {
        let count = unsafe { openqr_result_skipped_count(self.native.as_ptr()) };
        (0..count)
            .map(|index| unsafe {
                SkippedVersion {
                    version: openqr_result_skipped_version(self.native.as_ptr(), index),
                    reason: CStr::from_ptr(openqr_result_skipped_reason(
                        self.native.as_ptr(),
                        index,
                    ))
                    .to_str()
                    .expect("OpenQR returned invalid UTF-8"),
                }
            })
            .collect()
    }

    pub fn reference_preview(&self) -> Option<&[u8]> {
        let bytes = unsafe { openqr_result_reference_preview(self.native.as_ptr()) };
        (bytes.size != 0).then(|| unsafe { slice::from_raw_parts(bytes.data, bytes.size) })
    }

    pub fn reference_input(&self) -> Option<&[u8]> {
        let bytes = unsafe { openqr_result_reference_input(self.native.as_ptr()) };
        (bytes.size != 0).then(|| unsafe { slice::from_raw_parts(bytes.data, bytes.size) })
    }
}

pub fn generate(
    payload: &[u8],
    reference: Option<&[u8]>,
    background: Option<&[u8]>,
    options: &Options,
) -> Result<GeneratedBatch, Error> {
    if unsafe { openqr_abi_version() } != 1 {
        return Err(Error {
            status: 1,
            message: "The loaded OpenQR library does not support ABI version 1.".into(),
        });
    }
    let body = CString::new(options.body_shape.as_str()).map_err(|_| Error {
        status: 1,
        message: "The body style contains a NUL byte.".into(),
    })?;
    let frame = CString::new(options.eye_frame.as_str()).map_err(|_| Error {
        status: 1,
        message: "The eye frame style contains a NUL byte.".into(),
    })?;
    let ball = CString::new(options.eye_ball.as_str()).map_err(|_| Error {
        status: 1,
        message: "The eye center style contains a NUL byte.".into(),
    })?;
    let mut request = MaybeUninit::<NativeRequest>::uninit();
    unsafe { openqr_request_init(request.as_mut_ptr()) };
    let mut request = unsafe { request.assume_init() };
    request.payload = NativeBytes {
        data: payload.as_ptr(),
        size: payload.len(),
    };
    request.reference = match reference {
        Some(bytes) => NativeBytes {
            data: bytes.as_ptr(),
            size: bytes.len(),
        },
        None => NativeBytes {
            data: ptr::null(),
            size: 0,
        },
    };
    request.background = match background {
        Some(bytes) => NativeBytes {
            data: bytes.as_ptr(),
            size: bytes.len(),
        },
        None => NativeBytes {
            data: ptr::null(),
            size: 0,
        },
    };
    request.options.version = options.version;
    request.options.scale = options.scale;
    request.options.recovery_margin = options.recovery_margin;
    request.options.seed = options.seed.unwrap_or(0);
    request.options.flags = u32::from(options.color)
        | (u32::from(options.vary_url) << 1)
        | (u32::from(options.seed.is_some()) << 2)
        | (u32::from(options.randomize_seed) << 3);
    request.options.mode = options.mode as c_int;
    request.options.contrast = options.contrast;
    request.options.brightness = options.brightness;
    request.options.background_rgba = options.background_rgba;
    request.options.body_shape = body.as_ptr();
    request.options.eye_frame = frame.as_ptr();
    request.options.eye_ball = ball.as_ptr();
    let mut result = ptr::null_mut();
    let mut error = ptr::null_mut();
    let status = unsafe { openqr_generate(&request, &mut result, &mut error) };
    let error = NativeError(error);
    if status != 0 {
        let message = if error.0.is_null() {
            format!("OpenQR generation failed with status {status}.")
        } else {
            unsafe {
                CStr::from_ptr(openqr_error_message(error.0))
                    .to_string_lossy()
                    .into_owned()
            }
        };
        return Err(Error { status, message });
    }
    let native = NonNull::new(result).ok_or_else(|| Error {
        status: 5,
        message: "OpenQR returned no result.".into(),
    })?;
    Ok(GeneratedBatch { native })
}

pub fn version() -> &'static str {
    unsafe {
        CStr::from_ptr(openqr_version())
            .to_str()
            .expect("OpenQR returned invalid UTF-8")
    }
}

pub fn styles(category: StyleCategory) -> Vec<(&'static str, &'static str)> {
    let count = unsafe { openqr_style_count(category as c_int) };
    (0..count)
        .map(|index| unsafe {
            (
                CStr::from_ptr(openqr_style_id(category as c_int, index))
                    .to_str()
                    .expect("OpenQR returned invalid UTF-8"),
                CStr::from_ptr(openqr_style_label(category as c_int, index))
                    .to_str()
                    .expect("OpenQR returned invalid UTF-8"),
            )
        })
        .collect()
}

pub fn style_preview(category: StyleCategory, index: usize) -> Result<Vec<u8>, Error> {
    let mut buffer = ptr::null_mut();
    let mut error = ptr::null_mut();
    let status = unsafe { openqr_style_preview(category as c_int, index, &mut buffer, &mut error) };
    let error = NativeError(error);
    if status != 0 {
        let message = if error.0.is_null() {
            format!("OpenQR style preview failed with status {status}.")
        } else {
            unsafe {
                CStr::from_ptr(openqr_error_message(error.0))
                    .to_string_lossy()
                    .into_owned()
            }
        };
        return Err(Error { status, message });
    }
    let buffer = NativeBuffer(NonNull::new(buffer).ok_or_else(|| Error {
        status: 5,
        message: "OpenQR returned no style preview.".into(),
    })?);
    let bytes = unsafe { openqr_buffer_bytes(buffer.0.as_ptr()) };
    Ok(if bytes.size == 0 {
        Vec::new()
    } else {
        unsafe { slice::from_raw_parts(bytes.data, bytes.size).to_vec() }
    })
}
