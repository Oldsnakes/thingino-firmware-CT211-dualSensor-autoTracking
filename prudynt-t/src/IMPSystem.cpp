#include "imp_hal.hpp"
#include "IMPSystem.hpp"
#include "Config.hpp"

#define MODULE "IMP_SYSTEM"
#if 0
//struct sensor_conf {
//static int  i2c_addr = 0x37;
//static int  FPS_Num = 15;
//static int  FPS_Den = 1;
//static int  nrvbs = 2;
//static int  VideoWidth = 1280;
//static int  VideoHeight = 720;
//static int  videoBitrate = 2000;
//static int  direct_mode = 1;
//static char Sensor_Name[16];;
//};

//static sensor_conf sensor1;
//static sensor_conf sensor2;

//static int  gconf_i2c_addr = 0x37;
//static int  gconf_i2c_addr1 = 0x3f;

//static IMPSensorInfo sensor_info;
//static IMPSensorInfo sensor_info1;
//static char gconf_Sensor_Name[16] = "gc1084";
//static char gconf_Sensor_Name1[16] = "gc1084s1";
#endif

IMPSensorInfo IMPSystem::create_sensor_info(_sensor sensor, int id)
{
    IMPSensorInfo out;
    memset(&out, 0, sizeof(IMPSensorInfo));
    strcpy(out.name, sensor.model);
    out.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C;
    strcpy(out.i2c.type, sensor.model);
    out.i2c.addr = sensor.i2c_address;
    out.i2c.i2c_adapter_id = sensor.i2c_bus;
    out.sensor_id = id;

#if defined(PLATFORM_T40) || defined(PLATFORM_T41)
    // Additional fields required for T40/T41 platforms
    out.i2c.i2c_adapter_id = sensor.i2c_bus;
    out.rst_gpio = sensor.gpio_reset;
    out.pwdn_gpio = -1;
    out.power_gpio = -1;
    out.sensor_id = 0;
    out.video_interface = static_cast<IMPSensorVinType>(sensor.video_interface);
    out.mclk = static_cast<IMPSensorMclk>(sensor.mclk);
    out.default_boot = 0;
#endif

    return out;
}

#if 0
IMPSensorInfo IMPSystem::create_sensor_info(const char *sensor_name)
{
    IMPSensorInfo out;
    memset(&out, 0, sizeof(IMPSensorInfo));
    LOG_INFO("Sensor: " << cfg->sensor.model);
    strcpy(out.name, cfg->sensor.model);
    out.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C;
    strcpy(out.i2c.type, cfg->sensor.model);
    out.i2c.addr = cfg->sensor.i2c_address;

#if defined(PLATFORM_T40) || defined(PLATFORM_T41)
    // Additional fields required for T40/T41 platforms
    out.i2c.i2c_adapter_id = cfg->sensor.i2c_bus;
    out.rst_gpio = cfg->sensor.gpio_reset;
    out.pwdn_gpio = -1;
    out.power_gpio = -1;
    out.sensor_id = 0;
    out.video_interface = static_cast<IMPSensorVinType>(cfg->sensor.video_interface);
    out.mclk = static_cast<IMPSensorMclk>(cfg->sensor.mclk);
    out.default_boot = 0;
#endif

    return out;
}
#endif

IMPSystem *IMPSystem::createNew()
{
    return new IMPSystem();
}

int IMPSystem::init()
{
    LOG_DEBUG("IMPSystem::init()");
    int ret = 0;

    ret = IMP_OSD_SetPoolSize(cfg->general.osd_pool_size * 1024);
    LOG_DEBUG_OR_ERROR(ret, "IMP_OSD_SetPoolSize(" << (cfg->general.osd_pool_size * 1024) << ")");

    IMPVersion impVersion;
    ret = IMP_System_GetVersion(&impVersion);
    LOG_INFO("LIBIMP Version " << impVersion.aVersion);

    SUVersion suVersion;
    ret = SU_Base_GetVersion(&suVersion);
    LOG_INFO("SYSUTILS Version: " << suVersion.chr);

    cfg->sysinfo.cpu = IMP_System_GetCPUInfo();
    LOG_INFO("CPU Information: " << cfg->sysinfo.cpu);

    // ### TW:  test bypass
    // Configurable ISP bypass, does not work(T23)
    ret = hal::isp::set_isp_bypass(cfg->image.isp_bypass);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetISPBypass(" << (cfg->image.isp_bypass ? IMPISP_TUNING_OPS_MODE_ENABLE : IMPISP_TUNING_OPS_MODE_DISABLE) << ")");

    ret = IMP_ISP_Open();
    LOG_DEBUG_OR_ERROR_AND_EXIT(ret, "IMP_ISP_Open()");

    /* sensor */
    _sensor sensor0 = cfg->sensor;
    _sensor sensor1 = cfg->sensor1;

    // sinfo = create_sensor_info(cfg->sensor.model);
    int sensor_id = 0;
#if 1  // sensor 1
    sinfo = create_sensor_info(sensor1, sensor_id);
//    LOG_DEBUG("id: " << sensor1.id);
    LOG_INFO("Sensor: " << sinfo.name << " address: " << sinfo.i2c.addr << " adaptor: " << sinfo.i2c.i2c_adapter_id << " id: " << sinfo.sensor_id);
    ret = hal::isp::add_sensor(&sinfo);
    LOG_DEBUG_OR_ERROR_AND_EXIT(ret, "IMP_ISP_AddSensor-1(&sinfo)");
    sensor_id++;
#endif
#if 1  
    // sensor 0
    usleep(1000);
   sinfo = create_sensor_info(sensor0, sensor_id);
//    LOG_DEBUG("id: " << sensor0.id);  
    LOG_INFO("Sensor: " << sinfo.name << " address: " << sinfo.i2c.addr << " adaptor: " << sinfo.i2c.i2c_adapter_id << " id: " << sinfo.sensor_id);
    ret = hal::isp::add_sensor(&sinfo);
    LOG_DEBUG_OR_ERROR_AND_EXIT(ret, "IMP_ISP_AddSensor-0(&sinfo)");
#endif

    ret = hal::isp::enable_sensor(&sinfo);
    LOG_DEBUG_OR_ERROR_AND_EXIT(ret, "IMP_ISP_EnableSensor-1()");


    /* system */
    ret = IMP_System_Init();
    LOG_DEBUG_OR_ERROR_AND_EXIT(ret, "IMP_System_Init()");

    ret = IMP_ISP_EnableTuning();
    LOG_DEBUG_OR_ERROR_AND_EXIT(ret, "IMP_ISP_EnableTuning()");

#if !defined(NO_TUNINGS)
    ret = hal::isp::set_contrast(cfg->image.contrast);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetContrast(" << cfg->image.contrast << ")");

    ret = hal::isp::set_sharpness(cfg->image.sharpness);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetSharpness(" << cfg->image.sharpness << ")");

    ret = hal::isp::set_saturation(cfg->image.saturation);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetSaturation(" << cfg->image.saturation << ")");

    ret = hal::isp::set_brightness(cfg->image.brightness);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetBrightness(" << cfg->image.brightness << ")");

    ret = hal::isp::set_contrast(cfg->image.contrast);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetContrast(" << cfg->image.contrast << ")");

    ret = hal::isp::set_sharpness(cfg->image.sharpness);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetSharpness(" << cfg->image.sharpness << ")");

    ret = hal::isp::set_saturation(cfg->image.saturation);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetSaturation(" << cfg->image.saturation << ")");

    ret = hal::isp::set_brightness(cfg->image.brightness);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetBrightness(" << cfg->image.brightness << ")");

#if !defined(PLATFORM_T21)
    ret = hal::isp::set_sinter_strength(cfg->image.sinter_strength);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetSinterStrength(" << cfg->image.sinter_strength << ")");
#endif

    ret = hal::isp::set_temper_strength(cfg->image.temper_strength);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetTemperStrength(" << cfg->image.temper_strength << ")");

    ret = hal::isp::set_hflip(cfg->image.hflip);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetISPHflip(" << cfg->image.hflip << ")");

    ret = hal::isp::set_vflip(cfg->image.vflip);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetISPVflip(" << cfg->image.vflip << ")");

    ret = hal::isp::set_running_mode(cfg->image.running_mode);  // cfg default
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetISPRunningMode(" << cfg->image.running_mode << ")");

    // Configurable ISP bypass  should be done before isp_open
//    ret = hal::isp::set_isp_bypass(cfg->image.isp_bypass);
//    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetISPBypass(" << (cfg->image.isp_bypass ? IMPISP_TUNING_OPS_MODE_ENABLE : IMPISP_TUNING_OPS_MODE_DISABLE) << ")");
    // ### TW:  Configurable ISP module functions: testing
#if defined(NO_TUNINGS)
    IMPISPModuleCtl ispmodule;
    memset(&ispmodule, 0, sizeof(IMPISPModuleCtl));
    ret = IMP_ISP_Tuning_GetModuleControl(&ispmodule);
    LOG_DEBUG_OR_ERROR(ret, "* IMP_ISP_Tuning_SetModuleControl-bitBypassAG(" << ispmodule.bitBypassAG << ")");
		ispmodule.bitBypassBLC= 1; /* [0]  */
		ispmodule.bitBypassGIB= 1; /* [1]  */
		ispmodule.bitBypassAG= 1; /* [2]  */
		ispmodule.bitBypassDPC= 1; /* [4]  */
		ispmodule.bitBypassRDNS= 1; /* [5]	*/
		ispmodule.bitBypassLSC= 1; /* [6]  */
		ispmodule.bitBypassADR= 1; /* [7]	 */
		ispmodule.bitBypassDMSC= 1; /* [8]	 */
		ispmodule.bitBypassCCM= 1; /* [9]  */
		ispmodule.bitBypassGAMMA= 1; /* [10]  */
		ispmodule.bitBypassDEFOG= 1; /* [11]	 */
		ispmodule.bitBypassCSC= 1; /* [12]	 */
		ispmodule.bitBypassCLM= 1; /* [13]	 */
		ispmodule.bitBypassSP= 1; /* [14]  */
		ispmodule.bitBypassYDNS= 1; /* [15]	 */
		ispmodule.bitBypassBCSH= 1; /* [16]	 */
		ispmodule.bitBypassSDNS= 1; /* [17]	 */
		ispmodule.bitBypassHLDC= 1; /* [18]	 */
		ispmodule.bitRsv= 12; /* [19 ~ 30]	*/
		ispmodule.bitBypassMDNS= 1; /* [31]  */

//    ispmodule.bitBypassAG = cfg->image.isp_bypass;
    ret = IMP_ISP_Tuning_SetModuleControl(&ispmodule);
    LOG_DEBUG_OR_ERROR(ret, "# IMP_ISP_Tuning_SetModuleControl-bitBypassAG(" << ispmodule.bitBypassAG << ")");
#endif
    IMPISPAntiflickerAttr flickerAttr;
    memset(&flickerAttr, 0, sizeof(IMPISPAntiflickerAttr));
    ret = hal::isp::set_anti_flicker(cfg->image.anti_flicker);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetAntiFlickerAttr(" << cfg->image.anti_flicker << ")");

#if !defined(PLATFORM_T21)
    ret = hal::isp::set_ae_compensation(cfg->image.ae_compensation);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetAeComp(" << cfg->image.ae_compensation << ")");
#endif

    ret = hal::isp::set_max_again(cfg->image.max_again);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetMaxAgain(" << cfg->image.max_again << ")");

    ret = hal::isp::set_max_dgain(cfg->image.max_dgain);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetMaxDgain(" << cfg->image.max_dgain << ")");

    ret = hal::isp::set_wb(cfg->image.core_wb_mode, cfg->image.wb_rgain, cfg->image.wb_bgain);
    if (ret != 0)
    {
        LOG_ERROR("Unable to set white balance. Mode: " << cfg->image.core_wb_mode << ", rgain: "
                                                        << cfg->image.wb_rgain << ", bgain: " << cfg->image.wb_bgain);
    }
    else
    {
        LOG_DEBUG("Set white balance. Mode: " << cfg->image.core_wb_mode << ", rgain: "
                                              << cfg->image.wb_rgain << ", bgain: " << cfg->image.wb_bgain);
    }

#if defined(PLATFORM_T23) || defined(PLATFORM_T31) || defined(PLATFORM_C100)
    ret = hal::isp::set_hue(cfg->image.hue);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetBcshHue(" << cfg->image.hue << ")");

    ret = hal::isp::set_defog_strength(static_cast<uint8_t>(cfg->image.defog_strength));
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetDefog_Strength(" << cfg->image.defog_strength << ")");

    ret = hal::isp::set_dpc_strength(cfg->image.dpc_strength);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetDPC_Strength(" << cfg->image.dpc_strength << ")");
#endif
#if defined(PLATFORM_T21) || defined(PLATFORM_T23) || defined(PLATFORM_T31) || defined(PLATFORM_C100)
    ret = hal::isp::set_drc_strength(cfg->image.drc_strength);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetDRC_Strength(" << cfg->image.drc_strength << ")");
#endif

#if defined(PLATFORM_T23) || defined(PLATFORM_T31) || defined(PLATFORM_C100)
    if (cfg->image.backlight_compensation > 0)
    {
        ret = hal::isp::set_backlight_comp(cfg->image.backlight_compensation);
        LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetBacklightComp(" << cfg->image.backlight_compensation << ")");
    }
    else if (cfg->image.highlight_depress > 0)
    {
        ret = hal::isp::set_highlight_depress(cfg->image.highlight_depress);
        LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetHiLightDepress(" << cfg->image.highlight_depress << ")");
    }
#elif defined(PLATFORM_T21) || defined(PLATFORM_T30)
    ret = hal::isp::set_highlight_depress(cfg->image.highlight_depress);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Tuning_SetHiLightDepress(" << cfg->image.highlight_depress << ")");
#endif

    LOG_DEBUG("ISP Tuning Defaults set");

    // Clamp sensor FPS to a sane value based on stream0 desired FPS and sensor limits
    int desired_sensor_fps = cfg->stream0.fps > 0 ? cfg->stream0.fps : 25;
    // cfg->sensor.fps is read from /proc/jz/sensor/max_fps; min from min_fps
    if (cfg->sensor.min_fps > 0 && desired_sensor_fps < cfg->sensor.min_fps) {
        desired_sensor_fps = cfg->sensor.min_fps;
    }
    if (cfg->sensor.fps > 0 && desired_sensor_fps > (int)cfg->sensor.fps) {
        desired_sensor_fps = cfg->sensor.fps;
    }
    ret = IMP_ISP_Tuning_SetSensorFPS(desired_sensor_fps, 1);
    LOG_DEBUG_OR_ERROR_AND_EXIT(ret, "IMP_ISP_Tuning_SetSensorFPS(" << desired_sensor_fps << ", 1)");

#if defined(PLATFORM_T21)
    //T20 T21 only set FPS if it is read after set.
    uint32_t fps_num, fps_den;
    ret = IMP_ISP_Tuning_GetSensorFPS(&fps_num, &fps_den);
    LOG_DEBUG_OR_ERROR_AND_EXIT(ret, "IMP_ISP_Tuning_GetSensorFPS(" << fps_num << ", " << fps_den << ")");
#endif

    // Set the ISP to DAY on launch
    ret = hal::isp::set_running_mode(IMPISP_RUNNING_MODE_DAY);
    LOG_DEBUG_OR_ERROR_AND_EXIT(ret, "IMP_ISP_Tuning_SetISPRunningMode(" << IMPISP_RUNNING_MODE_DAY << ")");
#endif // #if !defined(NO_TUNINGS)

LOG_INFO("IMPSystem Done!");
    return ret;
}

int IMPSystem::destroy()
{
    int ret;

    ret = IMP_System_Exit();
    LOG_DEBUG_OR_ERROR(ret, "IMP_System_Exit()");

    ret = hal::isp::disable_sensor();
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_DisableSensor()");

    ret = hal::isp::del_sensor(&sinfo);
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_DelSensor()");

    ret = IMP_ISP_DisableTuning();
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_DisableTuning()");

    ret = IMP_ISP_Close();
    LOG_DEBUG_OR_ERROR(ret, "IMP_ISP_Close()");

    return 0;
}
