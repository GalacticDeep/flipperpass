#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>
#include <subghz/subghz_tx_rx_worker.h>
#include <subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include "protocol.h"
#include "flipperpass_icons.h"

#define FP_DIR         EXT_PATH("apps_data/flipperpass")
#define FP_MAX_CARDS   100
#define FP_RECORD_SIZE (FP_FRAME_SIZE + FP_LOCATION_SIZE + 2)
#define FP_CONFIG_SIZE (FP_RECORD_SIZE + 1)

typedef struct {
    FpProfile profile;
    char location[FP_LOCATION_SIZE];
} Card;

typedef enum {
    PageMenu,
    PageCards,
    PageCard,
    PageEdit,
    PageAvatar,
    PageBand,
    PageInfo
} Page;
typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    Submenu* menu;
    TextInput* input;
    Widget* widget;
    SubGhzTxRxWorker* worker;
    const SubGhzDevice* device;
    FpProfile profile;
    Card cards[FP_MAX_CARDS];
    bool occupied[FP_MAX_CARDS];
    size_t count;
    char location[FP_LOCATION_SIZE];
    char edit[FP_STATUS_SIZE];
    uint8_t editing;
    uint8_t band;
    bool config_saved;
    bool storage_error;
    const char* radio_status;
    Page page;
    FpParser parser;
    uint32_t next_tx;
} App;

static const uint32_t frequencies[] = {0, 915000000, 433920000, 868350000};
static const char* bands[] = {"Off", "915.00 MHz", "433.92 MHz", "868.35 MHz"};
static const char* avatars[] = {"Smile", "Cat", "Robot", "Ghost"};
static const Icon* avatar_icons[] = {&I_smile, &I_cat, &I_robot, &I_ghost};
static void show_menu(App* app);
static void show_cards(App* app);
static void show_info(App* app);
static void select_item(void* context, uint32_t index);

static bool read_blob(App* app, const char* path, uint8_t* data, size_t size) {
    File* file = storage_file_alloc(app->storage);
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(ok) ok = storage_file_size(file) == size && storage_file_read(file, data, size) == size;
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool write_blob(App* app, const char* path, const uint8_t* data, size_t size) {
    File* file = storage_file_alloc(app->storage);
    bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) ok = storage_file_write(file, data, size) == size && storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static void checksum(uint8_t* data, size_t size) {
    uint16_t crc = fp_crc(data, size - 2);
    data[size - 2] = crc & 255;
    data[size - 1] = crc >> 8;
}

static bool valid_checksum(const uint8_t* data, size_t size) {
    return fp_crc(data, size - 2) == (data[size - 2] | ((uint16_t)data[size - 1] << 8));
}

static bool load_config_path(App* app, const char* path) {
    uint8_t data[FP_CONFIG_SIZE];
    if(!read_blob(app, path, data, sizeof(data)) || !valid_checksum(data, sizeof(data)) ||
       !fp_decode(data, &app->profile) || data[85] >= COUNT_OF(frequencies) ||
       !fp_text_valid((char*)data + 60, FP_LOCATION_SIZE, false))
        return false;
    memcpy(app->location, data + 60, FP_LOCATION_SIZE);
    app->band = data[85];
    return true;
}

static bool save_config(App* app) {
    uint8_t data[FP_CONFIG_SIZE] = {0};
    fp_encode(&app->profile, data);
    memcpy(data + 60, app->location, FP_LOCATION_SIZE);
    data[85] = app->band;
    checksum(data, sizeof(data));
    bool ok = write_blob(app, FP_DIR "/profile.tmp", data, sizeof(data));
    // Keep a previous valid generation in case of interruption during replacement.
    if(ok && storage_file_exists(app->storage, FP_DIR "/profile.dat")) {
        storage_common_remove(app->storage, FP_DIR "/profile.bak");
        ok = storage_common_rename(app->storage, FP_DIR "/profile.dat", FP_DIR "/profile.bak") ==
             FSE_OK;
    }
    if(ok)
        ok = storage_common_rename(app->storage, FP_DIR "/profile.tmp", FP_DIR "/profile.dat") ==
             FSE_OK;
    app->config_saved = ok;
    if(!ok) app->storage_error = true;
    return ok;
}

static void card_path(char* path, size_t capacity, size_t index) {
    snprintf(path, capacity, FP_DIR "/card%03u.dat", (unsigned)index);
}

static void load_cards(App* app) {
    for(size_t i = 0; i < FP_MAX_CARDS; i++) {
        char path[80];
        uint8_t data[FP_RECORD_SIZE];
        card_path(path, sizeof(path), i);
        if(!storage_file_exists(app->storage, path)) continue;
        app->occupied[i] = true; // Never silently overwrite a damaged record.
        Card* card = &app->cards[app->count];
        if(read_blob(app, path, data, sizeof(data)) && valid_checksum(data, sizeof(data)) &&
           fp_decode(data, &card->profile) &&
           fp_text_valid((char*)data + 60, FP_LOCATION_SIZE, false)) {
            memcpy(card->location, data + 60, FP_LOCATION_SIZE);
            app->count++;
        } else {
            app->storage_error = true;
        }
    }
}

static void receive_card(App* app, const FpProfile* profile) {
    if(!memcmp(profile->id, app->profile.id, 8)) return;
    for(size_t i = 0; i < app->count; i++) {
        if(!memcmp(profile->id, app->cards[i].profile.id, 8) &&
           !strcmp(app->location, app->cards[i].location))
            return;
    }
    size_t slot = 0;
    while(slot < FP_MAX_CARDS && app->occupied[slot])
        slot++;
    if(slot == FP_MAX_CARDS) return;
    uint8_t data[FP_RECORD_SIZE] = {0};
    fp_encode(profile, data);
    memcpy(data + 60, app->location, FP_LOCATION_SIZE);
    checksum(data, sizeof(data));
    char path[80];
    card_path(path, sizeof(path), slot);
    if(!write_blob(app, FP_DIR "/card.tmp", data, sizeof(data)) ||
       storage_common_rename(app->storage, FP_DIR "/card.tmp", path) != FSE_OK) {
        app->storage_error = true;
        return;
    }
    app->occupied[slot] = true;
    Card* card = &app->cards[app->count++];
    card->profile = *profile;
    memcpy(card->location, app->location, FP_LOCATION_SIZE);
}

static void stop_radio(App* app) {
    if(subghz_tx_rx_worker_is_running(app->worker)) subghz_tx_rx_worker_stop(app->worker);
    app->parser.used = 0;
}

static void start_radio(App* app) {
    stop_radio(app);
    app->radio_status = "Off: select Radio band";
    if(!app->band) return;
    if(!app->config_saved) {
        app->radio_status = "Save failed: radio off";
        return;
    }
    uint32_t frequency = frequencies[app->band];
    // The SDK worker starts a thread even on a denied frequency. Guard first.
    if(!app->device || !subghz_devices_is_frequency_valid(app->device, frequency) ||
       !furi_hal_region_is_frequency_allowed(frequency)) {
        app->radio_status = "Band region-blocked";
        return;
    }
    if(subghz_tx_rx_worker_start(app->worker, app->device, frequency)) {
        app->radio_status = "Listening / beaconing";
        app->next_tx = furi_get_tick() + furi_ms_to_ticks(2000 + furi_hal_random_get() % 4000);
    } else {
        stop_radio(app);
        app->radio_status = "Radio start failed";
    }
}

static void tick(void* context) {
    App* app = context;
    if(!subghz_tx_rx_worker_is_running(app->worker)) return;
    size_t previous_count = app->count;
    bool previous_error = app->storage_error;
    uint8_t bytes[120];
    size_t size = subghz_tx_rx_worker_read(app->worker, bytes, sizeof(bytes));
    FpProfile profile;
    for(size_t i = 0; i < size; i++) {
        if(fp_feed(&app->parser, bytes[i], &profile)) receive_card(app, &profile);
    }
    if(app->count != previous_count || app->storage_error != previous_error) {
        if(app->page == PageInfo)
            show_info(app);
        else if(app->page == PageCards)
            show_cards(app);
    }
    uint32_t now = furi_get_tick();
    if((int32_t)(now - app->next_tx) >= 0) {
        uint8_t frame[FP_FRAME_SIZE];
        fp_encode(&app->profile, frame);
        if(!subghz_tx_rx_worker_write(app->worker, frame, sizeof(frame)))
            app->radio_status = "Radio queue full";
        app->next_tx = now + furi_ms_to_ticks(12000 + furi_hal_random_get() % 6001);
    }
}

static void menu_begin(App* app, Page page, const char* title) {
    app->page = page;
    submenu_reset(app->menu);
    submenu_set_header(app->menu, title);
}
static void menu_item(App* app, const char* label, uint32_t index) {
    submenu_add_item(app->menu, label, index, select_item, app);
}
static void menu_end(App* app) {
    view_dispatcher_switch_to_view(app->dispatcher, 0);
}

static void show_menu(App* app) {
    menu_begin(app, PageMenu, "FlipperPass");
    menu_item(app, "Exchange status", 0);
    menu_item(app, "Collected cards", 1);
    menu_item(app, "Edit status", 3);
    menu_item(app, "Choose avatar", 4);
    menu_item(app, "Current event/location", 5);
    menu_item(app, "Radio band / Off", 6);
    menu_end(app);
}

static void show_cards(App* app) {
    menu_begin(app, PageCards, "Collected cards");
    if(!app->count) menu_item(app, "No cards yet", FP_MAX_CARDS);
    for(size_t i = 0; i < app->count; i++) {
        char label[48];
        snprintf(
            label,
            sizeof(label),
            "%s @ %s",
            app->cards[i].profile.nickname,
            app->cards[i].location);
        menu_item(app, label, i);
    }
    menu_end(app);
}

static void show_card(App* app, size_t index) {
    Card* card = &app->cards[index];
    app->page = PageCard;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 2, 2, AlignLeft, AlignTop, FontPrimary, card->profile.nickname);
    widget_add_icon_element(app->widget, 112, 1, avatar_icons[card->profile.avatar]);
    char details[FP_STATUS_SIZE + FP_LOCATION_SIZE + 16];
    snprintf(details, sizeof(details), "%s\n\nAt: %s", card->profile.status, card->location);
    widget_add_text_scroll_element(app->widget, 2, 17, 124, 47, details);
    view_dispatcher_switch_to_view(app->dispatcher, 2);
}

static void show_info(App* app) {
    app->page = PageInfo;
    widget_reset(app->widget);
    char text[240];
    size_t occupied = 0;
    for(size_t i = 0; i < FP_MAX_CARDS; i++)
        occupied += app->occupied[i];
    snprintf(
        text,
        sizeof(text),
        "Name: %s\n%s\n%s | %u cards\nAt: %s\n%s",
        app->profile.nickname,
        app->radio_status,
        bands[app->band],
        (unsigned)app->count,
        app->location,
        app->storage_error       ? "SD error: check files" :
        occupied == FP_MAX_CARDS ? "Collection full" :
                                   "Back: menu; exit there");
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
    view_dispatcher_switch_to_view(app->dispatcher, 2);
}

static void edit_done(void* context) {
    App* app = context;
    size_t capacity = app->editing == 3 ? FP_STATUS_SIZE : FP_LOCATION_SIZE;
    if(!fp_text_valid(app->edit, capacity, app->editing == 3)) return;
    char* target = app->editing == 3 ? app->profile.status : app->location;
    memset(target, 0, capacity);
    memcpy(target, app->edit, strlen(app->edit));
    save_config(app);
    start_radio(app);
    show_menu(app);
}

static void select_item(void* context, uint32_t index) {
    App* app = context;
    if(app->page == PageCards) {
        if(index < app->count) show_card(app, index);
    } else if(app->page == PageAvatar) {
        app->profile.avatar = index;
        save_config(app);
        start_radio(app);
        show_menu(app);
    } else if(app->page == PageBand) {
        app->band = index;
        save_config(app);
        start_radio(app);
        show_info(app);
    } else if(app->page == PageMenu) {
        if(index == 0)
            show_info(app);
        else if(index == 1)
            show_cards(app);
        else if(index == 4) {
            menu_begin(app, PageAvatar, "Choose avatar");
            for(size_t i = 0; i < FP_AVATARS; i++)
                menu_item(app, avatars[i], i);
            menu_end(app);
        } else if(index == 6) {
            menu_begin(app, PageBand, "Shared legal band");
            for(size_t i = 0; i < COUNT_OF(bands); i++)
                menu_item(app, bands[i], i);
            menu_end(app);
        } else if(index == 3 || index == 5) {
            app->editing = index;
            app->page = PageEdit;
            const char* source = index == 3 ? app->profile.status : app->location;
            size_t capacity = index == 3 ? FP_STATUS_SIZE : FP_LOCATION_SIZE;
            memset(app->edit, 0, sizeof(app->edit));
            memcpy(app->edit, source, strlen(source));
            text_input_reset(app->input);
            text_input_set_header_text(
                app->input, index == 3 ? "Status (30 chars)" : "Event e.g. DEF CON");
            text_input_set_result_callback(app->input, edit_done, app, app->edit, capacity, true);
            text_input_set_minimum_length(app->input, index == 3 ? 0 : 1);
            view_dispatcher_switch_to_view(app->dispatcher, 1);
        }
    }
}

static bool back(void* context) {
    App* app = context;
    if(app->page == PageMenu) {
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
    if(app->page == PageCard)
        show_cards(app);
    else
        show_menu(app);
    return true;
}

int32_t flipperpass_app(void* context) {
    UNUSED(context);
    App* app = calloc(1, sizeof(App));
    app->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(app->storage, EXT_PATH("apps_data"));
    storage_common_mkdir(app->storage, FP_DIR);
    subghz_devices_init();
    app->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    app->config_saved = load_config_path(app, FP_DIR "/profile.dat") ||
                        load_config_path(app, FP_DIR "/profile.bak");
    if(!app->config_saved) {
        memset(&app->profile, 0, sizeof(app->profile));
        furi_hal_random_fill_buf(app->profile.id, sizeof(app->profile.id));
        strcpy(app->profile.status, "Hello nearby Flippers!");
        strcpy(app->location, "Unlabeled");
        app->band = 0;
        // Prefer a shared regional default, never a per-device quiet-channel scan.
        const uint8_t preferred_bands[] = {2, 3};
        for(size_t i = 0; i < COUNT_OF(preferred_bands); i++) {
            uint8_t band = preferred_bands[i];
            if(app->device && subghz_devices_is_frequency_valid(app->device, frequencies[band]) &&
               furi_hal_region_is_frequency_allowed(frequencies[band])) {
                app->band = band;
                break;
            }
        }
    }
    // Refresh old profiles too: the device passport is the source of our name.
    char passport_name[FP_NICK_SIZE] = {0};
    const char* device_name = furi_hal_version_get_name_ptr();
    if(device_name) {
        for(size_t i = 0; i < sizeof(passport_name) - 1 && device_name[i]; i++) {
            unsigned char c = (unsigned char)device_name[i];
            passport_name[i] = c >= 32 && c <= 126 ? (char)c : '?';
        }
    }
    if(!passport_name[0]) strcpy(passport_name, "Flipper");
    bool name_changed = strcmp(app->profile.nickname, passport_name) != 0;
    memcpy(app->profile.nickname, passport_name, sizeof(passport_name));
    if(!app->config_saved || name_changed) save_config(app);
    load_cards(app);
    app->gui = furi_record_open(RECORD_GUI);
    app->dispatcher = view_dispatcher_alloc();
    app->menu = submenu_alloc();
    app->input = text_input_alloc();
    app->widget = widget_alloc();
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, back);
    view_dispatcher_set_tick_event_callback(app->dispatcher, tick, 100);
    view_dispatcher_add_view(app->dispatcher, 0, submenu_get_view(app->menu));
    view_dispatcher_add_view(app->dispatcher, 1, text_input_get_view(app->input));
    view_dispatcher_add_view(app->dispatcher, 2, widget_get_view(app->widget));
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    app->worker = subghz_tx_rx_worker_alloc();
    start_radio(app);
    if(app->band)
        show_menu(app);
    else
        show_info(app);
    view_dispatcher_run(app->dispatcher);
    stop_radio(app);
    subghz_tx_rx_worker_free(app->worker);
    subghz_devices_deinit();
    for(uint32_t i = 0; i < 3; i++)
        view_dispatcher_remove_view(app->dispatcher, i);
    submenu_free(app->menu);
    text_input_free(app->input);
    widget_free(app->widget);
    view_dispatcher_free(app->dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    free(app);
    return 0;
}
