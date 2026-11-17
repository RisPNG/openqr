#include <erl_nif.h>
#include <openqr/openqr.h>

#include <string.h>

#if defined(OPENQR_DELAY_LOAD)
#include <windows.h>
#include <delayimp.h>
#include <wchar.h>

extern IMAGE_DOS_HEADER __ImageBase;

static FARPROC WINAPI load_engine_dependency(unsigned notification, PDelayLoadInfo information) {
    if (notification != dliNotePreLoadLibrary || strcmp(information->szDll, OPENQR_NATIVE_DLL_NAME)) return NULL;
    wchar_t path[32768];
    const DWORD length = GetModuleFileNameW((HMODULE)&__ImageBase, path, 32768);
    if (!length || length >= 32768) return NULL;
    wchar_t* name = wcsrchr(path, L'\\');
    if (!name || wcscpy_s(name + 1, 32768 - (size_t)(name + 1 - path), TEXT(OPENQR_NATIVE_DLL_NAME))) return NULL;
    return (FARPROC)LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
}

const PfnDliHook __pfnDliNotifyHook2 = load_engine_dependency;
#endif

static int load_engine(ErlNifEnv* env, void** private_data, ERL_NIF_TERM load_info) {
    (void)env;
    (void)private_data;
    (void)load_info;
#if defined(OPENQR_DELAY_LOAD)
    HRESULT status;
    __try { status = __HrLoadAllImportsForDll(OPENQR_NATIVE_DLL_NAME); }
    __except(EXCEPTION_EXECUTE_HANDLER) { status = E_FAIL; }
    if (FAILED(status)) {
        __FUnloadDelayLoadedDLL2(OPENQR_NATIVE_DLL_NAME);
        return -1;
    }
#endif
    if (openqr_abi_version() != OPENQR_ABI_VERSION) {
#if defined(OPENQR_DELAY_LOAD)
        __FUnloadDelayLoadedDLL2(OPENQR_NATIVE_DLL_NAME);
#endif
        return -1;
    }
    return 0;
}

static void unload_engine(ErlNifEnv* env, void* private_data) {
    (void)env;
    (void)private_data;
#if defined(OPENQR_DELAY_LOAD)
    __FUnloadDelayLoadedDLL2(OPENQR_NATIVE_DLL_NAME);
#endif
}

static ERL_NIF_TERM generate_native(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    ErlNifBinary payload;
    openqr_request request;
    openqr_result* result = NULL;
    openqr_error* error = NULL;
    ERL_NIF_TERM value;
    char body[64], frame[64], ball[64];
    if (argc != 2 || !enif_inspect_binary(env, argv[0], &payload) || !enif_is_map(env, argv[1])) return enif_make_badarg(env);
    openqr_request_init(&request);
    request.payload.data = payload.data;
    request.payload.size = payload.size;
    struct { const char* name; uint32_t* field; } integers[] = {
        {"version", &request.options.version}, {"scale", &request.options.scale},
        {"recovery_margin", &request.options.recovery_margin}, {"seed", &request.options.seed},
        {"background_rgba", &request.options.background_rgba}
    };
    for (size_t i = 0; i < sizeof(integers) / sizeof(integers[0]); ++i) {
        if (enif_get_map_value(env, argv[1], enif_make_atom(env, integers[i].name), &value)) {
            unsigned int number;
            if (!enif_get_uint(env, value, &number)) return enif_make_badarg(env);
            *integers[i].field = number;
            if (strcmp(integers[i].name, "seed") == 0) request.options.flags |= OPENQR_SEED_SET;
        }
    }
    struct { const char* name; double* field; } reals[] = {
        {"contrast", &request.options.contrast}, {"brightness", &request.options.brightness}
    };
    for (size_t i = 0; i < sizeof(reals) / sizeof(reals[0]); ++i) {
        if (enif_get_map_value(env, argv[1], enif_make_atom(env, reals[i].name), &value)) {
            if (!enif_get_double(env, value, reals[i].field)) {
                int number;
                if (!enif_get_int(env, value, &number)) return enif_make_badarg(env);
                *reals[i].field = number;
            }
        }
    }
    struct { const char* name; uint32_t flag; } flags[] = {
        {"color", OPENQR_COLOR}, {"vary_url", OPENQR_VARY_URL}, {"randomize_seed", OPENQR_RANDOMIZE_SEED}
    };
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
        if (enif_get_map_value(env, argv[1], enif_make_atom(env, flags[i].name), &value)) {
            if (enif_is_identical(value, enif_make_atom(env, "true"))) request.options.flags |= flags[i].flag;
            else if (enif_is_identical(value, enif_make_atom(env, "false"))) request.options.flags &= ~flags[i].flag;
            else return enif_make_badarg(env);
        }
    }
    if (enif_get_map_value(env, argv[1], enif_make_atom(env, "error_correction"), &value)) {
        ErlNifBinary correction;
        if (!enif_inspect_binary(env, value, &correction) || correction.size != 1) return enif_make_badarg(env);
        switch (correction.data[0]) {
            case 'L': request.options.flags |= OPENQR_ERROR_CORRECTION_L; break;
            case 'M': request.options.flags |= OPENQR_ERROR_CORRECTION_M; break;
            case 'Q': request.options.flags |= OPENQR_ERROR_CORRECTION_Q; break;
            case 'H': request.options.flags |= OPENQR_ERROR_CORRECTION_H; break;
            default: return enif_make_badarg(env);
        }
    }
    if (enif_get_map_value(env, argv[1], enif_make_atom(env, "mode"), &value)) {
        if (enif_is_identical(value, enif_make_atom(env, "picture"))) request.options.mode = OPENQR_PICTURE;
        else if (enif_is_identical(value, enif_make_atom(env, "mosaic"))) request.options.mode = OPENQR_MOSAIC;
        else return enif_make_badarg(env);
    }
    struct { const char* name; char* buffer; const char** field; } styles[] = {
        {"body_shape", body, &request.options.body_shape}, {"eye_frame", frame, &request.options.eye_frame},
        {"eye_ball", ball, &request.options.eye_ball}
    };
    for (size_t i = 0; i < sizeof(styles) / sizeof(styles[0]); ++i) {
        if (enif_get_map_value(env, argv[1], enif_make_atom(env, styles[i].name), &value)) {
            ErlNifBinary text;
            if (!enif_inspect_binary(env, value, &text) || text.size >= 64 || memchr(text.data, 0, text.size)) return enif_make_badarg(env);
            memcpy(styles[i].buffer, text.data, text.size);
            styles[i].buffer[text.size] = 0;
            *styles[i].field = styles[i].buffer;
        }
    }
    struct { const char* name; openqr_bytes* field; } images[] = {
        {"reference", &request.reference}, {"background", &request.background}
    };
    for (size_t i = 0; i < sizeof(images) / sizeof(images[0]); ++i) {
        if (enif_get_map_value(env, argv[1], enif_make_atom(env, images[i].name), &value)) {
            ErlNifBinary bytes;
            if (!enif_inspect_binary(env, value, &bytes)) return enif_make_badarg(env);
            images[i].field->data = bytes.data;
            images[i].field->size = bytes.size;
        }
    }
    if (openqr_generate(&request, &result, &error) != OPENQR_OK) {
        const char* message = error ? openqr_error_message(error) : "OpenQR generation failed.";
        ERL_NIF_TERM reason;
        unsigned char* bytes = enif_make_new_binary(env, strlen(message), &reason);
        memcpy(bytes, message, strlen(message));
        openqr_error_destroy(error);
        return enif_make_tuple2(env, enif_make_atom(env, "error"), reason);
    }
    ERL_NIF_TERM entries = enif_make_list(env, 0);
    for (size_t remaining = openqr_result_count(result); remaining > 0; --remaining) {
        openqr_entry entry;
        openqr_result_entry(result, remaining - 1, &entry);
        ERL_NIF_TERM map = enif_make_new_map(env);
        struct { const char* name; openqr_bytes bytes; } binaries[] = {
            {"payload", entry.payload}, {"image", entry.image}, {"preview", entry.preview},
            {"svg", entry.svg}, {"target_preview", entry.target_preview}
        };
        for (size_t i = 0; i < sizeof(binaries) / sizeof(binaries[0]); ++i) {
            ERL_NIF_TERM binary;
            unsigned char* bytes = enif_make_new_binary(env, binaries[i].bytes.size, &binary);
            if (binaries[i].bytes.size) memcpy(bytes, binaries[i].bytes.data, binaries[i].bytes.size);
            enif_make_map_put(env, map, enif_make_atom(env, binaries[i].name), binary, &map);
        }
        struct { const char* name; uint32_t number; } numbers[] = {
            {"version", entry.version}, {"seed", entry.seed}, {"verified_frames", entry.verified_frames},
            {"total_frames", entry.total_frames}, {"changed_codewords", entry.changed_codewords}, {"layout_attempts", entry.layout_attempts}
        };
        for (size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); ++i)
            enif_make_map_put(env, map, enif_make_atom(env, numbers[i].name), enif_make_uint(env, numbers[i].number), &map);
        ERL_NIF_TERM errors = enif_make_list(env, 0), capacities = enif_make_list(env, 0);
        for (size_t i = entry.block_count; i > 0; --i) {
            errors = enif_make_list_cell(env, enif_make_int(env, entry.block_errors[i - 1]), errors);
            capacities = enif_make_list_cell(env, enif_make_int(env, entry.block_capacities[i - 1]), capacities);
        }
        ERL_NIF_TERM correction;
        *enif_make_new_binary(env, 1, &correction) = (unsigned char)entry.error_correction;
        enif_make_map_put(env, map, enif_make_atom(env, "block_errors"), errors, &map);
        enif_make_map_put(env, map, enif_make_atom(env, "block_capacities"), capacities, &map);
        enif_make_map_put(env, map, enif_make_atom(env, "error_correction"), correction, &map);
        enif_make_map_put(env, map, enif_make_atom(env, "format"), enif_make_atom(env, entry.format == OPENQR_GIF ? "gif" : "png"), &map);
        enif_make_map_put(env, map, enif_make_atom(env, "mode"), enif_make_atom(env, entry.mode == OPENQR_PICTURE ? "picture" : "mosaic"), &map);
        enif_make_map_put(env, map, enif_make_atom(env, "artwork_fraction"), entry.artwork_fraction < 0 ? enif_make_atom(env, "nil") : enif_make_double(env, entry.artwork_fraction), &map);
        enif_make_map_put(env, map, enif_make_atom(env, "color_exposure"), enif_make_double(env, entry.color_exposure), &map);
        entries = enif_make_list_cell(env, map, entries);
    }
    ERL_NIF_TERM skipped = enif_make_list(env, 0);
    for (size_t i = openqr_result_skipped_count(result); i > 0; --i) {
        ERL_NIF_TERM item = enif_make_new_map(env), reason;
        const char* message = openqr_result_skipped_reason(result, i - 1);
        unsigned char* bytes = enif_make_new_binary(env, strlen(message), &reason);
        memcpy(bytes, message, strlen(message));
        enif_make_map_put(env, item, enif_make_atom(env, "version"), enif_make_uint(env, openqr_result_skipped_version(result, i - 1)), &item);
        enif_make_map_put(env, item, enif_make_atom(env, "reason"), reason, &item);
        skipped = enif_make_list_cell(env, item, skipped);
    }
    ERL_NIF_TERM batch = enif_make_new_map(env);
    enif_make_map_put(env, batch, enif_make_atom(env, "results"), entries, &batch);
    enif_make_map_put(env, batch, enif_make_atom(env, "skipped"), skipped, &batch);
    enif_make_map_put(env, batch, enif_make_atom(env, "seed"), enif_make_uint(env, openqr_result_seed(result)), &batch);
    struct { const char* name; openqr_bytes bytes; } references[] = {
        {"reference_preview", openqr_result_reference_preview(result)}, {"reference_input", openqr_result_reference_input(result)}
    };
    for (size_t i = 0; i < sizeof(references) / sizeof(references[0]); ++i) {
        ERL_NIF_TERM binary;
        unsigned char* bytes = enif_make_new_binary(env, references[i].bytes.size, &binary);
        if (references[i].bytes.size) memcpy(bytes, references[i].bytes.data, references[i].bytes.size);
        enif_make_map_put(env, batch, enif_make_atom(env, references[i].name), binary, &batch);
    }
    openqr_result_destroy(result);
    return enif_make_tuple2(env, enif_make_atom(env, "ok"), batch);
}

static ERL_NIF_TERM inspect_image(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    ErlNifBinary encoded, expected_payload;
    if (argc != 2 || !enif_inspect_binary(env, argv[0], &encoded) || !enif_inspect_binary(env, argv[1], &expected_payload)) return enif_make_badarg(env);
    openqr_image_info info;
    openqr_buffer* pixels = NULL;
    openqr_error* error = NULL;
    if (openqr_inspect_image((openqr_bytes){encoded.data, encoded.size}, (openqr_bytes){expected_payload.data, expected_payload.size}, &info, &pixels, &error) != OPENQR_OK) {
        const char* message = error ? openqr_error_message(error) : "OpenQR image inspection failed.";
        ERL_NIF_TERM reason;
        memcpy(enif_make_new_binary(env, strlen(message), &reason), message, strlen(message));
        openqr_error_destroy(error);
        return enif_make_tuple2(env, enif_make_atom(env, "error"), reason);
    }
    const openqr_bytes source = openqr_buffer_bytes(pixels);
    ERL_NIF_TERM binary, image = enif_make_new_map(env);
    memcpy(enif_make_new_binary(env, source.size, &binary), source.data, source.size);
    enif_make_map_put(env, image, enif_make_atom(env, "pixels"), binary, &image);
    enif_make_map_put(env, image, enif_make_atom(env, "width"), enif_make_uint(env, info.width), &image);
    enif_make_map_put(env, image, enif_make_atom(env, "height"), enif_make_uint(env, info.height), &image);
    enif_make_map_put(env, image, enif_make_atom(env, "verified"), enif_make_atom(env, info.verified ? "true" : "false"), &image);
    openqr_buffer_destroy(pixels);
    return enif_make_tuple2(env, enif_make_atom(env, "ok"), image);
}

static ERL_NIF_TERM styles(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    openqr_style_category category;
    if (argc != 1) return enif_make_badarg(env);
    if (enif_is_identical(argv[0], enif_make_atom(env, "body"))) category = OPENQR_BODY;
    else if (enif_is_identical(argv[0], enif_make_atom(env, "eye_frame"))) category = OPENQR_EYE_FRAME;
    else if (enif_is_identical(argv[0], enif_make_atom(env, "eye_ball"))) category = OPENQR_EYE_BALL;
    else return enif_make_badarg(env);
    ERL_NIF_TERM catalog = enif_make_new_map(env);
    for (size_t i = 0; i < openqr_style_count(category); ++i) {
        const char* id = openqr_style_id(category, i);
        const char* label = openqr_style_label(category, i);
        ERL_NIF_TERM key, value;
        memcpy(enif_make_new_binary(env, strlen(id), &key), id, strlen(id));
        memcpy(enif_make_new_binary(env, strlen(label), &value), label, strlen(label));
        enif_make_map_put(env, catalog, key, value, &catalog);
    }
    return catalog;
}

static ERL_NIF_TERM style_preview(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    openqr_style_category category;
    ErlNifBinary id;
    if (argc != 2 || !enif_inspect_binary(env, argv[1], &id)) return enif_make_badarg(env);
    if (enif_is_identical(argv[0], enif_make_atom(env, "body"))) category = OPENQR_BODY;
    else if (enif_is_identical(argv[0], enif_make_atom(env, "eye_frame"))) category = OPENQR_EYE_FRAME;
    else if (enif_is_identical(argv[0], enif_make_atom(env, "eye_ball"))) category = OPENQR_EYE_BALL;
    else return enif_make_badarg(env);
    size_t index;
    for (index = 0; index < openqr_style_count(category); ++index) {
        const char* candidate = openqr_style_id(category, index);
        if (strlen(candidate) == id.size && !memcmp(candidate, id.data, id.size)) break;
    }
    openqr_buffer* buffer = NULL;
    openqr_error* error = NULL;
    if (openqr_style_preview(category, index, &buffer, &error) != OPENQR_OK) {
        const char* message = error ? openqr_error_message(error) : "OpenQR preview failed.";
        ERL_NIF_TERM reason;
        memcpy(enif_make_new_binary(env, strlen(message), &reason), message, strlen(message));
        openqr_error_destroy(error);
        return enif_make_tuple2(env, enif_make_atom(env, "error"), reason);
    }
    const openqr_bytes source = openqr_buffer_bytes(buffer);
    ERL_NIF_TERM svg;
    memcpy(enif_make_new_binary(env, source.size, &svg), source.data, source.size);
    openqr_buffer_destroy(buffer);
    return enif_make_tuple2(env, enif_make_atom(env, "ok"), svg);
}

static ErlNifFunc functions[] = {
    {"generate_native", 2, generate_native, ERL_NIF_DIRTY_JOB_CPU_BOUND},
    {"inspect_image", 2, inspect_image, ERL_NIF_DIRTY_JOB_CPU_BOUND},
    {"styles", 1, styles, 0},
    {"style_preview", 2, style_preview, ERL_NIF_DIRTY_JOB_CPU_BOUND}
};
ERL_NIF_INIT(Elixir.OpenQR.Engine, functions, load_engine, NULL, NULL, unload_engine)
