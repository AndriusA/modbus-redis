/*
 *  Reads in data from a Sunsaver MPPT charge controller, stores to Redis.
 *  Compile with: cc -I/usr/local/include/modbus -I/usr/local/include/hiredis \
                     -L/usr/local/lib -lmodbus -lhiredis  mod-red.c -o mod-red
 */

#include "send.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>

#include <modbus/modbus.h>
#include <hiredis/hiredis.h>

#define SUNSAVERMPPT    0x01           // MODBUS address
#define SERIALPORT      "/dev/ttyUSB0" // SerialPort address

// Global Vars
redisContext *c;
static bool running = true;

// Method Definitions
modbus_t * modbus_conn(void);
void modbus_disconnect(modbus_t *ctx);
uint16_t * read_data(modbus_t *ctx);
void setData(char *key, float val, int t);
void handleData(uint16_t *, int t);
void termHandler(int dum);

int main()
{
  signal(SIGINT, termHandler);

  uint16_t *data;
  modbus_t *ctx;
  int t;

  c = redis_conn();

  ctx = modbus_conn();

  while(running) {
    t = (int)time(NULL);

    data = read_data(ctx);

    handleData(data, t);

    sleep(5);
  }

  modbus_disconnect(ctx);

  redis_disconn(c);

	return 0;
}

modbus_t * modbus_conn(void) {
  modbus_t *ctx;
	ctx = modbus_new_rtu(SERIALPORT, 9600, 'N', 8, 2);
	if (ctx == NULL) {
		fprintf(stderr, "Unable to create the libmodbus context\n");
		exit(-1);
	}

	/* Set the slave id to the SunSaver MPPT MODBUS id */
	modbus_set_slave(ctx, SUNSAVERMPPT);

	/* Open the MODBUS connection to the SunSaver MPPT */
  if (modbus_connect(ctx) == -1) {
      fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
      modbus_free(ctx);
      exit(-1);
  }
  fprintf(stdout, "Modbus connected\n");
  return ctx;
}

void modbus_disconnect(modbus_t *ctx) {
  modbus_close(ctx);
  modbus_free(ctx);
  fprintf(stdout, "Modbus Disconnected\n");
}

/* WARNING: caller is responsible for free()ing return value! */
uint16_t * read_data(modbus_t *ctx) {
	/* Read the RAM Registers */
  // TODO: Read in two separate requests because of limitations with MODBUS
  int rc;
  uint16_t *data = malloc(sizeof(uint16_t) * 50);
	rc = modbus_read_registers(ctx, 0x0008, 45, data);
	if (rc == -1) {
		fprintf(stderr, "%s\n", modbus_strerror(errno));
		exit(-1);
	}
  return data;
}

void setData(char *key, float val, int t) {
  redisReply *reply;

  reply = redisCommand(c ,"HSET solar:%s %d %f",key, t, val);

  switch (reply->type) {
    case REDIS_REPLY_STATUS: {
      printf("Received Str %s\n", reply->str);
      break;
    }
    case REDIS_REPLY_ERROR: {
      printf("Received Error %s\n", reply->str);
      break;
    }
    case REDIS_REPLY_INTEGER: {
      printf("Received Integer %lld\n", reply->integer);
      break;
    }
    case REDIS_REPLY_NIL: {
      printf("Received nil reply\n");
      break;
    }
    case REDIS_REPLY_STRING: {
      printf("Received Reply Str %s\n", reply->str);
      break;
    }
    case REDIS_REPLY_ARRAY: {
      printf("Received array of elements\n");
      break;
    }
    default: {
      break;
    }
  }

  printf("command: HSET %s %d to %f\n",key, t, val);

  freeReplyObject(reply);
}

void handleData(uint16_t *data, int t) {
  // Set a master set of timestamps. Makes it easier to access
  // things in HGET
  redisReply *reply;
  reply = redisCommand(c, "SADD timestamps %d", t);
  freeReplyObject(reply);

  // Voltage measured directly at the battery terminal
  setData("adc_vb_f", data[0] * 100.0 / 32768.0, t);

  // Terminal voltage of solar
  setData("adc_va_f", data[1] * 100.0 / 32768.0, t);

  // Terminal voltage of load
  setData("adc_vl_f", data[2] * 100.0 / 32768.0, t);

  // Charging current to the battery
	setData("adc_ic_f", data[3] * 79.16 / 32768.0, t);

  // Load current to load
	setData("adc_il_f", data[4] * 79.16 / 32768.0, t);

  // Heatsink temp
	setData("T_hs", data[5], t);

  // Ambient temp (builtin)
	setData("T_amb", data[7], t);

  // Charge state (nums in ref)
	setData("charge_state", data[9], t);

  // Battery Voltage (slow filtered)
	setData("Vb_f",data[11]*100.0/32768.0, t);

  // Target voltage to which battery will be charged (temp compensated)
	setData("Vb_ref", data[12]*96.667/32768.0, t);

  // Solar amp-hours since last reset
  // Reset-able
	setData("Ahc_r", ((data[13] << 16) + data[14])*0.1, t);

  // Solar amp-hours since last reset
  // cumulative
	setData("Ahc_t",((data[15] << 16) + data[16])*0.1, t);

  // Total solar kWh since last reset
  // reset-able
	setData("kWhc",data[17]*0.1, t);

  // Low voltage disconnect setpoint, current compensated
	setData("V_lvd", data[20]*100.0/32768.0, t);

  // Load amp-hours since last reset
  // Resetable
	setData("Ahl_r", ((data[21] << 16) + data[22])*0.1, t);

  // Load amp hours
  // Total cumulative
	setData("Ahl_t", ((data[23] << 16) + data[24])*0.1, t);

  // Total hours of operation since installed
	setData("hourmeter", (data[25] << 16) + data[26], t);

  // Output power to the battery
	setData("Power_out", data[31]*989.5/65536.0, t);

  // Max power voltage of the solar array during last sweep
	setData("Sweep_Vmp", data[32]*100.0/32768.0, t);

  // max power output of solar array during last sweep
	setData("Sweep_Pmax",data[33]*989.5/65536.0, t);

  // Open circuit voltage of solar array during last sweep
	setData("Sweep_Voc", data[34]*100.0/32768.0, t);

  // Minimum battery voltage (resets after dark)
	setData("Vb_min_daily",data[35]*100.0/32768.0, t);

  // Maximum battery voltage (resets after dark)
	setData("Vb_max_daily", data[36]*100.0/32768.0, t);

  // Total charging amp-hours today (resets after dark)
	setData("Ahc_daily", data[37]*0.1, t);

  // Total load amp hours today (resets after dark)
	setData("Ahl_daily", data[38]*0.1, t);

  free(data);
}

void termHandler(int dum) {
  running = false;
}