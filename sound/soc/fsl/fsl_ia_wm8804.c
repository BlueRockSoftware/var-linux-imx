#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../codecs/wm8804.h"

struct wm8805_clk_cfg {
    unsigned int sysclk_freq;
    unsigned int mclk_freq;
    unsigned int mclk_div;
}

struct snd_fsl_wm8805_drvdata {
	/* Required - pointer to the DAI structure */
	struct snd_soc_dai_link *dai;
	/* Required - snd_soc_card name */
	const char *card_name;
	/* Optional DT node names if card info is defined in DT */
	const char *card_name_dt;
	const char *dai_name_dt;
	const char *dai_stream_name_dt;
	/* Optional probe extension - called prior to register_card */
	int (*probe)(struct platform_device *pdev);
};

static struct gpio_desc *clk_44gpio;
static struct gpio_desc *clk_48gpio;
static struct gpio_desc *reset;
static int rate = 0;

#define CLK44EN_RATE 22579200UL
#define CLK48EN_RATE 24576000UL

static const char * const wm8805_input_select_text[] = {
    "spdif",
    "coax",
};
static const unsigned int wm8805_input_select_values[] = {
    0,
    1,
};  

static const struct soc_enum wm8805_input_select_enum = {
    SOC_ENUM_SINGLE(WM8804_PLL6, 0, 2, wm8805_input_select_text, wm8805_input_select_values);
};

static const struct snd_kcontrol_new wm8805_controls[] = {
    SOC_ENUM("Input Select", wm8805_input_select_enum),
};  
static int wm8805_add_controls(struct snd_soc_codec *codec) {
     snd_soc_add_codec_controls(codec, wm8805_controls, ARRAY_SIZE(wm8805_controls));
     return 0;
}

static unsigned int snd_enable_clk(unsigned int rate) {
    switch(rate){
        case 11025:
        case 22050:
        case 44100:
        case 88200:
        case 176400:
            gpiod_set_value(clk_44gpio, 1);
            gpiod_set_value(clk_48gpio, 0);
            return CLK44EN_RATE;
        default:
            gpiod_set_value(clk_44gpio, 0);
            gpiod_set_value(clk_48gpio, 1);
            return CLK48EN_RATE;
            }
}

static void snd_clk_cfg(unsigned int samplerate, struct wm8805_clk_cfg *cfg) {
    clk_cfg->sysclk_freq = sysclk_freq;
    
    if (samplerate <= 96000){
        clk_cfg->mclk_freq = samplerate * 256;
        clk_cfg->mclk_div = WM8804_MCLKDIV_256FS;
    }else{
        cfg->mclock_freq = samplerate * 128;
        cfg->mclk_div = WM8804_MCLKDIV_128FS; 
    }

    if((IS_ERR(clk_44gpio) || IS_ERR(clk_48gpio)) {
        clk_cfg->sysclk_freq = snd_enable_clk(samplerate);
    }
}

static int snd_fsl_wm8805_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params) {
    struct snd_soc_pcm_runtime *rtd = substream->private_data;
    struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
    struct snd_soc_component *component = asoc_rtd_to_codec(rtd, 0)->component;
    struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
    struct wm8805_clk_cfg clk_cfg;
    unsigned int sampling_freq = 1;
    int ret;
    int samplerate = params_rate(params);

    if (samplerate == params_rate(params)) {
        return 0;
    }

    rate = 0;

    snd_clk_cfg(samplerate, &clk_cfg);

    switch (samplerate){
        case 3200:
        sampling_freq = 0x03;
        break;
        case 44100:
        sampling_freq = 0x00;
        break;
        case 48000:
        sampling_freq = 0x02;
        break;
        case 88200:
        sampling_freq = 0x08;
        break;
        case 96000:
        sampling_freq = 0x0A;
        break;
        case 176400:
        sampling_freq = 0x0C;
        break;
        case 192000:
        sampling_freq = 0x0E;
        break;
        default:
        dev_err(rtd->card->dev, "Unsupported sample rate %d\n", samplerate);
    }

    snd_soc_dai_set_clkdiv(codec_dai, WM8804_MCLKDIV, clk_cfg.mclk_div);
    snd_soc_dai_set_pll(codec_dai, 0, 0, clk_cfg.sysclk_freq, clk_cfg.mclk_freq);
    ret = snd_soc_dai_set_sysclk(codec_dai, WM8804_TX_CLKSRC_PLL, clk_cfg.sysclk_freq, SND_SOC_CLOCK_OUT);

    if (ret < 0){
        dev_err(rtd->card->dev,"FAILED to set sysclk: %d\n", ret);
        return ret;
    }

    rate = samplerate;
    snd_soc_component_update_bits(component, WM8804_SPDTX4, 0x0F, sampling_freq);
    return snd_soc_dai_set_bclk_ratio(dai, 64);
}

static struct snd_soc_ops snd_fsl_wm8805_ops = {
    .hw_params = snd_fsl_wm8805_hw_params,
};

static const struct of_device_id fsl_wm8805_of_match[] = {
    { .compatible = "interludeaudio,interludeaudio-digital", .data = (void *) &drvdata_interludeaudio}
    { }
};

SND_SOC_DAILINK_DEFS(interlude_audio_digital,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static int snd_wm8805_probe(struct platform_device *pdev)  {
    int ret = 0;
    const struct of_device_id *id;
    fsl_wm8805_card.dev = &pdev->dev;
    of_id = of_match_node(fsl_wm8805_of_match, pdev->dev.of_node);

    if (pdev->dev.of_node && of_id->data){
        struct device_node *i2s_node;
        struct snd_fsm_wm8805_drvdata *drvdata = (struct snd_fsm_wm8805_drvdata *)of_id->data;
        struct snd_soc_dai_link *dai = drvdata->dai;

        snd_soc_card_set_drvdata(&fsl_wm8805_card, drvdata);

        if(!dai->ops)
            dai->ops = &snd_fsl_wm8805_ops;
        if(!dai->codecs->dai_name)
            dai->codecs->dai_name = "interludeaudio-digital";
        if(!dai->codecs->name)
            dai->codecs->name = "WM8805.1-003b";
        if(dai->dai_fmt)
            dai->dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM;

        fsl_wm8805_card.dai_link = dai;
        i2s_node = of_parse_phandle(pdev->dev.of_node, "i2s-controller", 0);

        if(!i2s_node){
            dev_err(&pdev->dev, "i2s-controller property not found\n");
            return -EINVAL;
        }

        fsl_wm8805_card.name = drvdata->card_name;

        if (drvdata->card_name_dt)
            of_property_read_string(i2s_node,drvdata->card_name_dt, &fsl_wm8805_card.name);
        
        if (drvdata->dai_name_dt)
            of_property_read_string(i2s_node,drvdata->dai_name_dt, &dai->name);
        
        if (drvdata->dai_stream_name_dt)
            of_property_read_string(i2s_node,drvdata->dai_stream_name_dt, &dai->stream_name);

        dai->cpus->of_node = i2s_node;
        dai->platforms->of_node = i2s_node;

        clk_44gpio = devm_gpiod_get(&pdev->dev, "clk44", GPIOD_OUT_LOW);

        clk_48gpio = devm_gpiod_get(&pdev->dev, "clk48", GPIOD_OUT_LOW);

        reset = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_LOW);

        gpiod_set_value_cansleep(reset, 0);
        mdelay(10);
        gpiod_set_value_cansleep(reset, 1);

        ret = wm8805_add_controls(component);
        if (ret < 0){
            dev_err(&pdev->dev, "Failed to add controls: %d\n", ret);
        }
        
        if (drvdata->probe){
            ret = drvdata->probe(pdev);
            if(ret<0){
                dev_err(&pdev->dev, "probe failed: %d\n", ret);
                return ret;
            }
        }
        ret = devm_snd_soc_register_card(&pdev->dev, &fsl_wm8805_card);
        if (ret && ret != -EPROBE_DEFER)
            dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);

        return ret;    
    }
}

static struct snd_soc_card fsl_wm8805_card = {
    .name = "fsl-wm8805",
    .owner = THIS_MODULE,
    .dai_link = NULL,
    .num_links = 1,
};