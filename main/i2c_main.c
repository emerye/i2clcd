/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* i2c - Simple Example

   Simple I2C example that shows how to initialize I2C
   as well as reading and writing from and to registers for a sensor connected over I2C.

   The sensor used in this example is a MPU9250 inertial measurement unit.
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"



#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#define LCD_ADDR                    0x27        /*!< Address of LCD backpack IO Expander*/

// Constants from the data sheet Command Table Instruction codes and timings
#define LCD_CLEAR_DISPLAY 	0b00000001
#define LCD_RETURN_HOME 	0b00000010
#define LCD_ENTRY_MODE_SET 	0b00000100
#define ENTRY_MODE_LEFT		0b00000010
#define ENTRY_MODE_S		0b00000001
#define LCD_DISPLAY_ON_OFF 	0b00001000
#define DISPLAY_ENTIRE		0b00000100
#define DISPLAY_CURSOR		0b00000010
#define	DISPLAY_CURSOR_POS	0b00000001
#define LCD_CURSOR_DISPLAY  0b00010000
#define CURSOR_DISPLAY_SC	0b00001000
#define CURSOR_DISPLAY_RL	0b00000100
#define LCD_FUNCTION_SET    0b00100000
#define LCD_CGRAM_ADDR		0b01000000
#define LCD_DDRAM_ADDR		0b10000000

#define LCD_LONG_DELAY 			1520
#define LCD_SHORT_DELAY			37
#define LCD_ROW_OFFSET_ADDR		0x40

#define READWRITE 2
#define ENABLE 4
#define BACKLED 8

static const char *TAG = "i2c";

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

/**
 * @brief i2c master initialization
 */
static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
{
  i2c_master_bus_config_t bus_config = {
      .i2c_port = I2C_MASTER_NUM,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

  i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = LCD_ADDR,
      .scl_speed_hz = I2C_MASTER_FREQ_HZ,
  };
  ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

/**
 * @brief Write a byte to a register.
 */
static esp_err_t i2c_write_byte(i2c_master_dev_handle_t dev_handle, uint8_t data)
{
    uint8_t write_buf[2] = {data};
    return i2c_master_transmit(dev_handle, write_buf, 1 , I2C_MASTER_TIMEOUT_MS);
}

//
void WriteI2CNibble(unsigned char msbtoWrite, int cmd)
{
  int ret;
  unsigned char bytetoWrite = BACKLED;

  if (cmd > 1)
    puts("Type is greater than 1 in function WriteI2CByte\n");

  bytetoWrite = bytetoWrite | (msbtoWrite & 0xF0) | ENABLE | cmd;
  ret = i2c_write_byte(dev_handle, bytetoWrite);
  if (ret < 0)
    puts("Failed to write byte 1 in WriteNibble.\n");
  bytetoWrite &= ~ENABLE;
  ret = i2c_write_byte(dev_handle, bytetoWrite);
  if (ret < 0)
    puts("Failed to write byte 2 in WriteNibble\n");
  bytetoWrite |= ENABLE;

  ret = i2c_write_byte(dev_handle, bytetoWrite);
  if (ret < 0)
    puts("Failed to write byte 3 in WriteNIbble.\n");
}

//Command = 0  Data = 1
void
WriteI2CByte (unsigned char bytetoWrite, int cmd)
{
  unsigned char lower = (bytetoWrite << 4) & 0b11110000;
  unsigned char upper = bytetoWrite & 0b11110000;

  if (cmd > 1)
    puts ("Type is greater than 1 in function WriteI2CByte\n");

  WriteI2CNibble (upper, cmd);
  usleep(1000);    //This delay is needed.
  WriteI2CNibble (lower, cmd);
  usleep(1000);    //This delay is needed.
}

/**
Setup 4 bit display
*/
extern void Setup4bit()
{
  usleep(1000 * 15);

  WriteI2CNibble(0x30, 0); // Manual write of Wake up!(first)
  usleep(1000 * 5);        // Sleep for at least 5ms

  WriteI2CNibble(0x30, 0); // Toggle the E bit, sends on falling edge
  usleep(1000);            // Sleep for at least 160us

  WriteI2CNibble(0x30, 0); // Manual write of Wake up! (second)
  usleep(2000);

  WriteI2CNibble(0x20, 0); // Function set to 4 bit
  usleep(2000);

  WriteI2CByte(0x28, 0); // Set 4-bit/2-line
  // Default Cursor, Display and Entry states set in the constructor
  usleep(2000);

  WriteI2CByte(LCD_CLEAR_DISPLAY, 0);
  usleep(1000);
  WriteI2CByte(LCD_RETURN_HOME, 0);
  usleep(2000);
  WriteI2CByte(LCD_CURSOR_DISPLAY, 0);
  usleep(2000);
  WriteI2CByte(LCD_DISPLAY_ON_OFF | DISPLAY_ENTIRE, 0);
  usleep(2000);
  WriteI2CByte(LCD_ENTRY_MODE_SET | ENTRY_MODE_LEFT, 0);
  usleep(2000);
  WriteI2CByte(LCD_RETURN_HOME, 0);
  usleep(2000);
}

/***
 * Clears the display by passing the LCD_CLEAR_DISPLAY command
 */
extern void
DisplayClear ()
{
WriteI2CByte (LCD_CLEAR_DISPLAY, 0);
 vTaskDelay(pdMS_TO_TICKS(2));
}

/***
 * Returns the cursor to the 00 address by passing the LCD_RETURN_HOME command
 */
extern void
DisplayHome ()
{
  WriteI2CByte (LCD_RETURN_HOME, 0);
  vTaskDelay(pdMS_TO_TICKS(2));
}

//Prints a string starting at position. 
extern void
WriteString (int row, int ypos, char message[])
{
  int stLength = strlen (message);
  int i, address=0;

  switch (row)
    {
    case 0:
      address = ypos;
      break;
    case 1:
      address = 0x40 + ypos;
      break;
    case 2:
      address = 20 + ypos;
      break;
    case 3:
      address = 0x54 + ypos;
      break;
    }
  address += 0x80;
  WriteI2CByte ((unsigned char) address, 0);
  for (i = 0; i < stLength; i++)
    {
      if (message[i] > 0x1f)
	{
	  WriteI2CByte (message[i], 1);
     vTaskDelay(pdMS_TO_TICKS(2));
	}
    }
}



/**
 * @brief Read a sequence of bytes.
 */
static esp_err_t mpu9250_register_read(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS);
}

/**
 * @brief Write a byte to a register.
 */
static esp_err_t lcd_register_write_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
}

void app_main(void)
{
  uint8_t data[2];
  // i2c_master_bus_handle_t bus_handle;
  // i2c_master_dev_handle_t dev_handle;
  i2c_master_init(&bus_handle, &dev_handle);
  ESP_LOGI(TAG, "I2C initialized successfully");

  /* Read the MPU9250 WHO_AM_I register, on power up the register should have the value 0x71 */
  //   ESP_ERROR_CHECK(mpu9250_register_read(dev_handle, MPU9250_WHO_AM_I_REG_ADDR, data, 1));
  //   ESP_LOGI(TAG, "WHO_AM_I = %X", data[0]);

  /* Demonstrate writing by resetting the MPU9250 */
  // ESP_ERROR_CHECK(lcd_register_write_byte(dev_handle, MPU9250_PWR_MGMT_1_REG_ADDR, 1 << MPU9250_RESET_BIT));
  // vTaskDelay(pdMS_TO_TICKS(2000));
  Setup4bit();

  DisplayClear();
  usleep(2000);

  WriteString(0, 0, "Hello");
  WriteString(1, 0, "There");
  WriteString(2, 0, "ABCDEFGHIJKLMNOP");
  printf("Running\n");

  ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
  ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
  ESP_LOGI(TAG, "I2C de-initialized successfully");
}
