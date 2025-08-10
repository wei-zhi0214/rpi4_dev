#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>   // 加這行

#define PWM_CTL     0x00
#define PWM_RNG1    0x10
#define PWM_DAT1    0x14


struct pwm_demo_dev {
    void __iomem *base;
    u32 clk_freq;
    u32 duty;
    struct clk *clk;         // <--
};


/* 共用的設定函數 */
static void pwm_set(struct pwm_demo_dev *p, u32 freq_hz, u32 duty_pc)
{
    u32 period, dat;

    if (!freq_hz) freq_hz = 1;
    if (duty_pc > 100) duty_pc = 100;

    period = p->clk_freq / freq_hz;
    if (!period) period = 1;
    dat = (period * duty_pc) / 100;

    writel(0,             p->base + PWM_CTL);
    writel(period,        p->base + PWM_RNG1);
    writel(dat,           p->base + PWM_DAT1);
    wmb(); /* 保險 */
    writel((1<<7)|(1<<0), p->base + PWM_CTL); /* MSEN1 | PWEN1 */
}

/* sysfs: duty (0..100) */
static ssize_t duty_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct pwm_demo_dev *p = dev_get_drvdata(dev);
    return sysfs_emit(buf, "%u\n", p->duty);
}
static ssize_t duty_store(struct device *dev, struct device_attribute *attr,
                          const char *buf, size_t count)
{
    struct pwm_demo_dev *p = dev_get_drvdata(dev);
    unsigned long v;
    if (kstrtoul(buf, 10, &v)) return -EINVAL;
    if (v > 100) v = 100;
    p->duty = v;
    pwm_set(p, 1000, p->duty); /* 預設維持 1kHz */
    return count;
}
static DEVICE_ATTR_RW(duty); /* 0644，root 可寫 */

static ssize_t freq_store(struct device *dev, struct device_attribute *attr,
                          const char *buf, size_t count)
{
    struct pwm_demo_dev *p = dev_get_drvdata(dev);
    unsigned long f;
    if (kstrtoul(buf, 10, &f) || !f) return -EINVAL;
    pwm_set(p, f, p->duty);
    return count;
}
static DEVICE_ATTR_WO(freq); /* 0200，root 可寫 */

static int pwm_demo_probe(struct platform_device *pdev)
{
    struct pwm_demo_dev *p;
    struct resource *res;
    struct device_node *np = pdev->dev.of_node;
    int ret;

    p = devm_kzalloc(&pdev->dev, sizeof(*p), GFP_KERNEL);
    if (!p) return -ENOMEM;

    /* 🔒 強制套 pinmux default，確保腳位在 PWM 模式 */
    devm_pinctrl_get_select_default(&pdev->dev);

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    p->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(p->base)) return PTR_ERR(p->base);

    of_property_read_u32(np, "default-duty", &p->duty);
    if (p->duty > 100) p->duty = 50;

    /* clock on + 取得真實頻率 */
    p->clk = devm_clk_get(&pdev->dev, NULL);
    if (IS_ERR(p->clk)) return dev_err_probe(&pdev->dev, PTR_ERR(p->clk), "clk get fail\n");
    ret = clk_prepare_enable(p->clk);
    if (ret) return dev_err_probe(&pdev->dev, ret, "clk enable fail\n");
    p->clk_freq = clk_get_rate(p->clk);
    if (!p->clk_freq)
        of_property_read_u32(np, "clock-frequency", &p->clk_freq); /* 後備 */

    platform_set_drvdata(pdev, p);

    /* sysfs 檔案 */
    device_create_file(&pdev->dev, &dev_attr_duty);
    device_create_file(&pdev->dev, &dev_attr_freq);

    /* ✅ 上電就「看得到」：
       方案1：先 100% 恆亮 300ms，再切回 1kHz/預設 duty
       （不想閃一下就直接留 100%） */
    pwm_set(p, 1000, 100);
    msleep(500);
    pwm_set(p, 1000, p->duty);

    dev_info(&pdev->dev, "PWM demo initialized, clk=%u Hz, duty=%u%%\n",
             p->clk_freq, p->duty);
    return 0;
}

static int pwm_demo_remove(struct platform_device *pdev)
{
    struct pwm_demo_dev *p = platform_get_drvdata(pdev);
    writel(0, p->base + PWM_CTL);
    clk_disable_unprepare(p->clk);
    device_remove_file(&pdev->dev, &dev_attr_duty);
    device_remove_file(&pdev->dev, &dev_attr_freq);
    return 0;
}

static const struct of_device_id pwm_demo_of_match[] = {
    { .compatible = "mycompany,pwm-demo" },
    {},
};
MODULE_DEVICE_TABLE(of, pwm_demo_of_match);

static struct platform_driver pwm_demo_driver = {
    .driver = {
        .name = "pwm_demo",
        .of_match_table = pwm_demo_of_match,
    },
    .probe = pwm_demo_probe,
    .remove = pwm_demo_remove,
};


module_platform_driver(pwm_demo_driver);
MODULE_LICENSE("GPL");

