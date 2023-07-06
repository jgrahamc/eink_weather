// WiFi networks (SSID (network name) and password)

const int wifi_count = 1;
const char *wifi_networks[1]  = {"TODO"};
const char *wifi_passwords[1] = {"TODO"};

// When to take up to check the weather forecast. This is an
// array of the minutes within an hour when the display should
// reload and update. For example, {0, 15, 30, 45} would update
// every 15 minutes starting at the top of the hour. {0, 30}
// would update at hh:00 and hh:30. These need to be in ascending
// order.

#define UPDATE_TIMES 2
uint8_t update_times[UPDATE_TIMES] = {0, 30};

// Pirate Weather API URL for a forecast. Here's where
// you put your API key, the lat/long you want weather
// for.

const char *api_key = "TODO";
const char *lat     = "TODO";
const char *lon     = "TODO";

// The title to show at the top of the display

const char *title = "TODO";

// Units for display, possible values: ca, uk, us, si.

const char *units = "si";
