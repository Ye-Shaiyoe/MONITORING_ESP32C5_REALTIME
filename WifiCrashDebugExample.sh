9:42:23.994 -> ELF file SHA256: 93ec1c02a
09:42:23.994 -> 
09:42:23.994 -> Rebooting...
09:42:24.033 -> ESP-ROM:esp32c5-eco2-20250121
09:42:24.033 -> Build:Jan 21 2025
09:42:24.033 -> rst:0xc (SW_CPU),boot:0x1e (SPI_FAST_FLASH_BOOT)
09:42:24.033 -> Core0 Saved PC:0x4080a444
09:42:24.033 -> SPI mode:DIO, clock div:1
09:42:24.033 -> load:0x408556b0,len:0x1258
09:42:24.033 -> load:0x4084bba0,len:0xca4
09:42:24.033 -> load:0x4084e5a0,len:0x315c
09:42:24.033 -> entry 0x4084bba0
09:42:24.312 -> E (292) MSPI Timing: Failed to allocate dummy cacheline for PSRAM memory barrier!
09:42:26.276 -> [WDT] Watchdog aktif, timeout 10 detik
09:42:26.415 -> [WiFi] Scanning jaringan 5GHz...
09:42:36.285 -> E (13613) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
09:42:36.285 -> E (13613) task_wdt:  - loopTask (CPU 0)
09:42:36.285 -> E (13613) task_wdt: Tasks currently running:
09:42:36.285 -> E (13613) task_wdt: CPU 0: IDLE
09:42:36.285 -> E (13613) task_wdt: Aborting.
09:42:36.285 -> E (13613) task_wdt: Print CPU 0 (current core) registers
09:42:36.783 -> 
09:35:41.796 -> E (13554) task_wdt: Tasks currently running:
09:35:41.796 -> E (13554) task_wdt: CPU 0: IDLE
09:35:41.796 -> E (13554) task_wdt: Aborting.
09:35:41.796 -> E (13554) task_wdt: Print CPU 0 (current core) registers
09:35:42.242 -> 
09:35:42.242 -> 
09:35:42.242 -> Core  0 register dump:
09:35:42.242 -> MEPC    : 0x4080680a  RA      : 0x408067f8  SP      : 0x40826890  GP      : 0x40815784  
09:35:42.242 -> TP      : 0x408268f0  T0      : 0x4002d624  T1      : 0x4080655e  T2      : 0x00000000  
09:35:42.242 -> S0/FP   : 0x40821000  S1      : 0x40821000  A0      : 0x00000000  A1      : 0xfefefefe  
09:35:42.242 -> A2      : 0x00000000  A3      : 0x00000080  A4      : 0x00000000  A5      : 0x600c2000  
09:35:42.242 -> A6      : 0x00000000  A7      : 0x00000000  S2      : 0x4082d8d0  S3      : 0x4081a000  
09:35:42.242 -> S4      : 0x00000000  S5      : 0x00000000  S6      : 0x00000000  S7      : 0x00000000  
09:35:42.242 -> S8      : 0x00000000  S9      : 0x00000000  S10     : 0x00000000  S11     : 0x00000000  
09:35:42.242 -> T3      : 0x00000000  T4      : 0x00000000  T5      : 0x00000000  T6      : 0x00000000  
09:35:42.242 -> MSTATUS : 0x4082d8d0  MTVEC   : 0x40821000  MCAUSE  : 0x40821000  MTVAL   : 0x4080c792  
09:35:42.242 -> MHARTID : 0x00000000  

Stack memory:
09:35:42.242 -> 40826890: 0x4082d8d0 0x40821000 0x40821000 0x4080c792 0x00000000 0x00000000 0x00000000 0x00000000
09:35:42.242 -> 408268b0: 0x00000000 0x00000000 0x00000000 0x4080b986 0x00000000 0x00000000 0x00000000 0x00000000
09:35:42.242 -> 408268d0: 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0xa5a5a5a5 0xa5a5a5a5 0xa5a5a5a5
09:35:42.242 -> 408268f0: 0xa5a5a5a5 0xa5a5a5a5 0xa5a5a5a5 0xa5a5a5a5 0xbaad5678 0x00000168 0xabba1234 0x0000015c
09:35:42.242 -> 40826910: 0x40826810 0x00000000 0x4081a47c 0x4081a47c 0x40826910 0x4081a474 0x00000019 0x00000000
09:35:42.242 -> 40826930: 0x00000000 0x40826910 0x00000000 0x00000000 0x40826000 0x454c4449 0x00000000 0x00000000
09:35:42.242 -> 40826950: 0x00000000 0x408268f0 0x00000003 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000
09:35:42.242 -> 40826970: 0x00abcffc 0x00000000 0x40821e00 0x40821e68 0x40821ed0 0x00000000 0x00000000 0x00000001
09:35:42.242 -> 40826990: 0x00000000 0x00000000 0x00000000 0x4201b2a4 0x00000000 0x00000000 0x00000000 0x00000000
...

09:35:42.242 -> ELF file SHA256: 93ec1c02a
09:35:42.242 -> 
09:35:42.242 -> Rebooting...
09:35:42.279 -> ESP-ROM:esp32c5-eco2-20250121
09:35:42.279 -> Build:Jan 21 2025
09:35:42.279 -> rst:0xc (SW_CPU),boot:0x1e (SPI_FAST_FLASH_BOOT)
09:35:42.279 -> Core0 Saved PC:0x4080a444
09:35:42.279 -> SPI mode:DIO, clock div:1
09:35:42.279 -> load:0x408556b0,len:0x1258
09:35:42.279 -> load:0x4084bba0,len:0xca4
09:35:42.279 -> load:0x4084e5a0,len:0x315c
09:35:42.279 -> entry 0x4084bba0
09:35:42.546 -> E (292) MSPI Timing: Failed to allocate dummy cacheline for PSRAM memory barrier!
09:35:44.502 -> [WDT] Watchdog aktif, timeout 10 detik
09:35:44.674 -> [WiFi] Scanning jaringan 5GHz...
