#ifndef CONFIG_H
#define CONFIG_H

/* Fijados por el enunciado */
#define SIMU_PERIOD_MS        100     /* 10 Hz */
#define TEST_PERIOD_MS        2000
#define NORMAL_REPORT_MS      5000
#define STATS_WINDOW_S        30
#define SIMU_BAUDRATE         115200

/* Sugeridos por el enunciado, a decidir */
#define GPS_PERIOD_MS         1000
#define LIGHT_PERIOD_MS       2000    /* sugerido: 2-5 s */
#define ENV_PERIOD_MS         2000    /* sugerido: 2-5 s */

/* LDR: se rellenan al calibrar (paso posterior) */
#define LDR_RAW_DARK          0       /* PENDIENTE: medir en oscuridad */
#define LDR_RAW_BRIGHT        4095    /* PENDIENTE: medir con luz maxima */

#endif /* CONFIG_H */