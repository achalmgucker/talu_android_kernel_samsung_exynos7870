#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/smc.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/pmu-cp.h>

#if defined(CONFIG_CP_SECURE_BOOT)
static u32 exynos_smc_read(enum cp_control reg)
{
	u32 cp_ctrl;

	cp_ctrl = exynos_smc(SMC_ID, READ_CTRL, 0, reg);
	if (!(cp_ctrl & 0xffff)) {
		cp_ctrl >>= 16;
	} else {
		pr_err("%s: ERROR! read failed: %d\n", __func__, cp_ctrl & 0xffff);

		return -1;
	}

	return cp_ctrl;
}

static u32 exynos_smc_write(enum cp_control reg, u32 value)
{
	int ret = 0;

	ret = exynos_smc(SMC_ID, WRITE_CTRL, value, reg);
	if (ret > 0) {
		pr_err("%s: ERROR! CP_CTRL write failed: %d\n", __func__, ret);
		return -1;
	}

	return 0;
}
#endif

/* reset cp */
int exynos_cp_reset(void)
{
	int ret = 0;
	u32 __maybe_unused cp_ctrl;

	pr_info("%s\n", __func__);

	/* set sys_pwr_cfg registers */
	exynos_sys_powerdown_conf_cp();

#if defined(CONFIG_CP_SECURE_BOOT)
	/* assert cp_reset */
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_RESET_SET);
	if (ret < 0) {
		pr_err("%s: ERROR! CP reset failed: %d\n", __func__, ret);
		return -1;
	}
#else
	ret = exynos_pmu_update(EXYNOS_PMU_CP_CTRL_NS,
			CP_RESET_SET, CP_RESET_SET);
	if (ret < 0) {
		pr_err("%s: ERROR! CP reset failed: %d\n", __func__, ret);
		return -1;
	}
#endif

	/* some delay */
	cpu_relax();
	usleep_range(80, 100);

	return ret;
}

/* release cp */
int exynos_cp_release(void)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_S);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_S, cp_ctrl | CP_START);
	if (ret < 0)
		pr_err("ERROR! CP release failed: %d\n", ret);
	else
		pr_info("%s, cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc_read(CP_CTRL_S));
#else
	ret = exynos_pmu_update(EXYNOS_PMU_CP_CTRL_S, CP_START, CP_START);
	if (ret < 0)
		pr_err("ERROR! CP release failed: %d\n", ret);
	else {
		exynos_pmu_read(EXYNOS_PMU_CP_CTRL_S, &cp_ctrl);
		pr_info("%s, PMU_CP_CTRL_S[0x%08x]\n", __func__, cp_ctrl);
	}
#endif

	return ret;
}

/* clear cp active */
int exynos_cp_active_clear(void)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_ACTIVE_REQ_CLR);
	if (ret < 0)
		pr_err("%s: ERROR! CP active_clear failed: %d\n", __func__, ret);
	else
		pr_info("%s: cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc_read(CP_CTRL_NS));
#else
	ret = exynos_pmu_update(EXYNOS_PMU_CP_CTRL_NS, CP_ACTIVE_REQ_CLR,
			CP_ACTIVE_REQ_CLR);
	if (ret < 0)
		pr_err("%s: ERROR! CP active_clear failed: %d\n", __func__, ret);
	else {
		exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
		pr_info("%s, PMU_CP_CTRL_NS[0x%08x]\n", __func__, cp_ctrl);
	}
#endif
	return ret;
}

/* clear cp_reset_req from cp */
int exynos_clear_cp_reset(void)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_RESET_REQ_CLR);
	if (ret < 0)
		pr_err("%s: ERROR! CP clear_cp_reset failed: %d\n", __func__, ret);
	else
		pr_info("%s: cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc_read(CP_CTRL_NS));
#else
	ret = exynos_pmu_update(EXYNOS_PMU_CP_CTRL_NS, CP_RESET_REQ_CLR,
			CP_RESET_REQ_CLR);
	if (ret < 0)
		pr_err("%s: ERROR! CP clear_cp_reset failed: %d\n", __func__, ret);
	else {
		exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
		pr_info("%s, PMU_CP_CTRL_NS[0x%08x]\n", __func__, cp_ctrl);
	}
#endif

	return ret;
}

int exynos_get_cp_power_status(void)
{
	u32 cp_state;

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_state = exynos_smc_read(CP_CTRL_NS);
	if (cp_state == -1)
		return -1;
#else
	exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_state);
#endif

	if (cp_state & CP_PWRON)
		return 1;
	else
		return 0;
}

int exynos_cp_init(void)
{
	u32 __maybe_unused cp_ctrl;
	int ret = 0;

	pr_info("%s: cp_ctrl init\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl & ~CP_RESET & ~CP_PWRON);
	if (ret < 0)
		pr_err("%s: ERROR! write failed: %d\n", __func__, ret);

	cp_ctrl = exynos_smc_read(CP_CTRL_S);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_S, cp_ctrl & ~CP_START);
	if (ret < 0)
		pr_err("%s: ERROR! write failed: %d\n", __func__, ret);
#else
	ret = exynos_pmu_update(EXYNOS_PMU_CP_CTRL_NS, CP_RESET_SET, 0);
	if (ret < 0)
		pr_err("%s: ERROR! CP_RESET_SET failed: %d\n", __func__, ret);

	ret = exynos_pmu_update(EXYNOS_PMU_CP_CTRL_NS, CP_PWRON, 0);
	if (ret < 0)
		pr_err("%s: ERROR! CP_PWRON failed: %d\n", __func__, ret);

	ret = exynos_pmu_update(EXYNOS_PMU_CP_CTRL_S, CP_START, 0);
	if (ret < 0)
		pr_err("%s: ERROR! CP_START failed: %d\n", __func__, ret);
#endif
	return ret;
}

int exynos_set_cp_power_onoff(enum cp_mode mode)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s: mode[%d]\n", __func__, mode);

#if defined(CONFIG_CP_SECURE_BOOT)
	/* set power on/off */
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	if (mode == CP_POWER_ON) {
		if (!(cp_ctrl & CP_PWRON)) {
			ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_PWRON);
			if (ret < 0)
				pr_err("%s: ERROR! write failed: %d\n", __func__, ret);
			else
				pr_info("%s: CP Power: [0x%08X] -> [0x%08X]\n",
					__func__, cp_ctrl, exynos_smc_read(CP_CTRL_NS));
		}
		cp_ctrl = exynos_smc_read(CP_CTRL_S);
		if (cp_ctrl == -1)
			return -1;

		ret = exynos_smc_write(CP_CTRL_S, cp_ctrl | CP_START);
		if (ret < 0)
			pr_err("%s: ERROR! write failed: %d\n", __func__, ret);
		else
			pr_info("%s: CP Start: [0x%08X] -> [0x%08X]\n", __func__,
				cp_ctrl, exynos_smc_read(CP_CTRL_S));
	} else {
		ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl & ~CP_PWRON);
		if (ret < 0)
			pr_err("ERROR! write failed: %d\n", ret);
		else
			pr_info("%s: CP Power Down: [0x%08X] -> [0x%08X]\n", __func__,
				cp_ctrl, exynos_smc_read(CP_CTRL_NS));
	}
#else
	exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
	if (mode == CP_POWER_ON) {
		if (!(cp_ctrl & CP_PWRON)) {
			ret = exynos_pmu_update(EXYNOS_PMU_CP_CTRL_NS,
				CP_PWRON, CP_PWRON);
			if (ret < 0)
				pr_err("%s: ERROR! write failed: %d\n",
						__func__, ret);
		}

		ret = exynos_pmu_update(EXYNOS_PMU_CP_CTRL_S,
			CP_START, CP_START);
		if (ret < 0)
			pr_err("%s: ERROR! write failed: %d\n", __func__, ret);
	} else {
		ret = exynos_pmu_update(EXYNOS_PMU_CP_CTRL_NS,
			CP_PWRON, 0);
		if (ret < 0)
			pr_err("ERROR! write failed: %d\n", ret);
	}
#endif

	return ret;
}

void exynos_sys_powerdown_conf_cp(void)
{
	pr_info("%s\n", __func__);

	exynos_pmu_write(EXYNOS_PMU_CENTRAL_SEQ_CP_CONFIGURATION, 0);
	exynos_pmu_write(EXYNOS_PMU_RESET_AHEAD_CP_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_LOGIC_RESET_CP_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_RESET_ASB_CP_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_TCXO_GATE_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_CLEANY_BUS_SYS_PWR_REG, 0);
}

#if !defined(CONFIG_CP_SECURE_BOOT)

#define MEMSIZE		136
#define SHDMEM_BASE	0xF0000000
#define MEMSIZE_OFFSET	16
#define MEMBASE_ADDR_OFFSET	0

static void __init set_shdmem_size(int memsz)
{
	u32 tmp;
	pr_info("[Modem_IF] Setting shared memory size to %dMB\n", memsz);

	memsz /= 4;
	exynos_pmu_update(EXYNOS_PMU_CP2AP_MEM_CONFIG,
			0x1ff << MEMSIZE_OFFSET, memsz << MEMSIZE_OFFSET);

	exynos_pmu_read(EXYNOS_PMU_CP2AP_MEM_CONFIG, &tmp);
	pr_info("[Modem_IF] EXYNOS_PMU_CP2AP_MEM_CONFIG: 0x%x\n", tmp);
}

static void __init set_shdmem_base(void)
{
	u32 tmp, base_addr;
	pr_info("[Modem_IF] Setting shared memory baseaddr to 0x%x\n", SHDMEM_BASE);

	base_addr = (SHDMEM_BASE >> 22);

	exynos_pmu_update(EXYNOS_PMU_CP2AP_MEM_CONFIG,
			0x3fff << MEMBASE_ADDR_OFFSET,
			base_addr << MEMBASE_ADDR_OFFSET);
	exynos_pmu_read(EXYNOS_PMU_CP2AP_MEM_CONFIG, &tmp);
	pr_info("[Modem_IF] EXYNOS_PMU_CP2AP_MEM_CONFIG: 0x%x\n", tmp);
}

static void set_batcher(void)
{
	exynos_pmu_write(EXYNOS_PMU_MODAPIF_CONFIG, 0x3);
}
#endif

int exynos_pmu_cp_init(void)
{

#if !defined(CONFIG_CP_SECURE_BOOT)
	set_shdmem_size(136);
	set_shdmem_base();
	set_batcher();

#ifdef CONFIG_SOC_EXYNOS8890
	/* set access window for CP */
	exynos_pmu_write(EXYNOS_PMU_CP2AP_MIF0_PERI_ACCESS_CON,
			0xffffffff);
	exynos_pmu_write(EXYNOS_PMU_CP2AP_MIF1_PERI_ACCESS_CON,
			0xffffffff);
	exynos_pmu_write(EXYNOS_PMU_CP2AP_MIF2_PERI_ACCESS_CON,
			0xffffffff);
	exynos_pmu_write(EXYNOS_PMU_CP2AP_MIF3_PERI_ACCESS_CON,
			0xffffffff);
	exynos_pmu_write(EXYNOS_PMU_CP2AP_CCORE_PERI_ACCESS_CON,
			0xffffffff);
#endif

#endif

	return 0;
}