#include "FosVbat.h"
#include "esp_attr.h"
#include "esp_rom_sys.h"
#include "soc/soc.h"
#include "soc/pmu_reg.h"
#include "hal/vbat_ll.h"

static DRAM_ATTR uint32_t latch_failures;
uint32_t fos_vbat_mode(void) {
    return REG_GET_FIELD(PMU_VDDBAT_CFG_REG, PMU_ANA_VDDBAT_MODE);
}
uint32_t fos_vbat_latch_failures(void) { return latch_failures; }
bool IRAM_ATTR fos_vbat_apply_power(bool deep_sleep) {
    // Never charge the installed coin cell. Both charger switches must be off.
    vbat_ll_start_battery_charge(false);
    // Espressif's AUTO recipe, with the LP sleep bank prepared for VBAT.
    REG_SET_FIELD(PMU_HP_SLEEP_LP_DIG_POWER_REG, PMU_HP_SLEEP_VDDBAT_MODE, 2);
    REG_SET_FIELD(PMU_HP_SLEEP_LP_DIG_POWER_REG, PMU_HP_SLEEP_BOD_SOURCE_SEL, 0);
    REG_SET_FIELD(PMU_LP_SLEEP_LP_DIG_POWER_REG, PMU_LP_SLEEP_VDDBAT_MODE, deep_sleep ? 1 : 2);
    REG_SET_FIELD(PMU_LP_SLEEP_LP_DIG_POWER_REG, PMU_LP_SLEEP_BOD_SOURCE_SEL, deep_sleep ? 1 : 0);
    REG_SET_FIELD(PMU_SLP_WAKEUP_CNTL5_REG, PMU_LP_ANA_WAIT_TARGET, 0xFF);
    REG_SET_BIT(PMU_VDDBAT_CFG_REG, PMU_VDDBAT_SW_UPDATE);
    // Espressif's example waits forever. Keep the product bootable on failure.
    for (unsigned wait = 0; wait < 1000; ++wait) {
        if (REG_GET_FIELD(PMU_VDDBAT_CFG_REG, PMU_ANA_VDDBAT_MODE) == 2) return true;
        esp_rom_delay_us(1);
    }
    ++latch_failures;
    return false;
}
