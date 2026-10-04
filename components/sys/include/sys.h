/*
 * sys.h -- actor Sys: OTA, watchdog, time, self-diagnostics, provisioning, boot banner.
 * Owning ADs: AD-7, AD-13, AD-17, AD-18, AD-20, AD-23, AD-26. Actor itself is a
 * stub: shape core/ port/ shell/ test/ only (AD-27).
 */
#ifndef GA_SYS_H
#define GA_SYS_H

/*
 * Log one identity line at boot: role, EUI-64, firmware version, GA_CONFIG_VERSION.
 * role: "logic" or "collector".
 * On ESP32-H2 the EUI-64 is the node's own 802.15.4 address. On ESP32-S3 (no
 * 802.15.4; the real EUI-64 belongs to the H2 RCP, AD-7) a provisional value is
 * derived from the base MAC and tagged "(provisional)"; it is a log value only.
 */
void sys_log_banner(const char *role);

#endif /* GA_SYS_H */
