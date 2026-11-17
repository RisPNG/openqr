package openqr

/*
#cgo LDFLAGS: -lopenqr
#include <stdlib.h>
#include <openqr/openqr.h>
*/
import "C"

import (
	"fmt"
	"strings"
	"unsafe"
)

type Mode int

const (
	Mosaic Mode = iota
	Picture
)

type ImageFormat int

const (
	PNG ImageFormat = iota
	GIF
)

type StyleCategory int

const (
	Body StyleCategory = iota
	EyeFrame
	EyeBall
)

type Options struct {
	Version         uint32
	Scale           uint32
	RecoveryMargin  uint32
	Seed            *uint32
	ErrorCorrection byte
	Color           bool
	VaryURL         bool
	RandomizeSeed   bool
	Mode            Mode
	Contrast        float64
	Brightness      float64
	BackgroundRGBA  uint32
	BodyShape       string
	EyeFrame        string
	EyeBall         string
}

type Generated struct {
	Version          uint32
	Seed             uint32
	Mode             Mode
	ImageFormat      ImageFormat
	VerifiedFrames   uint32
	TotalFrames      uint32
	ChangedCodewords uint32
	LayoutAttempts   uint32
	ErrorCorrection  byte
	ArtworkFraction  *float64
	ColorExposure    float64
	Payload          []byte
	Image            []byte
	Preview          []byte
	SVG              []byte
	TargetPreview    []byte
	BlockErrors      []int32
	BlockCapacities  []int32
}

type SkippedVersion struct {
	Version uint32
	Reason  string
}

type GeneratedBatch struct {
	Seed             uint32
	Results          []Generated
	Skipped          []SkippedVersion
	ReferencePreview []byte
	ReferenceInput   []byte
}

type Style struct {
	ID    string
	Label string
}

type Error struct {
	Status  int
	Message string
}

func (e *Error) Error() string {
	return e.Message
}

func DefaultOptions() Options {
	if uint32(C.openqr_abi_version()) != uint32(C.OPENQR_ABI_VERSION) {
		panic("The loaded OpenQR library does not match this binding's ABI version.")
	}
	var native C.openqr_options
	C.openqr_options_init(&native)
	options := Options{
		Version: uint32(native.version), Scale: uint32(native.scale), RecoveryMargin: uint32(native.recovery_margin),
		Color: native.flags&C.OPENQR_COLOR != 0, VaryURL: native.flags&C.OPENQR_VARY_URL != 0,
		RandomizeSeed: native.flags&C.OPENQR_RANDOMIZE_SEED != 0, Mode: Mode(native.mode),
		Contrast: float64(native.contrast), Brightness: float64(native.brightness), BackgroundRGBA: uint32(native.background_rgba),
		BodyShape: C.GoString(native.body_shape), EyeFrame: C.GoString(native.eye_frame), EyeBall: C.GoString(native.eye_ball),
	}
	if native.flags&C.OPENQR_SEED_SET != 0 {
		seed := uint32(native.seed)
		options.Seed = &seed
	}
	return options
}

func Generate(payload, reference, background []byte, options *Options) (GeneratedBatch, error) {
	if uint32(C.openqr_abi_version()) != uint32(C.OPENQR_ABI_VERSION) {
		return GeneratedBatch{}, &Error{Status: int(C.OPENQR_INVALID_ARGUMENT), Message: "The loaded OpenQR library does not match this binding's ABI version."}
	}
	var request C.openqr_request
	C.openqr_request_init(&request)
	if len(payload) != 0 {
		data := C.CBytes(payload)
		defer C.free(data)
		request.payload = C.openqr_bytes{data: (*C.uint8_t)(data), size: C.size_t(len(payload))}
	}
	if reference != nil {
		data := C.malloc(C.size_t(max(1, len(reference))))
		defer C.free(data)
		if len(reference) != 0 {
			copy(unsafe.Slice((*byte)(data), len(reference)), reference)
		}
		request.reference = C.openqr_bytes{data: (*C.uint8_t)(data), size: C.size_t(len(reference))}
	}
	if background != nil {
		data := C.malloc(C.size_t(max(1, len(background))))
		defer C.free(data)
		if len(background) != 0 {
			copy(unsafe.Slice((*byte)(data), len(background)), background)
		}
		request.background = C.openqr_bytes{data: (*C.uint8_t)(data), size: C.size_t(len(background))}
	}
	if options != nil {
		if strings.ContainsRune(options.BodyShape, '\x00') || strings.ContainsRune(options.EyeFrame, '\x00') || strings.ContainsRune(options.EyeBall, '\x00') {
			return GeneratedBatch{}, &Error{Status: int(C.OPENQR_INVALID_ARGUMENT), Message: "QR style IDs cannot contain NUL bytes."}
		}
		body := C.CString(options.BodyShape)
		frame := C.CString(options.EyeFrame)
		ball := C.CString(options.EyeBall)
		defer C.free(unsafe.Pointer(body))
		defer C.free(unsafe.Pointer(frame))
		defer C.free(unsafe.Pointer(ball))
		request.options.version = C.uint32_t(options.Version)
		request.options.scale = C.uint32_t(options.Scale)
		request.options.recovery_margin = C.uint32_t(options.RecoveryMargin)
		request.options.flags = 0
		if options.Color {
			request.options.flags |= C.OPENQR_COLOR
		}
		if options.VaryURL {
			request.options.flags |= C.OPENQR_VARY_URL
		}
		if options.RandomizeSeed {
			request.options.flags |= C.OPENQR_RANDOMIZE_SEED
		}
		if options.Seed != nil {
			request.options.seed = C.uint32_t(*options.Seed)
			request.options.flags |= C.OPENQR_SEED_SET
		}
		switch options.ErrorCorrection {
		case 0:
		case 'L':
			request.options.flags |= C.OPENQR_ERROR_CORRECTION_L
		case 'M':
			request.options.flags |= C.OPENQR_ERROR_CORRECTION_M
		case 'Q':
			request.options.flags |= C.OPENQR_ERROR_CORRECTION_Q
		case 'H':
			request.options.flags |= C.OPENQR_ERROR_CORRECTION_H
		default:
			return GeneratedBatch{}, &Error{Status: int(C.OPENQR_INVALID_ARGUMENT), Message: "Error correction must be L, M, Q, H, or zero for automatic selection."}
		}
		request.options.mode = C.openqr_mode(options.Mode)
		request.options.contrast = C.double(options.Contrast)
		request.options.brightness = C.double(options.Brightness)
		request.options.background_rgba = C.uint32_t(options.BackgroundRGBA)
		request.options.body_shape = body
		request.options.eye_frame = frame
		request.options.eye_ball = ball
	}
	var result *C.openqr_result
	var failure *C.openqr_error
	status := C.openqr_generate(&request, &result, &failure)
	if failure != nil {
		defer C.openqr_error_destroy(failure)
	}
	if status != C.OPENQR_OK {
		message := fmt.Sprintf("OpenQR generation failed with status %d.", int(status))
		if failure != nil {
			message = C.GoString(C.openqr_error_message(failure))
		}
		return GeneratedBatch{}, &Error{Status: int(status), Message: message}
	}
	defer C.openqr_result_destroy(result)
	batch := GeneratedBatch{Seed: uint32(C.openqr_result_seed(result))}
	for index := C.size_t(0); index < C.openqr_result_count(result); index++ {
		var raw C.openqr_entry
		status = C.openqr_result_entry(result, index, &raw)
		if status != C.OPENQR_OK {
			return GeneratedBatch{}, &Error{Status: int(status), Message: "The requested QR result does not exist."}
		}
		entry := Generated{
			Version: uint32(raw.version), Seed: uint32(raw.seed), Mode: Mode(raw.mode), ImageFormat: ImageFormat(raw.format),
			VerifiedFrames: uint32(raw.verified_frames), TotalFrames: uint32(raw.total_frames), ChangedCodewords: uint32(raw.changed_codewords),
			LayoutAttempts: uint32(raw.layout_attempts), ErrorCorrection: byte(raw.error_correction), ColorExposure: float64(raw.color_exposure),
		}
		if raw.artwork_fraction >= 0 {
			fraction := float64(raw.artwork_fraction)
			entry.ArtworkFraction = &fraction
		}
		if raw.payload.size != 0 {
			entry.Payload = append([]byte(nil), unsafe.Slice((*byte)(unsafe.Pointer(raw.payload.data)), int(raw.payload.size))...)
		}
		if raw.image.size != 0 {
			entry.Image = append([]byte(nil), unsafe.Slice((*byte)(unsafe.Pointer(raw.image.data)), int(raw.image.size))...)
		}
		if raw.preview.size != 0 {
			entry.Preview = append([]byte(nil), unsafe.Slice((*byte)(unsafe.Pointer(raw.preview.data)), int(raw.preview.size))...)
		}
		if raw.svg.size != 0 {
			entry.SVG = append([]byte(nil), unsafe.Slice((*byte)(unsafe.Pointer(raw.svg.data)), int(raw.svg.size))...)
		}
		if raw.target_preview.size != 0 {
			entry.TargetPreview = append([]byte(nil), unsafe.Slice((*byte)(unsafe.Pointer(raw.target_preview.data)), int(raw.target_preview.size))...)
		}
		if raw.block_count != 0 {
			entry.BlockErrors = append([]int32(nil), unsafe.Slice((*int32)(unsafe.Pointer(raw.block_errors)), int(raw.block_count))...)
			entry.BlockCapacities = append([]int32(nil), unsafe.Slice((*int32)(unsafe.Pointer(raw.block_capacities)), int(raw.block_count))...)
		}
		batch.Results = append(batch.Results, entry)
	}
	for index := C.size_t(0); index < C.openqr_result_skipped_count(result); index++ {
		batch.Skipped = append(batch.Skipped, SkippedVersion{
			Version: uint32(C.openqr_result_skipped_version(result, index)), Reason: C.GoString(C.openqr_result_skipped_reason(result, index)),
		})
	}
	preview := C.openqr_result_reference_preview(result)
	if preview.size != 0 {
		batch.ReferencePreview = append([]byte(nil), unsafe.Slice((*byte)(unsafe.Pointer(preview.data)), int(preview.size))...)
	}
	input := C.openqr_result_reference_input(result)
	if input.size != 0 {
		batch.ReferenceInput = append([]byte(nil), unsafe.Slice((*byte)(unsafe.Pointer(input.data)), int(input.size))...)
	}
	return batch, nil
}

func Version() string {
	return C.GoString(C.openqr_version())
}

func Styles(category StyleCategory) []Style {
	var result []Style
	for index := C.size_t(0); index < C.openqr_style_count(C.openqr_style_category(category)); index++ {
		result = append(result, Style{
			ID: C.GoString(C.openqr_style_id(C.openqr_style_category(category), index)), Label: C.GoString(C.openqr_style_label(C.openqr_style_category(category), index)),
		})
	}
	return result
}

func StylePreview(category StyleCategory, index int) ([]byte, error) {
	var buffer *C.openqr_buffer
	var failure *C.openqr_error
	status := C.openqr_style_preview(C.openqr_style_category(category), C.size_t(index), &buffer, &failure)
	if failure != nil {
		defer C.openqr_error_destroy(failure)
	}
	if status != C.OPENQR_OK {
		message := fmt.Sprintf("OpenQR style preview failed with status %d.", int(status))
		if failure != nil {
			message = C.GoString(C.openqr_error_message(failure))
		}
		return nil, &Error{Status: int(status), Message: message}
	}
	defer C.openqr_buffer_destroy(buffer)
	data := C.openqr_buffer_bytes(buffer)
	if data.size == 0 {
		return nil, nil
	}
	return append([]byte(nil), unsafe.Slice((*byte)(unsafe.Pointer(data.data)), int(data.size))...), nil
}
