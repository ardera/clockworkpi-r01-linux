// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for Allwinner sun8i-v536 Pulse Width Modulation Controller
 *
 * Copyright (C) 2021 Ban Tao <fengzheng923@gmail.com>
 *
 *
 * Limitations:
 * - When PWM is disabled, the output is driven to inactive.
 * - If the register is reconfigured while PWM is running,
 *   it does not complete the currently running period.
 * - If the user input duty is beyond acceptible limits,
 *   -EINVAL is returned.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pwm.h>
#include <linux/clk.h>
#include <linux/reset.h>

#define PWM_GET_CLK_OFFSET(chan)	(0x20 + ((chan >> 1) * 0x4))
#define PWM_CLK_APB_SCR			BIT(7)
#define PWM_DIV_M			0
#define PWM_DIV_M_MASK			GENMASK(3, PWM_DIV_M)

#define PWM_CLK_REG			0x40
#define PWM_CLK_GATING			BIT(0)

#define PWM_ENABLE_REG			0x80
#define PWM_EN				BIT(0)

#define PWM_CTL_REG(chan)		(0x100 + 0x20 * chan)
#define PWM_ACT_STA			BIT(8)
#define PWM_PRESCAL_K			0
#define PWM_PRESCAL_K_MASK		GENMASK(7, PWM_PRESCAL_K)

#define PWM_PERIOD_REG(chan)		(0x104 + 0x20 * chan)
#define PWM_ENTIRE_CYCLE			16
#define PWM_ENTIRE_CYCLE_MASK		GENMASK(31, PWM_ENTIRE_CYCLE)
#define PWM_ACT_CYCLE			0
#define PWM_ACT_CYCLE_MASK		GENMASK(15, PWM_ACT_CYCLE)

#define BIT_CH(bit, chan)		((bit) << (chan))
#define SET_BITS(shift, mask, reg, val) \
	    (((reg) & ~mask) | (val << (shift)))

#define PWM_OSC_CLK			24000000
#define PWM_PRESCALER_MAX		256
#define PWM_CLK_DIV_M__MAX		9
#define PWM_ENTIRE_CYCLE_MAX		65536

struct sun8i_pwm_data {
	unsigned int npwm;
};

struct sun8i_pwm_chip {
	struct pwm_chip chip;
	struct clk *clk;
	struct reset_control *rst_clk;
	void __iomem *base;
	const struct sun8i_pwm_data *data;
};

static inline struct sun8i_pwm_chip *to_sun8i_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct sun8i_pwm_chip, chip);
}

static inline u32 sun8i_pwm_readl(struct sun8i_pwm_chip *chip,
				   unsigned long offset)
{
	return readl(chip->base + offset);
}

static inline void sun8i_pwm_writel(struct sun8i_pwm_chip *chip,
				     u32 val, unsigned long offset)
{
	writel(val, chip->base + offset);
}

static void sun8i_pwm_get_state(struct pwm_chip *chip,
				 struct pwm_device *pwm,
				 struct pwm_state *state)
{
	struct sun8i_pwm_chip *pc = to_sun8i_pwm_chip(chip);
	u64 clk_rate;
	u32 tmp, entire_cycles, active_cycles;
	unsigned int prescaler, div_m;

	tmp = sun8i_pwm_readl(pc, PWM_GET_CLK_OFFSET(pwm->hwpwm));
	if (tmp & PWM_CLK_APB_SCR)
		clk_rate = clk_get_rate(pc->clk);
	else
		clk_rate = PWM_OSC_CLK;

	tmp = sun8i_pwm_readl(pc, PWM_GET_CLK_OFFSET(pwm->hwpwm));
	div_m = 0x1 << (tmp & PWM_DIV_M_MASK);

	tmp = sun8i_pwm_readl(pc, PWM_CTL_REG(pwm->hwpwm));
	prescaler = (tmp & PWM_PRESCAL_K_MASK) + 1;

	tmp = sun8i_pwm_readl(pc, PWM_PERIOD_REG(pwm->hwpwm));
	entire_cycles = (tmp >> PWM_ENTIRE_CYCLE) + 1;
	active_cycles = (tmp & PWM_ACT_CYCLE_MASK);

	/* (clk / div_m / prescaler) / entire_cycles = NSEC_PER_SEC / period_ns. */
	state->period = DIV_ROUND_CLOSEST_ULL(entire_cycles * NSEC_PER_SEC,
					      clk_rate) * div_m * prescaler;
	/* duty_ns / period_ns = active_cycles / entire_cycles. */
	state->duty_cycle = DIV_ROUND_CLOSEST_ULL(active_cycles * state->period,
						  entire_cycles);

	/* parsing polarity */
	tmp = sun8i_pwm_readl(pc, PWM_CTL_REG(pwm->hwpwm));
	if (tmp & PWM_ACT_STA)
		state->polarity = PWM_POLARITY_NORMAL;
	else
		state->polarity = PWM_POLARITY_INVERSED;

	/* parsing enabled */
	tmp = sun8i_pwm_readl(pc, PWM_ENABLE_REG);
	if (tmp & BIT_CH(PWM_EN, pwm->hwpwm))
		state->enabled = true;
	else
		state->enabled = false;

	dev_dbg(chip->dev, "duty_ns=%lld period_ns=%lld polarity=%s enabled=%s.\n",
				state->duty_cycle, state->period,
				state->polarity ? "inversed":"normal",
				state->enabled ? "true":"false");
}

static void sun8i_pwm_set_polarity(struct pwm_chip *chip, struct pwm_device *pwm,
				    enum pwm_polarity polarity)
{
	struct sun8i_pwm_chip *pc = to_sun8i_pwm_chip(chip);
	u32 temp;

	temp = sun8i_pwm_readl(pc, PWM_CTL_REG(pwm->hwpwm));

	if (polarity == PWM_POLARITY_NORMAL)
		temp |= PWM_ACT_STA;
	else
		temp &= ~PWM_ACT_STA;

	sun8i_pwm_writel(pc, temp, PWM_CTL_REG(pwm->hwpwm));
}

static int sun8i_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			      const struct pwm_state *state)
{
	struct sun8i_pwm_chip *pc = to_sun8i_pwm_chip(chip);
	unsigned long long c;
	unsigned long entire_cycles, active_cycles;
	unsigned int div_m, prescaler;
	u64 duty_ns = state->duty_cycle, period_ns = state->period;
	u32 config;
	int ret = 0;

	if (period_ns > 334) {
		/* if freq < 3M, then select 24M clock */
		c = PWM_OSC_CLK;
		config = sun8i_pwm_readl(pc, PWM_GET_CLK_OFFSET(pwm->hwpwm));
		config &= ~PWM_CLK_APB_SCR;
		sun8i_pwm_writel(pc, config, PWM_GET_CLK_OFFSET(pwm->hwpwm));
	} else {
		/* if freq > 3M, then select APB as clock */
		c = clk_get_rate(pc->clk);
		config = sun8i_pwm_readl(pc, PWM_GET_CLK_OFFSET(pwm->hwpwm));
		config |= PWM_CLK_APB_SCR;
		sun8i_pwm_writel(pc, config, PWM_GET_CLK_OFFSET(pwm->hwpwm));
	}

	dev_dbg(chip->dev, "duty_ns=%lld period_ns=%lld c =%llu.\n",
			duty_ns, period_ns, c);

	/*
	 * (clk / div_m / prescaler) / entire_cycles = NSEC_PER_SEC / period_ns.
	 * So, entire_cycles = clk * period_ns / NSEC_PER_SEC / div_m / prescaler.
	 */
	c = c * period_ns;
	c = DIV_ROUND_CLOSEST_ULL(c, NSEC_PER_SEC);
	for (div_m = 0; div_m < PWM_CLK_DIV_M__MAX; div_m++) {
		for (prescaler = 0; prescaler < PWM_PRESCALER_MAX; prescaler++) {
			/*
			 * actual prescaler = prescaler(reg value) + 1.
			 * actual div_m = 0x1 << div_m(reg value).
			 */
			entire_cycles = ((unsigned long)c >> div_m)/(prescaler + 1);
			if (entire_cycles <= PWM_ENTIRE_CYCLE_MAX)
				goto calc_end;
		}
	}
	ret = -EINVAL;
	goto exit;

calc_end:
	/*
	 * duty_ns / period_ns = active_cycles / entire_cycles.
	 * So, active_cycles = entire_cycles * duty_ns / period_ns.
	 */
	c = (unsigned long long)entire_cycles * duty_ns;
	c = DIV_ROUND_CLOSEST_ULL(c, period_ns);
	active_cycles = c;
	if (entire_cycles == 0)
		entire_cycles++;

	/* config  clk div_m*/
	config = sun8i_pwm_readl(pc, PWM_GET_CLK_OFFSET(pwm->hwpwm));
	config = SET_BITS(PWM_DIV_M, PWM_DIV_M_MASK, config, div_m);
	sun8i_pwm_writel(pc, config, PWM_GET_CLK_OFFSET(pwm->hwpwm));

	/* config prescaler */
	config = sun8i_pwm_readl(pc, PWM_CTL_REG(pwm->hwpwm));
	config = SET_BITS(PWM_PRESCAL_K, PWM_PRESCAL_K_MASK, config, prescaler);
	sun8i_pwm_writel(pc, config, PWM_CTL_REG(pwm->hwpwm));

	/* config active and period cycles */
	config = sun8i_pwm_readl(pc, PWM_PERIOD_REG(pwm->hwpwm));
	config = SET_BITS(PWM_ACT_CYCLE, PWM_ACT_CYCLE_MASK, config, active_cycles);
	config = SET_BITS(PWM_ENTIRE_CYCLE, PWM_ENTIRE_CYCLE_MASK,
			config, (entire_cycles - 1));
	sun8i_pwm_writel(pc, config, PWM_PERIOD_REG(pwm->hwpwm));

	dev_dbg(chip->dev, "active_cycles=%lu entire_cycles=%lu prescaler=%u div_m=%u\n",
			   active_cycles, entire_cycles, prescaler, div_m);

exit:
	return ret;
}

static void sun8i_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm,
			     bool enable)
{
	struct sun8i_pwm_chip *pc = to_sun8i_pwm_chip(chip);
	u32 clk, pwm_en;

	clk = sun8i_pwm_readl(pc, PWM_CLK_REG);
	pwm_en = sun8i_pwm_readl(pc, PWM_ENABLE_REG);

	if (enable) {
		clk |= BIT_CH(PWM_CLK_GATING, pwm->hwpwm);
		sun8i_pwm_writel(pc, clk, PWM_CLK_REG);

		pwm_en |= BIT_CH(PWM_EN, pwm->hwpwm);
		sun8i_pwm_writel(pc, pwm_en, PWM_ENABLE_REG);
	} else {
		pwm_en &= ~BIT_CH(PWM_EN, pwm->hwpwm);
		sun8i_pwm_writel(pc, pwm_en, PWM_ENABLE_REG);

		clk &= ~BIT_CH(PWM_CLK_GATING, pwm->hwpwm);
		sun8i_pwm_writel(pc, clk, PWM_CLK_REG);
	}
}

static int sun8i_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			      const struct pwm_state *state)
{
	struct pwm_state curstate;
	int ret;

	pwm_get_state(pwm, &curstate);

	ret = sun8i_pwm_config(chip, pwm, state);

	if (state->polarity != curstate.polarity)
		sun8i_pwm_set_polarity(chip, pwm, state->polarity);

	if (state->enabled != curstate.enabled)
		sun8i_pwm_enable(chip, pwm, state->enabled);

	return ret;
}

static const struct pwm_ops sun8i_pwm_ops = {
	.get_state = sun8i_pwm_get_state,
	.apply = sun8i_pwm_apply,
	.owner = THIS_MODULE,
};

static const struct sun8i_pwm_data sun8i_pwm_data_c9 = {
	.npwm = 9,
};

static const struct sun8i_pwm_data sun20i_pwm_data_c8 = {
	.npwm = 8,
};

static const struct sun8i_pwm_data sun50i_pwm_data_c16 = {
	.npwm = 16,
};

static const struct of_device_id sun8i_pwm_dt_ids[] = {
	{
		.compatible = "allwinner,sun8i-v536-pwm",
		.data = &sun8i_pwm_data_c9,
	}, {
		.compatible = "allwinner,sun20i-d1-pwm",
		.data = &sun20i_pwm_data_c8,
	}, {
		.compatible = "allwinner,sun50i-r818-pwm",
		.data = &sun50i_pwm_data_c16,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, sun8i_pwm_dt_ids);

static int sun8i_pwm_probe(struct platform_device *pdev)
{
	struct sun8i_pwm_chip *pc;
	int ret;

	pc = devm_kzalloc(&pdev->dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return dev_err_probe(&pdev->dev, -ENOMEM,
				     "memory allocation failed\n");

	pc->data = of_device_get_match_data(&pdev->dev);
	if (!pc->data)
		return dev_err_probe(&pdev->dev, -ENODEV,
				     "can't get match data\n");

	pc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pc->base))
		return dev_err_probe(&pdev->dev, PTR_ERR(pc->base),
				     "can't remap pwm resource\n");

	pc->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pc->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(pc->clk),
				     "get clock failed\n");

	pc->rst_clk = devm_reset_control_get_exclusive(&pdev->dev, NULL);
	if (IS_ERR(pc->rst_clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(pc->rst_clk),
				     "get reset failed\n");

	/* Deassert reset */
	ret = reset_control_deassert(pc->rst_clk);
	if (ret < 0)
		return dev_err_probe(&pdev->dev, ret,
				     "cannot deassert reset control\n");

	ret = clk_prepare_enable(pc->clk);
	if (ret) {
		dev_err(&pdev->dev, "cannot prepare and enable clk %pe\n",
			ERR_PTR(ret));
		goto err_clk;
	}

	pc->chip.dev = &pdev->dev;
	pc->chip.ops = &sun8i_pwm_ops;
	pc->chip.npwm = pc->data->npwm;
	pc->chip.of_xlate = of_pwm_xlate_with_flags;
	pc->chip.base = -1;
	pc->chip.of_pwm_n_cells = 3;

	ret = pwmchip_add(&pc->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add PWM chip: %d\n", ret);
		goto err_pwm_add;
	}

	platform_set_drvdata(pdev, pc);

	return 0;

err_pwm_add:
	clk_disable_unprepare(pc->clk);
err_clk:
	reset_control_assert(pc->rst_clk);

	return ret;
}

static int sun8i_pwm_remove(struct platform_device *pdev)
{
	struct sun8i_pwm_chip *pc = platform_get_drvdata(pdev);

	pwmchip_remove(&pc->chip);
	clk_disable_unprepare(pc->clk);
	reset_control_assert(pc->rst_clk);

	return 0;
}

static struct platform_driver sun8i_pwm_driver = {
	.driver = {
		.name = "sun8i-pwm-v536",
		.of_match_table = sun8i_pwm_dt_ids,
	},
	.probe = sun8i_pwm_probe,
	.remove = sun8i_pwm_remove,
};
module_platform_driver(sun8i_pwm_driver);

MODULE_ALIAS("platform:sun8i-v536-pwm");
MODULE_AUTHOR("Ban Tao <fengzheng923@gmail.com>");
MODULE_DESCRIPTION("Allwinner sun8i-v536 PWM driver");
MODULE_LICENSE("GPL v2");
