/*
 * LED-Kaktus Ansteuerungssoftware zur Demonstration von
 * Industrieautomatisationsprotokollen.
 *
 * 15.10.2018 O. Rutsch, Sympatec GmbH
 */
#include "esp32_digital_led_lib.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "tools.h"
#include <driver/gpio.h>
#include <math.h>
#include <string.h>
#include <vector>

#include "lwip/err.h"
#include "lwip/sys.h"

#include "CactusCommands.h"
#include "CactusErrors.h"

using namespace std;

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#define HIGH 1
#define LOW 0
#define OUTPUT GPIO_MODE_OUTPUT
#define INPUT GPIO_MODE_INPUT

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define floor(a) ((int)(a))
#define ceil(a) ((int)((int)(a) < (a) ? (a + 1) : (a)))

/* The examples use WiFi configuration that you can set via 'make menuconfig'.

	 If you'd rather not, just change the below entries to strings with
	 the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define CONFIG_WIFI_SSID "SympatecCactus"
#define CONFIG_WIFI_PASSWORD "cactus2019"
#define EXAMPLE_MAX_STA_CONN 10
//#define CONFIG_BROKER_URL "mqtt://swe-u758"
#define CONFIG_BROKER_URL "mqtt://192.168.0.100"
#define NUM_LEDS 144
#define MAX_LINE_SIZE 2048

/* FreeRTOS event group to signal when we are connected*/
// static EventGroupHandle_t s_wifi_event_group;

static const char *TAG = "Sympatec MQTT Cactus";
static bool smMQTTConnected = false;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static char *pCurrentCommandLine;
int gActiveEffect = 0;

// Prototypen
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
static esp_err_t wifi_event_handler(void *ctx, system_event_t *event);
void app_cpp_main(void *);
static void mqtt_app_start(void);
static void wifi_init(void);
void LedEffect(int aEffectNum, int aDuration_ms);
vector<uint8_t> GetLEDsPercent(double aPercent);
int CheckRGB(std::string &, std::string &, std::string &, int &, int &, int &);
void SetCactusPercent(int aPercentage, pixelColor_t aColor);
uint32_t ProcessCommandLine(char *aCmdLine);

strand_t STRANDS[] = {
	// Avoid using any of the strapping pins on the ESP32
	{.rmtChannel = 0, .gpioNum = 17, .ledType = LED_WS2813_V2, .brightLimit = 255, .numPixels = NUM_LEDS, .pixels = NULL, ._stateVars = NULL},
};

int STRANDCNT = sizeof(STRANDS) / sizeof(STRANDS[0]);

static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;

extern "C" int app_main(void)
{
	pCurrentCommandLine = (char *)malloc(MAX_LINE_SIZE);
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	ESP_LOGI(TAG, "ESP_WIFI_MODE_CLIENT");
	wifi_init();

	mqtt_app_start();
	xTaskCreatePinnedToCore(app_cpp_main,		/* Function to implement the task */
													"app_cpp_main", /* Name of the task */
													10000,					/* Stack size in words */
													NULL,						/* Task input parameter */
													0,							/* Priority of the task */
													NULL,						/* Task handle. */
													1);							/* Core where the task should run */
	while (true) {}
	free(pCurrentCommandLine);
	return 0;
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
	esp_mqtt_client_handle_t client = event->client;
	int msg_id;
	// your_context_t *context = event->context;
	switch (event->event_id)
	{
		case MQTT_EVENT_CONNECTED:
			smMQTTConnected = true;
			msg_id = esp_mqtt_client_subscribe(client, "/cactus/commands", 0);
			ESP_LOGI(TAG, "subscribe to /cactus/commands successful, msg_id=%d", msg_id);

			break;
		case MQTT_EVENT_DISCONNECTED:
			smMQTTConnected = false;
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			break;

		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			// msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
			// ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			// ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			// ESP_LOGI(TAG, "MQTT_EVENT_DATA");
			// printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
			// printf("Data offset: %d",event->current_data_offset);
			// printf("DATA=%.*s\r\n", event->data_len, event->data);
			if (std::string(event->topic, event->topic_len) == std::string("/cactus/commands"))
			{
				// Wir gehen immer von einem Textprotokoll aus, deshalb wird die Zeile kopiert und null-terminiert
				int length = min(MAX_LINE_SIZE, event->data_len + 1);
				memcpy(pCurrentCommandLine, event->data, length);
				pCurrentCommandLine[length - 1] = 0;
				ProcessCommandLine(pCurrentCommandLine);
			}
			else
			{
				printf("Unknown TOPIC='%.*s'\r\n", event->topic_len, event->topic);
			}

			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			break;
		case MQTT_EVENT_BEFORE_CONNECT:
			ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
			break;
	}
	return ESP_OK;
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
	switch (event->event_id)
	{
		case SYSTEM_EVENT_STA_START:
			esp_wifi_connect();
			break;
		case SYSTEM_EVENT_STA_GOT_IP:
			xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			esp_wifi_connect();
			xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
			break;
		default:
			break;
	}
	return ESP_OK;
}

static void wifi_init(void)
{
	tcpip_adapter_init();
	wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	// WiFi-Energiesparmodus ausschalten
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	wifi_config_t wifi_config = {};
	strcpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID);
	strcpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWORD);

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_LOGI(TAG, "start the WIFI SSID:[%s]", CONFIG_WIFI_SSID);
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "Waiting for wifi");
	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

static void mqtt_app_start(void)
{
	ESP_LOGI(TAG, "MQTT setup start...");
	esp_mqtt_client_config_t mqtt_cfg = {};
	mqtt_cfg.uri = CONFIG_BROKER_URL;
	mqtt_cfg.event_handle = mqtt_event_handler;
	mqtt_cfg.user_context = NULL;
	mqtt_cfg.buffer_size = 2048;
	ESP_LOGI(TAG, "MQTT setup finished");
	// .user_context = (void *)your_context
	ESP_LOGI(TAG, "MQTT init start...");
	mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
	ESP_LOGI(TAG, "MQTT init end...and starting client");
	esp_mqtt_client_start(mqtt_client);
	ESP_LOGI(TAG, "MQTT client started.");
}

void gpioSetup(int gpioNum, int gpioMode, int gpioVal)
{
#if defined(ARDUINO) && ARDUINO >= 100
	pinMode(gpioNum, gpioMode);
	digitalWrite(gpioNum, gpioVal);
#elif defined(ESP_PLATFORM)
	gpio_num_t gpioNumNative = (gpio_num_t)(gpioNum);
	gpio_mode_t gpioModeNative = (gpio_mode_t)(gpioMode);
	gpio_pad_select_gpio(gpioNumNative);
	gpio_set_direction(gpioNumNative, gpioModeNative);
	gpio_set_level(gpioNumNative, gpioVal);
#endif
}

uint32_t IRAM_ATTR millis() { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

void delay(uint32_t ms)
{
	if (ms > 0)
	{
		vTaskDelay(ms / portTICK_PERIOD_MS);
	}
}

class Rainbower
{
private:
	const uint8_t color_div = 4;
	const uint8_t anim_step = 5;
	uint8_t anim_max;
	uint8_t stepVal1;
	uint8_t stepVal2;
	uint8_t StepCounter = 0;
	pixelColor_t color1;
	pixelColor_t color2;

public:
	Rainbower();
	pixelColor_t drawNext();
};

Rainbower::Rainbower()
{
	anim_max = 255 - anim_step;
	stepVal1 = 0;
	stepVal2 = 0;
	color1 = pixelFromRGB(anim_max, 0, 0);
	color2 = pixelFromRGB(anim_max, 0, 0);
}

pixelColor_t Rainbower::drawNext()
{
	pixelColor_t CurrentColor = pixelFromRGB(color1.r / color_div, color1.g / color_div, color1.b / color_div);
	switch (stepVal1)
	{
		case 0:
			color1.g += anim_step;
			if (color1.g >= anim_max)
				stepVal1++;
			break;
		case 1:
			color1.r -= anim_step;
			if (color1.r == 0)
				stepVal1++;
			break;
		case 2:
			color1.b += anim_step;
			if (color1.b >= anim_max)
				stepVal1++;
			break;
		case 3:
			color1.g -= anim_step;
			if (color1.g == 0)
				stepVal1++;
			break;
		case 4:
			color1.r += anim_step;
			if (color1.r >= anim_max)
				stepVal1++;
			break;
		case 5:
			color1.b -= anim_step;
			if (color1.b == 0)
				stepVal1 = 0;
			break;
	}
	StepCounter++;
	return CurrentColor;
}

class Scannerer
{
private:
	strand_t *pStrand;
	pixelColor_t BlackColor;
	unsigned int currIdx;

public:
	Scannerer(strand_t *);
	void drawNext(pixelColor_t);
};

Scannerer::Scannerer(strand_t *pStrandIn)
{
	pStrand = pStrandIn;
	BlackColor = pixelFromRGB(0, 0, 0);
	currIdx = 0;
}

void Scannerer::drawNext(pixelColor_t aColor)
{
	const int GlowTail = 30;
	float r, g, b;
	for (int i = 0; i < GlowTail; i++)
	{
		pixelColor_t CurrentColor = aColor;
		r = CurrentColor.r;
		g = CurrentColor.g;
		b = CurrentColor.b;
		r /= ((20.0 / GlowTail) * i + 1);
		g /= ((20.0 / GlowTail) * i + 1);
		b /= ((20.0 / GlowTail) * i + 1);
		CurrentColor.r = r;
		CurrentColor.g = g;
		CurrentColor.b = b;
		pStrand->pixels[(currIdx - i) % pStrand->numPixels] = CurrentColor;
	}
	pStrand->pixels[(currIdx - GlowTail) % pStrand->numPixels] = BlackColor;
	digitalLeds_updatePixels(pStrand);
	currIdx++;
}

Rainbower MyRainbow;

void scanner(strand_t *pStrand, unsigned long delay_ms, unsigned long timeout_ms)
{
	Scannerer MyScanner(pStrand);

	for (int i = 0; i < pStrand->numPixels; i++)
	{
		pixelColor_t c = MyRainbow.drawNext();
		MyScanner.drawNext(c);
		delay(delay_ms);
	}
	// digitalLeds_resetPixels(pStrand);
}

// digitales Abbild des Kaktus, jeden cm abgerastert von unten nach oben
const vector<vector<uint8_t>> cactus{
	{0, 143},
	{1, 2, 142},
	{3, 4, 140, 141},
	{5, 139},
	{6, 138},
	{7, 8, 136, 137},
	{9, 135},
	{10, 133, 134},
	{11, 131, 132},
	{12, 13, 127, 128, 129, 130},
	{14, 15, 16, 125, 126, 96, 97, 98, 99},
	{17, 18, 19, 94, 95, 100, 101, 124, 123},
	{20, 48, 49, 92, 93, 102, 103, 121, 122},
	{21, 22, 46, 47, 50, 51, 90, 91, 104, 120},
	{23, 44, 45, 52, 53, 88, 89, 105, 119},
	{24, 43, 54, 87, 106, 107, 117, 118},
	{25, 26, 41, 42, 55, 86, 108, 116},
	{27, 40, 56, 57, 85, 109, 115},
	{28, 29, 38, 39, 58, 59, 84, 83, 110, 111, 113, 114},
	{30, 31, 37, 60, 81, 82, 112},
	{32, 36, 61, 80},
	{33, 34, 35, 62, 79, 78},
	{63, 64, 77},
	{65, 75, 76},
	{66, 74, 67},
	{68, 73},
	{69, 72},
	{70, 71},
};

vector<uint8_t> GetLEDsPercent(double aPercent)
{
	const int Columns = cactus.size();
	int MaxCol = int(Columns * (aPercent / 100.0) + 0.5);
	vector<uint8_t> Leds;
	for (int i = 0; i < MaxCol; i++)
	{
		Leds.insert(Leds.end(), cactus.at(i).begin(), cactus.at(i).end());
	}
	return Leds;
}

void app_cpp_main(void *pvParameters)
{
	bool toggle = false;
	gpioSetup(17, OUTPUT, LOW);
	gpioSetup(2, OUTPUT, HIGH);

	if (digitalLeds_initStrands(STRANDS, STRANDCNT))
	{
		ets_printf("Init FAILURE: halting\n");
		while (true)
		{
			toggle = !toggle;
			gpio_set_level(GPIO_NUM_2, (uint32_t)toggle);
			delay(100);
		};
	}

	uint32_t CurrentTime = 0;
	uint32_t LastTime = millis();
	strand_t *pStrand = &STRANDS[0];
	Rainbower MyRainbow;
	bool ConnectMsg = true;
	for (int i = 0; i <= 100; i += 5)
	{
		MyRainbow.drawNext();
		MyRainbow.drawNext();
		pixelColor_t Color = MyRainbow.drawNext();
		SetCactusPercent(i, Color);
		delay(50);
	}

	delay(500);
	for (int i = 100; i >= 0; i -= 5)
	{
		MyRainbow.drawNext();
		MyRainbow.drawNext();
		pixelColor_t Color = MyRainbow.drawNext();
		SetCactusPercent(i, Color);
		delay(50);
	}

	for (;;)
	{
		if (gActiveEffect > 0)
			LedEffect(-1, 0); // Effektschleife aktivieren
		// Status der LEDs jede Sekunde veröffentlichen
		CurrentTime = millis();
		if ((CurrentTime - LastTime) > 1000)
		{
			LastTime = CurrentTime;
			if (smMQTTConnected)
			{
				if (ConnectMsg)
				{
					ConnectMsg = false;
					pixelColor_t Color = pixelFromRGB(0, 100, 0);
					SetCactusPercent(100, Color);
					delay(50);
					SetCactusPercent(0, Color);
					delay(50);
					SetCactusPercent(100, Color);
					delay(50);
					SetCactusPercent(0, Color);
				}
				// ESP_LOGI(TAG, "publishing LED status");
				esp_mqtt_client_publish(mqtt_client, "/cactus/LED_STATUS", (char *)pStrand->pixels, pStrand->numPixels * sizeof(pixelColor_t), 1,
																0);
				// ESP_LOGI(TAG, "publish return status, msg_id=%d", msg_id);
				toggle = !toggle;
				gpio_set_level(GPIO_NUM_2, (uint32_t)toggle);
			}
			else
			{
				ConnectMsg = true;
				pixelColor_t Color = pixelFromRGB(100, 0, 0);
				SetCactusPercent(100, Color);
				delay(50);
				SetCactusPercent(0, Color);
			}
		}
	}
	vTaskDelete(NULL);
}

uint32_t ProcessCommandLine(char *aCmdLine)
{
	std::vector<std::string> iTokens;
	KillReturnAndEndl(aCmdLine);
	string InputLine(aCmdLine);
	ParseLine(InputLine, iTokens);

	TCactusCommand CurrentCommand = eCMD_UNKNOWN;

	if (iTokens.size() == 0)
	{
		ESP_LOGI(TAG, "Empty command detected...");
		return ERR_EMPTYCMD;
	}
	// Nachsehen, welches Kommando gesendet wurde
	for (unsigned int i = 0; i < sizeof(CactusCommands) / sizeof(CactusCommands[0]); i++)
	{
		if (iTokens.at(0) == std::string(CactusCommands[i].mCmdStr))
		{
			CurrentCommand = CactusCommands[i].mCmd;
			break;
		}
	}
	if (CurrentCommand == eCMD_UNKNOWN)
	{
		ESP_LOGI(TAG, "Unknown cactus command: %s", iTokens.at(0).c_str());
		return ERR_UNKNOWNCOMMAND;
	}
	if ((int(iTokens.size()) - 1) < CactusCommands[CurrentCommand].mNumParams)
	{
		ESP_LOGI(TAG, "Number of parameters incorrect: %d %d %s", int(iTokens.size()) - 1, CactusCommands[CurrentCommand].mNumParams,
						 InputLine.c_str());
		return ERR_NUMPARAMS;
	}

	// ESP_LOGI(TAG, "Executing command: %d", CurrentCommand);

	strand_t *pStrand = &STRANDS[0];

	// Jetzt können die Kommandos ausgewertet und verarbeitet werden.
	switch (CurrentCommand)
	{
		case eCMD_SET_LED:
		{
			int iLedNumber = -1;
			int r, g, b;
			iLedNumber = strtol(iTokens.at(1).c_str(), NULL, 10);
			if (iLedNumber < 0 || iLedNumber >= NUM_LEDS)
			{
				ESP_LOGI(TAG, "LED number out of bounds (0-%d): %s", NUM_LEDS, iTokens.at(1).c_str());
				return ERR_PARAM_OUT_OF_BOUNDS;
			}
			int Err = CheckRGB(iTokens.at(2), iTokens.at(3), iTokens.at(4), r, g, b);
			if (Err != ERR_OK)
			{
				return Err;
			}

			pixelColor_t Color = pixelFromRGB(r, g, b);

			pStrand->pixels[iLedNumber] = Color;
			digitalLeds_updatePixels(pStrand);
			ESP_LOGI(TAG, "SETLED %d %d %d %d executed.", iLedNumber, r, g, b);
			// delay(50);
			break;
		}
		case eCMD_SET_LED_RANGE:
		{
			int iStartLedNumber = -1;
			int iEndLedNumber = -1;
			int r, g, b;
			iStartLedNumber = strtol(iTokens.at(1).c_str(), NULL, 10);
			iEndLedNumber = strtol(iTokens.at(2).c_str(), NULL, 10);
			if (iStartLedNumber < 0 || iStartLedNumber > NUM_LEDS)
			{
				ESP_LOGI(TAG, "Start LED number out of bounds (0-%d): %s", NUM_LEDS, iTokens.at(1).c_str());
				return ERR_PARAM_OUT_OF_BOUNDS;
			}
			if (iEndLedNumber < 0 || iEndLedNumber > NUM_LEDS)
			{
				ESP_LOGI(TAG, "End LED number out of bounds (0-%d): %s", NUM_LEDS, iTokens.at(2).c_str());
				return ERR_PARAM_OUT_OF_BOUNDS;
			}
			if (iStartLedNumber > iEndLedNumber)
			{
				ESP_LOGI(TAG, "Start LED number > end LED number: Start:%s End:%s", iTokens.at(1).c_str(), iTokens.at(2).c_str());
				return ERR_PARAM_OUT_OF_BOUNDS;
			}
			int NumLedsToSet = iEndLedNumber - iStartLedNumber + 1;
			int ExpectedParams = NumLedsToSet * 3;
			if (ExpectedParams > iTokens.size() - 3)
			{
				ESP_LOGI(TAG, "Too few RGB parameters. Expected %d, got %d", ExpectedParams, iTokens.size() - 3);
				return ERR_PARAM_OUT_OF_BOUNDS;
			}
			// ESP_LOGI(TAG, "Start:%d End:%d Num LEDs to set:%d", iStartLedNumber, iEndLedNumber, NumLedsToSet);
			for (int i = iStartLedNumber; i <= iEndLedNumber; i++)
			{
				int Err = CheckRGB(iTokens.at(3 + (i - iStartLedNumber) * 3), iTokens.at(4 + (i - iStartLedNumber) * 3),
													 iTokens.at(5 + (i - iStartLedNumber) * 3), r, g, b);
				// printf("r:%d g%d b%d",r,g,b);
				if (Err != ERR_OK)
				{
					return Err;
				}

				pixelColor_t Color = pixelFromRGB(r, g, b);

				pStrand->pixels[i] = Color;
			}
			digitalLeds_updatePixels(pStrand);
			break;
		}
		case eCMD_LED_EFFECT:
		{
			int EffectNumber = strtol(iTokens.at(1).c_str(), NULL, 10);
			if (EffectNumber < 0 || EffectNumber > 6)
			{
				ESP_LOGI(TAG, "Effect number out of bounds (0-6): %s", iTokens.at(1).c_str());
				return ERR_PARAM_OUT_OF_BOUNDS;
			}
			gActiveEffect = EffectNumber;
			break;
		}
		case eCMD_FILL_CACTUS:
		{
			int r, g, b;
			int FillPercent = strtol(iTokens.at(1).c_str(), NULL, 10);
			if (FillPercent < 0 || FillPercent > 100)
			{
				ESP_LOGI(TAG, "Percentage out of bounds (0-100): %s", iTokens.at(1).c_str());
				return ERR_PARAM_OUT_OF_BOUNDS;
			}
			int Err = CheckRGB(iTokens.at(2), iTokens.at(3), iTokens.at(4), r, g, b);
			if (Err != ERR_OK)
			{
				return Err;
			}
			pixelColor_t Color = pixelFromRGB(r, g, b);
			SetCactusPercent(FillPercent, Color);
			break;
		}
		default:
		{
			ESP_LOGI(TAG, "Command not (yet) implemented: %s", InputLine.c_str());
			return ERR_CMD_NOTSUPPORTED;
		}
	}
	return ERR_OK;
}

int CheckRGB(std::string &rs, std::string &gs, std::string &bs, int &r, int &g, int &b)
{
	r = -1;
	g = -1;
	b = -1;
	r = strtol(rs.c_str(), NULL, 10);
	g = strtol(gs.c_str(), NULL, 10);
	b = strtol(bs.c_str(), NULL, 10);

	if (r < 0 || r > 255)
	{
		ESP_LOGI(TAG, "Parameter red out of bounds (0-255): %s", rs.c_str());
		return ERR_PARAM_OUT_OF_BOUNDS;
	}
	if (g < 0 || g > 255)
	{
		ESP_LOGI(TAG, "Parameter green out of bounds (0-255): %s", gs.c_str());
		return ERR_PARAM_OUT_OF_BOUNDS;
	}
	if (b < 0 || b > 255)
	{
		ESP_LOGI(TAG, "Parameter blue out of bounds (0-255): %s", bs.c_str());
		return ERR_PARAM_OUT_OF_BOUNDS;
	}
	return ERR_OK;
}

void SetCactusPercent(int aPercentage, pixelColor_t aColor)
{
	strand_t *pStrand = &STRANDS[0];
	pixelColor_t BlackColor = pixelFromRGB(0, 0, 0);
	vector<uint8_t> Leds = GetLEDsPercent(aPercentage);
	for (int t = 0; t < pStrand->numPixels; t++)
		pStrand->pixels[t] = BlackColor;
	for (int t = 0; t < Leds.size(); t++)
		pStrand->pixels[Leds.at(t)] = aColor;
	digitalLeds_updatePixels(pStrand);
}

void LedEffect(int aEffectNum, int aDuration_ms)
{
	bool toggle = true;
	strand_t *pStrand = &STRANDS[0];

	Rainbower MyRainbow;
	Scannerer MyScanner(pStrand);

	int StartTime = millis();
	int i = 0;
	// Bei aEffectNum=-1 wird auf den gActiveEffect gelauscht
	while (((millis() - StartTime) < aDuration_ms) || aEffectNum == -1)
	{
		int CurrentEffect = aEffectNum;
		if (aEffectNum == -1)
		{
			if (gActiveEffect == 0)
			{
				// Kaktus ausschalten
				pixelColor_t BlackColor = pixelFromRGB(0, 0, 0);
				for (int t = 0; t < pStrand->numPixels; t++)
				{
					pStrand->pixels[t] = BlackColor;
				}
				digitalLeds_updatePixels(pStrand);
				break; // Effektschleife beenden
			}
			else
			{
				CurrentEffect = gActiveEffect;
			}
		}

		switch (CurrentEffect)
		{
			case 1:
			{
				pixelColor_t c = MyRainbow.drawNext();
				MyScanner.drawNext(c);
				delay(15);

				break;
			}
			case 2:
			{
				for (int t = 0; t < pStrand->numPixels / 2; t++)
				{
					pixelColor_t c = MyRainbow.drawNext();
					pStrand->pixels[t] = c;
					pStrand->pixels[pStrand->numPixels - t - 1] = c;
					digitalLeds_updatePixels(pStrand);
					delay(15);
				}
				break;
			}
			case 3:
			{
				pixelColor_t BlackColor = pixelFromRGB(0, 0, 0);

				for (int t = 0; t < pStrand->numPixels; t++)
				{
					pixelColor_t BlinkColor = MyRainbow.drawNext();
					pixelColor_t c = toggle ? BlackColor : BlinkColor;
					pStrand->pixels[t] = c;
				}
				digitalLeds_updatePixels(pStrand);
				delay(50);

				break;
			}

			case 4:
			{
				pixelColor_t BlackColor = pixelFromRGB(0, 128, 0);
				pixelColor_t WhiteColor = pixelFromRGB(255, 255, 255);

				for (int t = 0; t < pStrand->numPixels; t++)
				{
					pixelColor_t BlinkColor = ((t + i) % 5) ? BlackColor : WhiteColor;
					pStrand->pixels[t] = BlinkColor;
				}
				digitalLeds_updatePixels(pStrand);
				delay(50);

				break;
			}

			case 5:
			{
				pixelColor_t BlackColor = pixelFromRGB(128, 0, 0);
				pixelColor_t WhiteColor = pixelFromRGB(255, 255, 255);

				for (int t = 0; t < pStrand->numPixels; t++)
				{
					pixelColor_t BlinkColor = ((t + i) % 5) ? BlackColor : WhiteColor;
					pStrand->pixels[t] = BlinkColor;
				}
				digitalLeds_updatePixels(pStrand);
				delay(50);

				break;
			}

			case 6:
			{
				pixelColor_t GreenColor = pixelFromRGB(rand() % 256, rand() % 256, rand() % 256);
				pixelColor_t BlackColor = pixelFromRGB(0, 0, 0);
				for (int i = 1; i <= 100; i++)
				{
					vector<uint8_t> Leds = GetLEDsPercent(i);
					for (int t = 0; t < pStrand->numPixels; t++)
						pStrand->pixels[t] = BlackColor;
					for (int t = 0; t < Leds.size(); t++)
						pStrand->pixels[Leds.at(t)] = GreenColor;
					digitalLeds_updatePixels(pStrand);
					delay(10);
				}
				break;
			}

		} // end switch
		toggle = !toggle;
		i++;
		gpio_set_level(GPIO_NUM_2, (uint32_t)toggle);
	}
}
