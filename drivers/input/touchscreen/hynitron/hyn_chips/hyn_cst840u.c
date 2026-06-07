#include "../hyn_core.h"


#include "cst840u_fw.h"

#define BOOT_I2C_ADDR   (0x5A)
#define MAIN_I2C_ADDR   (0x33) //use 2 slave addr

#define PART_NO_EN          (0)
#define MODULE_ID_EN        (0)

#define cst840u_BIN_SIZE    (44*1024) //(40*1024)
#define MODULE_ID_ADDR      (0xA400)
#define PARTNUM_ADDR        (0xFF10)

static struct hyn_ts_data *hyn_840udata = NULL;
static const u8 gest_map_tbl[] = { 
    IDX_POWER,   //GESTURE_LABEL_CLICK, 
    IDX_POWER,  //GESTURE_LABEL_CLICK2, 
    IDX_UP,     //GESTURE_LABEL_TOP, 
    IDX_DOWN,   //GESTURE_LABEL_BOTTOM, 
    IDX_LEFT,   //GESTURE_LABEL_LEFT, 
    IDX_RIGHT,  //GESTURE_LABEL_RIGHT,
    IDX_C,      //GESTURE_LABEL_C,
    IDX_e,      //GESTURE_LABEL_E,
    IDX_V,      //GESTURE_LABEL_v,
    IDX_NULL,   //GESTURE_LABEL_^,
    IDX_NULL,   //GESTURE_LABEL_>,
    IDX_NULL,   //GESTURE_LABEL_<,
    IDX_M,      //GESTURE_LABEL_M,
    IDX_W,      //GESTURE_LABEL_W,
    IDX_O,      //GESTURE_LABEL_O,
    IDX_S,      //GESTURE_LABEL_S,
    IDX_Z,      //GESTURE_LABEL_Z
};

static const struct hyn_chip_series hyn_xx_fw[] = {
    {0xCACA2213,0xffffffff,"cst840u",(u8*)fw_module},//if PART_NO_EN==0 use default chip
    {0,0,"",NULL}
};

static int cst840u_updata_judge(u8 *p_fw, u32 len);
static u32 cst840u_read_checksum(void);
static int cst840u_enter_boot(void);
static u32 cst840u_fread_word(u32 addr);
static void cst840u_rst(void);
// static u32 cst840u_read_file_checksum(u8 *p_fw, u32 len);
static int cst840u_read_file_addr(u32 part_no, u32 module_id);
#if MODULE_ID_EN
    static int cst840u_judge_module(void);
#endif

static int cst840u_init(struct hyn_ts_data* ts_data)
{
    int ret = 0;
    u32 read_part_no,module_id;
    HYN_ENTER();
    hyn_840udata = ts_data;
    hyn_840udata->fw_updata_len = cst840u_BIN_SIZE;
#if MODULE_ID_EN
    if (cst840u_judge_module())
#endif
    {
        ret = cst840u_enter_boot();
        if(ret) {
            HYN_ERROR("cst840u_enter_boot failed");
            return -1;
        }

        read_part_no = cst840u_fread_word(PARTNUM_ADDR);
        module_id =  cst840u_fread_word(MODULE_ID_ADDR);
        HYN_INFO("read_part_no:0x%08x module_id:0x%08x",read_part_no,module_id);
        // module_id = 0xffffffff;

        hyn_840udata->hw_info.fw_module_id = module_id;
        hyn_840udata->hw_info.ic_part_no = read_part_no;
        hyn_840udata->hw_info.ic_fw_checksum = cst840u_read_checksum();
        hyn_set_i2c_addr(hyn_840udata,MAIN_I2C_ADDR);
        cst840u_rst(); //exit boot
        mdelay(50);
    }
    
    cst840u_read_file_addr(hyn_840udata->hw_info.ic_part_no, hyn_840udata->hw_info.fw_module_id);
    hyn_840udata->need_updata_fw = cst840u_updata_judge(hyn_840udata->fw_updata_addr, hyn_840udata->fw_updata_len);
    // hyn_840udata->need_updata_fw = 1;
    HYN_INFO("not need updata FW !!!");
    if (hyn_840udata->need_updata_fw) {
        HYN_INFO("need updata FW !!!");
    }
    return 0;
}


static int cst840u_report(void)
{
    u8 buf[80],i=0;
    u8 finger_num = 0,key_num=0,report_typ= 0,key_state=0,key_id = 0,tmp_dat=0;
    int ret = 0,retry = 2;

    while(retry--){ //read point
        ret = hyn_wr_reg(hyn_840udata,0xD0070000,0x80|4,buf,9); 
        report_typ = buf[2];//FF:pos F0:ges E0:prox
        finger_num = buf[3]&0x0f;
        // HYN_INFO("finger_num = %d", finger_num);
        key_num    = ((buf[3]&0xf0)>>4);
        // HYN_INFO("key_num = %d", key_num);
        if(finger_num+key_num <= MAX_POINTS_REPORT){
            if(key_num + finger_num > 1){ 
                ret |= hyn_read_data(hyn_840udata,&buf[9],(key_num + finger_num -1)*5);
            }
            if(hyn_sum16(0x55,&buf[4],(key_num + finger_num)*5) != (buf[0] | buf[1]<<8)){
                ret = -1;
            }
        }
        else {
            ret = -2;
        }
        if(ret && retry) continue;
        ret |= hyn_wr_reg(hyn_840udata,0xD00002AB,4,buf,0);
        if(ret == 0) {
            break;
        }
    }
    if(ret) return ret;

    if(report_typ==0xff){
        if(key_num){
            key_id    = buf[8]&0x0f;
            key_state = buf[8]>>4;
            if(hyn_840udata->rp_buf.report_need == REPORT_NONE){ //Mutually exclusive reporting of coordinates and keys
                hyn_840udata->rp_buf.report_need |= REPORT_KEY;
            }
            hyn_840udata->rp_buf.key_id = key_id;
            hyn_840udata->rp_buf.key_state = key_state !=0 ? 1:0; 
        }

        if(finger_num){ //pos
            u16 index = 0;
            u8 touch_down = 0;
            if(hyn_840udata->rp_buf.report_need == REPORT_NONE){ //Mutually exclusive reporting of coordinates and keys
                hyn_840udata->rp_buf.report_need |= REPORT_POS;
            }    
            hyn_840udata->rp_buf.rep_num = finger_num;
            for(i = 0; i < finger_num; i++){
                index = (key_num+i)*5;
                hyn_840udata->rp_buf.pos_info[i].pos_id = buf[index+8]&0x0F;
                hyn_840udata->rp_buf.pos_info[i].event =  buf[index+8]>>4;
                hyn_840udata->rp_buf.pos_info[i].pos_x =  buf[index+4] + ((u16)(buf[index+7]&0x0F) <<8); //x is rx direction
                hyn_840udata->rp_buf.pos_info[i].pos_y =  buf[index+5] + ((u16)(buf[index+7]&0xf0) <<4);
                hyn_840udata->rp_buf.pos_info[i].pres_z = buf[index+6];
                if(hyn_840udata->rp_buf.pos_info[i].event){
                    touch_down++;
                }
            }
            if(0== touch_down){
                hyn_840udata->rp_buf.rep_num = 0;
            }
        }
    }
    else if(report_typ == 0xF0){ //gesture
        tmp_dat = buf[8]&0xff;
        if((tmp_dat&0x7F) < sizeof(gest_map_tbl) && gest_map_tbl[tmp_dat] != IDX_NULL){  
            hyn_840udata->gesture_id = gest_map_tbl[tmp_dat];
            hyn_840udata->rp_buf.report_need |= REPORT_GES;
            HYN_INFO("gesture_id:%d",tmp_dat);
        }
    }else if(report_typ == 0xE0){//proximity 
        u8 state = buf[4] ? PS_FAR_AWAY : PS_NEAR;
        if(hyn_840udata->prox_is_enable && hyn_840udata->prox_state != state){
            hyn_840udata->prox_state =state;
            hyn_840udata->rp_buf.report_need |= REPORT_PROX;
        }
    }
    return ret;
}

static int cst840u_prox_handle(u8 cmd)
{
    int ret = 0;
    switch(cmd){
        case 1: //enable
            hyn_840udata->prox_is_enable = 1;
            hyn_840udata->prox_state = 0xAA;
            ret = hyn_wr_reg(hyn_840udata,0xD004B001,4,NULL,0);
            break;
        case 0: //disable
            hyn_840udata->prox_is_enable = 0;
            hyn_840udata->prox_state = 0xAA;
            ret = hyn_wr_reg(hyn_840udata,0xD004B000,4,NULL,0);
            break;
        default: 
            break;
    }
    return ret;
}

static int cst840u_set_workmode(enum work_mode mode,u8 enable)
{
    int ret = 0,retry;//,i;
    u32 reg_cmd[2] = {0};
    u8  reg_buf[4] = {0};

    
    HYN_INFO("work_mode:%d, enable:%d",mode, enable);
    // if (enable != NONE) {
        hyn_esdcheck_switch(hyn_840udata,enable);
    // }
    if (mode < 10) {
        hyn_840udata->work_mode = mode;
    }

    if (mode == ENTER_BOOT_MODE) {
        ret = cst840u_enter_boot();
        if (ret) {
            HYN_INFO("enter_boot_mode ret:%d", ret);
        }
        return 1;
    }

    if(FAC_TEST_MODE == mode) {
        cst840u_rst();
        msleep(50);
    }

    for (retry = 0; retry < 5; retry++) {
        ret = hyn_wr_reg(hyn_840udata,0xD0000400,4,NULL,0); //disable lp i2c plu
        ret |= hyn_wr_reg(hyn_840udata,0xD02F0000,4,reg_buf,4);
        if(ret || U8TO16(reg_buf[2],reg_buf[3]) != 0x0400) {
            HYN_ERROR("disable lp i2c plu fail ret:%d buf:%x", ret, U8TO16(reg_buf[2],reg_buf[3]));
            mdelay(1);
            continue;
        }
        mdelay(1);
        break;
    }
    if (retry == 5) {
        HYN_INFO("disable lp i2c plu fail");
        return 1;
    }

    switch (mode) {
        case NOMAL_MODE:
            hyn_irq_set(hyn_840udata,ENABLE);
            reg_cmd[0] = 0xD0000000;
            reg_cmd[1] = 0xD0000100;
            break;
        case GESTURE_MODE:
            reg_cmd[0] = 0xD0000C01;
            break;
        case LP_MODE:
            reg_cmd[0] = 0xD00004AB;
            break;
        case DIFF_MODE:
        case RAWDATA_MODE:
        case BASELINE_MODE:
        case CALIBRATE_MODE:
            reg_cmd[0] = 0xD00002AB;
            reg_cmd[1] = 0xD00001AB;
            break;
        case FAC_TEST_MODE:
            reg_cmd[0] = 0xD00002AB;
            reg_cmd[1] = 0xD00000AB;
            break;
        case DEEPSLEEP:
            reg_cmd[0] = 0xD00022AB;
            break;
        default :
            ret = -2;
            break;
    }

    for (retry=0; retry < 3; retry++) {
        ret = hyn_wr_reg(hyn_840udata,reg_cmd[0],4,NULL,0);
        ret |= hyn_wr_reg(hyn_840udata,0xD02F0000,4,reg_buf,4);
        if (ret || (reg_cmd[0] & 0xFFFF) != U8TO16(reg_buf[2],reg_buf[3])) {
            HYN_ERROR("err mode %d ret:%d cmd:%x buf:%x", mode, ret, reg_cmd[0], U8TO16(reg_buf[2],reg_buf[3]));
            mdelay(1);
            continue;
        }
        if (reg_cmd[1]) {
            ret = hyn_wr_reg(hyn_840udata,reg_cmd[1],4,NULL,0);
            ret |= hyn_wr_reg(hyn_840udata,0xD02F0000,4,reg_buf,4);
            if (ret || (reg_cmd[1] & 0xFFFF) != U8TO16(reg_buf[2],reg_buf[3])) {
                HYN_ERROR("err mode %d ret:%d cmd:%x buf:%x", mode, ret, reg_cmd[1], U8TO16(reg_buf[2],reg_buf[3]));
                mdelay(1);
                continue;
            }
        }

        ret = hyn_wr_reg(hyn_840udata,0xD0000401,4,0,0);
        ret |= hyn_wr_reg(hyn_840udata,0xD02F0000,4,reg_buf,4);
        if(ret || U8TO16(reg_buf[2],reg_buf[3]) != 0x0401) {
            HYN_ERROR("disable lp i2c plu fail ret:%d buf:%x", ret, U8TO16(reg_buf[2],reg_buf[3]));
            mdelay(1);
            continue;
        }

        break;
    }
    if (retry == 3) {
        HYN_ERROR("err set mode %d", mode);
        return 1;
    }

    if (mode == FAC_TEST_MODE) {
        msleep(50); //wait  switch to fac mode
    }
    return 0;
}


static int cst840u_supend(void)
{
    HYN_ENTER();
    cst840u_set_workmode(DEEPSLEEP,DISABLE);
    return 0;
}

static int cst840u_resum(void)
{
    HYN_ENTER();
    cst840u_rst();
    msleep(50);
    cst840u_set_workmode(NOMAL_MODE,ENABLE);
    return 0;
}

static void cst840u_rst(void)
{
    HYN_ENTER();
    if(hyn_840udata->work_mode==ENTER_BOOT_MODE){
        hyn_set_i2c_addr(hyn_840udata,MAIN_I2C_ADDR);
    }
    gpio_set_value(hyn_840udata->plat_data.reset_gpio,0);
    msleep(8);
    gpio_set_value(hyn_840udata->plat_data.reset_gpio,1);
}

static int cst840u_wait_ready(u16 times,u8 ms,u16 reg,u16 check_vlue)
{
    int ret = 0;
    u8 buf[4];
    while(times--){
        ret = hyn_wr_reg(hyn_840udata,reg,2,buf,2);
        if(0==ret && U8TO16(buf[0],buf[1])==check_vlue){
            return 0;
        }
        mdelay(ms);
    }
    return -1;
}

#if MODULE_ID_EN
static int cst840u_judge_module(void) {
    int ret = 0;
    uint8_t buf[12];
    uint8_t retry = 3;
    uint32_t part_no = 0;
    uint32_t module_id = 0;
    uint32_t check_sum = 0;
    HYN_ENTER();
    cst840u_rst();
    mdelay(50);
    for (; retry > 0; retry--) {
        //module id
        ret = hyn_wr_reg(hyn_840udata,0xD0064800,4,buf, 12);
        if(ret) {
            HYN_ERROR("w D0 06 48 00 err");
            continue;
        }
        module_id = U8TO32(buf[3], buf[2], buf[1], buf[0]);
        part_no   = U8TO32(buf[7], buf[6], buf[5], buf[4]);
        check_sum = U8TO32(buf[11], buf[10], buf[9], buf[8]);
        if ((part_no >> 16) == 0xCACA) {
            part_no &= 0xffff;
            break;
        }
        HYN_INFO("continue: module_id:0x%08x, part_no:0x%08x, check_sum:0x%08x", module_id, part_no, check_sum);
    }

    HYN_INFO("part_no: 0x%08x", part_no);
    HYN_INFO("module_id: 0x%08x", module_id);
    HYN_INFO("check_sum: 0x%08x", check_sum);
    if (retry == 0) {
        HYN_INFO("main read info err");
        return FALSE;
    }

    // module_id = 0xffffffff;
    hyn_840udata->hw_info.fw_module_id = module_id;
    hyn_840udata->hw_info.ic_part_no = part_no;
    hyn_840udata->hw_info.ic_fw_checksum = check_sum;
    hyn_840udata->boot_is_pass = 1;
    
    return TRUE;
}
#endif 

static int cst840u_enter_boot(void)
{
    int retry = 0,ret = 0;
    hyn_set_i2c_addr(hyn_840udata,BOOT_I2C_ADDR);
    while(++retry<20){
        cst840u_rst();
        mdelay(12+retry);
        ret = hyn_wr_reg(hyn_840udata,0xA001A8,3,0,0);
        if(ret < 0){
            continue;
        }
        if(0==cst840u_wait_ready(10,2,0xA002,0x22DD)){
            return 0;
        }
    }
    return -1;
}

static int cst840u_updata_tpinfo(void)
{
    u8 buf[60];
    struct tp_info *ic = &hyn_840udata->hw_info;
    int ret = 0;
    int retry = 5;
    HYN_ENTER();
    while(--retry){
        //get all config info
        ret |= cst840u_set_workmode(NOMAL_MODE,ENABLE);
        ret |= hyn_wr_reg(hyn_840udata,0xD0030000,0x80|4,buf,50);
        if(ret == 0 && buf[3]==0xCA && buf[2]==0xCA) break; 
        mdelay(1);
        // ret |= hyn_wr_reg(hyn_840udata,0xD0000400,4,buf,0);
    }

    if(ret || retry==0) {
        HYN_ERROR("cst840u_updata_tpinfo failed");
        return -1;
    }

    ic->fw_project_id = U8TO32(buf[39],buf[38],buf[37],buf[36]);
    ic->fw_chip_type = U8TO32(buf[3],buf[2],buf[1],buf[0]);
    ic->fw_ver = U8TO32(buf[35],buf[34],buf[33],buf[32]);
    HYN_INFO("IC_INFO  project_id:%04x type:%04x fw_ver:%04x checksum:%04x",ic->fw_project_id,ic->fw_chip_type,ic->fw_ver,ic->ic_fw_checksum);

    ic->fw_sensor_txnum = buf[48];
    ic->fw_sensor_rxnum = buf[49];
    ic->fw_key_num = buf[27];
    ic->fw_res_y = (buf[31]<<8)|buf[30];
    ic->fw_res_x = (buf[29]<<8)|buf[28];
    HYN_INFO("IC_INFO  tx:%d rx:%d key:%d res-x:%d res-y:%d",ic->fw_sensor_txnum,ic->fw_sensor_rxnum,ic->fw_key_num,ic->fw_res_x,ic->fw_res_y);

    return 0;
}

static u32 cst840u_fread_word(u32 addr)
{
    int ret;
    u8 rec_buf[4],retry;
    u32 read_word = 0;

    retry = 3;
    while(retry--){
        ret = hyn_wr_reg(hyn_840udata,U8TO32(0xA0,0x06,(addr&0xFF),((addr>>8)&0xFF)),4,NULL,0); //set addr
        ret  |= hyn_wr_reg(hyn_840udata,0xA0080400,4,0,0); //set len
        ret  |= hyn_wr_reg(hyn_840udata,0xA00A0300,4,0,0); //?
        ret  |= hyn_wr_reg(hyn_840udata,0xA004D2,3,NULL,0);	//trig read
        if(ret ==0) break;
    }
    if(ret) return 0;

    retry = 20;
	while(retry--){
        ret = hyn_wr_reg(hyn_840udata,0xA020,2,rec_buf,2);	
		if(ret==0 && rec_buf[0]==0xD2 && rec_buf[1]==0x88){
            ret  |= hyn_wr_reg(hyn_840udata,0xA040,2,rec_buf,4);
            if(ret ==0){
                read_word = U8TO32(rec_buf[3],rec_buf[2],rec_buf[1],rec_buf[0]);
                break;
            }
		}
		mdelay(1);
	}
	return read_word;
}

static int  read_checksum(u16 start_val,u16 start_addr,u16 len ,u32 *check_sum)
{
    int ret,retry = 3;
    u8 buf[8] = {0};
    while(retry--){
        ret = hyn_wr_reg(hyn_840udata,U8TO32(0xA0,0x06,start_addr&0xFF,start_addr>>8),4,0,0);
        ret |= hyn_wr_reg(hyn_840udata,U8TO32(0xA0,0x08,len&0xFF,len>>8),4,0,0);
        ret |= hyn_wr_reg(hyn_840udata,U8TO32(0xA0,0x0A,start_val&0xFF,start_val>>8),4,0,0);
        ret |= hyn_wr_reg(hyn_840udata,0xA004D6,3,0,0);
        if(ret) continue;
        mdelay(len/0xc00 + 1);
        ret = cst840u_wait_ready(20,2,0xA020,0xD688);
        ret |= hyn_wr_reg(hyn_840udata,0xA040,2,buf,5);
        if(ret == 0 && buf[0] == 0xCA){
            *check_sum = U8TO32(buf[4],buf[3],buf[2],buf[1]);
            break;
        }
        else{
            ret = -1;
            continue;
        }
    }
    return ret ? -1:0;
}

static u32 cst840u_read_checksum(void)
{
    int ret = -1;
    u32 chipSum1 = 0,chipSum2 = 0,chipSum3=0,totalSum = 0;
    hyn_840udata->boot_is_pass = 0;
    if(0==read_checksum(0,0,0x9000,&chipSum1)){
        if (0==read_checksum(1,0xb000,0x0ffc,&chipSum2)) {
            ret = 0;
            if (hyn_840udata->fw_updata_len >= 44*1024)
                ret =read_checksum(1,0xc000,0x0ffc,&chipSum3);
        }
    }
    if(ret ==0){
        if (hyn_840udata->fw_updata_len >= 44*1024)
            totalSum = chipSum1 + chipSum2*2 - 0x55 + chipSum3*2 - 0x55; 
        else
            totalSum = chipSum1 + chipSum2*2 - 0x55; 
        hyn_840udata->boot_is_pass = 1;
    }
    HYN_INFO("chipSum1:%04x chipSum2:%04x chipSum3:%04x totalSum:%04x",chipSum1,chipSum2,chipSum3,totalSum);
    return totalSum;
}

static int cst840u_read_file_addr(u32 partno, u32 moduleId) {
    int ret = 0, i = 0;
    if(moduleId > 10) moduleId = 0xffffffff;
    for(i = 0; ;i++) {
#if PART_NO_EN
        if(hyn_xx_fw[i].part_no == partno && hyn_xx_fw[i].moudle_id == moduleId)
#else
        if(hyn_xx_fw[i].moudle_id == moduleId)
#endif
        {  
            hyn_840udata->fw_updata_addr = hyn_xx_fw[i].fw_bin;
            HYN_INFO("chip %s match fw success ,partNo check is [%s]", hyn_xx_fw[i].chip_name,PART_NO_EN ? "enable":"disable");
            break;
        }
        if(hyn_xx_fw[i].part_no == 0 && hyn_xx_fw[i].moudle_id == 0) {
            HYN_INFO("unknown chip or unknown moudle_id use hyn_xx_fw[0]");
            ret = -1;
            break;
        }
    }
    return ret;
}

// static u32 cst840u_read_file_checksum(u8 *p_fw, u32 len)
// {
//     u32 *main_checksum;
// 	u32 *config_checksum;
//     u32 cal_config_checksum;
//     u32 cal_main_checksum;
//     u32 f_check_all;

//     int config_bin_len = 2*1024;
//     config_checksum  = (u32*)(p_fw + (config_bin_len - 4));
//     main_checksum    = (u32*)(p_fw + (len - 4));
//     HYN_INFO("config_checksum %04x main_checksum %04x ", *config_checksum, *main_checksum);

//     cal_config_checksum = hyn_sum32(0x55,(u32*)p_fw,(config_bin_len - 4)/4);

//     cal_main_checksum = hyn_sum32(0x55,(u32*)(p_fw+config_bin_len),(len - config_bin_len - 4)/4);

//     if (*config_checksum != cal_config_checksum || *main_checksum != cal_main_checksum) {
//         HYN_INFO(".h file checksum erro: cal_config_checksum %04x cal_main_checksum %04x",cal_config_checksum, cal_main_checksum);
//         return 0;
//     }

//     f_check_all = ((*main_checksum) + (*config_checksum) ) * 2 - 0x55;
//     HYN_INFO("f_check_all %04x ", f_check_all);

//     return f_check_all;
// }

static int cst840u_updata_judge(u8 *p_fw, u32 len)
{
    u32 f_check_all,f_checksum,f_fw_ver,f_ictype,f_fw_project_id;
    u32 info_offset,check_offset;
    int ret;
    u8 *p_data;
    if (hyn_840udata->fw_updata_len >= 44*1024){
        info_offset = 40*1024;
        check_offset = 44*1024;
    }
    else{
        info_offset = 35*1024;
        check_offset = 40*1024;
    }
    p_data =  p_fw + info_offset;
    f_fw_project_id = U8TO32(p_data[39],p_data[38],p_data[37],p_data[36]);
    f_ictype        = U8TO32(p_data[3],p_data[2],p_data[1],p_data[0]);
    f_fw_ver        = U8TO32(p_data[35],p_data[34],p_data[33],p_data[32]);

    p_data = p_fw + check_offset;
    f_checksum = U8TO32(p_data[3],p_data[2],p_data[1],p_data[0]);
    HYN_INFO("BIN_INFO project_id:%04x type:%04x fw_ver:%04x checksum:%04x",f_fw_project_id,f_ictype,f_fw_ver,f_checksum);
    f_check_all = hyn_sum32(0x55,(u32*)p_fw,len/4);
    if (f_checksum != f_check_all) {
        HYN_ERROR("file checksum erro not updata");
        return 0;
    }
    
    if (hyn_840udata->boot_is_pass == 0) {
        HYN_ERROR("emty chip, need updata");
        return 1;
    }

    ret = cst840u_updata_tpinfo();
    if(ret) {
        HYN_ERROR("get tpinfo failed need updata");
        return 1;
    }
    
    if (f_ictype != hyn_840udata->hw_info.fw_chip_type) {
        HYN_ERROR("ic type not match not updata");
        return 0;
    }

    if (f_fw_ver > hyn_840udata->hw_info.fw_ver && f_checksum != hyn_840udata->hw_info.ic_fw_checksum) {
    // if (f_checksum != hyn_840udata->hw_info.ic_fw_checksum) {
        HYN_INFO("need updata");
        return 1;
    }
    else {
        HYN_INFO("no need updata");
    }

    return 0;
}

static int cst840u_erase_flash(u32 start_addr, u32 len, u16 type)
{
    int ret = 0;
    // HYN_INFO("addr:%04x len:%04x",0xA0060000+U16REV(start_addr),0xA0080000+U16REV(len));
    ret = hyn_wr_reg(hyn_840udata,0xA0060000+U16REV(start_addr),4,0,0);  //addr
    ret |= hyn_wr_reg(hyn_840udata,0xA0080000+U16REV(len),4,0,0); //len
    ret |= hyn_wr_reg(hyn_840udata,0xA00A0000+U16REV(type),4,0,0); //type
    ret |= hyn_wr_reg(hyn_840udata,0xA018CACA,4,0,0); //key
    ret |= hyn_wr_reg(hyn_840udata,0xA004E0,3,0,0); //trig
    if(ret) return -1;

    mdelay(20); //wait finsh earse flash
    ret = cst840u_wait_ready(100,1,0xA020,0xE088);
    return ret;
}

static int cst840u_updata_fw(u8 *bin_addr, u32 len)
{
    #define PKG_SIZE    (1024)
    int i,ret = -1, retry_fw= 4,pak_num;
	u8 i2c_buf[PKG_SIZE+10] , *p_bin_addr;
	u16 eep_addr = 0;
    u32 eep_len;
    u32 fw_checksum = 0;
    HYN_ENTER();
    HYN_INFO("len = %d",len);
    hyn_840udata->fw_updata_process = 0;
    if(len < cst840u_BIN_SIZE){
        HYN_ERROR("bin len erro");
        goto UPDATA_END;
    }
    len = cst840u_BIN_SIZE;
    if(0 == hyn_840udata->fw_file_name[0]){
        p_bin_addr = bin_addr + len;
        fw_checksum = U8TO32(p_bin_addr[3],p_bin_addr[2],p_bin_addr[1],p_bin_addr[0]);
    }
    else{
        ret = copy_for_updata(hyn_840udata,i2c_buf,cst840u_BIN_SIZE,4);
        if(ret)  goto UPDATA_END;
        fw_checksum = U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]);
    }
    HYN_INFO("fw_checksum_all:%04x",fw_checksum);
    hyn_irq_set(hyn_840udata,DISABLE);
    hyn_esdcheck_switch(hyn_840udata,DISABLE);
    pak_num = len/PKG_SIZE;
    while(--retry_fw){
        ret = cst840u_enter_boot();
        if(ret){
            HYN_INFO("cst840u_enter_boot faill");
            continue;
        }
        if(cst840u_erase_flash(0x0000,0x8000,0x02)) continue; //erase 32k 0x0000~0x8000
        if(cst840u_erase_flash(0x8000,0x1000,0x01)) continue; //erase 4k 0x8000~0x9000
        if(cst840u_erase_flash(0xB000,0x3000,0x04)) continue; //erase 12k 0xB000~0xE000
        //start trans fw
        eep_addr = 0;
        eep_len = 0;
        p_bin_addr = bin_addr;
        for (i=0; i<pak_num; i++){
            ret = hyn_wr_reg(hyn_840udata,U8TO32(0xA0,0x06,eep_addr,eep_addr>>8),4,0,0); 
            ret |= hyn_wr_reg(hyn_840udata,0xA0080004,4,0,0); 
            ret |= hyn_wr_reg(hyn_840udata,i>=36 ? 0xA00A0300:0xA00A0000,4,0,0);
            ret |= hyn_wr_reg(hyn_840udata,0xA018CACA,4,0,0); 
            if(ret) continue;

            i2c_buf[0] = 0xA0;
            i2c_buf[1] = 0x40;
            if(0 == hyn_840udata->fw_file_name[0]){
                memcpy(i2c_buf + 2, bin_addr+eep_len, PKG_SIZE);
            }else{
                ret |= copy_for_updata(hyn_840udata,i2c_buf + 2,eep_len,PKG_SIZE);
            }
            ret |= hyn_write_data(hyn_840udata, i2c_buf,2, PKG_SIZE+2);
            msleep(5);
            ret |= hyn_wr_reg(hyn_840udata, 0xA004E1, 3,0,0);

            eep_len += PKG_SIZE;
            eep_addr += PKG_SIZE;
            if(0x9000 == eep_addr){
                eep_addr = 0xB000;
            }
            mdelay(20); //wait finsh
            cst840u_wait_ready(100,1,0xA020,0xE188);
            hyn_840udata->fw_updata_process = i*100/pak_num;
            // HYN_INFO("FB_%d",hyn_840udata->fw_updata_process);
        }
        if(ret) continue;
        hyn_840udata->hw_info.ic_fw_checksum = cst840u_read_checksum();
        if(fw_checksum == hyn_840udata->hw_info.ic_fw_checksum && 0 != hyn_840udata->boot_is_pass){
            break;
        }
        else{
            ret = -2;
        }
    }
UPDATA_END:
    hyn_set_i2c_addr(hyn_840udata,MAIN_I2C_ADDR);
    cst840u_rst();
    if(ret == 0){
        msleep(50);
        cst840u_updata_tpinfo();
        hyn_840udata->fw_updata_process = 100;
        HYN_INFO("updata_fw success");
    }
    else{
        hyn_840udata->fw_updata_process |= 0x80;
        HYN_ERROR("updata_fw failed fw_checksum:%#x ic_checksum:%#x",fw_checksum,hyn_840udata->hw_info.ic_fw_checksum);
    }
    hyn_irq_set(hyn_840udata,ENABLE);
    return ret;
}

static u32 cst840u_check_esd(void)
{
    int ok = FALSE;
    u8 i2c_buf[6];
    u8 flag = 0,retry = 4;
    u32 esd_value = 0;

    while (retry--) {
        hyn_wr_reg(hyn_840udata,0xD0000D00,4,i2c_buf,0);
        udelay(200);
        ok = hyn_wr_reg(hyn_840udata,0xD0000D00,4,i2c_buf,1);
        if (ok == FALSE) {
            msleep(1);
            continue;
        } 

        if ((i2c_buf[0] & 0x0F) > 10) {
            HYN_ERROR("ESD timeout i2c_buf[0]:0x%04x", i2c_buf[0]);
            flag = 2;
            continue;
        }
        esd_value = i2c_buf[0] & 0xF0;
        if (esd_value == hyn_840udata->esd_last_value && esd_value != 0x00) {
            break;
        }

        switch (hyn_840udata->work_mode) {
            case NOMAL_MODE:
            // case XY_MODE:
                flag = esd_value == 0x20 ? 0 : 1;
                break;
            case GESTURE_MODE:
                flag = esd_value == 0xA0 ? 0 : 1;
                break;
            default:
                flag = 2;
                break;
        }
        if (flag) {
            // HYN_ERROR("ESD mode error,work_mode:%d esd_value:0x%x flag:%d",hyn_840udata->work_mode, esd_value, flag);
            continue;
        }

        break;
    }
	
    HYN_INFO("ESD data:0x%04x,0x%04x", esd_value, hyn_840udata->esd_last_value);
    if (flag) {
        HYN_ERROR("ESD mode error,work_mode:%d esd_value:0x%x flag:%d",hyn_840udata->work_mode, esd_value, flag);
        return 2;
    }

    hyn_840udata->esd_last_value = esd_value;
    return 0;
}

static int red_dbg_data(u8 *buf, u16 len ,u32 *cmd_list,u8 type)
{
    struct tp_info *ic = &hyn_840udata->hw_info;
    int ret = 0;
    u16 read_len = (ic->fw_sensor_rxnum*ic->fw_sensor_txnum)*type;
    u16 total_len = (ic->fw_sensor_rxnum*ic->fw_sensor_txnum + ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*type;
    if(total_len > len || read_len == 0){
        HYN_ERROR("buf too small or fw_sensor_rxnum fw_sensor_txnum erro");
        return -1;
    } 
    buf += read_len;
    read_len = ic->fw_sensor_rxnum*type;
    ret |= hyn_wr_reg(hyn_840udata,*cmd_list++,0x80|4,buf,read_len);
    // ret |= hyn_wr_reg(hyn_840udata,0xD00002AB,4,0,0); //end
    return ret < 0 ? -1:total_len;
}

static int cst840u_get_dbg_data(u8 *buf, u16 len)
{
    int read_len = -1,ret = 0;
    HYN_ENTER();
    switch(hyn_840udata->work_mode){
        case DIFF_MODE:
            read_len = red_dbg_data(buf,len,(u32[]){0xD01A0000},2);
            break;
        case RAWDATA_MODE:
            read_len = red_dbg_data(buf,len,(u32[]){0xD0190000},2);
            break;
        case BASELINE_MODE:
            read_len = red_dbg_data(buf,len,(u32[]){0xD01B0000},2);
            break;
        case CALIBRATE_MODE:{
                u16 tmp_len = len/2;
                len /= 2;
                read_len = red_dbg_data(buf+tmp_len,tmp_len,(u32[]){0xD0180000},1);
                if(read_len > 0){
                    u8 *src_ptr = buf+tmp_len,tmp;
                    s16 *des_ptr = (s16*)buf;
                    tmp_len = read_len;
                    while(tmp_len--){
                        tmp = (*src_ptr++)&0x7F;
                        *des_ptr++ = (tmp & 0x40) ? -(tmp&0x3F):tmp;
                    }
                    read_len *= 2;
                }
                break;
            }
        default:
            HYN_ERROR("work_mode:%d",hyn_840udata->work_mode);
            break;
    }
    // HYN_INFO("read_len:%d len:%d",(int)(sizeof(struct ts_frame)+read_len),len);
    if(read_len > 0 && len > (sizeof(struct ts_frame)+read_len)){
        ret = cst840u_report();
        if(ret ==0){
            memcpy(buf+read_len+4,(void*)&hyn_840udata->rp_buf,sizeof(struct ts_frame));
            read_len += sizeof(struct ts_frame);
        }
    }
    else{
        ret = hyn_wr_reg(hyn_840udata,0xD00002AB,4,0,0); //end
    }
    return  read_len;
}

static int get_fac_test_data(u32 cmd ,u8 *buf, u16 len ,u8 rev)
{
    int ret = 0;
    ret  = hyn_wr_reg(hyn_840udata,cmd,4,0,0);
    ret |= hyn_wait_irq_timeout(hyn_840udata,100);
    ret  |= hyn_wr_reg(hyn_840udata,0xD0120000,0x80|4,buf+rev,len);
    ret  |= hyn_wr_reg(hyn_840udata,0xD00002AB,4,0,0);
    if(ret==0 && rev){
        len /= 2;
        while(len--){
            *buf = *(buf+2);
            buf += 2;
        }
    }
    return ret;
}

#define FACTEST_PATH    "/sdcard/hyn_fac_test_cfg.ini"
#define FACTEST_LOG_PATH "/sdcard/hyn_fac_test.log"
#define FACTEST_ITEM      (MULTI_SCAP_TEST)
// #define HYN_CHECK_FACTORY_RATIO_MIN 50.0
// #define HYN_CHECK_FACTORY_RATIO_MAX 150.0
const uint16_t factory_test_threshold[28] = {
    2465, 2033, 1855, 1808, 1659, 1653, 1586, 2351,
    1677, 1559, 1507, 1407, 1349, 1219, 4352, 1865,
    1676, 1584, 1361, 1357, 1157, 3842, 1776, 1561,
    1468, 1579, 1328, 1328
};

static int cst840u_get_test_result(u8 *buf, u16 len)
{
    struct tp_info *ic = &hyn_840udata->hw_info;
    u16 st_len = ic->fw_sensor_rxnum*2;
    u8 *rbuf = buf;
    u16 *cp_buf = (u16*)buf;
    int ret = 0,i;
    HYN_ENTER();
    if((st_len*2 + 4) > len){
        HYN_ERROR("%d", ic->fw_sensor_rxnum);
        return FAC_GET_DATA_FAIL;
    } 

    if(get_fac_test_data(0xD0002500,rbuf,st_len,0)){ ///read scap test data
        HYN_ERROR("read scap failed");
        ret = -1;
        goto TEST_ERRO;
    }

    // rbuf += st_len;
    // cst840u_set_workmode(NOMAL_MODE,0);
    // cst840u_set_workmode(DIFF_MODE,0);
    // ret |= hyn_wait_irq_timeout(hyn_840udata,100);
    // ret |= hyn_wr_reg(hyn_840udata,0xD01A0000,0x80|4,rbuf,st_len);
    // ret |= hyn_wr_reg(hyn_840udata,0xD00002AB,4,0,0);
    // if(ret) { ///read scap test data
    //     HYN_ERROR("read scap diff failed");
    //     goto TEST_ERRO;
    // }

    // cst840u_set_workmode(NOMAL_MODE,0);

    for (i = 0; i < st_len>>1; i++) {
        u16 factory_test_threshold_min;
        u16 factory_test_threshold_max;

        factory_test_threshold_min = factory_test_threshold[i] >> 1;
        factory_test_threshold_max = factory_test_threshold[i] + factory_test_threshold_min;
        if (cp_buf[i] < factory_test_threshold_min){
            HYN_ERROR("check open_higdrv MIN ERROR:%d-%d-min-%d", i, cp_buf[i], factory_test_threshold_min);
            ret = -1;
        }
        if (cp_buf[i] > factory_test_threshold_max){
            HYN_ERROR("check open_higdrv MAX ERROR:%d-%d-max-%d", i, cp_buf[i], factory_test_threshold_max);
            ret = -1;
        }
    }

TEST_ERRO:
    // if(0 == fac_test_log_save(FACTEST_LOG_PATH,hyn_840udata,(s16*)buf,ret,FACTEST_ITEM)){
    //     HYN_INFO("fac_test log save success");
    // }
    cst840u_resum();
    return ret;
}


const struct hyn_ts_fuc cst840u_fuc = {
    .tp_rest = cst840u_rst,
    .tp_report = cst840u_report,
    .tp_supend = cst840u_supend,
    .tp_resum = cst840u_resum,
    .tp_chip_init = cst840u_init,
    .tp_updata_fw = cst840u_updata_fw,
    .tp_set_workmode = cst840u_set_workmode,
    .tp_check_esd = cst840u_check_esd,
    .tp_prox_handle = cst840u_prox_handle,
    .tp_get_dbg_data = cst840u_get_dbg_data,
    .tp_get_test_result = cst840u_get_test_result
};



