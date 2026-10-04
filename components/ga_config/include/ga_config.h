/*
 * ga_config.h -- the single source of every timeout, interval, buffer size and
 * other tunable constant of garageAlarms 2.0 (AD-25). Both firmwares build it.
 * Literal values of such constants anywhere else in the code are forbidden.
 *
 * Units, stated once for the whole file:
 *   - every duration is in MILLISECONDS, an unsigned 32-bit value (suffix U,
 *     same width on target and host; at most ~49.7 days). Never store one in a
 *     signed 32-bit variable: GA_TOMBSTONE_TTL exceeds INT32_MAX;
 *   - every size is in BYTES;
 *   - counts and percentages are dimensionless and say so in their comment.
 * Each macro repeats its unit in a comment.
 *
 * Constants the soft config may override are marked
 * "default, soft-overridable"; the value here is then only the default.
 *
 * Start values come from _bmad-output/specs/spec-garage-alarms-2/tunables.md
 * and are refined on the prototype. Plain C: no ESP-IDF or FreeRTOS includes,
 * so actor core/ code and host tests can use this header (AD-27).
 */
#ifndef GA_CONFIG_H
#define GA_CONFIG_H

/* Version of this file; carried in heartbeat, mismatch between nodes is a
 * health event (AD-25). Integer, bump on every change of a value below. */
#define GA_CONFIG_VERSION 1

#if defined(__cplusplus)
#define GA_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define GA_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

/* ------------------------------------------------------------------------- */
/* Network, leadership, delivery                                              */
/* ------------------------------------------------------------------------- */

/* T: follower backs up a critical event after this long. ms */
#define GA_CRIT_FOLLOWER_T          (45U * 1000U)
/* N: uplink considered lost after this long of failures. ms */
#define GA_UPLINK_LOST              (60U * 1000U)
/* M: uplink considered restored after this long of success. ms */
#define GA_UPLINK_RESTORED          (120U * 1000U)
/* Leadership returns to the priority node after this long of stable uplink. ms */
#define GA_LEADER_RETURN            (15U * 60U * 1000U)
/* Pause after start before leadership and getUpdates. ms */
#define GA_JOIN_GRACE               (30U * 1000U)
/* Heartbeat period of a logic node. ms */
#define GA_HB_LOGIC                 (10U * 1000U)
/* Heartbeat period of a collector. ms */
#define GA_HB_COLLECTOR             (30U * 1000U)
/* Node or channel becomes stale after this many missed heartbeats. count */
#define GA_STALE_MISSED_HB          3U
/* Telegram lease renew period. ms */
#define GA_LEASE_RENEW              (5U * 60U * 1000U)
/* Telegram lease lifetime. ms */
#define GA_LEASE_TTL                (15U * 60U * 1000U)
/* YIELDED pause without a lease; actual pause = GA_YIELD_BASE * (1 + rank). ms */
#define GA_YIELD_BASE               (30U * 1000U)
/* Anti-replay seq window per (node, boot). count of seq numbers */
#define GA_SEQ_WINDOW               512U
/* Stored boot windows per node for dedup. count */
#define GA_DEDUP_BOOTS              4U
/* Wait for ACK from all nodes. ms */
#define GA_ACK_ALL_TIMEOUT          (5U * 60U * 1000U)
/* Retries before a node is considered deaf. count */
#define GA_DEAF_MIN_RETRIES         10U
/* HMAC key (kid) rotation transition period. ms (7 days) */
#define GA_KID_TRANSITION           (7U * 24U * 60U * 60U * 1000U)
/* Maximum of one Zigbee frame (NFR5). bytes */
#define GA_ZB_FRAME_MAX_BYTES       80U

/* ------------------------------------------------------------------------- */
/* Buffers and memory                                                         */
/* ------------------------------------------------------------------------- */

/* Critical reserve in the collector buffer. count of events */
#define GA_COLLECTOR_CRIT_RESERVE       32U
/* Critical edges per channel before they are merged. count */
#define GA_COLLECTOR_CRIT_PER_CHANNEL   4U
/* Collector NVS write budget. count of writes per day */
#define GA_COLLECTOR_NVS_BUDGET         2000U
/* Critical quota of the outbox. percent of the queue */
#define GA_OUTBOX_CRIT_QUOTA            25U
/* Chunk of a Storage background write. bytes */
#define GA_STORAGE_CHUNK                512U
/* Maximum duration of one flash write operation. ms */
#define GA_FLASH_OP_MAX_MS              20U
/* Allowed lateness of a timeout after next_deadline. ms */
#define GA_TICK_JITTER                  50U

/* FRAM regions, start layout for a 256 KB chip (AD-28). Used only at first
 * format; afterwards the layout is defined by the superblock. bytes */
#define GA_FRAM_SIZE                (256U * 1024U) /* whole chip. bytes */
#define GA_FRAM_SUPERBLOCK          256U            /* size of one copy; GA_FRAM_SUPERBLOCK_COPIES copies (A/B). bytes */
#define GA_FRAM_SUPERBLOCK_COPIES   2U               /* count */
#define GA_FRAM_SYS_STATE           256U            /* Sys: boot, reboot notify times. bytes */
#define GA_FRAM_INBOX               (96U * 1024U)  /* Delivery: inbound journal = event history. bytes */
#define GA_FRAM_DEDUP               (3U * 1024U)   /* Delivery: (node, boot) -> seq window. bytes */
#define GA_FRAM_OUTBOX              (32U * 1024U)  /* Notify: outbox and delivery units. bytes */
#define GA_FRAM_NOTIFY_STATE        (8U * 1024U)   /* Notify: offset, update_id, receipts, index. bytes */
#define GA_FRAM_CONFIG              (48U * 1024U)  /* Config: soft config journal. bytes */
#define GA_FRAM_MQTT                (24U * 1024U)  /* Notify: MQTT buffer. bytes */
#define GA_FRAM_STATS               (4U * 1024U)   /* Membership: 7-day statistics. bytes */
/* The remainder of the chip is the migration reserve. */

GA_STATIC_ASSERT(GA_FRAM_SUPERBLOCK * GA_FRAM_SUPERBLOCK_COPIES + GA_FRAM_SYS_STATE +
                     GA_FRAM_INBOX + GA_FRAM_DEDUP + GA_FRAM_OUTBOX + GA_FRAM_NOTIFY_STATE +
                     GA_FRAM_CONFIG + GA_FRAM_MQTT + GA_FRAM_STATS <= GA_FRAM_SIZE,
                 "FRAM regions exceed the chip");

/* Task map (AD-26): core, priority, stack of every task. Start values, tuned in
 * story 2.1. Binding happens only in the actor shell (AD-27).
 * S3: core 0 = outside world (WiFi, lwIP, Notify, Ui, Sys);
 *     core 1 = alarm path (Mesh incl. Zigbee stack task, Delivery, Storage,
 *              Membership, Sensor, Rules, Config).
 * Collector (ESP32-H2, single core) ignores the _CORE fields.
 * Priorities: Mesh > Delivery > Storage > Membership = Sensor > Rules > Config
 *             > Notify > Ui > Sys, all below the lwIP task.
 * _CORE: core index; _PRIO: FreeRTOS priority; _STACK: bytes. */

/* lwIP tcpip task priority (ESP-IDF default CONFIG_LWIP_TCPIP_TASK_PRIO);
 * reference ceiling for the asserts below, not applied by this header. */
#define GA_TASK_LWIP_PRIO           18

#define GA_TASK_ZIGBEE_CORE         1      /* Zigbee stack task (part of Mesh) */
#define GA_TASK_ZIGBEE_PRIO         12
#define GA_TASK_ZIGBEE_STACK        8192U  /* bytes */

#define GA_TASK_MESH_CORE           1
#define GA_TASK_MESH_PRIO           12
#define GA_TASK_MESH_STACK          4096U  /* bytes */

#define GA_TASK_DELIVERY_CORE       1
#define GA_TASK_DELIVERY_PRIO       11
#define GA_TASK_DELIVERY_STACK      4096U  /* bytes */

#define GA_TASK_STORAGE_CORE        1
#define GA_TASK_STORAGE_PRIO        10
#define GA_TASK_STORAGE_STACK       4096U  /* bytes */

#define GA_TASK_MEMBERSHIP_CORE     1
#define GA_TASK_MEMBERSHIP_PRIO     9
#define GA_TASK_MEMBERSHIP_STACK    4096U  /* bytes */

#define GA_TASK_SENSOR_CORE         1
#define GA_TASK_SENSOR_PRIO         9
#define GA_TASK_SENSOR_STACK        4096U  /* bytes */

#define GA_TASK_RULES_CORE          1      /* logic node only */
#define GA_TASK_RULES_PRIO          8
#define GA_TASK_RULES_STACK         4096U  /* bytes */

#define GA_TASK_CONFIG_CORE         1      /* logic node only */
#define GA_TASK_CONFIG_PRIO         7
#define GA_TASK_CONFIG_STACK        4096U  /* bytes */

#define GA_TASK_NOTIFY_CORE         0      /* logic node only */
#define GA_TASK_NOTIFY_PRIO         6
#define GA_TASK_NOTIFY_STACK        8192U  /* bytes */

#define GA_TASK_UI_CORE             0      /* logic node only */
#define GA_TASK_UI_PRIO             5
#define GA_TASK_UI_STACK            8192U  /* bytes */

#define GA_TASK_SYS_CORE            0
#define GA_TASK_SYS_PRIO            4
#define GA_TASK_SYS_STACK           4096U  /* bytes */

GA_STATIC_ASSERT(GA_TASK_MESH_PRIO < GA_TASK_LWIP_PRIO && GA_TASK_ZIGBEE_PRIO < GA_TASK_LWIP_PRIO,
                 "Mesh must stay below lwIP");
GA_STATIC_ASSERT(GA_TASK_ZIGBEE_PRIO == GA_TASK_MESH_PRIO, "Zigbee stack task is part of Mesh");
GA_STATIC_ASSERT(GA_TASK_ZIGBEE_CORE <= 1 && GA_TASK_MESH_CORE <= 1 && GA_TASK_DELIVERY_CORE <= 1 &&
                     GA_TASK_STORAGE_CORE <= 1 && GA_TASK_MEMBERSHIP_CORE <= 1 &&
                     GA_TASK_SENSOR_CORE <= 1 && GA_TASK_RULES_CORE <= 1 && GA_TASK_CONFIG_CORE <= 1 &&
                     GA_TASK_NOTIFY_CORE <= 1 && GA_TASK_UI_CORE <= 1 && GA_TASK_SYS_CORE <= 1,
                 "task core must be 0 or 1");
GA_STATIC_ASSERT(GA_TASK_MESH_PRIO > GA_TASK_DELIVERY_PRIO &&
                     GA_TASK_DELIVERY_PRIO > GA_TASK_STORAGE_PRIO &&
                     GA_TASK_STORAGE_PRIO > GA_TASK_MEMBERSHIP_PRIO &&
                     GA_TASK_MEMBERSHIP_PRIO == GA_TASK_SENSOR_PRIO &&
                     GA_TASK_SENSOR_PRIO > GA_TASK_RULES_PRIO &&
                     GA_TASK_RULES_PRIO > GA_TASK_CONFIG_PRIO &&
                     GA_TASK_CONFIG_PRIO > GA_TASK_NOTIFY_PRIO &&
                     GA_TASK_NOTIFY_PRIO > GA_TASK_UI_PRIO &&
                     GA_TASK_UI_PRIO > GA_TASK_SYS_PRIO,
                 "task priorities must follow AD-26 order");

/* ------------------------------------------------------------------------- */
/* Rules, episodes, commands                                                  */
/* ------------------------------------------------------------------------- */

/* Maximum window of a rule. ms */
#define GA_RULE_WINDOW_MAX          (60U * 60U * 1000U)
/* Episode: pause before a new alarm after normal or ack. ms */
#define GA_ALARM_REARM              (10U * 60U * 1000U)
/* Episode: grace for late events. ms */
#define GA_EPISODE_GRACE            (60U * 1000U)
/* Episode: maximum length (<= GA_RULE_WINDOW_MAX). ms */
#define GA_EPISODE_MAX              (60U * 60U * 1000U)
/* Notify idempotency margin. ms */
#define GA_NOTIFY_IDEMP_MARGIN      (15U * 60U * 1000U)
/* Executed Telegram update_id values kept in memory. count */
#define GA_TG_EXECUTED_IDS          64U
/* Maximum age of a Telegram command. ms */
#define GA_CMD_MAX_AGE              (10U * 60U * 1000U)
/* Lifetime of a config tombstone. ms (30 days) */
#define GA_TOMBSTONE_TTL            (30U * 24U * 60U * 60U * 1000U)
/* Reminder of a lasting smoke alarm. ms */
#define GA_SMOKE_REMIND             (15U * 60U * 1000U)      /* default, soft-overridable */
/* PIR stuck active this long -> fault. ms */
#define GA_PIR_STUCK                (2U * 60U * 60U * 1000U) /* default, soft-overridable */
/* Critical rule disable: default duration. ms */
#define GA_CRIT_DISABLE_DEFAULT     (2U * 60U * 60U * 1000U) /* default, soft-overridable */
/* Critical rule disable: maximum duration. ms */
#define GA_CRIT_DISABLE_MAX         (12U * 60U * 60U * 1000U)
/* Debounce of smoke and dry-contact inputs. ms */
#define GA_ALARM_DEBOUNCE           200U                        /* default, soft-overridable */
/* Cooldown of smoke and dry-contact inputs. ms */
#define GA_ALARM_COOLDOWN           (2U * 60U * 1000U)        /* default, soft-overridable */
/* Debounce of PIR inputs. ms */
#define GA_MOTION_DEBOUNCE          400U                        /* default, soft-overridable */
/* Cooldown of PIR inputs. ms */
#define GA_MOTION_COOLDOWN          (5U * 60U * 1000U)        /* default, soft-overridable */
/* PIR warm-up after power-on. ms */
#define GA_PIR_WARMUP               (60U * 1000U)              /* default, soft-overridable */

GA_STATIC_ASSERT(GA_EPISODE_MAX <= GA_RULE_WINDOW_MAX, "episode longer than rule window");
GA_STATIC_ASSERT(GA_TOMBSTONE_TTL == 30ULL * 24ULL * 60ULL * 60ULL * 1000ULL,
                 "GA_TOMBSTONE_TTL overflowed 32 bits");
GA_STATIC_ASSERT(GA_CRIT_DISABLE_DEFAULT <= GA_CRIT_DISABLE_MAX, "disable default above maximum");

/* ------------------------------------------------------------------------- */
/* External channels and interfaces                                           */
/* ------------------------------------------------------------------------- */

/* Unexpected reboot notification: at most one per this interval. ms */
#define GA_REBOOT_NOTIFY_UNEXPECTED (15U * 60U * 1000U)
/* Planned reboot notification: at most one per this interval. ms */
#define GA_REBOOT_NOTIFY_PLANNED    (6U * 60U * 60U * 1000U)
/* Daily "alive" report time: offset from local midnight (09:00). ms */
#define GA_DAILY_REPORT_TIME        (9U * 60U * 60U * 1000U) /* default, soft-overridable */
/* Gap between Telegram sends. ms */
#define GA_TG_SEND_GAP              350U
/* Pushover emergency retry. ms */
#define GA_PUSHOVER_RETRY           (60U * 1000U)              /* default, soft-overridable */
/* Pushover emergency expire. ms */
#define GA_PUSHOVER_EXPIRE          (3U * 60U * 60U * 1000U) /* default, soft-overridable */
/* Confirmation of a dangerous bot command. ms */
#define GA_CONFIRM_TTL              (2U * 60U * 1000U)
/* Step of the alarm-disable duration picker. ms */
#define GA_DISABLE_STEP             (30U * 60U * 1000U)
/* Events per bot page. count */
#define GA_BOT_EVENTS_PAGE          10U
/* Web summary refresh (allowed range 5-10 s). ms */
#define GA_WEB_REFRESH              (5U * 1000U)
/* Web login: attempts before lockout. count */
#define GA_WEB_LOGIN_ATTEMPTS       5U
/* Web login: lockout duration. ms */
#define GA_WEB_LOGIN_LOCKOUT        (5U * 60U * 1000U)
/* New node key pack lifetime (single use). ms */
#define GA_KEYPACK_TTL              (30U * 60U * 1000U)
/* Collector provisioning window. ms */
#define GA_PROVISION_WINDOW         (5U * 60U * 1000U)
/* Owner statistics depth. count of daily buckets (days) */
#define GA_STATS_DAYS               7U

GA_STATIC_ASSERT(GA_LEASE_RENEW < GA_LEASE_TTL, "lease must be renewed before it expires");
GA_STATIC_ASSERT(GA_UPLINK_LOST < GA_UPLINK_RESTORED, "uplink hysteresis: lost must be shorter than restored");
GA_STATIC_ASSERT(GA_WEB_REFRESH >= 5U * 1000U && GA_WEB_REFRESH <= 10U * 1000U, "web refresh outside 5-10 s");
GA_STATIC_ASSERT(GA_DAILY_REPORT_TIME < 24U * 60U * 60U * 1000U, "daily report time must be within a day");

#endif /* GA_CONFIG_H */
