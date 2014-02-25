Thingsquare-Contiki-O.S-Porting-STM32F4-Discovery
==================================================

We have ported the Contiki Operating System to STM32F4 platform (http://www.st.com/stm32f4-discovery). We started with the work that was already done on an evaluation board based on STM32L1 platform (STEVAL-IKR001V1, http://www.st.com/web/en/catalog/tools/PF253893). The original project, called "Mist", was created by Thingsquare using Spirit1 radio transceiver (STEVAL-IKR001V8D, http://www.st.com/web/en/catalog/tools/FM116/SC1075/PF258319). We converted the system calls from the original platform to the target platform. We have adapted Spirit1 radio driver in order to work properly on STM32F4. Then, we created a Wireless Sensor Network and collected several communication and performance data. I worked on it with Daniele Saitta. To manage the entire work, we used IAR Embedded Workbench (http://www.iar.com/en/products/iar-embedded-workbench/arm/)
