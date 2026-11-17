use openqr::{
    ErrorCorrection, ImageFormat, Options, StyleCategory, generate, style_preview, styles,
};

#[test]
fn generate_literal_bytes_and_borrow_every_result_buffer() {
    let options = Options {
        version: 3,
        scale: 3,
        seed: Some(42),
        ..Options::default()
    };
    let batch = generate(b"A\0B", None, None, &options).unwrap();
    assert_eq!(batch.seed(), 42);
    assert_eq!(batch.len(), 1);
    let result = batch.get(0).unwrap();
    assert_eq!(result.payload, b"A\0B");
    assert_eq!(result.image_format, ImageFormat::Png);
    assert_eq!(&result.image[..8], b"\x89PNG\r\n\x1a\n");
    assert!(!result.preview.is_empty());
    assert!(result.svg.is_some());
    assert_eq!(result.verified_frames, result.total_frames);
    assert_eq!(result.block_errors.len(), result.block_capacities.len());
    assert!(batch.get(1).is_err());
    assert!(batch.skipped().is_empty());
    assert!(batch.reference_input().is_none());
    assert!(
        style_preview(StyleCategory::Body, 0)
            .unwrap()
            .starts_with(b"<?xml")
    );
    assert!(style_preview(StyleCategory::Body, usize::MAX).is_err());
    assert!(
        styles(StyleCategory::Body)
            .iter()
            .any(|(id, _)| *id == "rounded")
    );
}

#[test]
fn native_errors_are_owned_and_supplied_empty_reference_is_distinct() {
    let options = Options {
        version: 3,
        scale: 3,
        ..Options::default()
    };
    let error = generate(b"", None, None, &options).err().unwrap();
    assert_eq!(error.status, 1);
    assert!(!error.message.is_empty());
    let error = generate(b"QR", Some(b""), None, &options).err().unwrap();
    assert_eq!(error.status, 1);
    assert!(error.message.contains("empty"));
}

#[test]
fn requested_error_correction_is_preserved() {
    assert_eq!(Options::default().error_correction, None);
    for (correction, expected) in [
        (ErrorCorrection::Low, 'L'),
        (ErrorCorrection::Medium, 'M'),
        (ErrorCorrection::Quartile, 'Q'),
        (ErrorCorrection::High, 'H'),
    ] {
        let options = Options {
            version: 3,
            scale: 3,
            seed: Some(42),
            error_correction: Some(correction),
            ..Options::default()
        };
        let batch = generate(b"QR", None, None, &options).unwrap();
        assert_eq!(batch.len(), 1);
        assert_eq!(batch.get(0).unwrap().error_correction, expected);
    }
}
