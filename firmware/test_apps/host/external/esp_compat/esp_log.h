#pragma once
// Host stand-in for ESP-IDF logging: the widgets only use ESP_LOGE for
// allocation-failure breadcrumbs, which the tests assert via behaviour
// (degraded widget, no crash), not via log output.

#define ESP_LOGE(tag, fmt, ...) ((void)(tag))
#define ESP_LOGW(tag, fmt, ...) ((void)(tag))
#define ESP_LOGI(tag, fmt, ...) ((void)(tag))
#define ESP_LOGD(tag, fmt, ...) ((void)(tag))
