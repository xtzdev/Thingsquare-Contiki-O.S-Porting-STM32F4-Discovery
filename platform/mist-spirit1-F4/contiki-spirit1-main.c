/*
 * Copyright (c) 2012, Thingsquare, www.thingsquare.com.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of Thingsquare Mist.
 */

/*
 *  \file
 *      contiki-main.c
 *   \author
 *      Marcus Lunden <marcus@thingsquare.com>>
 *   \desc
 *      Contiki main file for mist-spirit1 platform 
 *      (stm32f4xxVB @ STEVAL-IKR001V4 + Spirit1)
 */

#include <stdio.h>
#include <string.h>

#include "contiki.h"
#include "contiki-net.h"
#include "sys/autostart.h"
#include "sys/profile.h"
#include "dev/leds.h"
#include "dev/serial-line.h"
#include "dev/slip.h"
#include "dev/watchdog.h"
#include "dev/xmem.h"
#include "lib/random.h"

#include "net/netstack.h"
#include "net/uip.h"
#include "net/mac/frame802154.h"
#include "net/rime/rimeaddr.h"
#include "net/rime.h"
#include "net/rime/rime-udp.h"

#include "stm32f4xx.h"                      
        
#include "stm32f4xx_dbgmcu.h"
#include "SDK_EVAL_Config.h"
#include "SPIRIT_Config.h"
#include "SPIRIT_Management.h"
#include "spirit1.h"
#include "spirit1-arch.h" 
// #include "usb_init.h"

#include "node-id.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) printf(" %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((u8_t *)addr)[0], ((u8_t *)addr)[1], ((u8_t *)addr)[2], ((u8_t *)addr)[3], ((u8_t *)addr)[4], ((u8_t *)addr)[5], ((u8_t *)addr)[6], ((u8_t *)addr)[7], ((u8_t *)addr)[8], ((u8_t *)addr)[9], ((u8_t *)addr)[10], ((u8_t *)addr)[11], ((u8_t *)addr)[12], ((u8_t *)addr)[13], ((u8_t *)addr)[14], ((u8_t *)addr)[15])
#define PRINTLLADDR(lladdr) printf(" %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x ",lladdr.u8[0], lladdr.u8[1], lladdr.u8[2], lladdr.u8[3],lladdr.u8[4], lladdr.u8[5], lladdr.u8[6], lladdr.u8[7])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

#if UIP_CONF_IPV6
PROCINIT(&etimer_process, &tcpip_process);
#else
PROCINIT(&etimer_process);
#warning "No TCP/IP process!"
#endif

#define BUSYWAIT_UNTIL(cond, max_time)                                  \
  do {                                                                  \
    rtimer_clock_t t0;                                                  \
    t0 = RTIMER_NOW();                                                  \
    while(!(cond) && RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + (max_time)));   \
  } while(0)
/*---------------------------------------------------------------------------*/
static void print_processes(struct process * const processes[]);
static void set_rime_addr(void);
/*---------------------------------------------------------------------------*/
static void
panic_main(void)
{
  volatile uint16_t k;
  while(1) {
    SdkEvalLedOff(LED2);
    for(k = 0; k < 0xffff/8; k += 1) { }
    SdkEvalLedOn(LED2);
    for(k = 0; k < 0xffff/8; k += 1) { }
  }
}
/*---------------------------------------------------------------------------*/
int
main(int argc, char *argv[])
{
  
  /* enable clock to ports and pins peripherals */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA |
                        RCC_AHB1Periph_GPIOB |
                        RCC_AHB1Periph_GPIOC |
                        RCC_AHB1Periph_GPIOD |
                        RCC_AHB1Periph_GPIOE, ENABLE );

  /* To be able to debug STM32 devices using low power modes, I.e. sleep, standby
  or stop mode, the chip needs to be configured to allow the debugger to access it
  during low power mode. The following line of code enables debugger access during
  sleep, standby and stop mode */
  DBGMCU_Config(DBGMCU_SLEEP | DBGMCU_STANDBY | DBGMCU_STOP, ENABLE);

  /* init LEDs, move to leds-arch */
  SdkEvalLedInit(LED1);
  SdkEvalLedInit(LED2);
  SdkEvalLedInit(LED3);
  SdkEvalLedInit(LED4);
  
  SdkEvalLedOff(LED1);
  SdkEvalLedOff(LED2);
  SdkEvalLedOff(LED3);
  SdkEvalLedOff(LED4);
  
  /* Regulate power supply voltage for RF etc */
  SdkEvalLedOn(LED1);
  SdkEvalPmRfSwitchInit(); 
  SdkEvalPmADCInit();
  SdkEvalPmI2CInit();
  SdkEvalPmRegulateVRfI(3.3);
  SdkEvalPmRfSwitchToVRf();  
  SdkEvalLedOff(LED1);
  /*  TODO, check that the proper voltage range is set. Minimum:
      32 MHz    range 1
      16 MHz    range 2
       4 MHz    range 3*/

  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
  /* USB virtual com-port setup 
  SdkEvalLedOn(LED1);
  SdkEvalVCInit();
  while(bDeviceState != CONFIGURED);
  SdkEvalLedOff(LED1);
  */
  /* Initialize Contiki and our processes. */  

  clock_init();
  ctimer_init();
  rtimer_init();
  watchdog_init();
  
leds_init();
process_init();
  
process_start(&etimer_process, NULL);
  

#if 0
#if WITH_SERIAL_LINE_INPUT
  uart1_set_input(serial_line_input_byte);
  serial_line_init();
#endif
#endif    /* if 0; code commented out */


  /* Restore node id if such has been stored in external mem */
#ifdef NODEID
  node_id = NODEID;

#ifdef BURN_NODEID
  
  node_id_burn(node_id);
  
#endif /* BURN_NODEID */

#else/* NODE_ID */
	
  node_id_restore(); /* also configures node_mac[] */
  
#endif /* NODE_ID */

#define STARTUP_DELAY 1
#if STARTUP_DELAY
  {
    #define WAIT_SECONDS 4
    int secs = WAIT_SECONDS;
   
    while (secs-- > 0) {
		
      volatile clock_time_t endwait = clock_time() + CLOCK_SECOND;
	  
      while(clock_time() < endwait);
	  
      watchdog_periodic();
      SdkEvalLedToggle(LED3);  
      //printf("Delayed startup. Starting in %d seconds...\n", secs);
	  
    }
    SdkEvalLedOff(LED3); 
  }
#endif /* STARTUP_DELAY */


  set_rime_addr();
  
  random_init(node_id);

  netstack_init();

  spirit_radio_driver.on();

  energest_init();
  ENERGEST_ON(ENERGEST_TYPE_CPU);


  printf("\nStarting ");
  printf(CONTIKI_VERSION_STRING);
  printf(" on Thingsquare Mist Spirit1\n");

  if(node_id > 0) {
    printf("Node id %u.\n", node_id);
  } else {
    printf("Node id not set.\n");
  }

  /*printf("%s %lu %u\n",
         NETSTACK_RDC.name,
         CLOCK_SECOND / (NETSTACK_RDC.channel_check_interval() == 0? 1:
                         NETSTACK_RDC.channel_check_interval()),
         RF_CHANNEL);
*/

#if WITH_UIP6
  memcpy(&uip_lladdr.addr, node_mac, sizeof(uip_lladdr.addr));
  /* Setup nullmac-like MAC for 802.15.4 */

  queuebuf_init();

  /*printf("%s %lu %u\n",
         NETSTACK_RDC.name,
         CLOCK_SECOND / (NETSTACK_RDC.channel_check_interval() == 0 ? 1:
                         NETSTACK_RDC.channel_check_interval()),
         RF_CHANNEL);
*/
  process_start(&tcpip_process, NULL);

  printf("IPv6 ");
  {
    uip_ds6_addr_t *lladdr;
    int i;
    lladdr = uip_ds6_get_link_local(-1);
    /*for(i = 0; i < 7; ++i) {
      printf("%02x%02x:", lladdr->ipaddr.u8[i * 2],
             lladdr->ipaddr.u8[i * 2 + 1]);
    }*/
    printf("%02x%02x\n", lladdr->ipaddr.u8[14], lladdr->ipaddr.u8[15]);
  }

  if(1) {
    uip_ipaddr_t ipaddr;
    int i;
    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&ipaddr, 0, ADDR_TENTATIVE);
    printf("Tentative global IPv6 address ");
    /*for(i = 0; i < 7; ++i) {
      printf("%02x%02x:",
             ipaddr.u8[i * 2], ipaddr.u8[i * 2 + 1]);
    }*/
    /*printf("%02x%02x\n",
           ipaddr.u8[7 * 2], ipaddr.u8[7 * 2 + 1]);*/
  }

#else /* WITH_UIP6 */

  /*printf("%s %lu %u\n",
         NETSTACK_RDC.name,
         CLOCK_SECOND / (NETSTACK_RDC.channel_check_interval() == 0? 1:
                         NETSTACK_RDC.channel_check_interval()),
         RF_CHANNEL);*/
#endif /* WITH_UIP6 */

#if !WITH_UIP6
  uart1_set_input(serial_line_input_byte);
  serial_line_init();
#endif


#ifdef NETSTACK_AES_H
#ifndef NETSTACK_AES_KEY
#error Please define NETSTACK_AES_KEY!
#endif /* NETSTACK_AES_KEY */
  {
    const uint8_t key[] = NETSTACK_AES_KEY;
    netstack_aes_set_key(key);
  }
  /*printf("AES encryption is enabled: '%s'\n", NETSTACK_AES_KEY);*/
  printf("AES encryption is enabled\n");
#else /* NETSTACK_AES_H */
  printf("Warning: AES encryption is disabled\n");
#endif /* NETSTACK_AES_H */


  print_processes(autostart_processes);
  autostart_start(autostart_processes);
  watchdog_start();

#if 0
  /* interferes with the Spirit radio */
  {
    GPIO_InitTypeDef  GPIO_InitStructure;

    /* Enable the GPIO_LED Clock */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    /* Configure the GPIO_LED pin */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Pin = 1<<8;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIOC->BSRRH = 1<<8;
    GPIOC->BSRRL = 1<<8;
  }
#endif    /* if 0; code commented out */

  /* infinitely tests rtimers by toggling a pin */
  //test_rtimers();

  while(1){
    int r = 0;
    do {
      watchdog_periodic();
      r = process_run();
    } while(r > 0);

    /* sleep */
    ENERGEST_OFF(ENERGEST_TYPE_CPU);
    watchdog_stop();    
    ENERGEST_ON(ENERGEST_TYPE_LPM);
    //halSleepWithOptions(SLEEPMODE_IDLE,0);

    /* woke up */
    watchdog_start();
    ENERGEST_OFF(ENERGEST_TYPE_LPM);
    ENERGEST_ON(ENERGEST_TYPE_CPU);  
  }
}
/*---------------------------------------------------------------------------*/
#if 0
#include <stdarg.h>

int vfprintf(FILE *stream, const char *format, va_list ap){
  return 0;
}
#endif    /* if 0; code commented out */
/*---------------------------------------------------------------------------*/
static void
test_rtimers(void)
{
  /* test rtimers */
  #define PINFLIP     (1<<10)
  GPIO_InitTypeDef  GPIO_InitStructure;

  /* Enable the GPIO_LED Clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

  /* Configure the GPIO_LED pin */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

  GPIO_InitStructure.GPIO_Pin = PINFLIP;
  GPIO_Init(GPIOC, &GPIO_InitStructure);

  while(1) {
    GPIOC->BSRRH = PINFLIP;
    BUSYWAIT_UNTIL(0, RTIMER_SECOND/8);

    GPIOC->BSRRL = PINFLIP;
    BUSYWAIT_UNTIL(0, RTIMER_SECOND/8);
  }
}

/*---------------------------------------------------------------------------*/
static void
set_rime_addr(void)
{
  rimeaddr_t addr;
  int i;

  memset(&addr, 0, sizeof(rimeaddr_t));
#if UIP_CONF_IPV6
  memcpy(addr.u8, node_mac, sizeof(addr.u8));
#else
  if(node_id == 0) {
    for(i = 0; i < sizeof(rimeaddr_t); ++i) {
      addr.u8[i] = node_mac[7 - i];
    }
  } else {
    addr.u8[0] = node_id & 0xff;
    addr.u8[1] = node_id >> 8;
  }
#endif
  rimeaddr_set_node_addr(&addr);
  printf("Rime addr ");
  for(i = 0; i < sizeof(addr.u8) - 1; i++) {
    printf("%u.", addr.u8[i]);
  }
  printf("%u\n", addr.u8[i]);
}
/*---------------------------------------------------------------------------*/
static void
print_processes(struct process * const processes[])
{
  const struct process * const * p = processes;
  printf("Starting");
  while(*processes != NULL) {
    printf(" %s", (*processes)->name);
    processes++;
  }
  putchar('\n');
}
/*---------------------------------------------------------------------------*/
/* use a variable placed in a section that will not be initialized at startup,
so that it can be read when MCU is halted to see what caused this error */
#ifdef IAR
//uint8_t errcode @ .noinit;
#else
//uint8_t errcode __attribute__(( section(".noinit") ));
#endif    /* IAR */

/* XXX XXX temporary declaration, should be removed when support for placing the variable
    in .noinit works. */
static uint8_t errcode = 0;

void Default_Handler();
/*---------------------------------------------------------------------------*/
void
halBaseBandIsr()
{
  errcode = 1;
  SdkEvalLedOn(LED4);
  while(1) {;}
}
/*---------------------------------------------------------------------------*/
void
BusFault_Handler()
{
  errcode = 2; 
  SdkEvalLedOn(LED4);
  while(1) {;}
}
/*---------------------------------------------------------------------------*/
void
halDebugIsr()
{
  errcode = 3;
  SdkEvalLedOn(LED4);
  while(1) {;}
}
/*---------------------------------------------------------------------------*/
void
DebugMon_Handler()
{
  errcode = 4;
  SdkEvalLedOn(LED4);
  while(1) {;}
}
/*---------------------------------------------------------------------------*/
void
HardFault_Handler()
{
  errcode = 5; 
  SdkEvalLedOn(LED4);
  while(1) {;}
}
/*---------------------------------------------------------------------------*/
void
MemManage_Handler()
{
  errcode = 6; 
  SdkEvalLedOn(LED4);
  while(1) {;}
}
/*---------------------------------------------------------------------------*/
void
UsageFault_Handler()
{
  errcode = 7; 
  SdkEvalLedOn(LED4);
  while(1) {;}
}
/*---------------------------------------------------------------------------*/
void
Default_Handler()
{ 
  errcode = 8; 
  SdkEvalLedOn(LED4);
  
  /* flip around a pin that can be seen on a logic analyzer for example */
  {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = 1<<7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    GPIO_Init(GPIOC, &GPIO_InitStructure);
  }
  
  while (1)
  {
    GPIOC->BSRRH = 1<<7;  // go high
    GPIOC->BSRRL = 1<<7;  // go low
  }
}
/*---------------------------------------------------------------------------*/
