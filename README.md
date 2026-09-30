# DEOS Core Protocol Stack

## DEOS Core Nedir
DEOS Core, STM32 tabanlı Zephyr node'ları arasında CAN-FD üzerinden ortak olarak kullanılacak iletişim kütüphanesidir.
Traction, Steering, Brake gibi fiziksel kontrol düğümleri, CAN frame'leriyle uğraşmak yerine `deos_send` ve `deos_register_handler` aracılığıyla DEOS Core'u kullanır.

## CAN-FD Gereksinimi
Bu protokol **TAMAMEN** CAN-FD altyapısı üzerine tasarlanmıştır. Classic CAN desteği veya fallback mekanizması bulunmamaktadır. 
Eğer çalışılan geliştirme kartı (örn. nucleo_f439zi) CAN-FD desteklemiyorsa, `deos_init` ve `deos_start` API'leri `ENOTSUP` (veya ilgili driver hatasını) döndürür. Bu durum bir hardware/development-board limitasyonudur, protokolün kısıtlaması değildir.

## 29-bit CAN ID Formatı
CAN-FD mesajları 29-bit Extended Identifier kullanır. Yapısı şöyledir:
- **Bits 28..26:** Priority (3 bit)
- **Bits 25..22:** Message Class (4 bit)
- **Bits 21..16:** Service ID (6 bit)
- **Bits 15..8:** Destination Node (8 bit)
- **Bits 7..0:** Source Node (8 bit)

## Directory Structure
```
lib/deos_core/
├── CMakeLists.txt
├── include/
│   └── deos/
│       ├── deos.h          (Public API)
│       ├── deos_icd.h      (Protocol Definitions)
│       └── deos_types.h    (Structs & Types)
└── src/
    ├── deos_internal.h
    ├── deos_core.c
    ├── deos_codec.c
    ├── deos_rx.c
    ├── deos_tx.c
    ├── deos_dispatch.c
    ├── deos_network.c
    └── deos_router.c
```

## Initialization ve Kullanım

```c
#include <deos/deos.h>

void main(void) {
    struct deos_config config = {
        .node_id = DEOS_NODE_STEERING,
        .can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus)),
        .router_enabled = false
    };

    /* 1. Initialize core */
    deos_init(&config);

    /* 2. Register Application Handlers */
    deos_register_handler(
        DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING,
        DEOS_CMD_STEERING_SET_TARGET_ANGLE,
        steering_handler, NULL);

    /* 3. Start RX threads */
    deos_start();

    /* 4. Send Message (Source is spoof-protected automatically) */
    uint16_t current_angle = 450; 
    deos_send(
        DEOS_NODE_MAIN_STM32, DEOS_PRIO_STATUS,
        DEOS_CLASS_STATUS, DEOS_SERVICE_STEERING,
        DEOS_CMD_STEERING_GET_STATUS,
        &current_angle, sizeof(current_angle));
}
```

## PING / PONG
Ağdaki cihazların canlılığını kontrol etmek için kullanılır.
```c
deos_send_ping(DEOS_NODE_MAIN_STM32, 0x12345678);
```
Geçerli bir PING alan `deos_core`, application'a sormadan **otomatik olarak** PONG yanıtı üretir. (Broadcast PING'ler network storm'u engellemek için reject edilir).

## Routing Concept
Protokol, BMS Gateway veya Main STM32 gibi düğümler üzerinden farklı alt ağlara (CAN-FD, Ethernet vb.) mesaj yönlendirmeyi destekler. Main STM32 yapılandırıldığında `router_enabled` bayrağı üzerinden, kendisine ait olmayan veya broadcast olan paketleri `deos_router.c` vasıtasıyla hedef ağlara iletebilir.

## Application / Core Ayrımı
**Felsefe:**
- Application: "Motoru şu açıya döndürmek istiyorum."
- DEOS Core: "Bu veriyi CAN-FD üzerinden sequence, version ve endian kurallarıyla nasıl serialize/deserialize ederim?"
- Control Thread: "PID algoritmasını uygulayarak fiziksel donanımı nasıl süreceğim?"
Bu katmanlar birbirine karışmaz. Malloc kullanılmaz.

## Mevcut Durum (Development Status)
- ✅ Codec & Validation tamamlandı.
- ✅ Handler Registration & Dispatch tamamlandı.
- ✅ TX / RX kuyrukları ve statik worker thread tamamlandı.
- ✅ Network & Ping/Pong otonom cevabı eklendi.
- ⚠️ F439ZI için CAN-FD limitasyonu donanımsaldır.
- 🚧 (PROVISIONAL) Heartbeat payload, Calibration exact payloads ve Ethernet/LoRa framing ICD üzerinde henüz "frozen" olmadığından stub olarak bırakılmıştır.
# deos-core-protocol-stack
