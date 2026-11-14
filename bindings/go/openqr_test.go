package openqr

import (
	"bytes"
	"errors"
	"testing"
)

func TestLiteralPayloadAndOwnedOutput(t *testing.T) {
	options := DefaultOptions()
	options.Version = 3
	options.Scale = 3
	seed := uint32(42)
	options.Seed = &seed
	payload := []byte{'A', 0, 'B'}
	batch, err := Generate(payload, nil, nil, &options)
	if err != nil {
		t.Fatal(err)
	}
	if batch.Seed != seed || len(batch.Results) != 1 {
		t.Fatalf("unexpected batch metadata: seed=%d, results=%d", batch.Seed, len(batch.Results))
	}
	result := batch.Results[0]
	if !bytes.Equal(result.Payload, payload) || !bytes.HasPrefix(result.Image, []byte("\x89PNG\r\n\x1a\n")) {
		t.Fatal("literal payload or encoded image changed")
	}
	if len(result.Preview) == 0 || len(result.SVG) == 0 || result.VerifiedFrames != result.TotalFrames {
		t.Fatal("missing verified output buffers")
	}
	if len(result.BlockErrors) != len(result.BlockCapacities) || len(Styles(Body)) == 0 {
		t.Fatal("missing planner metadata or style catalog")
	}
	preview, err := StylePreview(Body, 0)
	if err != nil || !bytes.HasPrefix(preview, []byte("<?xml")) {
		t.Fatalf("style preview was not returned: %v", err)
	}
	if _, err = StylePreview(Body, -1); err == nil {
		t.Fatal("invalid style preview index was accepted")
	}
	_, err = Generate(nil, nil, nil, &options)
	var nativeError *Error
	if !errors.As(err, &nativeError) || nativeError.Status != 1 || nativeError.Message == "" {
		t.Fatalf("native error was not preserved: %v", err)
	}
	if !bytes.Equal(result.Payload, payload) {
		t.Fatal("output bytes depended on a destroyed native result")
	}
}

func TestSuppliedEmptyReferenceIsDistinct(t *testing.T) {
	options := DefaultOptions()
	options.Version = 3
	options.Scale = 3
	_, err := Generate([]byte("QR"), []byte{}, nil, &options)
	var nativeError *Error
	if !errors.As(err, &nativeError) || nativeError.Status != 1 {
		t.Fatalf("supplied empty image was accepted: %v", err)
	}
}
