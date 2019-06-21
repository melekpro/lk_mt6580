#include <target/board.h>

#define MTK_KERNEL_POWER_OFF_CHARGING
#ifdef MTK_KERNEL_POWER_OFF_CHARGING
#define CFG_POWER_CHARGING
#endif
#ifdef CFG_POWER_CHARGING
#include <platform/mt_typedefs.h>
#include <platform/mt_reg_base.h>
#include <platform/mt_pmic.h>
#include <platform/upmu_hw.h>
#include <platform/upmu_common.h>
#include <platform/boot_mode.h>
#include <platform/mt_gpt.h>
#include <platform/mt_rtc.h>
//#include <platform/mt_disp_drv.h>
//#include <platform/mtk_wdt.h>
//#include <platform/mtk_key.h>
//#include <platform/mt_logo.h>
//#include <platform/mt_leds.h>
#include <printf.h>
#include <sys/types.h>
#include <target/cust_battery.h>

#if defined(MTK_BQ24261_SUPPORT)
#include <platform/bq24261.h>
#endif

#if defined(MTK_BQ24296_SUPPORT)
#include <platform/bq24296.h>
#endif

#if defined(MTK_NCP1854_SUPPORT)
#include <platform/ncp1854.h>
#endif

#ifdef MTK_FAN5405_SUPPORT
#include <platform/fan5405.h>
#endif

#undef printf


/*****************************************************************************
 *  Type define
 ****************************************************************************/
#if defined(CUST_BATTERY_LOWVOL_THRESOLD)
#define BATTERY_LOWVOL_THRESOLD CUST_BATTERY_LOWVOL_THRESOLD
#else
#define BATTERY_LOWVOL_THRESOLD             3450
#endif

/*****************************************************************************
 *  Global Variable
 ****************************************************************************/
bool g_boot_reason_change = false;

#if defined(STD_AC_LARGE_CURRENT)
int g_std_ac_large_current_en=1;
#else
int g_std_ac_large_current_en=0;
#endif

/*****************************************************************************
 *  Externl Variable
 ****************************************************************************/
extern bool g_boot_menu;
extern void mtk_wdt_restart(void);

void kick_charger_wdt(void)
{
	/*
	//mt6325_upmu_set_rg_chrwdt_td(0x0);           // CHRWDT_TD, 4s
	mt6325_upmu_set_rg_chrwdt_td(0x3);           // CHRWDT_TD, 32s for keep charging for lk to kernel
	mt6325_upmu_set_rg_chrwdt_wr(1);             // CHRWDT_WR
	mt6325_upmu_set_rg_chrwdt_int_en(1);         // CHRWDT_INT_EN
	mt6325_upmu_set_rg_chrwdt_en(1);             // CHRWDT_EN
	mt6325_upmu_set_rg_chrwdt_flag_wr(1);        // CHRWDT_WR
	*/

	pmic_set_register_value(PMIC_RG_CHRWDT_TD,3);  // CHRWDT_TD, 32s for keep charging for lk to kernel
	pmic_set_register_value(PMIC_RG_CHRWDT_WR,1); // CHRWDT_WR
	pmic_set_register_value(PMIC_RG_CHRWDT_INT_EN,1);	// CHRWDT_INT_EN
	pmic_set_register_value(PMIC_RG_CHRWDT_EN,1);		// CHRWDT_EN
	pmic_set_register_value(PMIC_RG_CHRWDT_FLAG_WR,1);// CHRWDT_WR
}

#if defined(MTK_BATLOWV_NO_PANEL_ON_EARLY)
kal_bool is_low_battery(kal_int32  val)
{
    static UINT8 g_bat_low = 0xFF;

    //low battery only justice once in lk
    if(0xFF != g_bat_low)
        return g_bat_low;
    else
        g_bat_low = FALSE;

    #if defined(SWCHR_POWER_PATH)
    if(0 == val)
        val = get_i_sense_volt(1);
    #else
    if(0 == val)
        val = get_bat_sense_volt(1);
    #endif

    if (val < BATTERY_LOWVOL_THRESOLD)
    {
        printf("%s, TRUE\n", __FUNCTION__);
        g_bat_low = 0x1;
    }

    if(FALSE == g_bat_low)
        printf("%s, FALSE\n", __FUNCTION__);

    return g_bat_low;
}
#endif

void pchr_turn_on_charging(kal_bool bEnable)
{
	pmic_set_register_value(PMIC_RG_USBDL_RST,1);//force leave USBDL mode
	//mt6325_upmu_set_rg_usbdl_rst(1);       //force leave USBDL mode

	kick_charger_wdt();

	pmic_set_register_value(PMIC_RG_CS_VTH,0xC);	// CS_VTH, 450mA
	//mt6325_upmu_set_rg_cs_vth(0xC);             // CS_VTH, 450mA
	pmic_set_register_value(PMIC_RG_CSDAC_EN,1);
	//mt6325_upmu_set_rg_csdac_en(1);				// CSDAC_EN
	pmic_set_register_value(PMIC_RG_CHR_EN,1);
	//mt6325_upmu_set_rg_chr_en(1);				// CHR_EN

#ifdef MTK_FAN5405_SUPPORT
	fan5405_hw_init();
	fan5405_turn_on_charging();
	fan5405_dump_register();
#endif

#if defined(MTK_BQ24261_SUPPORT)
	bq24261_hw_init();
	bq24261_charging_enable(bEnable);
	bq24261_dump_register();
#endif

#if defined(MTK_BQ24296_SUPPORT)
	bq24296_hw_init();
	bq24296_charging_enable(bEnable);
	bq24296_dump_register();
#endif

#if defined(MTK_NCP1854_SUPPORT)
	ncp1854_hw_init();
	ncp1854_charging_enable(bEnable);
	ncp1854_dump_register();
#endif

}

void pchr_turn_off_charging(void)
{
	pmic_set_register_value(PMIC_RG_CHRWDT_INT_EN,0);// CHRWDT_INT_EN
	pmic_set_register_value(PMIC_RG_CHRWDT_EN,0);// CHRWDT_EN
	pmic_set_register_value(PMIC_RG_CHRWDT_FLAG_WR,0);// CHRWDT_FLAG
	pmic_set_register_value(PMIC_RG_CSDAC_EN,0);// CSDAC_EN
	pmic_set_register_value(PMIC_RG_CHR_EN,0);// CHR_EN
	pmic_set_register_value(PMIC_RG_HWCV_EN,0);// RG_HWCV_EN
}




//enter this function when low battery with charger
void check_bat_protect_status()
{
    kal_int32 bat_val = 0;
	int current,chr_volt,cnt=0,i;
    
    #if defined(SWCHR_POWER_PATH)
    bat_val = get_i_sense_volt(5);
    #else
    bat_val = get_bat_sense_volt(5);
    #endif
    
    dprintf(CRITICAL, "[%s]: check VBAT=%d mV with %d mV, start charging... \n", __FUNCTION__, bat_val, BATTERY_LOWVOL_THRESOLD);
    
    while (bat_val < BATTERY_LOWVOL_THRESOLD)
    {
        mtk_wdt_restart();
        if(upmu_is_chr_det() == KAL_FALSE)
        {
            dprintf(CRITICAL, "[BATTERY] No Charger, Power OFF !\n");
            mt6575_power_off();
            while(1);
        }
    
        pchr_turn_on_charging(KAL_TRUE);

#if defined(SWCHR_POWER_PATH)
		mdelay(5000);
#else
/*
	cnt=0;
	for(i=0;i<10;i++)
	{
		current=get_charging_current(1);
		chr_volt=get_charger_volt(1);	
		if(current<100 && chr_volt<4400)
		{
			cnt++;
			dprintf(CRITICAL, "[BATTERY] charging current=%d charger volt=%d\n\r",current,chr_volt);
		}
		else
		{
			dprintf(CRITICAL, "[BATTERY] charging current=%d charger volt=%d\n\r",current,chr_volt);
			cnt=0;
		}
	}

	if(cnt>=8)
	{

            dprintf(CRITICAL, "[BATTERY] charging current and charger volt too low !! \n\r",cnt);

            pchr_turn_off_charging();
    #ifndef NO_POWER_OFF
            mt6575_power_off();
    #endif            
            while(1)
            {
                dprintf(CRITICAL, "If you see the log, please check with RTC power off API\n\r");
            }
	}
	mdelay(50);
	*/
#endif



        #if defined(SWCHR_POWER_PATH)
        #ifndef MTK_NCP1854_SUPPORT /* NCP1854 needs enable charging to have power path */
        pchr_turn_on_charging(KAL_FALSE);
        mdelay(100);
        #endif
        bat_val = get_i_sense_volt(5);
        #else
        bat_val = get_bat_sense_volt(5);
        #endif
		 dprintf(CRITICAL, "[%s]: check VBAT=%d mV  \n", __FUNCTION__, bat_val);
    }

    dprintf(CRITICAL, "[%s]: check VBAT=%d mV with %d mV, stop charging... \n", __FUNCTION__, bat_val, BATTERY_LOWVOL_THRESOLD);
}

void mt65xx_bat_init(void)
{    
    kal_int32 bat_vol;
    
    // Low Battery Safety Booting
    
    #if defined(SWCHR_POWER_PATH)
    bat_vol = get_i_sense_volt(1);
    #else
    bat_vol = get_bat_sense_volt(1);
    #endif

    //pchr_turn_on_charging(KAL_TRUE);
    dprintf(CRITICAL, "[mt65xx_bat_init] check VBAT=%d mV with %d mV\n", bat_vol, BATTERY_LOWVOL_THRESOLD);    
	


	
    if(g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT && (pmic_get_register_value(PMIC_PWRKEY_DEB)==0) ) {
            dprintf(CRITICAL, "[mt65xx_bat_init] KPOC+PWRKEY => change boot mode\n");        
    
            g_boot_reason_change = true;
    }
    rtc_boot_check(false);

    #ifndef MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
    #ifndef MTK_BATLOWV_NO_PANEL_ON_EARLY
    if (bat_vol < BATTERY_LOWVOL_THRESOLD)
    #else
    if (is_low_battery(bat_vol))
    #endif
    {
        if(g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT && upmu_is_chr_det() == KAL_TRUE)
        {
            dprintf(CRITICAL, "[%s] Kernel Low Battery Power Off Charging Mode\n", __func__);
            g_boot_mode = LOW_POWER_OFF_CHARGING_BOOT;
            
            check_bat_protect_status();
            
            return;
        }
        else
        {
            dprintf(CRITICAL, "[BATTERY] battery voltage(%dmV) <= CLV ! Can not Boot Linux Kernel !! \n\r",bat_vol);
    #ifndef NO_POWER_OFF
            mt6575_power_off();
    #endif            
            while(1)
            {
                dprintf(CRITICAL, "If you see the log, please check with RTC power off API\n\r");
            }
        }
    }
    #endif
    return;
}

#else

#include <platform/mt_typedefs.h>
#include <platform/mt_reg_base.h>
#include <printf.h>

void mt65xx_bat_init(void)
{
    dprintf(CRITICAL, "[BATTERY] Skip mt65xx_bat_init !!\n\r");
    dprintf(CRITICAL, "[BATTERY] If you want to enable power off charging, \n\r");
    dprintf(CRITICAL, "[BATTERY] Please #define CFG_POWER_CHARGING!!\n\r");
}

#endif
