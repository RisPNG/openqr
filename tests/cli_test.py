import json
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

from PIL import Image
import pytest
import zxingcpp


@pytest.fixture(scope="module")
def cli():
    configured = os.environ.get("OPENQR_CLI")
    if configured:
        return Path(configured)
    root = Path(__file__).resolve().parents[1]
    name = "openqr.exe" if sys.platform == "win32" else "openqr"
    candidates = [root / "build" / "Release" / name, root / "build" / name, root / "build" / "Debug" / name]
    return next(path for path in candidates if path.is_file())


@pytest.mark.parametrize("payload,directory", [
    ("https://example.com/cli?x=1#literal", "artifacts"),
    ("  caf\u00e9 \u674e\u534e \U0001f30d\n", "unicode-payload"),
    ("  caf\u00e9 \u674e\u534e \U0001f30d\n", "\u8f93\u51fa-\u03b4-\U0001f30d"),
])
def test_cli_writes_decodable_artifacts_and_exact_json_metadata(cli, tmp_path, payload, directory):
    output = tmp_path / directory
    process = subprocess.run([str(cli), "--payload", payload, "--output", str(output), "--version", "8", "--scale", "3", "--seed", "42"], capture_output=True, encoding="utf-8", check=False)
    assert process.returncode == 0, process.stderr
    manifest = json.loads(process.stdout)
    assert json.loads((output / "manifest.json").read_text(encoding="utf-8")) == manifest
    assert manifest["seed"] == 42
    assert manifest["skipped"] == []
    entry, = manifest["results"]
    assert entry["version"] == 8
    assert bytes.fromhex(entry["payload_hex"]) == payload.encode("utf-8")
    assert entry["verified_frames"] == entry["total_frames"] == 1
    assert entry["changed_codewords"] == 0
    assert entry["block_errors"] and all(error == 0 for error in entry["block_errors"])
    assert len(entry["block_errors"]) == len(entry["block_capacities"])
    with Image.open(output / entry["file"]) as image:
        assert image.size == (171, 171)
        codes = zxingcpp.read_barcodes(image.convert("RGB"), formats=zxingcpp.BarcodeFormat.QRCode)
        assert any(code.bytes == payload.encode("utf-8") for code in codes)
    with Image.open(output / "qr-v8-preview.png") as preview:
        assert preview.size == (171, 171)
    assert ET.parse(output / "qr-v8.svg").getroot().tag == "{http://www.w3.org/2000/svg}svg"


def test_cli_payload_file_retains_nul_and_unicode_input_paths(cli, tmp_path):
    payload = b" leading\x00literal\x01bytes\n "
    source = tmp_path / "\u8f93\u5165-\u03b4.bin"
    source.write_bytes(payload)
    output = tmp_path / "results"
    process = subprocess.run([str(cli), "--payload-file", str(source), "--output", str(output), "--version", "3", "--scale", "3", "--seed", "0"], capture_output=True, encoding="utf-8", check=False)
    assert process.returncode == 0, process.stderr
    manifest = json.loads(process.stdout)
    entry, = manifest["results"]
    assert bytes.fromhex(entry["payload_hex"]) == payload
    assert entry["seed"] == 0
    with Image.open(output / entry["file"]) as image:
        codes = zxingcpp.read_barcodes(image.convert("RGB"), formats=zxingcpp.BarcodeFormat.QRCode)
        assert any(code.bytes == payload for code in codes)


def test_cli_explicit_empty_reference_fails_without_artifacts(cli, tmp_path):
    reference = tmp_path / "empty.png"
    reference.write_bytes(b"")
    output = tmp_path / "must-not-exist"
    process = subprocess.run([str(cli), "--payload", "QR", "--reference", str(reference), "--output", str(output), "--version", "3"], capture_output=True, encoding="utf-8", check=False)
    assert process.returncode == 1
    assert process.stderr.strip()
    assert process.stdout == ""
    assert not output.exists()


@pytest.mark.parametrize("correction", ["L", "M", "Q", "H"])
def test_cli_uses_the_requested_error_correction(cli, tmp_path, correction):
    output = tmp_path / "results"
    process = subprocess.run([str(cli), "--payload", "ECC", "--output", str(output), "--version", "3", "--scale", "3", "--seed", "42", "--error-correction", correction], capture_output=True, encoding="utf-8", check=False)
    assert process.returncode == 0, process.stderr
    entry, = json.loads(process.stdout)["results"]
    assert entry["error_correction"] == correction
    with Image.open(output / entry["file"]) as image:
        codes = zxingcpp.read_barcodes(image.convert("RGB"), formats=zxingcpp.BarcodeFormat.QRCode)
        assert any(code.bytes == b"ECC" for code in codes)


@pytest.mark.parametrize("correction", ["auto", "invalid"])
def test_cli_rejects_unknown_error_correction(cli, tmp_path, correction):
    output = tmp_path / "must-not-exist"
    process = subprocess.run([str(cli), "--payload", "ECC", "--output", str(output), "--error-correction", correction], capture_output=True, encoding="utf-8", check=False)
    assert process.returncode == 1
    assert "Error correction must be" in process.stderr
    assert process.stdout == ""
    assert not output.exists()


def test_cli_keeps_the_selected_level_when_it_cannot_fit(cli, tmp_path):
    output = tmp_path / "must-not-exist"
    process = subprocess.run([str(cli), "--payload", "abcdefghijklmnopq", "--output", str(output), "--version", "1", "--error-correction", "H"], capture_output=True, encoding="utf-8", check=False)
    assert process.returncode == 3
    assert "selected error correction level" in process.stderr
    assert process.stdout == ""
    assert not output.exists()


@pytest.mark.parametrize("option", ["--brightness", "--contrast"])
@pytest.mark.parametrize("value", ["0.1", ".5", "1.", "1e+0", "3"])
def test_cli_accepts_decimal_and_scientific_reference_adjustments(cli, tmp_path, option, value):
    output = tmp_path / "results"
    process = subprocess.run([str(cli), "--payload", "QR", "--output", str(output), "--version", "3", "--scale", "3", "--seed", "42", option, value], capture_output=True, encoding="utf-8", check=False)
    assert process.returncode == 0, process.stderr
    entry, = json.loads(process.stdout)["results"]
    with Image.open(output / entry["file"]) as image:
        codes = zxingcpp.read_barcodes(image.convert("RGB"), formats=zxingcpp.BarcodeFormat.QRCode)
        assert any(code.bytes == b"QR" for code in codes)


@pytest.mark.parametrize("option", ["--brightness", "--contrast"])
@pytest.mark.parametrize("value", ["", " 1", "1 ", "1,5", "1extra", "+1", "0x1p0", "1e+", "nan", "inf", "1e500", "1e-500", "0.09", "3.01"])
def test_cli_rejects_invalid_reference_adjustments_without_artifacts(cli, tmp_path, option, value):
    output = tmp_path / "must-not-exist"
    process = subprocess.run([str(cli), "--payload", "QR", "--output", str(output), option, value], capture_output=True, encoding="utf-8", check=False)
    assert process.returncode == 1
    assert process.stderr.strip()
    assert process.stdout == ""
    assert not output.exists()
