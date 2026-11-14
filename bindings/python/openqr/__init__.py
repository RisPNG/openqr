"""Use the OpenQR C ABI without a Python image-processing runtime."""

from __future__ import annotations

import ctypes
import ctypes.util
from dataclasses import dataclass
from functools import cache
import os
from pathlib import Path
import sys

__all__ = ["Engine", "Generated", "GeneratedBatch", "OpenQRError", "SkippedVersion", "generate"]


class _Bytes(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(ctypes.c_uint8)), ("size", ctypes.c_size_t)]


class _Options(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32), ("abi_version", ctypes.c_uint32),
        ("version", ctypes.c_uint32), ("scale", ctypes.c_uint32),
        ("recovery_margin", ctypes.c_uint32), ("seed", ctypes.c_uint32),
        ("flags", ctypes.c_uint32), ("mode", ctypes.c_int),
        ("contrast", ctypes.c_double), ("brightness", ctypes.c_double),
        ("background_rgba", ctypes.c_uint32), ("body_shape", ctypes.c_char_p),
        ("eye_frame", ctypes.c_char_p), ("eye_ball", ctypes.c_char_p),
    ]


class _Request(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32), ("abi_version", ctypes.c_uint32),
        ("payload", _Bytes), ("reference", _Bytes), ("background", _Bytes),
        ("options", _Options),
    ]


class _Entry(ctypes.Structure):
    _fields_ = [
        ("version", ctypes.c_uint32), ("seed", ctypes.c_uint32),
        ("mode", ctypes.c_int), ("format", ctypes.c_int),
        ("verified_frames", ctypes.c_uint32), ("total_frames", ctypes.c_uint32),
        ("changed_codewords", ctypes.c_uint32), ("layout_attempts", ctypes.c_uint32),
        ("error_correction", ctypes.c_char), ("artwork_fraction", ctypes.c_double),
        ("color_exposure", ctypes.c_double), ("payload", _Bytes), ("image", _Bytes),
        ("preview", _Bytes), ("svg", _Bytes), ("target_preview", _Bytes),
        ("block_errors", ctypes.POINTER(ctypes.c_int32)),
        ("block_capacities", ctypes.POINTER(ctypes.c_int32)),
        ("block_count", ctypes.c_size_t),
    ]


class OpenQRError(RuntimeError):
    def __init__(self, message: str, status: int):
        super().__init__(message)
        self.status = status


@dataclass(frozen=True, slots=True)
class Generated:
    image: bytes
    preview: bytes
    image_format: str
    svg: bytes | None
    payload: bytes
    version: int
    mode: str
    verified_frames: int
    total_frames: int
    changed_codewords: int
    block_errors: tuple[int, ...]
    block_capacities: tuple[int, ...]
    target_preview: bytes | None
    seed: int
    error_correction: str
    layout_attempts: int
    artwork_fraction: float | None
    color_exposure: float


@dataclass(frozen=True, slots=True)
class SkippedVersion:
    version: int
    reason: str


@dataclass(frozen=True, slots=True)
class GeneratedBatch:
    results: tuple[Generated, ...]
    skipped: tuple[SkippedVersion, ...]
    seed: int
    payload: bytes
    reference_preview: bytes | None
    reference_input: bytes | None


class Engine:
    def __init__(self, library: str | os.PathLike[str] | None = None):
        if library is None:
            library = os.environ.get("OPENQR_LIBRARY")
        if library is None:
            filename = "openqr.dll" if sys.platform == "win32" else "libopenqr.dylib" if sys.platform == "darwin" else "libopenqr.so"
            root = Path(__file__).resolve().parents[3]
            candidates = [root / "build" / "Release" / filename, root / "build" / filename, root / "build" / "Debug" / filename, root / "build" / "native" / "Release" / filename, root / "build" / "native" / filename]
            library = next((str(candidate) for candidate in candidates if candidate.is_file()), None)
            if library is None:
                library = ctypes.util.find_library("openqr")
            if library is None:
                raise OSError("OpenQR was not found. Set OPENQR_LIBRARY to the native library path or pass library= to Engine.")
        self._lib = ctypes.CDLL(os.fspath(library))
        signatures = {
            "openqr_abi_version": (ctypes.c_uint32, []),
            "openqr_version": (ctypes.c_char_p, []),
            "openqr_request_init": (None, [ctypes.POINTER(_Request)]),
            "openqr_generate": (ctypes.c_int, [ctypes.POINTER(_Request), ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_void_p)]),
            "openqr_result_destroy": (None, [ctypes.c_void_p]),
            "openqr_result_count": (ctypes.c_size_t, [ctypes.c_void_p]),
            "openqr_result_seed": (ctypes.c_uint32, [ctypes.c_void_p]),
            "openqr_result_entry": (ctypes.c_int, [ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(_Entry)]),
            "openqr_result_skipped_count": (ctypes.c_size_t, [ctypes.c_void_p]),
            "openqr_result_skipped_version": (ctypes.c_uint32, [ctypes.c_void_p, ctypes.c_size_t]),
            "openqr_result_skipped_reason": (ctypes.c_char_p, [ctypes.c_void_p, ctypes.c_size_t]),
            "openqr_result_reference_preview": (_Bytes, [ctypes.c_void_p]),
            "openqr_result_reference_input": (_Bytes, [ctypes.c_void_p]),
            "openqr_error_message": (ctypes.c_char_p, [ctypes.c_void_p]),
            "openqr_error_destroy": (None, [ctypes.c_void_p]),
            "openqr_style_count": (ctypes.c_size_t, [ctypes.c_int]),
            "openqr_style_id": (ctypes.c_char_p, [ctypes.c_int, ctypes.c_size_t]),
            "openqr_style_label": (ctypes.c_char_p, [ctypes.c_int, ctypes.c_size_t]),
            "openqr_style_preview": (ctypes.c_int, [ctypes.c_int, ctypes.c_size_t, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_void_p)]),
            "openqr_buffer_bytes": (_Bytes, [ctypes.c_void_p]),
            "openqr_buffer_destroy": (None, [ctypes.c_void_p]),
        }
        for name, (result, arguments) in signatures.items():
            function = getattr(self._lib, name)
            function.restype = result
            function.argtypes = arguments
        if self._lib.openqr_abi_version() != 1:
            raise OSError("This Python binding requires OpenQR ABI version 1.")

    @property
    def version(self) -> str:
        return self._lib.openqr_version().decode("ascii")

    def styles(self, category: str) -> dict[str, str]:
        category_id = {"body": 0, "eye_frame": 1, "eye_ball": 2}[category]
        return {
            self._lib.openqr_style_id(category_id, index).decode("ascii"): self._lib.openqr_style_label(category_id, index).decode("utf-8")
            for index in range(self._lib.openqr_style_count(category_id))
        }

    def style_preview(self, category: str, key: str) -> bytes:
        category_id = {"body": 0, "eye_frame": 1, "eye_ball": 2}[category]
        index = tuple(self.styles(category)).index(key)
        buffer = ctypes.c_void_p()
        error = ctypes.c_void_p()
        try:
            status = self._lib.openqr_style_preview(category_id, index, ctypes.byref(buffer), ctypes.byref(error))
            if status:
                message = self._lib.openqr_error_message(error)
                raise OpenQRError(message.decode("utf-8") if message else "OpenQR style preview failed.", status)
            data = self._lib.openqr_buffer_bytes(buffer)
            return ctypes.string_at(data.data, data.size)
        finally:
            self._lib.openqr_buffer_destroy(buffer)
            self._lib.openqr_error_destroy(error)

    def generate(self, payload: str | bytes, reference: bytes | None = None, *, mode: str = "mosaic", version: int | None = None, seed: int | None = None, recovery_margin: int = 2, scale: int = 12, color: bool = True, contrast: float = 1, brightness: float = 1, vary_url: bool = False, background_color: str | int = "transparent", background_image: bytes | None = None, randomize_seed: bool = False, body_shape: str = "rounded", eye_frame: str = "rounded-dots", eye_ball: str = "dots") -> GeneratedBatch:
        if isinstance(payload, str):
            payload = payload.encode("utf-8")
        elif not isinstance(payload, bytes):
            raise TypeError("payload must be text or bytes")
        for name, value in (("version", version), ("seed", seed), ("recovery_margin", recovery_margin), ("scale", scale)):
            if value is not None and (isinstance(value, bool) or not isinstance(value, int) or value < 0 or value > 0xffffffff):
                raise ValueError(f"{name} must be an unsigned 32-bit integer")
        if version == 0:
            raise ValueError("version must be between 1 and 40, or None for all versions")
        if mode not in ("mosaic", "picture"):
            raise ValueError("mode must be mosaic or picture")
        if any("\0" in style for style in (body_shape, eye_frame, eye_ball)):
            raise ValueError("Style identifiers cannot contain NUL")
        if isinstance(background_color, str):
            if background_color == "transparent":
                background_rgba = 0
            elif background_color.startswith("#") and len(background_color) in (4, 7):
                digits = background_color[1:]
                if len(digits) == 3:
                    digits = "".join(digit * 2 for digit in digits)
                background_rgba = (int(digits, 16) << 8) | 255
            else:
                raise ValueError("background_color must be transparent, #RGB, #RRGGBB, or a 32-bit RGBA integer")
        elif isinstance(background_color, int) and not isinstance(background_color, bool) and 0 <= background_color <= 0xffffffff:
            background_rgba = background_color
        else:
            raise ValueError("background_color must be transparent, #RGB, #RRGGBB, or a 32-bit RGBA integer")
        request = _Request()
        self._lib.openqr_request_init(ctypes.byref(request))
        buffers = []
        for name, data in (("payload", payload), ("reference", reference), ("background", background_image)):
            if data is not None:
                if not isinstance(data, bytes):
                    raise TypeError(f"{name} must be bytes")
                buffer = ctypes.create_string_buffer(data)
                buffers.append(buffer)
                setattr(request, name, _Bytes(ctypes.cast(buffer, ctypes.POINTER(ctypes.c_uint8)), len(data)))
        request.options.version = version or 0
        request.options.seed = seed or 0
        request.options.flags = int(bool(color)) | (2 if vary_url else 0) | (4 if seed is not None else 0) | (8 if randomize_seed else 0)
        request.options.mode = 0 if mode == "mosaic" else 1
        request.options.recovery_margin = recovery_margin
        request.options.scale = scale
        request.options.contrast = contrast
        request.options.brightness = brightness
        request.options.background_rgba = background_rgba
        request.options.body_shape = body_shape.encode("ascii")
        request.options.eye_frame = eye_frame.encode("ascii")
        request.options.eye_ball = eye_ball.encode("ascii")
        result = ctypes.c_void_p()
        error = ctypes.c_void_p()
        try:
            status = self._lib.openqr_generate(ctypes.byref(request), ctypes.byref(result), ctypes.byref(error))
            if status:
                message = self._lib.openqr_error_message(error)
                raise OpenQRError(message.decode("utf-8") if message else "OpenQR generation failed.", status)
            entries = []
            for index in range(self._lib.openqr_result_count(result)):
                entry = _Entry()
                status = self._lib.openqr_result_entry(result, index, ctypes.byref(entry))
                if status:
                    raise OpenQRError("Unable to read the OpenQR result.", status)
                values = {name: ctypes.string_at(getattr(entry, name).data, getattr(entry, name).size) for name in ("payload", "image", "preview", "svg", "target_preview")}
                entries.append(Generated(
                    image=values["image"], preview=values["preview"], image_format="PNG" if entry.format == 0 else "GIF", svg=values["svg"] or None,
                    payload=values["payload"], version=entry.version, mode="mosaic" if entry.mode == 0 else "picture",
                    verified_frames=entry.verified_frames, total_frames=entry.total_frames, changed_codewords=entry.changed_codewords,
                    block_errors=tuple(entry.block_errors[i] for i in range(entry.block_count)), block_capacities=tuple(entry.block_capacities[i] for i in range(entry.block_count)),
                    target_preview=values["target_preview"] or None, seed=entry.seed, error_correction=entry.error_correction.decode("ascii"),
                    layout_attempts=entry.layout_attempts, artwork_fraction=None if entry.artwork_fraction < 0 else entry.artwork_fraction, color_exposure=entry.color_exposure,
                ))
            skipped = tuple(SkippedVersion(self._lib.openqr_result_skipped_version(result, index), self._lib.openqr_result_skipped_reason(result, index).decode("utf-8")) for index in range(self._lib.openqr_result_skipped_count(result)))
            preview = self._lib.openqr_result_reference_preview(result)
            source = self._lib.openqr_result_reference_input(result)
            return GeneratedBatch(tuple(entries), skipped, self._lib.openqr_result_seed(result), payload, ctypes.string_at(preview.data, preview.size) if preview.size else None, ctypes.string_at(source.data, source.size) if source.size else None)
        finally:
            self._lib.openqr_result_destroy(result)
            self._lib.openqr_error_destroy(error)


@cache
def _default_engine() -> Engine:
    return Engine()


def generate(payload: str | bytes, reference: bytes | None = None, **options) -> GeneratedBatch:
    return _default_engine().generate(payload, reference, **options)
