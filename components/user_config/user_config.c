#include "user_config.h"

#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "user_config";
static const char *NVS_NS = "user_cfg";

static void apply_defaults(user_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    strlcpy(cfg->timezone, "UTC0", sizeof(cfg->timezone));
    cfg->rotation_interval_sec = 60;
    cfg->countdown_seconds = 5 * 60;
}

static esp_err_t read_string(nvs_handle_t nvs, const char *key, char *out, size_t len)
{
    size_t required = len;
    esp_err_t err = nvs_get_str(nvs, key, out, &required);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        out[0] = '\0';
    }
    return (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_OK : err;
}

static esp_err_t load_from_nvs(user_config_t *cfg, uint32_t *asset_id)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    err = read_string(nvs, "ssid", cfg->ssid, sizeof(cfg->ssid));
    if (err == ESP_OK) err = read_string(nvs, "pass", cfg->password, sizeof(cfg->password));
    if (err == ESP_OK) err = read_string(nvs, "tz", cfg->timezone, sizeof(cfg->timezone));
    if (err == ESP_OK) (void)nvs_get_u32(nvs, "rot", &cfg->rotation_interval_sec);
    if (err == ESP_OK) (void)nvs_get_u32(nvs, "count", &cfg->countdown_seconds);
    if (err == ESP_OK && asset_id != NULL) (void)nvs_get_u32(nvs, "asset_id", asset_id);
    nvs_close(nvs);
    return err;
}

static esp_err_t save_to_nvs(const user_assets_config_t *asset_cfg)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    if (err == ESP_OK) err = nvs_set_str(nvs, "ssid", asset_cfg->ssid);
    if (err == ESP_OK) err = nvs_set_str(nvs, "pass", asset_cfg->password);
    if (err == ESP_OK) err = nvs_set_str(nvs, "tz", asset_cfg->timezone);
    if (err == ESP_OK) err = nvs_set_u32(nvs, "rot", asset_cfg->rotation_interval_sec);
    if (err == ESP_OK) err = nvs_set_u32(nvs, "count", asset_cfg->countdown_seconds);
    if (err == ESP_OK) err = nvs_set_u32(nvs, "asset_id", asset_cfg->asset_id);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

esp_err_t user_config_load(user_config_t *out_config)
{
    if (out_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    apply_defaults(out_config);
    uint32_t nvs_asset_id = 0;
    (void)load_from_nvs(out_config, &nvs_asset_id);

    user_assets_config_t asset_cfg;
    if (user_assets_get_config(&asset_cfg) == ESP_OK &&
        asset_cfg.ssid[0] != '\0' && asset_cfg.asset_id != nvs_asset_id) {
        ESP_LOGI(TAG, "migrating flashed user config to NVS");
        ESP_RETURN_ON_ERROR(save_to_nvs(&asset_cfg), TAG, "save user config");
        strlcpy(out_config->ssid, asset_cfg.ssid, sizeof(out_config->ssid));
        strlcpy(out_config->password, asset_cfg.password, sizeof(out_config->password));
        strlcpy(out_config->timezone, asset_cfg.timezone, sizeof(out_config->timezone));
        out_config->rotation_interval_sec = asset_cfg.rotation_interval_sec;
        out_config->countdown_seconds = asset_cfg.countdown_seconds;
    }

    if (out_config->ssid[0] == '\0') {
        ESP_LOGW(TAG, "no Wi-Fi credentials found in NVS or assets partition");
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}
