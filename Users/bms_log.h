#ifndef __BMS_LOG_H
#define __BMS_LOG_H

#include "stdio.h"

/* Global and module log switches. */
#define BMS_LOG_ENABLE                 1U
#define BMS_LOG_ERROR_ENABLE           1U
#define BMS_LOG_HW_FAULT_ENABLE        1U
#define BMS_LOG_RUNTIME_ENABLE         1U

#define BMS_LOG_PROTECT_ENABLE         0U
#define BMS_LOG_BALANCE_ENABLE         0U
#define BMS_LOG_CAN_ENABLE             0U
#define BMS_LOG_PERIODIC_ENABLE        1U

#define BMS_LOG_TEST_ENABLE            1U

/*
 * Temporary integration tests. These remain enabled to preserve the current
 * ALERT simulation behavior until the real ALERT/SYS_STAT path replaces it.
 */
#define BMS_TEST_FAKE_ALERT_EXTI       0U
#define BMS_TEST_FAKE_HW_FAULT         0U

#if ((BMS_LOG_ENABLE != 0U) && (BMS_LOG_ERROR_ENABLE != 0U))
#define BMS_LOG_ERROR(...)             printf(__VA_ARGS__)
#else
#define BMS_LOG_ERROR(...)             ((void)0)
#endif

#if ((BMS_LOG_ENABLE != 0U) && (BMS_LOG_HW_FAULT_ENABLE != 0U))
#define BMS_LOG_HW_FAULT(...)          printf(__VA_ARGS__)
#else
#define BMS_LOG_HW_FAULT(...)          ((void)0)
#endif

#if ((BMS_LOG_ENABLE != 0U) && (BMS_LOG_RUNTIME_ENABLE != 0U))
#define BMS_LOG_RUNTIME(...)           printf(__VA_ARGS__)
#else
#define BMS_LOG_RUNTIME(...)           ((void)0)
#endif

#if ((BMS_LOG_ENABLE != 0U) && (BMS_LOG_PROTECT_ENABLE != 0U))
#define BMS_LOG_PROTECT(...)           printf(__VA_ARGS__)
#else
#define BMS_LOG_PROTECT(...)           ((void)0)
#endif

#if ((BMS_LOG_ENABLE != 0U) && (BMS_LOG_BALANCE_ENABLE != 0U))
#define BMS_LOG_BALANCE(...)           printf(__VA_ARGS__)
#else
#define BMS_LOG_BALANCE(...)           ((void)0)
#endif

#if ((BMS_LOG_ENABLE != 0U) && (BMS_LOG_CAN_ENABLE != 0U))
#define BMS_LOG_CAN(...)               printf(__VA_ARGS__)
#else
#define BMS_LOG_CAN(...)               ((void)0)
#endif

#if ((BMS_LOG_ENABLE != 0U) && (BMS_LOG_PERIODIC_ENABLE != 0U))
#define BMS_LOG_PERIODIC(...)          printf(__VA_ARGS__)
#else
#define BMS_LOG_PERIODIC(...)          ((void)0)
#endif

#if ((BMS_LOG_ENABLE != 0U) && (BMS_LOG_TEST_ENABLE != 0U) && \
     (BMS_TEST_FAKE_ALERT_EXTI != 0U))
#define BMS_LOG_TEST_ALERT(...)        printf(__VA_ARGS__)
#else
#define BMS_LOG_TEST_ALERT(...)        ((void)0)
#endif

#if ((BMS_LOG_ENABLE != 0U) && (BMS_LOG_TEST_ENABLE != 0U) && \
     (BMS_TEST_FAKE_HW_FAULT != 0U))
#define BMS_LOG_TEST_HW_FAULT(...)     printf(__VA_ARGS__)
#else
#define BMS_LOG_TEST_HW_FAULT(...)     ((void)0)
#endif

#endif
