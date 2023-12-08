/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2022 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdbool.h>
#include <string.h>
#include "sgtl5000.h"
#include "testAudio.h"
#include "audioPlay.h"
#include "lcdUartComm.h"
#include "driver/include/m2m_wifi.h"
#include "conf_winc.h"

#include "ble_manager.h"
#include "driver/include/m2m_periph.h"
#include "m2m_ble.h"
#include "at_ble_api.h"
#include "socket/include/socket.h"
#include "wifi_provisioning.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_spi2_tx;
DMA_HandleTypeDef hdma_i2s2_ext_rx;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi3;
SPI_HandleTypeDef hspi5;
DMA_HandleTypeDef hdma_spi5_tx;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart7;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = { .name = "defaultTask",
		.stack_size = 1024 * 4, .priority = (osPriority_t) osPriorityNormal, };
/* USER CODE BEGIN PV */
osThreadId_t commTaskHandle;
const osThreadAttr_t commTask_attributes = { .name = "commTask", .stack_size =
		1024 * 4, .priority = (osPriority_t) osPriorityNormal, };

osThreadId_t wifiProvTaskHandle;
const osThreadAttr_t wifiProvTask_attributes = { .name = "wifiProvTask",
		.stack_size = 1024 * 4, .priority = (osPriority_t) osPriorityNormal, };

PlayState playState = PLAY_NONE;
char audioFileName[128] = { 0, };

FATFS fs;
FATFS *pfs;
FIL fil;
FRESULT fres;
DWORD fre_clust;
uint32_t totalSpace, freeSpace;

extern uint32 nmi_inet_addr(char *pcIpAddr);

#define APP_WIFI_PROV_DISPLAY_NAME  ("3400 DEMO")
typedef enum {
	APP_STATE_IDLE = 0,
	APP_STATE_WAITING_FOR_WIFI_DISCONNECTION,
	APP_STATE_PROVISIONING,
	APP_STATE_WAITING_FOR_BUTTON_PRESS,
	APP_STATE_WAITING_FOR_WIFI_CONNECTION,
	APP_STATE_WAITING_FOR_PROFIFE_SWITCH,
	APP_STATE_COMPLETED
} app_state_t;

app_state_t app_state = APP_STATE_WAITING_FOR_WIFI_DISCONNECTION;

#define CONNECTION_ATTEMPT_CONNECTING				0
#define CONNECTION_ATTEMPT_CONNECTED				1
#define CONNECTION_ATTEMPT_PROVISIONED				2
#define CONNECTION_ATTEMPT_DISCONNECTED				3
#define CONNECTION_ATTEMPT_FAILED					4
#define WIFI_CONNECT_TIMEOUT						15000

static volatile uint8 gu8WiFiConnectionState = M2M_WIFI_UNDEF;
static volatile uint8 gu8BtnEvent;
static uint8 gu8ScanIndex;
static at_ble_event_parameter_t gu8BleParam __aligned(4);

// UDP Client Variables
/** Message format definitions. */
typedef struct PayloadFormat_t {
	uint8_t wifiStation;
	uint8_t seqNumber;
	uint8_t data[APP_BUFFER_SIZE];
} payloadFormat_t;

static payloadFormat_t appPayload;
static uint16_t retryCnt = 0;
static uint16_t iterationCnt = 0;

/** Socket for Tx */
static SOCKET tx_socket = -1;

/** UDP packet count */
static uint8_t packetCnt = 0;

extern volatile uint32_t msTicks;
extern volatile uint32_t runCnt;

// SMTP Variables
/** IP address of host. */
uint32_t gu32HostIp = 0;

uint8_t gu8SocketStatus = SocketInit;

/** SMTP information. */
uint8_t gu8SmtpStatus = SMTP_INIT;

/** SMTP email error information. */
int8_t gs8EmailError = MAIN_EMAIL_ERROR_NONE;

/** Send and receive buffer definition. */
char gcSendRecvBuffer[MAIN_SMTP_BUF_LEN];

/** Handler buffer definition. */
char gcHandlerBuffer[MAIN_SMTP_BUF_LEN];

/** Username basekey definition. */
char gcUserBasekey[128];

/** Password basekey definition. */
char gcPasswordBasekey[128];

/** Retry count. */
uint8_t gu8RetryCount = 0;

/** TCP client socket handler. */
static SOCKET tcp_client_socket = -1;

/** Wi-Fi status variable. */
static bool gbConnectedWifi = false;

/** Get host IP status variable. */
static bool gbHostIpByName = false;

char strLogBuffer[4096] = { 0 };
char strSendingEmail[4096] = { 0 };

extern void ConvertToBase64(char *pcOutStr, const char *pccInStr, int iLen);

/** Return Codes */
const char cSmtpCodeReady[] = { '2', '2', '0', '\0' };
const char cSmtpCodeOkReply[] = { '2', '5', '0', '\0' };
const char cSmtpCodeIntermedReply[] = { '3', '5', '4', '\0' };
const char cSmtpCodeAuthReply[] = { '3', '3', '4', '\0' };
const char cSmtpCodeAuthSuccess[] = { '2', '3', '5', '\0' };

/** Send Codes */
const char cSmtpHelo[] = { 'H', 'E', 'L', 'O', '\0' };
const char cSmtpMailFrom[] = { 'M', 'A', 'I', 'L', ' ', 'F', 'R', 'O', 'M', ':',
		' ', '\0' };
const char cSmtpRcpt[] = { 'R', 'C', 'P', 'T', ' ', 'T', 'O', ':', ' ', '\0' };
const char cSmtpData[] = "DATA";
const char cSmtpCrlf[] = "\r\n";
const char cSmtpSubject[] = "Subject: ";
const char cSmtpTo[] = "To: ";
const char cSmtpFrom[] = "From: ";
const char cSmtpDataEnd[] = { '\r', '\n', '.', '\r', '\n', '\0' };
const char cSmtpQuit[] = { 'Q', 'U', 'I', 'T', '\r', '\n', '\0' };

RTC_DateTypeDef rtc_date;
RTC_TimeTypeDef rtc_time;
uint8_t flagOneMinute = 0;
uint8_t flagOneMinuteOnce = 0;

uint8_t systemVolume = 32;
//RTC_AlarmTypeDef  rtc_alarm;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_UART7_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_DMA_Init(void);
static void MX_I2S2_Init(void);
static void MX_I2C2_Init(void);
static void MX_SPI3_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI5_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_RTC_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
void StartCommTask(void *argument);
void StartWiFiProvTask(void *argument);

int __io_putchar(int ch) {
	(void) HAL_UART_Transmit(&huart7, (uint8_t*) &ch, 1, 100);
	return ch;
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t B2D(uint8_t byte) {
	uint8_t low, high;
	low = byte & 0x0F;
	high = ((byte >> 4) & 0x0F) * 10;
	return high + low;
}

uint8_t D2B(uint8_t byte) {
	return ((byte / 10) << 4) + (byte % 10);
}

static uint8_t btRxBuffer[1];
static uint8_t wifiRxBuffer[1];

void StartBTUartReceiveProc(void) {
	if (HAL_UART_Receive_IT(&huart2, btRxBuffer, 1) != HAL_OK)
		Error_Handler();
}

void BTUartReceiveCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART2) {
//		HAL_UART_Transmit(&huart7, (uint8_t*) &btRxBuffer, 1, 100);
		HAL_UART_Receive_IT(&huart2, btRxBuffer, 1);
	}
}

void StartWiFiUartReceiveProc(void) {
	if (HAL_UART_Receive_IT(&huart3, wifiRxBuffer, 1) != HAL_OK)
		Error_Handler();
}

void WiFiUartReceiveCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART3) {
//		HAL_UART_Transmit(&huart7, (uint8_t*) &wifiRxBuffer, 1, 100);
		HAL_UART_Receive_IT(&huart3, wifiRxBuffer, 1);
	}
}

typedef struct {
	uint16_t red;
	uint16_t green;
	uint16_t blue;
	uint16_t w6500;
	uint16_t w2400;
} LedStrip;

void SetLedStrip(LedStrip led) {
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, led.blue);
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, led.red);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, led.w2400);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, led.w6500);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, led.green);
}

void pwm_update_duty_cycle(uint8_t red, uint8_t green, uint8_t blue,
		uint8_t white, uint8_t w_white) {
	LedStrip ledstrip;
	ledstrip.red = red;
	ledstrip.green = green;
	ledstrip.blue = blue;
	ledstrip.w2400 = white;
	ledstrip.w6500 = w_white;
	SetLedStrip(ledstrip);
}

void StartPlayAudioFile(char *filename) {
	if (playState == PLAY_STARTED)
		sgtl5000_stop_play();

	HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BCD);
	HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BCD);

	/* Mount SDCARD */
	if (f_mount(&fs, "", 0) != FR_OK)
		Error_Handler();

	/* Open file to write */
	if (f_open(&fil, "logTXT.txt", FA_OPEN_APPEND | FA_READ | FA_WRITE)
			!= FR_OK)
		Error_Handler();

	/* Check freeSpace space */
	if (f_getfree("", &fre_clust, &pfs) != FR_OK)
		Error_Handler();

	totalSpace = (uint32_t) ((pfs->n_fatent - 2) * pfs->csize * 0.5);
	freeSpace = (uint32_t) (fre_clust * pfs->csize * 0.5);

	/* free space is less than 1kb */
	if (freeSpace < 1)
		Error_Handler();

	/* Writing text */

	sprintf(strLogBuffer, "Play %s file : %04d-%02d-%02d %02d:%02d:%02d \r\n",
			filename, B2D(rtc_date.Year) + 2000, B2D(rtc_date.Month),
			B2D(rtc_date.Date), B2D(rtc_time.Hours), B2D(rtc_time.Minutes),
			B2D(rtc_time.Seconds));
	f_puts(strLogBuffer, &fil);

	/* Close file */
	if (f_close(&fil) != FR_OK)
		Error_Handler();

	/* Unmount SDCARD */
	if (f_mount(NULL, "", 1) != FR_OK)
		Error_Handler();

	sprintf(audioFileName, "%s%s", "0:/", filename);
	playState = PLAY_READY;
}

void SetPlayState(PlayState playstate) {
	playState = playstate;
}

static void app_wifi_init(tpfAppWifiCb wifi_cb_func) {
	tstrWifiInitParam param;
	uint32 pinmask = (M2M_PERIPH_PULLUP_DIS_HOST_WAKEUP
			| M2M_PERIPH_PULLUP_DIS_SD_CMD_SPI_SCK
			| M2M_PERIPH_PULLUP_DIS_SD_DAT0_SPI_TXD);

	sint8 ret;

	uint8 mac_addr[6];
	uint8 u8IsMacAddrValid;
	uint8 deviceName[] = M2M_DEVICE_NAME;

#ifdef _STATIC_PS_
	nm_bsp_register_wake_isr(wake_cb, PS_SLEEP_TIME_MS);
#endif

	m2m_memset((uint8*) &param, 0, sizeof(param));
	param.pfAppWifiCb = wifi_cb_func;
#ifdef ETH_MODE
	param.strEthInitParam.pfAppEthCb = ethernet_demo_cb;
	param.strEthInitParam.au8ethRcvBuf = gau8ethRcvBuf;
	param.strEthInitParam.u16ethRcvBufSize = sizeof(gau8ethRcvBuf);
#endif
	ret = m2m_ble_wifi_init(&param);

	if (M2M_SUCCESS != ret) {
		M2M_ERR("Driver Init Failed <%d>\n", ret);
		M2M_ERR("Resetting\n");
		// Catastrophe - problem with booting. Nothing but to try and reset
		//system_reset(); //ToDo TF

		while (1) {
		}
	}

	m2m_periph_pullup_ctrl(pinmask, 0);

	m2m_wifi_get_otp_mac_address(mac_addr, &u8IsMacAddrValid);
	if (!u8IsMacAddrValid) {
		uint8 DEFAULT_MAC[] = MAC_ADDRESS;
		M2M_INFO("Default MAC\n");
		m2m_wifi_set_mac_address(DEFAULT_MAC);
	} else {
		M2M_INFO("OTP MAC\n");
	}
	m2m_wifi_get_mac_address(mac_addr);
	M2M_INFO("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac_addr[0],
			mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

	/* Name must be in the format WINC3400_00:00 */
	{
#define HEX2ASCII(x) (((x)>=10)? (((x)-10)+'A') : ((x)+'0'))

		uint16 len;
		len = m2m_strlen(deviceName);
		if (len >= 5) {
			deviceName[len - 1] = HEX2ASCII((mac_addr[5] >> 0) & 0x0f);
			deviceName[len - 2] = HEX2ASCII((mac_addr[5] >> 4) & 0x0f);
			deviceName[len - 4] = HEX2ASCII((mac_addr[4] >> 0) & 0x0f);
			deviceName[len - 5] = HEX2ASCII((mac_addr[4] >> 4) & 0x0f);
		}
	}
	m2m_wifi_set_device_name((uint8*) deviceName,
			(uint8) m2m_strlen((uint8*) deviceName));

#ifdef _DYNAMIC_PS_
	{
		tstrM2mLsnInt strM2mLsnInt;
		M2M_INFO("M2M_PS_DEEP_AUTOMATIC\r\n");
		m2m_wifi_set_sleep_mode(M2M_PS_DEEP_AUTOMATIC, 1);
		strM2mLsnInt.u16LsnInt = M2M_LISTEN_INTERVAL;
		m2m_wifi_set_lsn_int(&strM2mLsnInt);
	}
#elif (defined _STATIC_PS_)
	M2M_INFO("M2M_PS_MANUAL\r\n");
	m2m_wifi_set_sleep_mode(M2M_PS_MANUAL, 1);
#else
	M2M_INFO("M2M_NO_PS\r\n");
	m2m_wifi_set_sleep_mode(M2M_NO_PS, 1);
#endif
}

/**
 * \brief Callback to get the Wi-Fi status update.
 *
 * \param[in] u8MsgType type of Wi-Fi notification. Possible types are:
 *  - [M2M_WIFI_RESP_CON_STATE_CHANGED](@ref M2M_WIFI_RESP_CON_STATE_CHANGED)
 *  - [M2M_WIFI_RESP_SCAN_DONE](@ref M2M_WIFI_RESP_SCAN_DONE)
 *  - [M2M_WIFI_RESP_SCAN_RESULT](@ref M2M_WIFI_RESP_SCAN_RESULT)
 *  - [M2M_WIFI_REQ_DHCP_CONF](@ref M2M_WIFI_REQ_DHCP_CONF)
 * \param[in] pvMsg A pointer to a buffer containing the notification parameters
 * (if any). It should be casted to the correct data type corresponding to the
 * notification type.
 */
static void app_wifi_handle_event(uint8 u8MsgType, void *pvMsg) {
	if (u8MsgType == M2M_WIFI_RESP_CON_STATE_CHANGED) {
		tstrM2mWifiStateChanged *pstrWifiState =
				(tstrM2mWifiStateChanged*) pvMsg;

		M2M_INFO("Wifi State :: %s ::\r\n",
				pstrWifiState->u8CurrState ? "CONNECTED" : "DISCONNECTED");

		if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED) {
			gu8WiFiConnectionState = M2M_WIFI_DISCONNECTED;
			gbConnectedWifi = false;
			gbHostIpByName = false;
			pwm_update_duty_cycle(255,255,255,0,0);
			HAL_GPIO_WritePin(FAN_ON_GPIO_Port, FAN_ON_Pin, GPIO_PIN_RESET);
		} else {
            pwm_update_duty_cycle(0,0,0,0,0);
    		osDelay(500);
            pwm_update_duty_cycle(255,255,255,0,0);
		}
	} else if (u8MsgType == M2M_WIFI_REQ_DHCP_CONF) {
		tstrM2MIPConfig *pstrM2MIpConfig = (tstrM2MIPConfig*) pvMsg;
		uint8 *pu8IPAddress = (uint8*) &pstrM2MIpConfig->u32StaticIP;

		M2M_INFO("DHCP IP Address :: %u.%u.%u.%u ::\n", pu8IPAddress[0],
				pu8IPAddress[1], pu8IPAddress[2], pu8IPAddress[3]);

		gu8WiFiConnectionState = M2M_WIFI_CONNECTED;
		gbConnectedWifi = true;

		/* Obtain the IP Address by network name */
		gethostbyname((uint8_t*) MAIN_GMAIL_HOST_NAME);

	} else if (u8MsgType == M2M_WIFI_RESP_SCAN_DONE) {
		tstrM2mScanDone *pstrInfo = (tstrM2mScanDone*) pvMsg;

		if (gu8WiFiConnectionState != M2M_WIFI_CONNECTED) {
			gu8ScanIndex = 0;

			if (pstrInfo->u8NumofCh >= 1) {
				m2m_wifi_req_scan_result(gu8ScanIndex);
				gu8ScanIndex++;
			}
		}
	} else if (u8MsgType == M2M_WIFI_RESP_SCAN_RESULT) {
		uint8 u8NumFoundAPs = m2m_wifi_get_num_ap_found();

		if (gu8WiFiConnectionState != M2M_WIFI_CONNECTED) {
			tstrM2mWifiscanResult *pstrScanResult =
					(tstrM2mWifiscanResult*) pvMsg;

			ble_prov_scan_result(pstrScanResult, u8NumFoundAPs - gu8ScanIndex);
			if (gu8ScanIndex < u8NumFoundAPs) {
				m2m_wifi_req_scan_result(gu8ScanIndex);
				gu8ScanIndex++;
			}
		}
	} else if (u8MsgType == M2M_WIFI_RESP_CURRENT_RSSI) {
		sint8 *rssi = (sint8*) pvMsg;
//		M2M_INFO("(%lu) rssi %d\n",NM_BSP_TIME_MSEC,*rssi);
		M2M_INFO("rssi %d\n", *rssi);
	} else if (u8MsgType == M2M_WIFI_RESP_SET_GAIN_TABLE) {
		tstrM2MGainTableRsp *pstrRsp = (tstrM2MGainTableRsp*) pvMsg;
		M2M_ERR("Gain Table Load Fail %d\n", pstrRsp->s8ErrorCode);
	} else if (u8MsgType == M2M_WIFI_RESP_GET_SYS_TIME) {
		tstrSystemTime *strSysTime_now = (tstrSystemTime*) pvMsg;

		/* Print the hour, minute and second.
		 * GMT is the time at Greenwich Meridian.
		 */
		printf("socket_cb: The GMT time is %u:%02u:%02u\r\n",
				strSysTime_now->u8Hour, /* hour (86400 equals secs per day) */
				strSysTime_now->u8Minute, /* minute (3600 equals secs per minute) */
				strSysTime_now->u8Second); /* second */

		RTC_TimeTypeDef sTime = { 0 };
		RTC_DateTypeDef sDate = { 0 };

		sTime.Hours = D2B(strSysTime_now->u8Hour);
		sTime.Minutes = D2B(strSysTime_now->u8Minute);
		sTime.Seconds = D2B(strSysTime_now->u8Second);
		sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
		sTime.StoreOperation = RTC_STOREOPERATION_RESET;
		if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK) {
			Error_Handler();
		}
		sDate.WeekDay = RTC_WEEKDAY_THURSDAY;
		sDate.Month = D2B(strSysTime_now->u8Month);
		sDate.Date = D2B(strSysTime_now->u8Day);
		sDate.Year = D2B(strSysTime_now->u16Year - 2000);

		if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK) {
			Error_Handler();
		}
	}
}

// This is an example of using onchip_profile, ble_prov API.
static void app_ble_wifi_provisioning(void) {
	uint8_t app_state = APP_STATE_IDLE;
	uint8_t wifi_con_state = M2M_WIFI_UNDEF;
	/*uint8_t btn_event;*/
	at_ble_events_t ble_event;

	// Initialize BLE stack on 3400.
	m2m_ble_init();
	m2m_wifi_req_unrestrict_ble();
	ble_prov_init((uint8_t*) "WiFi Prov1", AT_BLE_AUTH_NO_MITM_NO_BOND);

	while (1) {
		if (m2m_ble_event_get(&ble_event, &gu8BleParam) == AT_BLE_SUCCESS) {
			ble_prov_process_event(ble_event, &gu8BleParam);
		}

		if (wifi_con_state != gu8WiFiConnectionState) {
			if (gu8WiFiConnectionState != M2M_WIFI_UNDEF) {
				ble_prov_wifi_con_update(
						gu8WiFiConnectionState ?
								WIFIPROV_CON_STATE_CONNECTED :
								WIFIPROV_CON_STATE_DISCONNECTED);
			}
			wifi_con_state = gu8WiFiConnectionState;
		}

		switch (app_state) {
		case APP_STATE_IDLE: {
			if (wifi_con_state == M2M_WIFI_CONNECTED) {
				m2m_wifi_disconnect();
				app_state = APP_STATE_WAITING_FOR_WIFI_DISCONNECTION;
			} else {
				gu8WiFiConnectionState = M2M_WIFI_UNDEF;
				if (ble_prov_start() == AT_BLE_SUCCESS) {
					app_state = APP_STATE_PROVISIONING;
				}
			}
			break;
		}
		case APP_STATE_WAITING_FOR_WIFI_DISCONNECTION: {
			if (wifi_con_state == M2M_WIFI_DISCONNECTED) {
				if (ble_prov_start() == AT_BLE_SUCCESS) {
					app_state = APP_STATE_PROVISIONING;
					wifi_con_state = M2M_WIFI_UNDEF;
					gu8WiFiConnectionState = M2M_WIFI_UNDEF;
				}
			}
			break;
		}
		case APP_STATE_PROVISIONING: {
			// BLE requests are handled in ble_prv framework.
			// The application layer handles scan_result (handle ble_prov_scan_result)
			// Here we check if process has been completed.
			switch (ble_prov_get_provision_state()) {
			case BLE_PROV_STATE_SUCCESS: {
				credentials mycred;
				printf("Provisioning data received\r\n");
				if (ble_prov_get_credentials(&mycred) == CREDENTIALS_VALID) {
					printf("Connecting to %s ", (char*) mycred.ssid);
					nm_bsp_sleep(1000);
					m2m_wifi_connect((char*) mycred.ssid, mycred.ssid_length,
							mycred.sec_type, mycred.passphrase,
							M2M_WIFI_CH_ALL);
					ble_prov_wifi_con_update(WIFIPROV_CON_STATE_CONNECTING);
					app_state = APP_STATE_WAITING_FOR_WIFI_CONNECTION;
				} else {
					ble_prov_stop();
					app_state = APP_STATE_IDLE;
				}
				break;
			}
			case BLE_PROV_STATE_FAILED: {
				ble_prov_stop();
				app_state = APP_STATE_IDLE;
				break;
			}
			}
			break;
		}
		case APP_STATE_WAITING_FOR_WIFI_CONNECTION: {
			if (wifi_con_state == M2M_WIFI_CONNECTED) {
				printf("Provisioning Completed\r\n");
				ble_prov_wifi_con_update(WIFIPROV_CON_STATE_CONNECTED);
				app_state = APP_STATE_COMPLETED;
				wifi_con_state = M2M_WIFI_UNDEF;
				nm_bsp_sleep(1000);
				ble_prov_stop();
				nm_bsp_sleep(1000);
				//Re-init the BLE to put it into a default, known state.
				//m2m_ble_init();
				//Now we have finished provisioning, we can place BLE in restricted mode to save power
				//m2m_wifi_req_restrict_ble();
			}
			if (wifi_con_state == M2M_WIFI_DISCONNECTED) {
//					nm_bsp_stop_timer();
				printf("WiFi Connect failed.\r\n");
				ble_prov_stop();
				ble_prov_wifi_con_update(WIFIPROV_CON_STATE_DISCONNECTED);
				app_state = APP_STATE_IDLE;
				wifi_con_state = M2M_WIFI_UNDEF;
				nm_bsp_sleep(1000);
			}
			break;
		}
		case APP_STATE_COMPLETED: {
			break;
		}
		}
	}
}

static void app_main(void) {
	/* Initialize WiFi interface first.
	 3400 WiFi HIF is used to convey BLE API primitives.*/
	app_wifi_init(app_wifi_handle_event);
	//nm_bsp_btn_init(app_button_press_callback);

	/* Demo application using profile.*/
	app_ble_wifi_provisioning();
}

static void udpAppInit(void) {
	msTicks = 0;
	packetCnt = 0;
	printf("t = %ld ms -> starting run %d\r\n", runCnt, iterationCnt + 1);
	appPayload.wifiStation = APP_WIFI_STATION;
	appPayload.seqNumber = 0;
	// fill the application buffer
	for (uint16_t i = 0; i < APP_BUFFER_SIZE; i++) {
		appPayload.data[i] = 0x30;					//i & 0xFF ;
	}
}

// SMTP Funtions

/**
 * \brief Creates and connects to an unsecure socket to be used for SMTP.
 *
 * \param[in] None.
 *
 * \return SOCK_ERR_NO_ERROR if success, -1 if socket create error, SOCK_ERR_INVALID if socket connect error.
 */
static int8_t smtpConnect(void) {
	struct sockaddr_in addr_in;

	addr_in.sin_family = AF_INET;
	addr_in.sin_port = _htons(MAIN_GMAIL_HOST_PORT);
	addr_in.sin_addr.s_addr = gu32HostIp;

	/* Create secure socket */
	if (tcp_client_socket < 0) {
		tcp_client_socket = socket(AF_INET, SOCK_STREAM, SOCKET_FLAGS_SSL);
	}

	/* Check if socket was created successfully */
	if (tcp_client_socket == -1) {
		printf("socket error.\r\n");
		close(tcp_client_socket);
		return -1;
	}

	/* If success, connect to socket */
	if (connect(tcp_client_socket, (struct sockaddr*) &addr_in,
			sizeof(struct sockaddr_in)) != SOCK_ERR_NO_ERROR) {
		printf("connect error.\r\n");
		return SOCK_ERR_INVALID;
	}

	/* Success */
	return SOCK_ERR_NO_ERROR;
}

/**
 * \brief Generates Base64 Key needed for authentication.
 *
 * \param[in] input is the string to be converted to base64.
 * \param[in] basekey1 is the base64 converted output.
 *
 * \return None.
 */
static void generateBase64Key(char *input, char *basekey) {
	/* In case the input string needs to be modified before conversion, define */
	/*  new string to pass-through Use InputStr and *pIn */
	int16_t InputLen = strlen(input);
	char InputStr[128];
	char *pIn = (char*) InputStr;

	/* Generate Base64 string, right now is only the function input parameter */
	memcpy(pIn, input, InputLen);
	pIn += InputLen;

	/* to64frombits function */
	ConvertToBase64(basekey, (void*) InputStr, InputLen);
}

/**
 * \brief Sends an SMTP command and provides the server response.
 *
 * \param[in] socket is the socket descriptor to be used for sending.
 * \param[in] cmd is the string of the command.
 * \param[in] cmdpara is the command parameter.
 * \param[in] respBuf is a pointer to the SMTP response from the server.
 *
 * \return None.
 */
static void smtpSendRecv(long socket, char *cmd, char *cmdparam, char *respBuf) {
	uint16_t sendLen = 0;
	memset(gcSendRecvBuffer, 0, sizeof(gcSendRecvBuffer));

	if (cmd != NULL) {
		sendLen = strlen(cmd);
		memcpy(gcSendRecvBuffer, cmd, strlen(cmd));
	}

	if (cmdparam != NULL) {
		memcpy(&gcSendRecvBuffer[sendLen], cmdparam, strlen(cmdparam));
		sendLen += strlen(cmdparam);
	}

	memcpy(&gcSendRecvBuffer[sendLen], cSmtpCrlf, strlen(cSmtpCrlf));
	sendLen += strlen(cSmtpCrlf);
	send(socket, gcSendRecvBuffer, sendLen, 0);

	if (respBuf != NULL) {
		memset(respBuf, 0, MAIN_SMTP_BUF_LEN);
		recv(socket, respBuf, MAIN_SMTP_BUF_LEN, 0);
	}
}

/**
 * \brief SMTP state handler.
 *
 * \param[in] None.
 *
 * \return MAIN_EMAIL_ERROR_NONE if success, MAIN_EMAIL_ERROR_FAILED if handler error.
 */
static int8_t smtpStateHandler(void) {
	/* Check for acknowledge from SMTP server */
	switch (gu8SmtpStatus) {
	/* Send Introductory "HELO" to SMTP server */
	case SMTP_HELO:
		smtpSendRecv(tcp_client_socket, (char*) "HELO localhost", NULL,
				gcHandlerBuffer);
		break;

		/* Send request to server for authentication */
	case SMTP_AUTH:
		smtpSendRecv(tcp_client_socket, (char*) "AUTH LOGIN", NULL,
				gcHandlerBuffer);
		break;

		/* Handle Authentication with server username */
	case SMTP_AUTH_USERNAME:
		smtpSendRecv(tcp_client_socket, gcUserBasekey, NULL, gcHandlerBuffer);
		break;

		/* Handle Authentication with server password */
	case SMTP_AUTH_PASSWORD:
		smtpSendRecv(tcp_client_socket, gcPasswordBasekey, NULL,
				gcHandlerBuffer);
		break;

		/* Send source email to the SMTP server */
	case SMTP_FROM:
		smtpSendRecv(tcp_client_socket, (char*) cSmtpMailFrom,
				(char*) MAIN_SENDER_RFC, gcHandlerBuffer);
		break;

		/* Send the destination email to the SMTP server */
	case SMTP_RCPT:
		smtpSendRecv(tcp_client_socket, (char*) cSmtpRcpt,
				(char*) MAIN_RECIPIENT_RFC, gcHandlerBuffer);
		break;

		/* Send the "DATA" message to the server */
	case SMTP_DATA:
		smtpSendRecv(tcp_client_socket, (char*) cSmtpData, NULL,
				gcHandlerBuffer);
		break;

		/* Send actual Message, preceded by From, To and Subject */
	case SMTP_MESSAGE_SUBJECT:
		/* Start with E-Mail's "Subject:" field */
		smtpSendRecv(tcp_client_socket, (char*) cSmtpSubject,
				(char*) MAIN_EMAIL_SUBJECT, NULL);
		break;

	case SMTP_MESSAGE_TO:
		/* Add E-mail's "To:" field */
		printf("Recipient email address is %s\r\n", (char*) MAIN_TO_ADDRESS);
		smtpSendRecv(tcp_client_socket, (char*) cSmtpTo,
				(char*) MAIN_TO_ADDRESS, NULL);
		break;

	case SMTP_MESSAGE_FROM:
		/* Add E-mail's "From:" field */
		smtpSendRecv(tcp_client_socket, (char*) cSmtpFrom,
				(char*) MAIN_FROM_ADDRESS, NULL);
		break;

	case SMTP_MESSAGE_CRLF:
		/* Send CRLF */
		send(tcp_client_socket, (char*) cSmtpCrlf, strlen(cSmtpCrlf), 0);
		break;

	case SMTP_MESSAGE_BODY:
		/* Send body of message */
		smtpSendRecv(tcp_client_socket, (char*) strSendingEmail, NULL, NULL);
		break;

	case SMTP_MESSAGE_DATAEND:
		/* End Message */
		smtpSendRecv(tcp_client_socket, (char*) cSmtpDataEnd, NULL,
				gcHandlerBuffer);
		break;

	case SMTP_QUIT:
		send(tcp_client_socket, (char*) cSmtpQuit, strlen(cSmtpQuit), 0);
		break;

		/* Error Handling for SMTP */
	case SMTP_ERROR:
		return MAIN_EMAIL_ERROR_FAILED;

	default:
		break;
	}
	return MAIN_EMAIL_ERROR_NONE;
}

/**
 * \brief Callback function of IP address.
 *
 * \param[in] hostName Domain name.
 * \param[in] hostIp Server IP.
 *
 * \return None.
 */
static void resolve_cb(uint8_t *hostName, uint32_t hostIp) {
	gu32HostIp = hostIp;
	gbHostIpByName = true;
	printf("Host IP is %d.%d.%d.%d\r\n", (int) IPV4_BYTE(hostIp, 0),
			(int) IPV4_BYTE(hostIp, 1), (int) IPV4_BYTE(hostIp, 2),
			(int) IPV4_BYTE(hostIp, 3));
	printf("Host Name is %s\r\n", hostName);
}

/**
 * \brief Callback function of TCP client socket.
 *
 * \param[in] sock socket handler.
 * \param[in] u8Msg Type of Socket notification
 * \param[in] pvMsg A structure contains notification informations.
 *
 * \return None.
 */
static void socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg) {
	/* Check for socket event on TCP socket. */
	if (sock == tcp_client_socket) {
		switch (u8Msg) {
		case SOCKET_MSG_CONNECT: {
			tstrSocketConnectMsg *pstrConnect = (tstrSocketConnectMsg*) pvMsg;
			if (pstrConnect && pstrConnect->s8Error >= SOCK_ERR_NO_ERROR) {
				memset(gcHandlerBuffer, 0, MAIN_SMTP_BUF_LEN);
				recv(tcp_client_socket, gcHandlerBuffer,
						sizeof(gcHandlerBuffer), 0);
			} else {
				printf("SOCKET_MSG_CONNECT : connect error!\r\n");
				gu8SocketStatus = SocketError;
			}
		}
			break;

		case SOCKET_MSG_SEND: {
			switch (gu8SmtpStatus) {
			case SMTP_MESSAGE_SUBJECT:
				gu8SocketStatus = SocketConnect;
				gu8SmtpStatus = SMTP_MESSAGE_TO;
				break;

			case SMTP_MESSAGE_TO:
				gu8SocketStatus = SocketConnect;
				gu8SmtpStatus = SMTP_MESSAGE_FROM;
				break;

			case SMTP_MESSAGE_FROM:
				gu8SocketStatus = SocketConnect;
				gu8SmtpStatus = SMTP_MESSAGE_CRLF;
				break;

			case SMTP_MESSAGE_CRLF:
				gu8SocketStatus = SocketConnect;
				gu8SmtpStatus = SMTP_MESSAGE_BODY;
				break;

			case SMTP_MESSAGE_BODY:
				gu8SocketStatus = SocketConnect;
				gu8SmtpStatus = SMTP_MESSAGE_DATAEND;
				break;

			case SMTP_QUIT:
				gu8SocketStatus = SocketComplete;
				gu8SmtpStatus = SMTP_INIT;
				break;

			default:
				break;
			}
		}
			break;

		case SOCKET_MSG_RECV: {
			tstrSocketRecvMsg *pstrRecv = (tstrSocketRecvMsg*) pvMsg;

			if (gu8SocketStatus == SocketWaiting) {
				gu8SocketStatus = SocketConnect;
				switch (gu8SmtpStatus) {
				case SMTP_INIT:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 220 'OK' from server, set state to HELO */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeReady[0]
								&& pstrRecv->pu8Buffer[1] == cSmtpCodeReady[1]
								&& pstrRecv->pu8Buffer[2]
										== cSmtpCodeReady[2]) {
							gu8SmtpStatus = SMTP_HELO;
						} else {
							printf("No response from server.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_INIT;
						}
					} else {
						printf("SMTP_INIT : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_INIT;
					}

					break;

				case SMTP_HELO:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 220, set state to HELO */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeOkReply[0]
								&& pstrRecv->pu8Buffer[1] == cSmtpCodeOkReply[1]
								&& pstrRecv->pu8Buffer[2]
										== cSmtpCodeOkReply[2]) {
							gu8SmtpStatus = SMTP_AUTH;
						} else {
							printf("No response for HELO.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_HELO;
						}
					} else {
						printf("SMTP_HELO : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_HELO;
					}

					break;

				case SMTP_AUTH:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* Function handles authentication for all services */
						generateBase64Key((char*) MAIN_FROM_ADDRESS,
								gcUserBasekey);

						/* If buffer is 334, give username in base64 */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeAuthReply[0]
								&& pstrRecv->pu8Buffer[1]
										== cSmtpCodeAuthReply[1]
								&& pstrRecv->pu8Buffer[2]
										== cSmtpCodeAuthReply[2]) {
							gu8SmtpStatus = SMTP_AUTH_USERNAME;
						} else {
							printf("No response for authentication.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_AUTH;
						}
					} else {
						printf("SMTP_AUTH : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_AUTH;
					}

					break;

				case SMTP_AUTH_USERNAME:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						generateBase64Key((char*) MAIN_FROM_PASSWORD,
								gcPasswordBasekey);

						/* If buffer is 334, give password in base64 */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeAuthReply[0]
								&& pstrRecv->pu8Buffer[1]
										== cSmtpCodeAuthReply[1]
								&& pstrRecv->pu8Buffer[2]
										== cSmtpCodeAuthReply[2]) {
							gu8SmtpStatus = SMTP_AUTH_PASSWORD;
						} else {
							printf(
									"No response for username authentication.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_AUTH_USERNAME;
						}
					} else {
						printf("SMTP_AUTH_USERNAME : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_AUTH_USERNAME;
					}

					break;

				case SMTP_AUTH_PASSWORD:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeAuthSuccess[0]
								&& pstrRecv->pu8Buffer[1]
										== cSmtpCodeAuthSuccess[1]
								&& pstrRecv->pu8Buffer[2]
										== cSmtpCodeAuthSuccess[2]) {
							/* Authentication was successful, set state to FROM */
							gu8SmtpStatus = SMTP_FROM;
						} else {
							printf(
									"No response for password authentication.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_AUTH_PASSWORD;
						}
					} else {
						printf("SMTP_AUTH_PASSWORD : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_AUTH_PASSWORD;
					}

					break;

				case SMTP_FROM:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 250, set state to RCPT */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeOkReply[0]
								&& pstrRecv->pu8Buffer[1] == cSmtpCodeOkReply[1]
								&& pstrRecv->pu8Buffer[2]
										== cSmtpCodeOkReply[2]) {
							gu8SmtpStatus = SMTP_RCPT;
						} else {
							printf("No response for sender transmission.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_FROM;
						}
					} else {
						printf("SMTP_FROM : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_FROM;
					}

					break;

				case SMTP_RCPT:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 250, set state to DATA */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeOkReply[0]
								&& pstrRecv->pu8Buffer[1] == cSmtpCodeOkReply[1]
								&& pstrRecv->pu8Buffer[2]
										== cSmtpCodeOkReply[2]) {
							gu8SmtpStatus = SMTP_DATA;
						} else {
							printf(
									"No response for recipient transmission.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_RCPT;
						}
					} else {
						printf("SMTP_RCPT : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_RCPT;
					}

					break;

				case SMTP_DATA:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 250, set state to DATA */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeIntermedReply[0]
								&& pstrRecv->pu8Buffer[1]
										== cSmtpCodeIntermedReply[1]
								&& pstrRecv->pu8Buffer[2]
										== cSmtpCodeIntermedReply[2]) {
							gu8SmtpStatus = SMTP_MESSAGE_SUBJECT;
						} else {
							printf("No response for data transmission.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_DATA;
						}
					} else {
						printf("SMTP_DATA : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_DATA;
					}

					break;

				case SMTP_MESSAGE_DATAEND:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 250, set state to DATA */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeOkReply[0]
								&& pstrRecv->pu8Buffer[1] == cSmtpCodeOkReply[1]
								&& pstrRecv->pu8Buffer[2]
										== cSmtpCodeOkReply[2]) {
							gu8SmtpStatus = SMTP_QUIT;
						} else {
							printf("No response for dataend transmission.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_MESSAGE;
						}
					} else {
						printf("SMTP_MESSAGE_DATAEND : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_MESSAGE;
					}

					break;

				default:
					break;
				}
			}
		}
			break;

		default:
			break;
		}
	}
}

/**
 * \brief Close socket function.
 * \return None.
 */
static void close_socket(void) {
	close(tcp_client_socket);
	tcp_client_socket = -1;
}

/**
 * \brief Retry SMTP server function.
 * \return None.
 */
static void retry_smtp_server(void) {
	close_socket();
	gu8SocketStatus = SocketInit;
	gu8SmtpStatus = SMTP_INIT;
	gbHostIpByName = false;
	osDelay(MAIN_WAITING_TIME);
	m2m_wifi_disconnect();
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */
	uint32_t tmpRGB = 0;
	uint8_t tmpFlag = 0;
	LedStrip ledstrip;
	ledstrip.red = 0;
	ledstrip.green = 0;
	ledstrip.blue = 0;
	ledstrip.w2400 = 0;
	ledstrip.w6500 = 0;

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_I2C1_Init();
	MX_UART7_Init();
	MX_TIM1_Init();
	MX_TIM3_Init();
	MX_TIM4_Init();
	MX_DMA_Init();
	MX_I2S2_Init();
	MX_I2C2_Init();
	MX_SPI3_Init();
	MX_FATFS_Init();
	MX_SPI1_Init();
	MX_TIM2_Init();
	MX_USART1_UART_Init();
	MX_SPI5_Init();
	MX_USART2_UART_Init();
	MX_USART3_UART_Init();
	MX_RTC_Init();
	/* USER CODE BEGIN 2 */
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
	SetLedStrip(ledstrip);

	HAL_Delay(1000);
	/* Mount SD Card */
	if (f_mount(&fs, "", 0) != FR_OK)
		Error_Handler();

	if (f_open(&fil, "logTXT.txt", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
		Error_Handler();

	if (f_close(&fil) != FR_OK)
		Error_Handler();

	/* Unmount SDCARD */
	if (f_mount(NULL, "", 1) != FR_OK)
		Error_Handler();

	HAL_GPIO_WritePin(FAN_ON_GPIO_Port, FAN_ON_Pin, GPIO_PIN_SET); // fan on
	printf("Fan Test Started...will run for duration of POST\r\n");
	HAL_Delay(1000);
	pwm_update_duty_cycle(0,255,255,0,0);  // light blue
	printf("2000ms LED Light Blue Test Started.\r\n");
	HAL_Delay(1000);
	printf("2000ms LED Light Blue Test Completed.\r\n");
	pwm_update_duty_cycle(0,0,255,0,0); // blue
	printf("2000ms LED Blue Test Started.\r\n");
	HAL_Delay(1000);
	printf("2000ms LED Blue Test Completed.\r\n");
	pwm_update_duty_cycle(255,0,255,0,0); // fuschia
	printf("2000ms LED Fuschia Test Started.\r\n");
	HAL_Delay(1000);
	printf("2000ms LED Fuschia Test Completed.\r\n");
	pwm_update_duty_cycle(0,255,0,0,0); // green
	printf("2000ms LED Green Test Started.\r\n");
	HAL_Delay(1000);
	printf("2000ms LED Green Test Completed.\r\n");
	pwm_update_duty_cycle(0,0,0,255,0); // white
	printf("2000ms LED White Test Started.\r\n");
	HAL_Delay(1000);
	printf("2000ms LED White Test Completed.\r\n");
	//pwm_update_duty_cycle(0,0,0,0,255); // warm white
	//printf("2000ms LED Warm White Test Started.\r\n");
	HAL_Delay(1000);
	printf("2000ms LED Warm White Test Completed.\r\n");
	pwm_update_duty_cycle(0,0,0,0,0); // LED off
	HAL_Delay(1000);
	printf("1000ms seat heater relay test started.\r\n");
	//nrf_gpio_pin_set(PLAY_OUT);
	HAL_Delay(1000);
	HAL_GPIO_WritePin(PLAY_OUT_GPIO_Port, PLAY_OUT_Pin, GPIO_PIN_RESET);
	printf("1000ms power relay test completed.\r\n");
	pwm_update_duty_cycle(255,255,255,255,0); // white
	// fan off
	HAL_GPIO_WritePin(FAN_ON_GPIO_Port, FAN_ON_Pin, GPIO_PIN_RESET); // fan on

	/* USER CODE END 2 */

	/* Init scheduler */
	osKernelInitialize();

	/* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
	/* USER CODE END RTOS_MUTEX */

	/* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
	/* USER CODE END RTOS_SEMAPHORES */

	/* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
	/* USER CODE END RTOS_TIMERS */

	/* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
	/* USER CODE END RTOS_QUEUES */

	/* Create the thread(s) */
	/* creation of defaultTask */
	defaultTaskHandle = osThreadNew(StartDefaultTask, NULL,
			&defaultTask_attributes);

	/* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */
	commTaskHandle = osThreadNew(StartCommTask, NULL, &commTask_attributes);
	wifiProvTaskHandle = osThreadNew(StartWiFiProvTask, NULL,
			&wifiProvTask_attributes);
	/* USER CODE END RTOS_THREADS */

	/* USER CODE BEGIN RTOS_EVENTS */
	/* add events, ... */
	/* USER CODE END RTOS_EVENTS */

	/* Start scheduler */
	osKernelStart();

	/* We should never get here as control is now taken by the scheduler */
	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {

		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}

	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE
			| RCC_OSCILLATORTYPE_LSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 180;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Activate the Over-Drive mode
	 */
	if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}

	/** Configure Analogue filter
	 */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE)
			!= HAL_OK) {
		Error_Handler();
	}

	/** Configure Digital filter
	 */
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief I2C2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C2_Init(void) {

	/* USER CODE BEGIN I2C2_Init 0 */

	/* USER CODE END I2C2_Init 0 */

	/* USER CODE BEGIN I2C2_Init 1 */

	/* USER CODE END I2C2_Init 1 */
	hi2c2.Instance = I2C2;
	hi2c2.Init.ClockSpeed = 100000;
	hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c2.Init.OwnAddress1 = 0;
	hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c2.Init.OwnAddress2 = 0;
	hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
		Error_Handler();
	}

	/** Configure Analogue filter
	 */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE)
			!= HAL_OK) {
		Error_Handler();
	}

	/** Configure Digital filter
	 */
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C2_Init 2 */

	/* USER CODE END I2C2_Init 2 */

}

/**
 * @brief I2S2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2S2_Init(void) {

	/* USER CODE BEGIN I2S2_Init 0 */

	/* USER CODE END I2S2_Init 0 */

	/* USER CODE BEGIN I2S2_Init 1 */

	/* USER CODE END I2S2_Init 1 */
	hi2s2.Instance = SPI2;
	hi2s2.Init.Mode = I2S_MODE_MASTER_TX;
	hi2s2.Init.Standard = I2S_STANDARD_PHILIPS;
	hi2s2.Init.DataFormat = I2S_DATAFORMAT_16B;
	hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
	hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_44K;
	hi2s2.Init.CPOL = I2S_CPOL_LOW;
	hi2s2.Init.ClockSource = I2S_CLOCK_PLL;
	hi2s2.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_ENABLE;
	if (HAL_I2S_Init(&hi2s2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2S2_Init 2 */

	/* USER CODE END I2S2_Init 2 */

}

/**
 * @brief RTC Initialization Function
 * @param None
 * @retval None
 */
static void MX_RTC_Init(void) {

	/* USER CODE BEGIN RTC_Init 0 */

	/* USER CODE END RTC_Init 0 */

	RTC_TimeTypeDef sTime = { 0 };
	RTC_DateTypeDef sDate = { 0 };
	RTC_AlarmTypeDef sAlarm = { 0 };

	/* USER CODE BEGIN RTC_Init 1 */

	/* USER CODE END RTC_Init 1 */

	/** Initialize RTC Only
	 */
	hrtc.Instance = RTC;
	hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
	hrtc.Init.AsynchPrediv = 127;
	hrtc.Init.SynchPrediv = 255;
	hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
	hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
	hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
	if (HAL_RTC_Init(&hrtc) != HAL_OK) {
		Error_Handler();
	}

	/* USER CODE BEGIN Check_RTC_BKUP */

	/* USER CODE END Check_RTC_BKUP */

	/** Initialize RTC and set the Time and Date
	 */
	sTime.Hours = 0x0;
	sTime.Minutes = 0x0;
	sTime.Seconds = 0x0;
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_RESET;
	if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK) {
		Error_Handler();
	}
	sDate.WeekDay = RTC_WEEKDAY_THURSDAY;
	sDate.Month = RTC_MONTH_JUNE;
	sDate.Date = 0x23;
	sDate.Year = 0x22;

	if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK) {
		Error_Handler();
	}

	/** Enable the Alarm A
	 */
	sAlarm.AlarmTime.Hours = 0x0;
	sAlarm.AlarmTime.Minutes = 0x0;
	sAlarm.AlarmTime.Seconds = 0x0;
	sAlarm.AlarmTime.SubSeconds = 0x0;
	sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
	sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY | RTC_ALARMMASK_HOURS
			| RTC_ALARMMASK_MINUTES;
	sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
	sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_WEEKDAY;
	sAlarm.AlarmDateWeekDay = RTC_WEEKDAY_TUESDAY;
	sAlarm.Alarm = RTC_ALARM_A;
	if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN RTC_Init 2 */

	/* USER CODE END RTC_Init 2 */

}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

	/* USER CODE BEGIN SPI1_Init 0 */

	/* USER CODE END SPI1_Init 0 */

	/* USER CODE BEGIN SPI1_Init 1 */

	/* USER CODE END SPI1_Init 1 */
	/* SPI1 parameter configuration*/
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI1_Init 2 */

	/* USER CODE END SPI1_Init 2 */

}

/**
 * @brief SPI3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI3_Init(void) {

	/* USER CODE BEGIN SPI3_Init 0 */

	/* USER CODE END SPI3_Init 0 */

	/* USER CODE BEGIN SPI3_Init 1 */

	/* USER CODE END SPI3_Init 1 */
	/* SPI3 parameter configuration*/
	hspi3.Instance = SPI3;
	hspi3.Init.Mode = SPI_MODE_MASTER;
	hspi3.Init.Direction = SPI_DIRECTION_2LINES;
	hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi3.Init.NSS = SPI_NSS_SOFT;
	hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi3.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI3_Init 2 */

	/* USER CODE END SPI3_Init 2 */

}

/**
 * @brief SPI5 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI5_Init(void) {

	/* USER CODE BEGIN SPI5_Init 0 */

	/* USER CODE END SPI5_Init 0 */

	/* USER CODE BEGIN SPI5_Init 1 */

	/* USER CODE END SPI5_Init 1 */
	/* SPI5 parameter configuration*/
	hspi5.Instance = SPI5;
	hspi5.Init.Mode = SPI_MODE_MASTER;
	hspi5.Init.Direction = SPI_DIRECTION_2LINES;
	hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi5.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi5.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi5.Init.NSS = SPI_NSS_SOFT;
	hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
	hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi5.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi5) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI5_Init 2 */

	/* USER CODE END SPI5_Init 2 */

}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void) {

	/* USER CODE BEGIN TIM1_Init 0 */

	/* USER CODE END TIM1_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = { 0 };

	/* USER CODE BEGIN TIM1_Init 1 */

	/* USER CODE END TIM1_Init 1 */
	htim1.Instance = TIM1;
	htim1.Init.Prescaler = 89;
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = 255;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_SET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
	if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM1_Init 2 */

	/* USER CODE END TIM1_Init 2 */
	HAL_TIM_MspPostInit(&htim1);

}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 89;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 255;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */
	HAL_TIM_MspPostInit(&htim2);

}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void) {

	/* USER CODE BEGIN TIM3_Init 0 */

	/* USER CODE END TIM3_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM3_Init 1 */

	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 89;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 255;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM3_Init 2 */

	/* USER CODE END TIM3_Init 2 */
	HAL_TIM_MspPostInit(&htim3);

}

/**
 * @brief TIM4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM4_Init(void) {

	/* USER CODE BEGIN TIM4_Init 0 */

	/* USER CODE END TIM4_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM4_Init 1 */

	/* USER CODE END TIM4_Init 1 */
	htim4.Instance = TIM4;
	htim4.Init.Prescaler = 89;
	htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim4.Init.Period = 255;
	htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_PWM_Init(&htim4) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_3)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM4_Init 2 */

	/* USER CODE END TIM4_Init 2 */
	HAL_TIM_MspPostInit(&htim4);

}

/**
 * @brief UART7 Initialization Function
 * @param None
 * @retval None
 */
static void MX_UART7_Init(void) {

	/* USER CODE BEGIN UART7_Init 0 */

	/* USER CODE END UART7_Init 0 */

	/* USER CODE BEGIN UART7_Init 1 */

	/* USER CODE END UART7_Init 1 */
	huart7.Instance = UART7;
	huart7.Init.BaudRate = 115200;
	huart7.Init.WordLength = UART_WORDLENGTH_8B;
	huart7.Init.StopBits = UART_STOPBITS_1;
	huart7.Init.Parity = UART_PARITY_NONE;
	huart7.Init.Mode = UART_MODE_TX_RX;
	huart7.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart7.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart7) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN UART7_Init 2 */

	/* USER CODE END UART7_Init 2 */

}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 9600;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 460800;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */

}

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART3_UART_Init(void) {

	/* USER CODE BEGIN USART3_Init 0 */

	/* USER CODE END USART3_Init 0 */

	/* USER CODE BEGIN USART3_Init 1 */

	/* USER CODE END USART3_Init 1 */
	huart3.Instance = USART3;
	huart3.Init.BaudRate = 460800;
	huart3.Init.WordLength = UART_WORDLENGTH_8B;
	huart3.Init.StopBits = UART_STOPBITS_1;
	huart3.Init.Parity = UART_PARITY_NONE;
	huart3.Init.Mode = UART_MODE_TX_RX;
	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART3_Init 2 */

	/* USER CODE END USART3_Init 2 */

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();
	__HAL_RCC_DMA2_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Stream3_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 14, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
	/* DMA1_Stream4_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 13, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
	/* DMA2_Stream4_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA2_Stream4_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream4_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(BT_WIFI_SPI_CS_GPIO_Port, BT_WIFI_SPI_CS_Pin,
			GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(BT_WIFI_CHIP_EN_GPIO_Port, BT_WIFI_CHIP_EN_Pin,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, SD_SPI3_CS_Pin | FAN_ON_Pin | PLAY_OUT_Pin,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(BT_WIFI_RESETN_GPIO_Port, BT_WIFI_RESETN_Pin,
			GPIO_PIN_RESET);

	/*Configure GPIO pin : BT_WIFI_IRQN_Pin */
	GPIO_InitStruct.Pin = BT_WIFI_IRQN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(BT_WIFI_IRQN_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : SD_DETECTS_Pin LID_UPDN_Pin SYS_LOCK_Pin */
	GPIO_InitStruct.Pin = SD_DETECTS_Pin | LID_UPDN_Pin | SYS_LOCK_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/*Configure GPIO pin : BT_WIFI_SPI_CS_Pin */
	GPIO_InitStruct.Pin = BT_WIFI_SPI_CS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(BT_WIFI_SPI_CS_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : BT_WIFI_CHIP_EN_Pin */
	GPIO_InitStruct.Pin = BT_WIFI_CHIP_EN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(BT_WIFI_CHIP_EN_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : SD_SPI3_CS_Pin */
	GPIO_InitStruct.Pin = SD_SPI3_CS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(SD_SPI3_CS_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : BT_WIFI_RESETN_Pin */
	GPIO_InitStruct.Pin = BT_WIFI_RESETN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(BT_WIFI_RESETN_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : FAN_ON_Pin PLAY_OUT_Pin */
	GPIO_InitStruct.Pin = FAN_ON_Pin | PLAY_OUT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI4_IRQn, 10, 0);
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);

}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	LcdUartReceiveCallback(huart);
	BTUartReceiveCallback(huart);
	WiFiUartReceiveCallback(huart);
}

void StartCommTask(void *argument) {
	StartLcdUartReceiveProc();

	StartBTUartReceiveProc();
	StartWiFiUartReceiveProc();
	/* Initialize the BSP. */

	/* Infinite loop */
	for (;;) {
		EsimationLcdUartComm();
		osDelay(5);
	}
}

void SendEmailInit(void) {
	gu8RetryCount = 0;
	gu8SocketStatus = SocketInit;
}

void StartWiFiProvTask(void *argument) {
	uint8_t app_state = APP_STATE_IDLE;
	uint8_t wifi_con_state = M2M_WIFI_UNDEF;
	/*uint8_t btn_event;*/
	at_ble_events_t ble_event;

	int8_t ret;
	struct sockaddr_in addr;

	printf("This is BLE Test version\r\n");
	sprintf(strSendingEmail, "This email is from SOMA Board\r\n");
	nm_bsp_init();
//	app_main();

//	// Init socket address structure
//	addr.sin_family = AF_INET;
//	addr.sin_port = _htons(MAIN_WIFI_M2M_SERVER_PORT);
//	addr.sin_addr.s_addr = _htonl(MAIN_WIFI_M2M_SERVER_IP);

	/* Initialize WiFi interface first.
	 3400 WiFi HIF is used to convey BLE API primitives.*/
	app_wifi_init(app_wifi_handle_event);
	//nm_bsp_btn_init(app_button_press_callback);

	// Init socket Module
	socketInit();
	registerSocketCallback(socket_cb, resolve_cb);

	/* SNTP configuration */
	ret = m2m_wifi_configure_sntp((uint8_t*) MAIN_WORLDWIDE_NTP_POOL_HOSTNAME,
			strlen(MAIN_WORLDWIDE_NTP_POOL_HOSTNAME), SNTP_ENABLE_DHCP);
	if (M2M_SUCCESS != ret) {
		printf("main: SNTP %s configuration Failure\r\n",
				MAIN_WORLDWIDE_NTP_POOL_HOSTNAME);
	}

	/* Connect to AP. */
	m2m_wifi_connect((char*) MAIN_WLAN_SSID, sizeof(MAIN_WLAN_SSID),
	MAIN_WLAN_AUTH, (char*) MAIN_WLAN_PSK, M2M_WIFI_CH_ALL);
//	// Init UDP app
//	udpAppInit() ;

	// Initialize BLE stack on 3400.
//	m2m_ble_init();
//	m2m_wifi_req_unrestrict_ble();
//	ble_prov_init((uint8_t *)"WiFi Prov1",AT_BLE_AUTH_NO_MITM_NO_BOND);

	/* Infinite loop */
	for (;;) {
//		if (m2m_ble_event_get(&ble_event, &gu8BleParam) == AT_BLE_SUCCESS)
//		{
//			ble_prov_process_event(ble_event, &gu8BleParam);
//		}
//
//		if (wifi_con_state != gu8WiFiConnectionState)
//		{
//			if (gu8WiFiConnectionState != M2M_WIFI_UNDEF)
//			{
//				ble_prov_wifi_con_update(
//					gu8WiFiConnectionState ? WIFIPROV_CON_STATE_CONNECTED:
//											 WIFIPROV_CON_STATE_DISCONNECTED);
//			}
//			wifi_con_state = gu8WiFiConnectionState;
//		}
//
//		switch (app_state)
//		{
//			case APP_STATE_IDLE:
//			{
//				if (wifi_con_state == M2M_WIFI_CONNECTED)
//				{
//					m2m_wifi_disconnect();
//					app_state = APP_STATE_WAITING_FOR_WIFI_DISCONNECTION;
//				}
//				else
//				{
//					gu8WiFiConnectionState = M2M_WIFI_UNDEF;
//					if (ble_prov_start() == AT_BLE_SUCCESS)
//					{
//						app_state = APP_STATE_PROVISIONING;
//					}
//				}
//				break;
//			}
//			case APP_STATE_WAITING_FOR_WIFI_DISCONNECTION:
//			{
//				if (wifi_con_state == M2M_WIFI_DISCONNECTED)
//				{
//					if (ble_prov_start() == AT_BLE_SUCCESS)
//					{
//						app_state = APP_STATE_PROVISIONING;
//						wifi_con_state = M2M_WIFI_UNDEF;
//						gu8WiFiConnectionState = M2M_WIFI_UNDEF;
//					}
//				}
//				break;
//			}
//			case APP_STATE_PROVISIONING:
//			{
//				// BLE requests are handled in ble_prv framework.
//				// The application layer handles scan_result (handle ble_prov_scan_result)
//				// Here we check if process has been completed.
//				switch (ble_prov_get_provision_state())
//				{
//					case BLE_PROV_STATE_SUCCESS:
//					{
//						credentials mycred;
//						printf("Provisioning data received\r\n");
//						if (ble_prov_get_credentials(&mycred) == CREDENTIALS_VALID)
//						{
//							printf("Connecting to %s ",(char *)mycred.ssid);
//							nm_bsp_sleep(1000);
//							m2m_wifi_connect((char *)mycred.ssid, mycred.ssid_length,
//							mycred.sec_type, mycred.passphrase, M2M_WIFI_CH_ALL);
//							ble_prov_wifi_con_update(WIFIPROV_CON_STATE_CONNECTING);
//							app_state = APP_STATE_WAITING_FOR_WIFI_CONNECTION;
//						}
//						else
//						{
//							ble_prov_stop();
//							app_state = APP_STATE_IDLE;
//						}
//						break;
//					}
//					case BLE_PROV_STATE_FAILED:
//					{
//						ble_prov_stop();
//						app_state = APP_STATE_IDLE;
//						break;
//					}
//				}
//				break;
//			}
//			case APP_STATE_WAITING_FOR_WIFI_CONNECTION:
//			{
//				if (wifi_con_state == M2M_WIFI_CONNECTED)
//				{
//					printf("Provisioning Completed\r\n");
//					ble_prov_wifi_con_update(WIFIPROV_CON_STATE_CONNECTED);
//					app_state = APP_STATE_COMPLETED;
//					wifi_con_state = M2M_WIFI_UNDEF;
//					nm_bsp_sleep(1000);
//					ble_prov_stop();
//					nm_bsp_sleep(1000);
//					//Re-init the BLE to put it into a default, known state.
//					//m2m_ble_init();
//					//Now we have finished provisioning, we can place BLE in restricted mode to save power
//					//m2m_wifi_req_restrict_ble();
//				}
//				if (wifi_con_state == M2M_WIFI_DISCONNECTED)
//				{
////					nm_bsp_stop_timer();
//					printf("WiFi Connect failed.\r\n");
//					ble_prov_stop();
//					ble_prov_wifi_con_update(WIFIPROV_CON_STATE_DISCONNECTED);
//					app_state = APP_STATE_IDLE;
//					wifi_con_state = M2M_WIFI_UNDEF;
//					nm_bsp_sleep(1000);
//				}
//				break;
//			}
//			case APP_STATE_COMPLETED:
//			{
////				printf("BLE return to WiFi Provisioning state.\r\n");
////				ble_prov_stop();
////				ble_prov_wifi_con_update(WIFIPROV_CON_STATE_DISCONNECTED);
////				app_state = APP_STATE_IDLE;
////				wifi_con_state = M2M_WIFI_UNDEF;
////				nm_bsp_sleep(1000);
//				break;
//			}
//		}

//		//UDP TEST
//
//		if (gu8WiFiConnectionState == M2M_WIFI_CONNECTED)
//		{
//			while (m2m_wifi_handle_events(NULL) != M2M_SUCCESS)
//			{
//			}
//
//			if (packetCnt == APP_PACKET_COUNT)
//			{
//				// transmitted all the UDP packets
//				printf("Time elapsed for the run (including prints): %ld ms \r\n", msTicks) ;
//				printf("\r\n") ;
//				iterationCnt ++ ;
//				if (iterationCnt == APP_NUMBER_OF_RUN)
//				{
//					printf("******* RESULT ********\r\n") ;
//					printf("Sent %d UDP messages x %d times = %d packets\r\n", APP_PACKET_COUNT, iterationCnt, APP_PACKET_COUNT*iterationCnt) ;
//					printf("Sent %d bytes x %d packets = %d bytes\r\n", sizeof(payloadFormat_t), APP_PACKET_COUNT*iterationCnt, APP_PACKET_COUNT*sizeof(payloadFormat_t)*iterationCnt) ;
//					printf("Retries: %d\r\n", retryCnt) ;
//					printf("Total time (including prints): %ld ms\r\n", runCnt) ;
//					close(tx_socket) ;
//					tx_socket = -1 ;
//					break ;
//				}
//				else
//				{
//					udpAppInit() ;
//				}
//			}
//			m2m_wifi_handle_events(NULL) ;
//
//
//			// Create socket for Tx UDP
//			if (tx_socket < 0)
//			{
//				if ((tx_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
//				{
//					printf("main : failed to create TX UDP client socket error!\r\n");
//					continue;
//				}
//			}
//			// Send data on the UDP socket
//			sprintf(&appPayload.data[0], "UDP TEST!!!");
//
//			ret = sendto(tx_socket, &appPayload, sizeof(payloadFormat_t), 0, (struct sockaddr *)&addr, sizeof(addr)) ;
//			//delay_ms(10) ;	// to avoid the retry; drawback: delay added
//
//			printf("WiFi Station: %d | Sequence Number: %d | Packet Size: %d\r\n", appPayload.wifiStation, appPayload.seqNumber, sizeof(payloadFormat_t)) ;
//			if (ret == M2M_SUCCESS)
//			{
//				printf(">> main: message sent\r\n");
//				packetCnt += 1;
//				appPayload.seqNumber += 1 ;	// increase seq number
//				if (packetCnt == APP_PACKET_COUNT)
//				{
//					printf("UDP Client test Complete!\r\n");
//				}
//			}
//			else
//			{
//				printf(">> Issue: %d - main: failed to send status report error!\r\n", ret);
//				if (ret == SOCK_ERR_BUFFER_FULL )
//				{
//					printf("No buffer space available to be used for the requested socket operation. Retry! \r\n") ;
//					// Try again next time through the main loop
//					retryCnt ++ ;
//				}
//			}
//		}

		// SMTP Test
//		if (gu8WiFiConnectionState == M2M_WIFI_CONNECTED)
		m2m_wifi_handle_events(NULL);

		if (flagOneMinute) {
			if (gbConnectedWifi && gbHostIpByName) {
				if (gu8SocketStatus == SocketInit) {
					if (tcp_client_socket < 0) {
						gu8SocketStatus = SocketWaiting;
						if (smtpConnect() != SOCK_ERR_NO_ERROR) {
							gu8SocketStatus = SocketInit;
						}
					}
				} else if (gu8SocketStatus == SocketConnect) {
					gu8SocketStatus = SocketWaiting;
					if (smtpStateHandler() != MAIN_EMAIL_ERROR_NONE) {
						if (gs8EmailError == MAIN_EMAIL_ERROR_INIT) {
							gu8SocketStatus = SocketError;
						} else {
							close_socket();
							flagOneMinute = 0;
//							break;
						}
					}
				} else if (gu8SocketStatus == SocketComplete) {
					printf("main: Email was successfully sent.\r\n");
					close_socket();
					flagOneMinute = 0;
//					break;
				} else if (gu8SocketStatus == SocketError) {
					if (gu8RetryCount < MAIN_RETRY_COUNT) {
						gu8RetryCount++;
						printf(
								"main: Waiting to connect server.(30 seconds)\r\n\r\n");
						retry_smtp_server();
					} else {
						printf(
								"main: Failed retry to server. Press reset your board.\r\n");
						gu8RetryCount = 0;
						close_socket();
						flagOneMinute = 0;
//						break;
					}
				}
			}
		}
		osDelay(10);
	}
}

HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == BT_WIFI_IRQN_Pin) {
		isr();
	}

}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc) {
	char msg[21]; //YYYY-MM-DD HH:mm:SS

	HAL_RTC_GetTime(hrtc, &rtc_time, RTC_FORMAT_BCD);
	HAL_RTC_GetDate(hrtc, &rtc_date, RTC_FORMAT_BCD);

	sprintf(msg, "%04d-%02d-%02d %02d:%02d:%02d\r\n", B2D(rtc_date.Year) + 2000,
			B2D(rtc_date.Month), B2D(rtc_date.Date), B2D(rtc_time.Hours),
			B2D(rtc_time.Minutes), B2D(rtc_time.Seconds));
	printf(msg);

	flagOneMinuteOnce = 1;
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument) {
	/* USER CODE BEGIN 5 */
	uint8_t testNum = 0;
	osDelay(500);
	sgtl5000_init();
	sgtl5000_set_volume(systemVolume);
	osDelay(500);

	/* Mount SDCARD */
	if (f_mount(&fs, "", 0) != FR_OK)
		Error_Handler();

	/* Open file to write */
	if (f_open(&fil, "logTXT.txt", FA_OPEN_APPEND | FA_READ | FA_WRITE)
			!= FR_OK)
		Error_Handler();

	/* Check freeSpace space */
	if (f_getfree("", &fre_clust, &pfs) != FR_OK)
		Error_Handler();

	totalSpace = (uint32_t) ((pfs->n_fatent - 2) * pfs->csize * 0.5);
	freeSpace = (uint32_t) (fre_clust * pfs->csize * 0.5);

	/* free space is less than 1kb */
	if (freeSpace < 1)
		Error_Handler();

	/* Writing text */
	f_puts("This is for testing SD card.\r\n", &fil);

	/* Close file */
	if (f_close(&fil) != FR_OK)
		Error_Handler();

	/* Unmount SDCARD */
	if (f_mount(NULL, "", 1) != FR_OK)
		Error_Handler();

	/* Infinite loop */
	for (;;) {
		switch (playState) {
		case PLAY_NONE:
			if (flagOneMinuteOnce) {
				flagOneMinuteOnce = 0;
				HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BCD);
				HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BCD);

				sprintf(strSendingEmail,
						"This email is from SOMA Board\r\n %04d-%02d-%02d %02d:%02d:%02d\r\n",
						B2D(rtc_date.Year) + 2000, B2D(rtc_date.Month),
						B2D(rtc_date.Date), B2D(rtc_time.Hours),
						B2D(rtc_time.Minutes), B2D(rtc_time.Seconds));

				memcpy(strLogBuffer, 0, sizeof(strLogBuffer));
				/* Mount SDCARD */
				if (f_mount(&fs, "", 0) != FR_OK)
					Error_Handler();

				if (f_open(&fil, "logTXT.txt", FA_READ) != FR_OK)
					Error_Handler();

				while (f_gets(strLogBuffer, sizeof(strLogBuffer), &fil)) {
					strcpy(strSendingEmail, strLogBuffer);
				}
				printf(strSendingEmail);

				if (f_close(&fil) != FR_OK)
					Error_Handler();

//					/* Open file to write */
//					if(f_open(&fil, "logTXT.txt", FA_CREATE_NEW | FA_READ | FA_WRITE) != FR_OK)
//						Error_Handler();
//
//					/* Check freeSpace space */
//					if(f_getfree("", &fre_clust, &pfs) != FR_OK)
//						Error_Handler();
//
//					totalSpace = (uint32_t)((pfs->n_fatent - 2) * pfs->csize * 0.5);
//					freeSpace = (uint32_t)(fre_clust * pfs->csize * 0.5);
//
//					/* free space is less than 1kb */
//					if(freeSpace < 1)
//						Error_Handler();
//
//					/* Writing text */
//					f_puts("This is for testing SD card.\r\n", &fil);
//
//					/* Close file */
//					if(f_close(&fil) != FR_OK)
//						Error_Handler();

				/* Unmount SDCARD */
				if (f_mount(NULL, "", 1) != FR_OK)
					Error_Handler();

				if (flagOneMinute == 0) {
					sprintf(strSendingEmail,
							"%s\r\n This email is from SOMA Board\r\n GMT %04d-%02d-%02d %02d:%02d:%02d\r\n",
							strSendingEmail, B2D(rtc_date.Year) + 2000,
							B2D(rtc_date.Month), B2D(rtc_date.Date),
							B2D(rtc_time.Hours), B2D(rtc_time.Minutes),
							B2D(rtc_time.Seconds));

					SendEmailInit();
					flagOneMinute = 1;
				}
			}
			osDelay(10);
			break;

		case PLAY_READY:
			sgtl5000_start_play();
			playState = PLAY_STARTED;
			break;

		case PLAY_STARTED:
			/* Mount SDCARD */
			if (f_mount(&fs, "", 0) != FR_OK)
				Error_Handler();
			PlayAudioFile(audioFileName, "playMP3");
			/* Unmount SDCARD */
			if (f_mount(NULL, "", 1) != FR_OK)
				Error_Handler();
			break;
		}
	}

	/* USER CODE END 5 */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM5 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	/* USER CODE BEGIN Callback 0 */

	/* USER CODE END Callback 0 */
	if (htim->Instance == TIM5) {
		HAL_IncTick();
	}
	/* USER CODE BEGIN Callback 1 */

	/* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
