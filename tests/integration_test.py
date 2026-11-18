from concurrent.futures import ThreadPoolExecutor
import io
import hashlib
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image, ImageDraw, ImageSequence
import pytest
import zxingcpp

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "bindings" / "python"))
from openqr import Engine, OpenQRError


@pytest.fixture(scope="session")
def engine():
    return Engine()


@pytest.fixture(scope="session")
def reference_png():
    image = Image.new("RGBA", (128, 96), (240, 240, 240, 0))
    drawing = ImageDraw.Draw(image)
    drawing.rectangle((32, 16, 95, 79), fill=(15, 50, 80, 255))
    drawing.ellipse((43, 28, 83, 68), fill=(230, 235, 240, 255))
    output = io.BytesIO()
    image.save(output, format="PNG")
    return output.getvalue()


def decode_frames(data, expected):
    with Image.open(io.BytesIO(data)) as image:
        durations = []
        for frame in ImageSequence.Iterator(image):
            pixels = frame.convert("RGB")
            codes = []
            for binarizer in (zxingcpp.Binarizer.LocalAverage, zxingcpp.Binarizer.GlobalHistogram):
                codes.extend(zxingcpp.read_barcodes(pixels, formats=zxingcpp.BarcodeFormat.QRCode, binarizer=binarizer))
            assert any(code.bytes == expected for code in codes)
            durations.append(frame.info.get("duration"))
        return durations, image.info.get("loop")


def vector_geometry_digest(data):
    document = ET.fromstring(data)
    geometry = []
    for element in document.iter():
        attributes = {}
        for key, value in element.attrib.items():
            if key in ("d", "points", "viewBox"):
                attributes[key] = [token if token.isalpha() else round(float(token), 7) for token in re.findall(r"[A-Za-z]|[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?", value)]
            elif key in ("x", "y", "cx", "cy", "rx", "ry", "width", "height") and value != "100%":
                attributes[key] = round(float(value), 7)
            else:
                attributes[key] = value
        geometry.append((element.tag.rsplit("}", 1)[-1], attributes))
    return hashlib.sha256(json.dumps(geometry, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


@pytest.mark.parametrize("scale", [3, 12])
def test_all_versions_are_independently_decodable_with_white_quiet_zone(engine, scale):
    batch = engine.generate("QR", scale=scale, seed=42)
    assert {entry.version for entry in batch.results} == set(range(1, 41))
    assert batch.skipped == ()
    for entry in batch.results:
        assert entry.verified_frames == entry.total_frames == 1
        assert entry.payload == b"QR"
        decode_frames(entry.image, entry.payload)
        with Image.open(io.BytesIO(entry.image)) as image:
            assert image.size == ((25 + 4 * entry.version) * scale,) * 2
            pixels = np.asarray(image.convert("RGB"))
            border = 4 * scale
            assert np.all(pixels[:border] == 255)
            assert np.all(pixels[-border:] == 255)
            assert np.all(pixels[:, :border] == 255)
            assert np.all(pixels[:, -border:] == 255)
        vector = ET.fromstring(entry.svg)
        assert vector.tag == "{http://www.w3.org/2000/svg}svg"
        assert tuple(map(int, vector.attrib["viewBox"].split())) == (0, 0, 25 + 4 * entry.version, 25 + 4 * entry.version)


@pytest.mark.parametrize("category", ["body", "eye_frame", "eye_ball"])
def test_each_published_style_produces_a_verified_artifact(engine, category):
    options_key = "body_shape" if category == "body" else category
    catalog = engine.styles(category)
    assert catalog
    for key, label in catalog.items():
        assert key and label
        entry = engine.generate("https://example.com/style#keep", version=19, seed=42, **{options_key: key}).results[0]
        assert entry.svg is not None, (category, key)
        decode_frames(entry.image, entry.payload)
        assert entry.changed_codewords == 0
        assert all(error == 0 for error in entry.block_errors)


@pytest.mark.parametrize("category", ["body", "eye_frame", "eye_ball"])
def test_style_previews_match_source_geometry_and_own_their_memory(engine, category):
    vectors = json.loads((Path(__file__).parent / "style_vectors.json").read_text())["geometry"][category]
    assert set(engine.styles(category)) - {"follow-body"} == set(vectors)
    previews = [engine.style_preview(category, key) for key in vectors]
    for (key, expected), preview in zip(vectors.items(), previews):
        assert vector_geometry_digest(preview) == expected, (category, key)
        root = ET.fromstring(preview)
        assert root.attrib["width"] == root.attrib["height"] == "88"
    assert len(set(previews)) == len(previews)


@pytest.mark.parametrize("eyes, modules", [({"eye_frame": "follow-body"}, 24), ({"eye_ball": "follow-body"}, 9), ({"eye_frame": "follow-body", "eye_ball": "follow-body"}, 33)])
def test_follow_body_eyes_preserve_finder_modules_with_selected_body_geometry(engine, eyes, modules):
    entry = engine.generate("https://openqr.my", version=8, scale=3, seed=42, body_shape="circle", **eyes).results[0]
    decode_frames(entry.image, entry.payload)
    vector = ET.fromstring(entry.svg)
    finder_modules = [element for element in vector if element.tag.endswith("}ellipse") and 4 <= float(element.attrib["cx"]) < 11 and 4 <= float(element.attrib["cy"]) < 11]
    assert len(finder_modules) == modules
    assert all(float(element.attrib["rx"]) == float(element.attrib["ry"]) == 0.5 for element in finder_modules)
    assert entry.verified_frames == entry.total_frames == 1
    assert entry.changed_codewords == 0


def test_follow_body_eyes_keep_existing_rounded_module_geometry(engine):
    legacy = engine.generate("https://openqr.my", version=3, scale=3, seed=42).results[0]
    inherited = engine.generate("https://openqr.my", version=3, scale=3, seed=42, eye_frame="follow-body", eye_ball="follow-body").results[0]
    assert inherited.image == legacy.image
    assert inherited.svg == legacy.svg
    assert engine.style_preview("eye_frame", "follow-body") == engine.style_preview("eye_frame", "rounded-dots")
    assert engine.style_preview("eye_ball", "follow-body") == engine.style_preview("eye_ball", "dots")


@pytest.mark.parametrize("eye_frame, eye_ball", [("follow-body", "follow-body"), ("square", "small-dots")])
def test_mosaic_finder_frames_and_balls_follow_local_artwork_colors(engine, eye_frame, eye_ball):
    source = Image.new("RGB", (256, 256))
    corner_colors = ((224, 52, 96), (81, 181, 48), (37, 102, 218), (216, 169, 38))
    for corner, color in enumerate(corner_colors):
        row, column = divmod(corner, 2)
        source.paste(color, (column * 128, row * 128, (column + 1) * 128, (row + 1) * 128))
    data = io.BytesIO()
    source.save(data, format="PNG")

    scale = 12
    payload = b"https://example.com/illustration"
    entry = engine.generate(payload, data.getvalue(), version=8, scale=scale, seed=42,
                            body_shape="circle", eye_frame=eye_frame, eye_ball=eye_ball).results[0]
    decode_frames(entry.image, payload)
    with Image.open(io.BytesIO(entry.image)) as image:
        modules = np.asarray(image.convert("RGB"))[4 * scale + scale // 2:-4 * scale:scale, 4 * scale + scale // 2:-4 * scale:scale]
    svg_fills = {element.get("fill") for element in ET.fromstring(entry.svg)}
    finder = np.ones((7, 7), dtype=bool)
    finder[1:-1, 1:-1] = False
    finder[2:-2, 2:-2] = True
    finder_count = 0
    for corner, color in enumerate(corner_colors):
        row, column = divmod(corner, 2)
        y, x = row * (len(modules) - 7), column * (len(modules) - 7)
        sample = modules[y:y + 7, x:x + 7]
        if not np.array_equal(np.any(sample < 255, axis=2), finder):
            continue
        finder_count += 1
        for ink in (sample[0, 3], sample[3, 3]):
            assert np.ptp(ink) >= 10
            np.testing.assert_array_equal(np.argsort(ink), np.argsort(color))
            assert "#" + bytes(ink).hex() in svg_fills
    assert finder_count == 3


@pytest.mark.parametrize("payload", [b" leading and trailing \t\r\n", b"a\x00b\x01\x1f", "Contact: \u674e\u534e, caf\u00e9@\u4f8b\u5b50.test", "https://example.com/page?x=%20&y=1#exact"])
def test_literal_payload_bytes_survive_generation_and_serialization(engine, payload):
    expected = payload.encode("utf-8") if isinstance(payload, str) else payload
    entry = engine.generate(payload, version=8, seed=0).results[0]
    assert entry.payload == expected
    decode_frames(entry.image, expected)


@pytest.mark.parametrize("filename", ["lego.ico", "playstation.jpg", "shopify.png"])
def test_existing_logo_fixtures_and_reed_solomon_reserve(engine, filename):
    source = (Path(__file__).parent / "fixtures" / filename).read_bytes()
    batch = engine.generate("https://example.com", source, version=8, scale=6, seed=42, color=False, recovery_margin=2)
    entry = batch.results[0]
    assert batch.reference_input and batch.reference_preview
    assert entry.target_preview
    assert entry.artwork_fraction is not None
    assert len(entry.block_errors) == len(entry.block_capacities) > 0
    assert entry.changed_codewords == sum(entry.block_errors)
    assert all(0 <= errors <= capacity - 2 for errors, capacity in zip(entry.block_errors, entry.block_capacities))
    decode_frames(entry.image, entry.payload)


def test_mosaic_distinguishes_pale_reference_colors_without_losing_the_payload(engine):
    source = Image.new("RGB", (512, 512), (255, 224, 238))
    drawing = ImageDraw.Draw(source)
    drawing.ellipse((132, 92, 380, 401), fill=(255, 224, 184))
    drawing.rectangle((125, 54, 393, 122), fill=(40, 40, 40))
    data = io.BytesIO()
    source.save(data, format="PNG")

    payload = b"https://example.com/portrait"
    entry = engine.generate(payload, data.getvalue(), version=14, scale=12, seed=42,
                            body_shape="circle", eye_frame="follow-body", eye_ball="small-dots").results[0]
    decode_frames(entry.image, payload)
    assert all(errors <= capacity - 2 for errors, capacity in zip(entry.block_errors, entry.block_capacities))

    with Image.open(io.BytesIO(entry.image)) as image:
        modules = np.asarray(image.convert("RGB"), dtype=int)[54:-48:12, 54:-48:12]
    pink = modules[26:44, 9:16].reshape(-1, 3)
    peach = modules[26:44, 29:44].reshape(-1, 3)
    pink = pink[np.any(pink < 255, axis=1)]
    peach = peach[np.any(peach < 255, axis=1)]

    assert len(pink) > 10 and len(peach) > 10
    assert (pink[:, 2] - pink[:, 0]).mean() > (peach[:, 2] - peach[:, 0]).mean() + 15


@pytest.mark.parametrize(("background", "matte", "artwork"), [
    ("transparent", (248, 249, 248), (252, 175, 123)),
    ("#ffffff", (248, 249, 248), (252, 175, 123)),
    ("#ed1c24", (240, 35, 41), (255, 255, 255)),
])
def test_mosaic_uses_fewer_dark_modules_for_colors_close_to_the_matte(engine, background, matte, artwork):
    source = Image.new("RGB", (512, 512), matte)
    drawing = ImageDraw.Draw(source)
    drawing.rounded_rectangle((80, 80, 430, 360), radius=42, outline=(41, 55, 100), width=26)
    drawing.rectangle((170, 155, 350, 282), fill=artwork)
    data = io.BytesIO()
    source.save(data, format="PNG")

    payload = b"https://openqr.my"
    options = dict(version=14, scale=12, seed=42, body_shape="circle",
                   eye_frame="follow-body", eye_ball="small-dots", background_color=background)
    batch = engine.generate(payload, data.getvalue(), **options)
    entry = batch.results[0]
    decode_frames(entry.image, payload)
    assert all(errors <= capacity - 2 for errors, capacity in zip(entry.block_errors, entry.block_capacities))

    with Image.open(io.BytesIO(entry.image)) as image:
        modules = np.asarray(image.convert("RGB"), dtype=int)[54:-48:12, 54:-48:12]
    dark = np.min(modules, axis=2) < 230
    matte_density = dark[52:65, 10:61].mean()
    artwork_density = dark[23:39, 26:47].mean()
    assert matte_density < 0.39
    assert artwork_density > matte_density + 0.07

    regenerated = engine.generate(payload, batch.reference_input, **options).results[0]
    assert regenerated.image == entry.image


def test_flat_matte_does_not_trigger_artwork_search(engine):
    source = Image.new("RGBA", (48, 32), (242, 39, 44, 255))
    data = io.BytesIO()
    source.save(data, format="PNG")
    payload = b"Blank matte"
    options = {"version": 10, "seed": 42, "background_color": "#ed1c24"}
    plain = engine.generate(payload, **options).results[0]
    matched = engine.generate(payload, data.getvalue(), **options).results[0]

    decode_frames(matched.image, payload)
    assert matched.image == plain.image
    assert matched.layout_attempts == plain.layout_attempts
    assert matched.changed_codewords == 0


def test_mosaic_ignores_a_solid_color_hidden_by_an_opaque_backdrop_image(engine):
    source = io.BytesIO()
    background = io.BytesIO()
    Image.new("RGBA", (128, 128), (0, 0, 0, 0)).save(source, format="PNG")
    Image.new("RGBA", (128, 128), (237, 28, 36, 255)).save(background, format="PNG")
    payload = b"Opaque backdrop"
    options = dict(version=8, seed=42, background_image=background.getvalue())
    red = engine.generate(payload, source.getvalue(), background_color="#ed1c24", **options).results[0]
    blue = engine.generate(payload, source.getvalue(), background_color="#3030ab", **options).results[0]
    decode_frames(red.image, payload)
    assert red.image == blue.image
    assert red.changed_codewords == blue.changed_codewords > 0


def test_ink_in_a_near_red_region_stays_light_even_when_those_are_the_only_two_colors(engine):
    source = io.BytesIO()
    image = Image.new("RGB", (128, 128), (237, 28, 36))
    ImageDraw.Draw(image).rectangle((64, 0, 127, 127), fill=(218, 21, 27))
    image.save(source, format="PNG")
    payload = b"Subtle red"
    entry = engine.generate(payload, source.getvalue(), version=8, scale=12, seed=42,
                            background_color="#ed1c24").results[0]
    decode_frames(entry.image, payload)
    with Image.open(io.BytesIO(entry.target_preview)) as target:
        assert target.convert("RGB").getcolors() == [(target.width * target.height, (255, 255, 255))]


@pytest.mark.parametrize("image_format", ["PNG", "JPEG", "WEBP"])
def test_still_picture_mode_decodes_common_input_formats(engine, reference_png, image_format):
    output = io.BytesIO()
    with Image.open(io.BytesIO(reference_png)) as image:
        image.convert("RGB").save(output, format=image_format)
    entry = engine.generate("https://example.com/photo#details", output.getvalue(), mode="picture", version=8, scale=6, seed=12).results[0]
    plain = engine.generate(entry.payload, mode="picture", version=8, scale=6, seed=12).results[0]
    assert entry.image != plain.image
    assert entry.svg is None
    assert entry.target_preview is None
    assert entry.changed_codewords == 0
    assert entry.verified_frames == entry.total_frames == 1
    decode_frames(entry.image, entry.payload)


@pytest.mark.parametrize("image_format", ["GIF", "PNG", "WEBP"])
def test_animated_picture_preserves_timing_loop_and_every_verified_frame(engine, image_format):
    frames = []
    for bounds in ((12, 12, 52, 52), (30, 12, 70, 52)):
        image = Image.new("RGBA", (84, 64), (255, 255, 255, 0))
        ImageDraw.Draw(image).ellipse(bounds, fill=(25, 45, 60, 255))
        frames.append(image)
    source = io.BytesIO()
    frames[0].save(source, format=image_format, save_all=True, append_images=frames[1:], duration=[90, 170], loop=3, disposal=2)
    background = io.BytesIO()
    Image.new("RGB", (180, 100), (220, 235, 245)).save(background, format="PNG")
    batch = engine.generate("https://example.com/animation", source.getvalue(), mode="picture", version=8, scale=6, seed=42, background_color="#000000", background_image=background.getvalue())
    entry = batch.results[0]
    assert entry.image_format == "GIF"
    assert entry.verified_frames == entry.total_frames == 2
    assert decode_frames(entry.image, entry.payload) == ([90, 170], 3)
    with Image.open(io.BytesIO(batch.reference_input)) as reusable:
        assert reusable.n_frames == 2


def test_apng_default_image_survives_the_complete_public_picture_pipeline(engine):
    source = (Path(__file__).parent / "fixtures" / "poster.apng").read_bytes()
    batch = engine.generate("QR", source, mode="picture", version=8, scale=6, seed=42)
    entry = batch.results[0]
    assert entry.verified_frames == entry.total_frames == 4
    assert decode_frames(entry.image, entry.payload) == ([100, 90, 170, 230], 3)
    with Image.open(io.BytesIO(batch.reference_input)) as reusable:
        assert reusable.n_frames == 4


def test_animated_background_is_rejected_by_the_public_image_contract(engine, reference_png):
    background = (Path(__file__).parent / "fixtures" / "poster.apng").read_bytes()
    with pytest.raises(OpenQRError) as failure:
        engine.generate("QR", reference_png, background_image=background, version=8, scale=3, seed=42)
    assert failure.value.status == 2


def test_replay_is_deterministic_and_reusable_reference_avoids_double_enhancement(engine, reference_png):
    options = dict(version=8, scale=6, seed=0xffffffff, color=False, contrast=1.3, brightness=0.8)
    first = engine.generate("https://example.com/replay", reference_png, **options)
    second = engine.generate("https://example.com/replay", reference_png, **options)
    reused = engine.generate("https://example.com/replay", first.reference_input, **options)
    assert first.seed == second.seed == reused.seed == 0xffffffff
    assert first.results == second.results == reused.results


def test_url_variation_reports_actual_encoded_payload_and_retains_original(engine):
    payload = b"https://example.com/path?x=1"
    batch = engine.generate(payload, version=8, scale=6, seed=42, vary_url=True)
    entry = batch.results[0]
    assert batch.payload == payload
    assert entry.payload.startswith(payload + b"#")
    assert entry.payload != payload
    decode_frames(entry.image, entry.payload)


@pytest.mark.parametrize("payload", [
    "lego.com", "www.lego.com/catalog?item=42", "example.com:8443/path",
    "example.com/?next=https://other.example/path", "example.com/a%20b?x=%25",
    "HTTP://EXAMPLE.COM:08080/a?b=c", "https://sub.example.co.uk./",
    "\u4f8b\u5b50.test/\u76ee\u5f55?\u540d\u79f0=\u503c", "xn--fsqu00a.test", "https://example.\u4e2d\u56fd/",
    "http://127.0.0.1:8080/path", "http://[::1]:8080/path", "http://localhost:8080/path",
    "http://intranet/status", "localhost:8080/path", "app.localhost/path",
])
def test_url_variation_accepts_existing_valid_website_forms(engine, payload):
    original = payload.encode("utf-8")
    entry = engine.generate(original, version=8, scale=3, seed=42, vary_url=True).results[0]
    assert entry.payload.startswith(original + b"#")


@pytest.mark.parametrize("payload", [
    "ordinary text", "ordinary", "person@example.com", "Contact: person@example.com", "1.2", "1.2.3", "v1.2.3", "127.0.0.1", "[::1]",
    "ftp://example.com", "file:///tmp/image.png", "mailto:person@example.com", "javascript:alert(1)", "https:example.com", "//example.com", "https://",
    "https://user:password@example.com", "https://@example.com", "example.com@another.example", " example.com", "example.com ", "https://example.com/a b",
    "https://example.com/\nnext", "https://example.com/\x00next", "https://example.com/\x80next", "https://exam\u200bple.com", "https://example.com/\\another.example",
    'https://example.com/"quoted"', "https://example.com/<tag>", "https://example.com/%ZZ", "https://example.com/%2", "https://example.com:",
    "https://example.com:0", "https://example.com:65536", "https://example.com:-1", "https://example.com:abc", "https://[::1]junk", "https://[::1]:abc", "https://[::gg]",
    "https://1234", "https://127.1", "https://-example.com", "https://example-.com", "https://under_score.example.com", "https://example..com", "https://example.com..",
    "https://xn--a.com", "https://" + "a" * 64 + ".com", "https://" + ".".join(["a" * 63] * 4) + ".com", "https://example.com/keep#original",
])
def test_url_variation_preserves_plain_text_and_malformed_website_forms(engine, payload):
    original = payload.encode("utf-8")
    entry = engine.generate(original, version=20 if len(original) > 100 else 8, scale=3, seed=42, vary_url=True).results[0]
    assert entry.payload == original


@pytest.mark.parametrize("reference", [b"", b"invalid image", b"\x89PNG\r\n\x1a\n", b"GIF89a\xff\xff\xff\xff\x00\x00\x00"])
def test_malformed_inputs_fail_without_poisoning_later_requests(engine, reference):
    with pytest.raises(OpenQRError) as failure:
        engine.generate("QR", reference, version=3, seed=42)
    assert failure.value.status == (1 if reference == b"" else 2)
    assert engine.generate("QR", version=3, scale=3, seed=42).results[0].payload == b"QR"


@pytest.mark.parametrize("options", [{"scale": 4}, {"contrast": float("nan")}, {"brightness": 0}, {"body_shape": "missing"}, {"eye_frame": "missing"}, {"eye_ball": "missing"}, {"background_color": 0x12345680}])
def test_invalid_options_are_rejected_by_the_shared_native_contract(engine, options):
    with pytest.raises(OpenQRError):
        engine.generate("QR", version=3, **options)


@pytest.mark.parametrize("correction", ["L", "M", "Q", "H"])
def test_requested_error_correction_is_used_without_boosting(engine, correction):
    entry = engine.generate("QR", version=3, scale=3, seed=42, error_correction=correction).results[0]
    assert entry.error_correction == correction
    assert entry.payload == b"QR"
    decode_frames(entry.image, entry.payload)


@pytest.mark.parametrize("correction", ["", "A", "low", False, 3, b"L"])
def test_invalid_error_correction_is_rejected(engine, correction):
    with pytest.raises(ValueError, match="error_correction"):
        engine.generate("QR", version=3, scale=3, error_correction=correction)


def test_omitted_and_automatic_error_correction_match(engine):
    implicit = engine.generate("QR", version=3, scale=3, seed=42)
    automatic = engine.generate("QR", version=3, scale=3, seed=42, error_correction=None)
    assert automatic == implicit


def test_result_memory_is_owned_by_python_and_engine_calls_are_concurrent(engine):
    requests = [(f"concurrent-{index}".encode(), index) for index in range(8)]
    with ThreadPoolExecutor(max_workers=4) as executor:
        futures = [executor.submit(engine.generate, payload, version=3, scale=3, seed=seed) for payload, seed in requests]
        batches = [future.result() for future in futures]
    for (payload, seed), batch in zip(requests, batches):
        assert batch.seed == seed
        assert batch.results[0].payload == payload
        decode_frames(batch.results[0].image, payload)


def test_batch_reports_overflowed_versions_without_losing_successes(engine):
    payload = "literal text " * 30
    batch = engine.generate(payload, seed=42, scale=3)
    successful = {entry.version for entry in batch.results}
    skipped = {entry.version for entry in batch.skipped}
    assert successful and skipped
    assert successful.isdisjoint(skipped)
    assert successful | skipped == set(range(1, 41))
    assert all(entry.reason for entry in batch.skipped)
    decode_frames(batch.results[-1].image, payload.encode())


def test_batch_reports_unreadable_designs_without_losing_verified_versions(engine):
    batch = engine.generate("https://openqr.my", seed=42, scale=3, body_shape="classy", eye_frame="scalloped", eye_ball="cross")
    successful = {entry.version for entry in batch.results}
    skipped = {entry.version for entry in batch.skipped}
    assert successful and skipped
    assert successful.isdisjoint(skipped)
    assert successful | skipped == set(range(1, 41))
    assert all(re.fullmatch(r"No verified layout was found after \d+ attempts with the selected design\.", entry.reason) for entry in batch.skipped)
    for entry in batch.results:
        assert entry.verified_frames == entry.total_frames == 1
        decode_frames(entry.image, entry.payload)
