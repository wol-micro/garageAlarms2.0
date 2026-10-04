---
review: verify-tech
target: ../ARCHITECTURE-SPINE.md
scope: 'AD-26, AD-27, AD-28, таблица регионов FRAM, строка Stack «FRAM»'
date: '2026-10-04'
method: 'grep Kconfig/исходников/документации локального ESP-IDF v6.0.3 (~/esp/v6.0.3/esp-idf, git describe = v6.0.3); скачан и разобран бинарный компонент espressif/esp-zigbee-lib 2.0.4 (nm по .a для esp32s3); пример zigbee_gateway из esp-zigbee-sdk (GitHub API, ветка main); даташиты MB85RS2MTA (DS6v1) и FM25V20A (Infineon), pdftotext'
---

# Проверка технологических утверждений — AD-26, AD-27, AD-28 (garageAlarms 2.0)

## Вердикт

Все технологические опоры новых решений реальны и проверены по исходникам ESP-IDF v6.0.3, бинарям esp-zigbee-lib 2.0.4 и даташитам. Ложных утверждений нет. Есть три уточнения, которые нужно внести в спайн или в story 1.1, иначе реализация разойдётся с замыслом:

1. Привязка lwIP к ядру 0 **по умолчанию не включена**: по умолчанию `LWIP_TCPIP_TASK_AFFINITY_NO_AFFINITY`. Нужен явный `CONFIG_LWIP_TCPIP_TASK_AFFINITY_CPU0=y`.
2. «Ёмкость читается командой RDID» — верно, но **формат RDID у двух выбранных микросхем разный**: 4 байта против 9, другое место поля плотности и другая кодировка. Нужна таблица известных ID, а не одна формула.
3. Стек esp-zigbee-lib 2.0 **сам пишет в NVS** на флеше логического узла, в обход `Storage`. Это не противоречит технологии, но идёт вразрез с формулировкой AD-6 и AD-28 («только `Storage`», «файловой системы нет»), и об этом нужно сказать явно.

## Сводка

| # | Утверждение в спайне | Статус | Коротко |
|---|---|---|---|
| 1 | lwIP привязан к ядру 0 (AD-26) | ✅ подтверждено, ⚠️ не по умолчанию | Опция `LWIP_TCPIP_TASK_AFFINITY` есть (CPU0/CPU1/NO_AFFINITY), по умолчанию NO_AFFINITY |
| 2 | WiFi на ядре 0 (AD-26) | ✅ подтверждено | `ESP_WIFI_TASK_CORE_ID`, по умолчанию `ESP_WIFI_TASK_PINNED_TO_CORE_0` |
| 3 | Сторожевой таймер задач видит голодание обоих ядер (подразумевается в AD-26) | ✅ подтверждено | `ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0/CPU1`, обе по умолчанию `y` |
| 4 | Хост Zigbee (`Mesh`) можно закрепить за ядром 1 | ✅ подтверждено | Главную задачу стека создаёт **приложение** (`xTaskCreate` в примере); в .a библиотеки нет вызовов `xTaskCreate*` |
| 5 | Приоритеты акторов ниже системных WiFi/lwIP | ✅ реализуемо | WiFi 23, esp_timer 22, lwIP 18 (по умолчанию), `configMAX_PRIORITIES` = 25 |
| 6 | Стеки задач во внутренней RAM, код во время записи во флеш — во внутренней RAM | ✅ подтверждено | Документация IDF: во время записи во флеш PSRAM недоступна; `xTaskCreate` всегда берёт стек из внутренней RAM |
| 7 | Крупные буферы TLS/HTTP в PSRAM | ✅ подтверждено | `MBEDTLS_EXTERNAL_MEM_ALLOC` (зависит от `SPIRAM_USE_CAPS_ALLOC` или `SPIRAM_USE_MALLOC`) |
| 8 | Тесты ядер и симулятор на Linux-таргете ESP-IDF (AD-27, Structural Seed) | ✅ подтверждено, ⚠️ preview | `linux` входит в `PREVIEW_TARGETS`; Unity портирован; есть POSIX-порт FreeRTOS; покрытие `--coverage` + gcovr уже используется в самом IDF |
| 9 | Покрытие строк и ветвей ≥ 90 % | ✅ инструментально реализуемо | gcov/gcovr на хосте дают покрытие строк и ветвей; сама цифра — решение, а не факт |
| 10 | Ёмкость FRAM читается командой RDID (AD-28) | ✅ подтверждено, ⚠️ форматы разные | MB85RS2MTA: `04 7F 48 03`, плотность в битах [4:0] байта 3 = `01000b`; FM25V20A: `7F×6 C2 25 48`, плотность в битах [12:8] ID продукта = `00101b` |
| 11 | Семантика записи FRAM: без стирания, без ожидания, побайтно (основа порядка записи в AD-28) | ✅ подтверждено | Оба даташита: запись сразу после 8-го такта; FM25V20A: «при потере питания посреди записи будет записан только последний завершённый байт» |
| 12 | FRAM 256 КБ, SPI (Stack) | ✅ подтверждено | Обе микросхемы — 2 Мбит = 256 К × 8, SPI mode 0/3 |

## 1. ESP-IDF v6.0.3: ядра, приоритеты, сторожевой таймер (AD-26)

### 1.1 lwIP: привязка задачи tcpip
`components/lwip/Kconfig`, строки 908–925:
```
choice LWIP_TCPIP_TASK_AFFINITY
    default LWIP_TCPIP_TASK_AFFINITY_NO_AFFINITY
    ... CPU0 / CPU1 (depends on !FREERTOS_UNICORE)
    "Currently this applies to "TCP/IP" task and "Ping" task."
```
Приоритет: `LWIP_TCPIP_TASK_PRIO`, по умолчанию 18, диапазон 1–24 (строка 21).

**Вывод:** опция есть. Но формулировку «lwIP (привязан к ядру 0)» нужно подкрепить явной строкой в `sdkconfig.defaults` приложения `logic-s3`: `CONFIG_LWIP_TCPIP_TASK_AFFINITY_CPU0=y`. Без неё tcpip может выполняться на ядре 1, на тракте тревоги. Это предлагается внести в story 1.1 или в Rule AD-26.

### 1.2 WiFi: привязка задачи
`components/esp_wifi/Kconfig`, строки 227–237: `choice ESP_WIFI_TASK_CORE_ID` (depends on `!FREERTOS_UNICORE`), по умолчанию `ESP_WIFI_TASK_PINNED_TO_CORE_0`. **Подтверждено**, совпадает с AD-26 без доп. настройки.

### 1.3 Сторожевой таймер задач на обоих ядрах
`components/esp_system/Kconfig`, строки 295–356: `ESP_TASK_WDT_EN` = y, `ESP_TASK_WDT_INIT` = y, `ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0` = y, `ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1` = y (depends on `!FREERTOS_UNICORE`). **Подтверждено.** Следствие для AD-26: ядро 1 заполнено задачами высокого приоритета (`Mesh` > `Delivery` > `Storage` …). Если любая из них крутится без блокировки дольше `ESP_TASK_WDT_TIMEOUT_S` (5 с), сработает сторожевой таймер. Это желаемое поведение (голодание видно), но оболочки акторов (AD-27) обязаны блокироваться на очереди.

### 1.4 Системные приоритеты
`components/esp_system/include/esp_task.h`: `ESP_TASK_PRIO_MAX = configMAX_PRIORITIES` (25); `ESP_TASK_TIMER_PRIO = MAX-3` (22); `ESP_TASKD_EVENT_PRIO = MAX-5` (20); `ESP_TASK_TCPIP_PRIO = CONFIG_LWIP_TCPIP_TASK_PRIO` (18). WiFi — 23. Правило «приоритеты акторов ниже системных WiFi/lwIP» реализуемо: акторам остаётся диапазон 1–17.

Замечание: задача `esp_timer` по умолчанию закреплена за ядром 0 (`ESP_TIMER_TASK_AFFINITY_CPU0`); перенос на CPU1 помечен как experimental (`components/esp_timer/Kconfig`, строки 54–76). Если тракт тревоги на ядре 1 будет пользоваться колбэками `esp_timer`, их задержка зависит от загрузки ядра 0. Для оболочек акторов лучше таймауты очереди FreeRTOS (`xQueueReceive` с тайм-аутом).

### 1.5 esp-zigbee-lib 2.0.4: кто создаёт задачу хоста (ZBOSS) на S3 с RCP по UART
Проверено двумя способами.

- **Пример** `examples/zigbee_gateway/main/zigbee_gateway.c` (esp-zigbee-sdk, main, зависимость `espressif/esp-zigbee-lib >=2.0.0`). Задачу создаёт приложение:
  ```c
  xTaskCreate(esp_zigbee_stack_main_task, "Zigbee_main", 4096 * 2, NULL, 5, NULL);
  ```
  Внутри задачи по порядку: `esp_zigbee_init(&cfg)` (с `radio_mode = ESP_ZIGBEE_RADIO_MODE_UART_RCP`, UART1, 460800), затем `esp_zigbee_start()`, затем `esp_zigbee_launch_mainloop()` (блокирующий цикл).
- **Бинарь** `espressif__esp-zigbee-lib-v2.0.4.zip`, `lib/esp32s3/*.a`. В `xtensa-esp32s3-elf-nm` нет неопределённых символов `xTaskCreate*` / `xTaskCreatePinnedToCore` / `xTaskCreateStatic` ни в одной библиотеке. Радио-spinel (`esp_radio_spinel_*` из `components/openthread/src/spinel` в IDF) задач тоже не создаёт. UART ставится через `uart_driver_install` из задачи, вызвавшей `esp_zigbee_init`, поэтому прерывание UART окажется на том же ядре. Платформенный таймер (`esp_zigbee_plat_alarm.c`) опрашивается в цикле mainloop через `esp_timer_get_time` и не зависит от задачи `esp_timer` на ядре 0. `esp_timer_create` встречается только в `compat.c` (совместимость с API 1.x).

**Вывод: подтверждено.** Хост Zigbee можно закрепить за ядром 1 через `xTaskCreatePinnedToCore(..., 1)` в оболочке `Mesh`. Нюанс для AD-27: `esp_zigbee_launch_mainloop()` блокирует задачу. Поэтому оболочка `Mesh` не может быть обычным циклом «очередь → `step`». Вызовы стека из других задач требуют `esp_zigbee_lock_acquire/release` (`include/esp_zigbee.h`, строки 186–203), события стека приходят колбэками в задаче mainloop. Форма оболочки `Mesh` — отдельное проектное решение story про Mesh.

**Связанное наблюдение (вне технологии, но влияет на AD-6/AD-28):** `esp_zigbee_platform_config_t.storage_partition_name`, а в бинаре — `nvs_open_from_partition`, `nvs_set_blob`, `nvs_commit`. Стек сам хранит сетевые данные в NVS на флеше S3. Кроме того, NVS на логическом узле нужна для калибровки PHY/WiFi и для `network_provisioning`. Формулировка «к FRAM (логический) и NVS (коллектор) обращается только `Storage`» верна только для данных приложения. Стоит явно записать исключение «NVS стека Zigbee/WiFi — вне `Storage`». Также стоит учесть, что запись NVS стеком останавливает кэш флеша на обоих ядрах (см. 2.1).

**Перенесено из ревью 2026-09-29 (не закрыто):** README esp-zigbee-sdk по-прежнему рекомендует **ESP-IDF v5.5.4**. Строка Stack «ESP-IDF v6.0.x + esp-zigbee-lib 2.0.4» официально не подтверждена производителем. Нужна пробная сборка `zigbee_gateway` для esp32s3 на v6.0.3 (story 1.1).

## 2. PSRAM на ESP32-S3 N16R8 (octal)

### 2.1 Стеки задач и запись во флеш
`docs/en/api-guides/external-ram.rst`, строки 219–233:
> «When flash cache is disabled (for example, if the flash is being written to), the external RAM also becomes inaccessible… This is also the reason why ESP-IDF does not by default allocate any task stacks in external RAM.»
> «xTaskCreate and similar functions will always allocate internal memory for stack and task TCBs.»

`components/freertos/Kconfig`, строки 573–590: `FREERTOS_TASK_CREATE_ALLOW_EXT_MEM` (на S3 по умолчанию `y`) разрешает внешнюю память **только** для `xTaskCreateStatic` и «only for tasks where the stack is never accessed while the cache is disabled».

**Подтверждено.** Правило AD-26 «стеки задач во внутренней RAM» совпадает с поведением `xTaskCreate` по умолчанию. Его стоит закрепить запретом на `xTaskCreateStatic` с PSRAM-буфером в оболочках акторов. Буферы TLS/HTTP в PSRAM безопасны: задачи, которые к ним обращаются, во время записи во флеш приостановлены, а IRAM-обработчики прерываний к ним не обращаются.

Дополнительная опция: на S3 есть `SPIRAM_XIP_FROM_PSRAM` (= `SPIRAM_FETCH_INSTRUCTIONS` + `SPIRAM_RODATA`, `components/esp_psram/esp32s3/Kconfig.spiram`, строки 49–81). С ней код и константы копируются в PSRAM, и запись во флеш не отключает кэш для пользовательского кода (пример `system/xip_from_psram`). Включать её не обязательно, но она снимает часть ограничений «код во время записи во флеш — в IRAM». Спайн её не упоминает. Это вариант для прототипа, а не ошибка.

### 2.2 Octal PSRAM
`components/esp_psram/esp32s3/Kconfig.spiram`: `choice SPIRAM_MODE` → `SPIRAM_MODE_OCT` (строки 11–20). `flash_psram_config.rst`, строка 70: «Octal PSRAM only supports DTR mode». Для N16R8 (quad flash 16 МБ + octal PSRAM 8 МБ) нужны `CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_MODE_OCT=y`.

Аппаратное следствие для разводки шины FRAM (AD-6: «FRAM — на своей шине»): на модулях с octal PSRAM **GPIO35–37 заняты** под память и недоступны (источник: заметки Espressif по GPIO модулей S3-WROOM, см. ссылки). Пины SPI для FRAM выбирать вне GPIO26–37.

### 2.3 mbedTLS во внешней памяти
`components/mbedtls/Kconfig`, строки 166–200: `choice MBEDTLS_MEM_ALLOC_MODE`, по умолчанию `MBEDTLS_INTERNAL_MEM_ALLOC`; `MBEDTLS_EXTERNAL_MEM_ALLOC` depends on `SPIRAM_USE_CAPS_ALLOC || SPIRAM_USE_MALLOC`. В справке сказано, что на S2/S3 при включённом flash encryption содержимое PSRAM шифруется, поэтому внешняя аллокация безопасна. **Подтверждено.** Для `sdkconfig.defaults`: `CONFIG_SPIRAM_USE_CAPS_ALLOC=y` (или `USE_MALLOC`) + `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y`.

## 3. Linux-таргет ESP-IDF для тестов (AD-27)

- **Статус:** `tools/idf_py_actions/constants.py:53` — `PREVIEW_TARGETS = ['linux', 'esp32h21', 'esp32h4']`. Сборка: `idf.py --preview set-target linux` (`docs/en/api-guides/host-apps.rst`, строка 79). **Preview, не stable.**
- **POSIX-порт FreeRTOS:** есть `components/freertos/FreeRTOS-Kernel/portable/linux/` (`port.c`, `port_idf.c`) и `FreeRTOS-Kernel-SMP/portable/linux/`. Ограничения (`host-apps.rst`, строки 45–62): симуляция однопоточная даже с SMP-ядром; используются сигналы POSIX; нельзя вызывать `printf` из задач разного приоритета; блокирующие системные вызовы планировщик видит как «готовые».
- **Unity:** `components/unity/CMakeLists.txt` для `linux` собирает `unity_port_linux.c` и `port/linux/unity_utils_memory_linux.c`. **Подтверждено.**
- **gcov:** в самом IDF есть прецедент, `components/nvs_flash/host_test/nvs_page_test` (таргет linux, Unity):
  ```cmake
  target_compile_options(${COMPONENT_LIB} PUBLIC --coverage)
  target_link_libraries(${COMPONENT_LIB} --coverage)
  add_custom_target(coverage ... COMMAND gcovr --root ... --html-details ...)
  ```
  **Подтверждено:** хост-приложение с Unity и покрытием gcov/gcovr на Linux-таргете собирается штатно.

**Замечание по согласованности AD-27 и Structural Seed.** AD-27 требует, чтобы ядра собирались «отдельной целью без заголовков IDF и FreeRTOS». `test/host/` размещает тесты и симулятор «на Linux-таргете ESP-IDF». Противоречия нет, если ядра — компоненты без `REQUIRES freertos`, а симулятор сети не использует POSIX-FreeRTOS. Детерминизм по seed несовместим с планировщиком на сигналах. Рекомендация: симулятор вызывает `step()` из собственного цикла событий, а FreeRTOS на хосте применяется только для тестов оболочек, если такие тесты понадобятся. Можно рассмотреть и чистый CMake/CTest без IDF: ядра от IDF не зависят, а preview-статус Linux-таргета тогда не влияет на CI.

## 4. FRAM: MB85RS2MTA и Infineon FM25V20A (AD-28, Stack)

### 4.1 RDID (0x9F) и определение ёмкости
**MB85RS2MTA** (RAMXEED, DS6v1, раздел RDID): 32 такта, 4 байта: Manufacturer ID `04h` (RAMXEED), Continuation `7Fh`, Product ID 1 `48h` (биты [4:0] — плотность `01000b` = 2 Мбит), Product ID 2 `03h`.

**FM25V20A** (Infineon, раздел 5.5, табл. 7): 9 байт: `7F 7F 7F 7F 7F 7F C2` (банк 7 JEDEC, Ramtron), затем Product ID `25 48h`: Family [15:13] = `001`, **Density [12:8] = `00101b`**, Sub [7:6] = `01`, Rev [5:3] = `001`.

**Вывод: подтверждено с оговоркой.** Обе микросхемы отдают плотность в RDID, поэтому ёмкость определить можно. Но:
- длина ответа разная: 4 байта против 9. Читать 9 байт и определять производителя по первым байтам: `04 7F` — RAMXEED/Fujitsu, `7F…7F C2` — Infineon/Cypress/Ramtron;
- поле и кодировка плотности разные: 2 Мбит = `01000b` у RAMXEED и `00101b` у Infineon;
- поэтому драйвер `Storage` держит **таблицу известных пар (производитель, плотность) → байты**, а неизвестный ID обрабатывает как отказ «здоровье» и не раскладывает регионы наугад. Это стоит одной фразой добавить в AD-28.

Дополнительно: старшие биты адреса у MB85RS2MTA игнорируются («The 6-bit upper address bit is invalid»), адрес при переполнении сворачивается к нулю. Ошибка определения ёмкости даст тихую перезапись начала FRAM, то есть суперблока. Это ещё один довод за строгую таблицу ID.

### 4.2 Семантика записи
- MB85RS2MTA: «does not take long time to write data like Flash memories or E2PROM, and … takes no wait time»; «When 8 bits of writing data is input, data is written to FeRAM memory cell array»; ресурс 10¹⁴ циклов на байт (в одном месте текста — 10¹³, в Features — 10¹⁴).
- FM25V20A: «No write delays are incurred. Data is written to the memory array immediately after each byte is successfully transferred»; «F-RAM memories do not have page buffers…»; **«If the power is lost in the middle of the write operation, only the last completed byte will be written.»**; ресурс 10¹⁴.
- Стирания нет ни у одной, команд erase в наборе нет.

**Подтверждено.** Это прямо обосновывает порядок записи AD-28: тело с CRC, затем указатель в двух копиях с номером и CRC. Обрыв даёт префикс полностью записанных байтов, рваная запись многобайтового указателя закрывается второй копией. Пауз ожидания записи и выравнивания по страницам не нужно.

Отличие, важное для драйвера: у **FM25V20A** WEL сбрасывается по фронту CS после каждого WRITE/WRSR, поэтому WREN нужен перед каждой записью. У **MB85RS2MTA** есть «continual programming mode»: WEL после WRITE **не** сбрасывается. Переносимый драйвер обязан слать WREN перед каждым WRITE. Можно добавить и WRDI после записи, это защищает от случайной записи помехой на шине.

Частота SPI: выбранный в даташите вариант FM25V20A с расширенным температурным диапазоном — **33 МГц**, промышленный — 40 МГц; MB85RS2MTA — 40 МГц. Константа частоты в `ga_config.h` должна быть ≤ 33 МГц, если допускаются оба варианта.

### 4.3 Доступность
MB85RS2MTA: даташит обновлялся в 2026 (DS6v2 J, 04.2026). FM25V20A-G у дистрибьюторов имеет статус Active. Пометку NRND в найденных источниках не найдено, но и на сайте Infineon не проверено.

### 4.4 Таблица регионов при 256 КБ
Арифметика: 512 + 256 Б + 96 + 3 + 32 + 8 + 48 + 24 + 4 КБ = 215 КБ + 768 Б, то есть ≈ 40 КБ резерва. Укладывается в 256 КБ. Технологических ограничений FRAM на такую разметку нет: выравнивание и границы страниц не нужны.

## Что внести (предложения, спайн не правился)

| # | Куда | Что |
|---|---|---|
| P1 | AD-26 / story 1.1 | Явно: `CONFIG_LWIP_TCPIP_TASK_AFFINITY_CPU0=y` (по умолчанию NO_AFFINITY) |
| P2 | AD-28 | RDID разбирается по таблице известных ID (длина 4/9 байт, поле плотности различается); неизвестный ID — отказ, а не раскладка |
| P3 | AD-6 / AD-28 | Исключение: NVS стека Zigbee, WiFi и `network_provisioning` на логическом узле — вне `Storage` |
| P4 | Драйвер FRAM | WREN перед каждым WRITE (семантика WEL у двух микросхем разная); SPI ≤ 33 МГц |
| P5 | AD-27 / test/host | Симулятор сети — без POSIX-FreeRTOS (детерминизм); Linux-таргет — preview |
| P6 | story 1.1 | Пробная сборка esp-zigbee-lib 2.0.4 на IDF v6.0.3 для esp32s3: производитель рекомендует v5.5.4 (перенесено из ревью 2026-09-29) |
| P7 | Разводка | SPI-пины FRAM вне GPIO26–37 на N16R8 (octal PSRAM занимает GPIO35–37) |

## Источники

Локальные (ESP-IDF v6.0.3, `~/esp/v6.0.3/esp-idf`):
- `components/lwip/Kconfig` (стр. 21, 908–925)
- `components/esp_wifi/Kconfig` (стр. 227–237)
- `components/esp_system/Kconfig` (стр. 295–356); `components/esp_system/include/esp_task.h`
- `components/esp_timer/Kconfig` (стр. 54–76)
- `components/freertos/Kconfig` (стр. 573–590); `components/freertos/FreeRTOS-Kernel/portable/linux/`
- `components/esp_psram/esp32s3/Kconfig.spiram`; `docs/en/api-guides/external-ram.rst` (стр. 152–233); `docs/en/api-guides/flash_psram_config.rst`
- `components/mbedtls/Kconfig` (стр. 166–200)
- `docs/en/api-guides/host-apps.rst`; `tools/idf_py_actions/constants.py:53`; `components/unity/CMakeLists.txt`
- `components/nvs_flash/host_test/nvs_page_test/{CMakeLists.txt,main/CMakeLists.txt,README.md,sdkconfig.defaults}`
- `components/openthread/src/spinel/` (esp_radio_spinel)

Внешние:
- esp-zigbee-lib 2.0.4, реестр компонентов: https://components.espressif.com/components/espressif/esp-zigbee-lib (архив https://components-file.espressif.com/components/espressif/esp-zigbee-lib/2.0.4/espressif__esp-zigbee-lib-v2.0.4.zip, релиз 2026-08-14)
- Пример zigbee_gateway: https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/zigbee_gateway
- README esp-zigbee-sdk (рекомендуемый IDF v5.5.4): https://github.com/espressif/esp-zigbee-sdk
- MB85RS2MTA DS6v1: https://www.ramxeed.com/assets/images/products/datasheet/FeRAM/s3/MB85RS2MTA-DS6v1-E.pdf
- MB85RS2MTA DS6v2 J (04.2026): https://www.ramxeed.com/jp/home/wp-content/uploads/2026/04/MB85RS2MTA-DS6v2-J.pdf
- FM25V20A (Infineon): https://www.infineon.com/assets/row/public/documents/10/49/infineon-fm25v20a-2-mbit-256-k-8-serial-spi-f-ram-serial-spi-256-k-8-33-mhz-extended-industrial-datasheet-en.pdf
- FM25V20A-G, страница продукта: https://infineon.com/cms/de/product/memories/f-ram-ferroelectric-ram/fm25v20a-g ; статус у дистрибьютора: https://www.futureelectronics.com/p/2055460
- GPIO35–37 при octal PSRAM на S3-WROOM: https://basic-starter-kit-for-esp32-s3-wroom.readthedocs.io/en/latest/_sources/preparation/Notes_For_GPIO.rst.txt
