#include "../hyn_core.h"


#include "cst76xx_fw.h"

#define BOOT_I2C_ADDR   (0x5A)
#define MAIN_I2C_ADDR   (0x5A) //use 2 slave addr

#define MODULE_ID_EN        (0)
#define PART_NO_EN          (0)

#define MODULE_ID_ADDR      (0x1EC00)
#define PARTNUM_ADDR        (0x1FF10)

static struct hyn_ts_data *hyn_76xxdata = NULL;
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
    {0xCACA3800,0xffffffff,"cst76xx",(u8*)fw_module},//if PART_NO_EN==0 use default chip
    {0xCACA3801,0xffffffff,"cst76xx",(u8*)fw_module},  //HYT7864BG
    {0xCACA3806,0xffffffff,"cst76xx",(u8*)fw_module},  //HYT7964BG
    {0xCACA3807,0xffffffff,"cst76xx",(u8*)fw_module},  //HYT7864JL 
    {0xCACA3808,0xffffffff,"cst76xx",(u8*)fw_module},  //HYT7760BG || HYT7760TR
    {0xCACA3809,0xffffffff,"cst76xx",(u8*)fw_module},  //CST6960BG
    {0,0,"",NULL}
};

static int cst76xx_updata_judge(u8 *p_fw, u32 len);
static u32 cst76xx_read_checksum(void);
static int cst76xx_enter_boot(void);
static u32 cst76xx_fread_word(u32 addr);
static void cst76xx_rst(void);
static u32 cst76xx_read_file_checksum(u8 *p_fw, u32 len);
static int cst76xx_read_file_addr(u32 part_no, u32 module_id);
#if MODULE_ID_EN
static int cst76xx_judge_module(void);
#endif

static int cst76xx_init(struct hyn_ts_data* ts_data)
{
    int ret = 0;
    u32 read_part_no,module_id;
    HYN_ENTER();
    hyn_76xxdata = ts_data;
#if MODULE_ID_EN
    if (cst76xx_judge_module())
#endif
    {
        ret = cst76xx_enter_boot();
        if(ret){
            HYN_ERROR("cst76xx_enter_boot failed");
            return -1;
        }

        read_part_no = cst76xx_fread_word(PARTNUM_ADDR);
        module_id =  cst76xx_fread_word(MODULE_ID_ADDR);
        HYN_INFO("read_part_no:0x%08x module_id:0x%08x",read_part_no,module_id);
        if(module_id > 10) module_id = 0xffffffff;

        hyn_76xxdata->hw_info.fw_module_id = module_id;
        hyn_76xxdata->hw_info.ic_part_no = read_part_no;
        hyn_76xxdata->hw_info.ic_fw_checksum = cst76xx_read_checksum();
        hyn_set_i2c_addr(hyn_76xxdata,MAIN_I2C_ADDR);
        cst76xx_rst(); //exit boot
        mdelay(50);
    }

    cst76xx_read_file_addr(hyn_76xxdata->hw_info.ic_part_no, hyn_76xxdata->hw_info.fw_module_id);
    hyn_76xxdata->need_updata_fw = cst76xx_updata_judge(hyn_76xxdata->fw_updata_addr, hyn_76xxdata->fw_updata_len);
    // hyn_76xxdata->need_updata_fw = 0;
    if (hyn_76xxdata->need_updata_fw) {
        HYN_INFO("need updata FW !!!");
    }
    return 0;
}


static int cst76xx_report(void)
{
    u8 buf[80],i=0;
    u8 finger_num = 0,key_num=0,report_typ= 0,key_state=0,key_id = 0,tmp_dat=0;
    int ret = 0,retry = 2;

    while(retry--){ //read point
        ret = hyn_wr_reg(hyn_76xxdata,0xD0070000,0x80|4,buf,9); 
        report_typ = buf[2];//FF:pos F0:ges E0:prox
        finger_num = buf[3]&0x0f;

        // HYN_INFO("finger_num = %d", finger_num);
        key_num    = ((buf[3]&0xf0)>>4);
        // HYN_INFO("key_num = %d", key_num);
        if(finger_num+key_num <= MAX_POINTS_REPORT){
            if(key_num + finger_num > 1){ 
                ret |= hyn_read_data(hyn_76xxdata,&buf[9],(key_num + finger_num -1)*5);
            }
            if(hyn_sum16(0x55,&buf[4],(key_num + finger_num)*5) != (buf[0] | buf[1]<<8)){
                ret = -1;
            }
        }
        else {
            ret = -2;
        }
        if(ret && retry) continue;
        ret |= hyn_wr_reg(hyn_76xxdata,0xD00002AB,4,buf,0);
        if(ret == 0) {
            break;
        }
    }
    if(ret) return ret;
    if((report_typ==0xff)&&((finger_num+key_num)>0)) {
        if(key_num){
            key_id    = buf[8]&0x0f;
            key_state = buf[8]>>4;
            if(hyn_76xxdata->rp_buf.report_need == REPORT_NONE){ //Mutually exclusive reporting of coordinates and keys
                hyn_76xxdata->rp_buf.report_need |= REPORT_KEY;
            }
            hyn_76xxdata->rp_buf.key_id = key_id;
            hyn_76xxdata->rp_buf.key_state = key_state !=0 ? 1:0; 
        }

        if(finger_num){ //pos
            u16 index = 0;
            u8 touch_down = 0;
            if(hyn_76xxdata->rp_buf.report_need == REPORT_NONE){ //Mutually exclusive reporting of coordinates and keys
                hyn_76xxdata->rp_buf.report_need |= REPORT_POS;
            }    
            hyn_76xxdata->rp_buf.rep_num = finger_num;
            for(i = 0; i < finger_num; i++){
                index = (key_num+i)*5;
                hyn_76xxdata->rp_buf.pos_info[i].pos_id = buf[index+8]&0x0F;
                hyn_76xxdata->rp_buf.pos_info[i].event =  buf[index+8]>>4;
                hyn_76xxdata->rp_buf.pos_info[i].pos_x =  buf[index+4] + ((u16)(buf[index+7]&0x0F) <<8); //x is rx direction
                hyn_76xxdata->rp_buf.pos_info[i].pos_y =  buf[index+5] + ((u16)(buf[index+7]&0xf0) <<4);
                hyn_76xxdata->rp_buf.pos_info[i].pres_z = buf[index+6];
                if(hyn_76xxdata->rp_buf.pos_info[i].event){
                    touch_down++;
                }
            }
            if(0== touch_down){
                hyn_76xxdata->rp_buf.rep_num = 0;
            }
        }
    }
    else if(report_typ == 0xF0){ //gesture
        tmp_dat = buf[8]&0xff;
        if((tmp_dat&0x7F) < sizeof(gest_map_tbl) && gest_map_tbl[tmp_dat] != IDX_NULL){  
            hyn_76xxdata->gesture_id = gest_map_tbl[tmp_dat];
            hyn_76xxdata->rp_buf.report_need |= REPORT_GES;
            HYN_INFO("gesture_id:%d",tmp_dat);
        }
    }else if(report_typ == 0xE0){//proximity 
        u8 state = buf[4] ? PS_FAR_AWAY : PS_NEAR;
        if(hyn_76xxdata->prox_is_enable && hyn_76xxdata->prox_state != state){
            hyn_76xxdata->prox_state =state;
            hyn_76xxdata->rp_buf.report_need |= REPORT_PROX;
        }
    }
    return ret;
}

static int cst76xx_prox_handle(u8 cmd)
{
    int ret = 0;
    switch(cmd){
        case 1: //enable
            hyn_76xxdata->prox_is_enable = 1;
            hyn_76xxdata->prox_state = 0xAA;
            ret = hyn_wr_reg(hyn_76xxdata,0xD004B001,4,NULL,0);
            break;
        case 0: //disable
            hyn_76xxdata->prox_is_enable = 0;
            hyn_76xxdata->prox_state = 0xAA;
            ret = hyn_wr_reg(hyn_76xxdata,0xD004B000,4,NULL,0);
            break;
        default: 
            break;
    }
    return ret;
}

static int cst76xx_set_workmode(enum work_mode mode,u8 enable)
{
    int ret = 0,retry;//,i;
    u32 reg_cmd[2] = {0};
    u8  reg_buf[4] = {0};

    
    HYN_INFO("work_mode:%d, enable:%d",mode, enable);
    hyn_esdcheck_switch(hyn_76xxdata,enable);

    if (mode < 10) {
        hyn_76xxdata->work_mode = mode;
    }

    if (mode == ENTER_BOOT_MODE) {
        ret = cst76xx_enter_boot();
        if (ret) {
            HYN_INFO("enter_boot_mode ret:%d", ret);
        }
        return 1;
    }

    if(FAC_TEST_MODE == mode) {
        cst76xx_rst();
        msleep(50);
    }

    for (retry = 0; retry < 5; retry++) {
        ret = hyn_wr_reg(hyn_76xxdata,0xD0000400,4,NULL,0); //disable lp i2c plu
        ret |= hyn_wr_reg(hyn_76xxdata,0xD02F0000,4,reg_buf,4);
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
            hyn_irq_set(hyn_76xxdata,ENABLE);
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
        ret = hyn_wr_reg(hyn_76xxdata,reg_cmd[0],4,NULL,0);
        ret |= hyn_wr_reg(hyn_76xxdata,0xD02F0000,4,reg_buf,4);
        if (ret || (reg_cmd[0] & 0xFFFF) != U8TO16(reg_buf[2],reg_buf[3])) {
            HYN_ERROR("err mode %d ret:%d cmd:%x buf:%x", mode, ret, reg_cmd[0], U8TO16(reg_buf[2],reg_buf[3]));
            mdelay(1);
            continue;
        }
        if (reg_cmd[1]) {
            ret = hyn_wr_reg(hyn_76xxdata,reg_cmd[1],4,NULL,0);
            ret |= hyn_wr_reg(hyn_76xxdata,0xD02F0000,4,reg_buf,4);
            if (ret || (reg_cmd[1] & 0xFFFF) != U8TO16(reg_buf[2],reg_buf[3])) {
                HYN_ERROR("err mode %d ret:%d cmd:%x buf:%x", mode, ret, reg_cmd[1], U8TO16(reg_buf[2],reg_buf[3]));
                mdelay(1);
                continue;
            }
        }

        ret = hyn_wr_reg(hyn_76xxdata,0xD0000401,4,0,0);
        ret |= hyn_wr_reg(hyn_76xxdata,0xD02F0000,4,reg_buf,4);
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


static int cst76xx_supend(void)
{
    HYN_ENTER();
    cst76xx_set_workmode(DEEPSLEEP,DISABLE);
    return 0;
}

static int cst76xx_resum(void)
{
    HYN_ENTER();
    cst76xx_rst();
    msleep(50);
    cst76xx_set_workmode(NOMAL_MODE,ENABLE);
    return 0;
}

static void cst76xx_rst(void)
{
    HYN_ENTER();
    if(hyn_76xxdata->work_mode==ENTER_BOOT_MODE){
        hyn_set_i2c_addr(hyn_76xxdata,MAIN_I2C_ADDR);
    }
    gpio_set_value(hyn_76xxdata->plat_data.reset_gpio,0);
    msleep(8);
    gpio_set_value(hyn_76xxdata->plat_data.reset_gpio,1);
}

static int cst76xx_wait_ready(u16 times,u8 ms,u16 reg,u16 check_vlue)
{
    int ret = 0;
    u8 buf[4];
    while(times--){
        ret = hyn_wr_reg(hyn_76xxdata,reg,2,buf,2);
        if(0==ret && U8TO16(buf[0],buf[1])==check_vlue){
            return 0;
        }
        mdelay(ms);
    }
    return -1;
}

#if MODULE_ID_EN
static int cst76xx_judge_module(void) {
    int ret = 0;
    uint8_t buf[44];
    uint8_t retry = 3;
    uint32_t part_no = 0;
    uint32_t module_id = 0;
    uint32_t check_sum = 0;
    HYN_ENTER();
    cst76xx_rst();
    mdelay(50);
    for (; retry > 0; retry--) {
        //module id
        ret = hyn_wr_reg(hyn_76xxdata,0xD0060000,4,buf, 44);
        if(ret) {
            continue;
        }
        module_id = U8TO32(buf[35], buf[34], buf[33], buf[32]);
        part_no   = U8TO32(buf[39], buf[38], buf[37], buf[36]);
        check_sum = U8TO32(buf[43], buf[42], buf[41], buf[40]);
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

    if(module_id > 10) module_id = 0xffffffff;
    hyn_76xxdata->hw_info.fw_module_id = module_id;
    hyn_76xxdata->hw_info.ic_part_no = part_no;
    hyn_76xxdata->hw_info.ic_fw_checksum = check_sum;
    hyn_76xxdata->boot_is_pass = 1;
    
    return TRUE;
}
#endif

static int cst76xx_enter_boot(void)
{
    int retry = 0,ret = 0;
    hyn_set_i2c_addr(hyn_76xxdata,BOOT_I2C_ADDR);
    while(++retry<20){
        cst76xx_rst();
        mdelay(12+retry);
        ret = hyn_wr_reg(hyn_76xxdata,0xA001A2,3,0,0);
        if(ret < 0){
            continue;
        }
        if(0==cst76xx_wait_ready(10,2,0xA002,0x88AA)){
            return 0;
        }
    }
    return -1;
}

static int cst76xx_updata_tpinfo(void)
{
    u8 buf[60];
    struct tp_info *ic = &hyn_76xxdata->hw_info;
    int ret = 0;
    int retry = 5;
    HYN_ENTER();
    while(--retry){
        //get all config info
        ret |= cst76xx_set_workmode(NOMAL_MODE,ENABLE);
        ret |= hyn_wr_reg(hyn_76xxdata,0xD0030000,0x80|4,buf,50);
        if(ret == 0 && buf[3]==0xCA && buf[2]==0xCA) break; 
        mdelay(1);
        // ret |= hyn_wr_reg(hyn_76xxdata,0xD0000400,4,buf,0);
    }

    if(ret || retry==0) {
        HYN_ERROR("cst76xx_updata_tpinfo failed");
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

static u32 cst76xx_fread_word(u32 addr)
{
    int ret;
    u8 rec_buf[4],retry,i2c_buf[6];
    u32 read_word = 0;

    retry = 3;
    while(retry--){
        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x08;
        i2c_buf[2] = addr&0xFF;
        i2c_buf[3] = (addr>>8)&0xFF;
        i2c_buf[4] = (addr>>16)&0xFF;
        i2c_buf[5] = addr>>24;
        ret = hyn_write_data(hyn_76xxdata, i2c_buf,0, 6); //set addr

        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x0C;
        i2c_buf[2] = 0x04;
        i2c_buf[3] = 0x00;
        i2c_buf[4] = 0x00;
        i2c_buf[5] = 0x00;
        ret |= hyn_write_data(hyn_76xxdata, i2c_buf,0, 6); //set len

        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x10;
        i2c_buf[2] = 0x03;
        i2c_buf[3] = 0x00;
        i2c_buf[4] = 0x00;
        i2c_buf[5] = 0x00;
        ret |= hyn_write_data(hyn_76xxdata, i2c_buf,0, 6); //set type

        ret  |= hyn_wr_reg(hyn_76xxdata,0xA004D2,3,NULL,0);//trig read
        if(ret ==0) break;
    }
    if(ret) return 0;

    retry = 20;
	while(retry--){
        ret = hyn_wr_reg(hyn_76xxdata,0xA020,2,rec_buf,2);	
		if(ret==0 && rec_buf[0]==0xD2 && rec_buf[1]==0x88){
            ret  |= hyn_wr_reg(hyn_76xxdata,0xA030,2,rec_buf,4);
            if(ret ==0){
                read_word = U8TO32(rec_buf[3],rec_buf[2],rec_buf[1],rec_buf[0]);
                break;
            }
		}
		mdelay(1);
	}
	return read_word;
}

static int  read_checksum(u32 start_val,u32 start_addr,u32 len ,u32 *check_sum)
{
    int ret,retry = 3;
    u8 buf[8] = {0};
    u8 i2c_buf[6] = {0};
    while(retry--){
        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x08;
        i2c_buf[2] = start_addr&0xFF;
        i2c_buf[3] = (start_addr>>8)&0xFF;
        i2c_buf[4] = (start_addr>>16)&0xFF;
        i2c_buf[5] = start_addr>>24;
        ret = hyn_write_data(hyn_76xxdata, i2c_buf,0, 6);

        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x0C;
        i2c_buf[2] = len&0xFF;
        i2c_buf[3] = (len>>8)&0xFF;
        i2c_buf[4] = (len>>16)&0xFF;
        i2c_buf[5] = len>>24;
        ret |= hyn_write_data(hyn_76xxdata, i2c_buf,0, 6);

        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x10;
        i2c_buf[2] = start_val&0xFF;
        i2c_buf[3] = (start_val>>8)&0xFF;
        i2c_buf[4] = (start_val>>16)&0xFF;
        i2c_buf[5] = start_val>>24;
        ret |= hyn_write_data(hyn_76xxdata, i2c_buf,0, 6);

        ret |= hyn_wr_reg(hyn_76xxdata,0xA004D6,3,0,0);
        if(ret) continue;
        // mdelay(len/0xc00 + 1);
        mdelay(300);
        ret = cst76xx_wait_ready(20,25,0xA020,0xD688);

        ret |= hyn_wr_reg(hyn_76xxdata,0xA030,2,buf,5);

        if(ret == 0 && buf[0] == 0xCA){
            HYN_INFO("ok");
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

static u32 cst76xx_read_checksum(void)
{
    u32 chipSum = 0;
    hyn_76xxdata->boot_is_pass = 0;
    if(0==read_checksum(0,0x2000,hyn_76xxdata->fw_updata_len,&chipSum)){
        hyn_76xxdata->boot_is_pass = 1;
    }
    HYN_INFO("chipSum:%04x ",chipSum);
    return chipSum;
}

static int cst76xx_read_file_addr(u32 partno, u32 moduleId) {
    int ret = 0 ,i = 0;
    u8 *p_data;
    u8 flag[4];
    HYN_ENTER();
    for (i = 0; ;i++) {
#if PART_NO_EN
        if(hyn_xx_fw[i].part_no == partno && hyn_xx_fw[i].moudle_id == moduleId)
#else
        if( hyn_xx_fw[i].moudle_id == moduleId)
#endif
        {   
            hyn_76xxdata->fw_updata_addr = hyn_xx_fw[i].fw_bin;
            HYN_INFO("chip %s match fw success ,partNo check is [%s]",hyn_xx_fw[i].chip_name,PART_NO_EN ? "enable":"disable");
            break;
        }

        if (hyn_xx_fw[i].part_no == 0 && hyn_xx_fw[i].moudle_id == 0) {
            HYN_INFO("unknown chip or unknown moudle_id use hyn_xx_fw[0]");
            //ret = -1;
            break;
        }
    }
    p_data = hyn_76xxdata->fw_updata_addr + 0x0A00;
    memcpy(flag, p_data, 4);
    if(flag[3] != 0xCA && flag[2] != 0xCA && flag[1] != 0xCA && flag[0] > 114)
        HYN_ERROR("get flag failed");
    
    hyn_76xxdata->fw_block_cnt   = flag[0] * 2 + 4;
    hyn_76xxdata->fw_updata_len  = hyn_76xxdata->fw_block_cnt * 512;
    HYN_INFO("fw_block_cnt %d fw_updata_len %d", hyn_76xxdata->fw_block_cnt, hyn_76xxdata->fw_updata_len);

    return ret;
}

static u32 cst76xx_read_file_checksum(u8 *p_fw, u32 len)
{
    u32 *main_checksum;
	u32 *config_checksum;
    u32 cal_config_checksum;
    u32 cal_main_checksum;
    u32 f_check_all;

    int config_bin_len = 2*1024;
    config_checksum  = (u32*)(p_fw + (config_bin_len - 4));
    main_checksum    = (u32*)(p_fw + (len - 4));
    HYN_INFO("config_checksum %04x main_checksum %04x ", *config_checksum, *main_checksum);

    cal_config_checksum = hyn_sum32(0x55,(u32*)p_fw,(config_bin_len - 4)/4);

    cal_main_checksum = hyn_sum32(0x55,(u32*)(p_fw+config_bin_len),(len - config_bin_len - 4)/4);

    if (*config_checksum != cal_config_checksum || *main_checksum != cal_main_checksum) {
        HYN_INFO(".h file checksum erro: cal_config_checksum %04x cal_main_checksum %04x",cal_config_checksum, cal_main_checksum);
        return 0;
    }

    f_check_all = ((*main_checksum) + (*config_checksum) ) * 2 - 0x55;
    HYN_INFO("f_check_all %04x ", f_check_all);

    return f_check_all;
}

static int cst76xx_updata_judge(u8 *p_fw, u32 len)
{
    u32 f_fw_ver,f_ictype,f_fw_project_id,f_checksum;
    u8 *p_data = p_fw;
    int ret;

    f_fw_project_id = U8TO32(p_data[39],p_data[38],p_data[37],p_data[36]);
    f_ictype        = U8TO32(p_data[3],p_data[2],p_data[1],p_data[0]);
    f_fw_ver        = U8TO32(p_data[35],p_data[34],p_data[33],p_data[32]);
    f_checksum      = cst76xx_read_file_checksum(p_fw, len);
    HYN_INFO("BIN_INFO project_id:%04x type:%04x fw_ver:%04x checksum:%04x",f_fw_project_id,f_ictype,f_fw_ver,f_checksum);
    if (f_checksum == 0) {
        HYN_ERROR("file checksum erro not updata");
        return 0;
    }
    
    if (hyn_76xxdata->boot_is_pass == 0) {
        HYN_ERROR("emty chip, need updata");
        return 1;
    }

    ret = cst76xx_updata_tpinfo();
    if(ret) {
        HYN_ERROR("get tpinfo failed need updata");
        return 1;
    }
    
    if (f_ictype != hyn_76xxdata->hw_info.fw_chip_type) {
        HYN_ERROR("ic type not match not updata");
        return 0;
    }

    // if (f_fw_ver > hyn_76xxdata->hw_info.fw_ver && f_checksum != hyn_76xxdata->hw_info.ic_fw_checksum) {
    if (f_checksum != hyn_76xxdata->hw_info.ic_fw_checksum) {
        HYN_INFO("match new ver file, need updata");
        return 1;
    }
    else {
        HYN_INFO("no need updata");
    }

    return 0;
}

static int cst76xx_erase_flash(u32 start_addr, u32 len, u16 type)
{
    int ret = 0;
    u8 i2c_buf[6] = {0};

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x08;
    i2c_buf[2] = start_addr&0xFF;
    i2c_buf[3] = (start_addr>>8)&0xFF;
    i2c_buf[4] = (start_addr>>16)&0xFF;
    i2c_buf[5] = start_addr>>24;
    ret = hyn_write_data(hyn_76xxdata, i2c_buf,0, 6);

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x0C;
    i2c_buf[2] = len&0xFF;
    i2c_buf[3] = (len>>8)&0xFF;
    i2c_buf[4] = (len>>16)&0xFF;
    i2c_buf[5] = len>>24;
    ret |= hyn_write_data(hyn_76xxdata, i2c_buf,0, 6);

    i2c_buf[0] = 0xA0;
    i2c_buf[1] = 0x10;
    i2c_buf[2] = type&0xFF;
    i2c_buf[3] = (type>>8)&0xFF;
    i2c_buf[4] = (type>>16)&0xFF;
    i2c_buf[5] = type>>24;
    ret |= hyn_write_data(hyn_76xxdata, i2c_buf,0, 6);

    ret |= hyn_wr_reg(hyn_76xxdata,0xA018CACA,4,0,0); //key
    ret |= hyn_wr_reg(hyn_76xxdata,0xA004E0,3,0,0); //trig
    if(ret) return -1;

    mdelay(12*25); //wait finsh earse flash
    ret = cst76xx_wait_ready(100,50,0xA020,0xE088);
    return ret;
}

static void cst76xx_erase_flash_all(void)
{
    u32 total_len = 114 * 1024;
    u32 start_addr = 8 * 1024;

    u32 done = 0, step = 0, erase_type = 0;
    while(done < 32 * 1024 - 8 * 1024)
    {
        erase_type = 1;
        step = (32 * 1024 - start_addr) & ~0xfff;
        if(cst76xx_erase_flash(start_addr,step,erase_type)) break;

        HYN_INFO("1 erase %d, from %d",step, start_addr);
        done += step;
        start_addr += step;
    }

    while(done<total_len)
    {
        u32 left_size = total_len - done;
        if (left_size <= 0) {
            HYN_INFO("erase left %d", left_size);
            break;
        }
        
        if ((start_addr & 0x7fff) == 0 && (left_size & ~0x7fff) > 0) {
            step = left_size & ~0x7fff;
            erase_type = 2;
        }
        else if ((start_addr & 0xfff) == 0 && (left_size & ~0xfff) > 0) {
            step = left_size & ~0xfff;
            erase_type = 1;
        }
        else {
            step = left_size & ~0xff;
            erase_type = 0;
        }
        if(cst76xx_erase_flash(start_addr,step,erase_type)) break;

        HYN_INFO("2 erase %d, from %d",step, start_addr);
        done += step;
        start_addr += step;
    }
}

static int cst76xx_transfer_fw(u8 *bin_addr)
{
    #define PKG_SIZE    (512)
    u8 i2c_buf[PKG_SIZE+10];
    int i,ret = -1;
    u16 blk_len = 512;
    u16 blk_cnt = hyn_76xxdata->fw_block_cnt;
    u32 start_addr1 = 8 * 1024;
    u16 prog_type = 0;
    u32 send_idx = 0;

    for (i=0; i<blk_cnt; i++){
        if (i&0x01) {
            i2c_buf[0] = 0xA2;
            i2c_buf[1] = 0x30;
        } else {
            i2c_buf[0] = 0xA0;
            i2c_buf[1] = 0x30;
        }

        if(0 == hyn_76xxdata->fw_file_name[0]){
            memcpy(i2c_buf + 2, bin_addr+send_idx, 512);
        }else{
            // ret |= copy_for_updata(hyn_76xxdata,i2c_buf + 2,send_idx,512);
        }
        ret |= hyn_write_data(hyn_76xxdata, i2c_buf,2, 512+2);
        mdelay(2*25);
        send_idx += blk_len;

        // HYN_INFO("send_idx %d", send_idx);

        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x08;
        i2c_buf[2] = start_addr1&0xFF;
        i2c_buf[3] = (start_addr1>>8)&0xFF;
        i2c_buf[4] = (start_addr1>>16)&0xFF;
        i2c_buf[5] = start_addr1>>24;
        ret = hyn_write_data(hyn_76xxdata, i2c_buf,0, 6);
        mdelay(2);

        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x0C;
        i2c_buf[2] = blk_len&0xFF;
        i2c_buf[3] = (blk_len>>8)&0xFF;
        i2c_buf[4] = (blk_len>>16)&0xFF;
        i2c_buf[5] = blk_len>>24;
        ret |= hyn_write_data(hyn_76xxdata, i2c_buf,0, 6);
        mdelay(2);

        i2c_buf[0] = 0xA0;
        i2c_buf[1] = 0x10;
        i2c_buf[2] = prog_type&0xFF;
        i2c_buf[3] = (prog_type>>8)&0xFF;
        i2c_buf[4] = (prog_type>>16)&0xFF;
        i2c_buf[5] = prog_type>>24;
        ret |= hyn_write_data(hyn_76xxdata, i2c_buf,0, 6);
        mdelay(2);
        
        if (i&0x01) {
            i2c_buf[0] = 0xA0;
            i2c_buf[1] = 0x1C;
            i2c_buf[2] = blk_len&0xFF;
            i2c_buf[3] = (blk_len>>8)&0xFF;
            i2c_buf[4] = (blk_len>>16)&0xFF;
            i2c_buf[5] = blk_len>>24;
            ret |= hyn_write_data(hyn_76xxdata, i2c_buf,0, 6);
            mdelay(2);
        } else {
            i2c_buf[0] = 0xA0;
            i2c_buf[1] = 0x1C;
            i2c_buf[2] = 0x00;
            i2c_buf[3] = 0x00;
            i2c_buf[4] = 0x00;
            i2c_buf[5] = 0x00;
            ret |= hyn_write_data(hyn_76xxdata, i2c_buf,0, 6);
            mdelay(2);
        }

        ret |= hyn_wr_reg(hyn_76xxdata,0xA018CACA,4,0,0); 
        mdelay(2);
        if(ret) continue;

        ret |= hyn_wr_reg(hyn_76xxdata, 0xA004E1, 3,0,0);
        mdelay(25);

        // mdelay(25*7); //wait finsh
        cst76xx_wait_ready(100,25,0xA020,0xE188);

        start_addr1 += blk_len;

        // HYN_INFO("start_addr1 %d", start_addr1);

        hyn_76xxdata->fw_updata_process = i*100/blk_cnt;
        // HYN_INFO("FB_%d",hyn_76xxdata->fw_updata_process);
    }
    return ret;
}

static int cst76xx_updata_fw(u8 *bin_addr, u32 len)
{
    int ret = -1, retry_fw= 4; 
    u32 fw_checksum = 0;
    u8 *p_data = bin_addr + 0x0A00;
    u8 flag[4];
    HYN_ENTER();
    // HYN_INFO("len = %d ",len);
    memcpy(flag, p_data, 4);
    if(flag[3] != 0xCA && flag[2] != 0xCA && flag[1] != 0xCA && flag[0] > 114)
        HYN_ERROR("get flag failed");
    
    hyn_76xxdata->fw_block_cnt   = flag[0] * 2 + 4;
    hyn_76xxdata->fw_updata_len  = hyn_76xxdata->fw_block_cnt * 512;
    len = hyn_76xxdata->fw_updata_len;

    hyn_76xxdata->fw_updata_process = 0;
    if(0 == hyn_76xxdata->fw_file_name[0]){
        fw_checksum = cst76xx_read_file_checksum(bin_addr, len);
    }
    else{
        // ret = copy_for_updata(hyn_76xxdata,i2c_buf,cst76xx_BIN_SIZE,4);
        // if(ret)  goto UPDATA_END;
        // fw_checksum = U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]);
    }

    hyn_irq_set(hyn_76xxdata,DISABLE);
    hyn_esdcheck_switch(hyn_76xxdata,DISABLE);
    while(--retry_fw){
        ret = cst76xx_enter_boot();
        if(ret){
            HYN_INFO("cst76xx_enter_boot faill");
            continue;
        }
        //erase
        cst76xx_erase_flash_all();
        //start trans fw
        ret = cst76xx_transfer_fw(bin_addr);
        if(ret) {
            HYN_INFO("cst76xx_transfer_fw faill");
            continue;
        }

        hyn_76xxdata->hw_info.ic_fw_checksum = cst76xx_read_checksum();
        HYN_INFO("fw_checksum %#x ic_fw_checksum %#x", fw_checksum, hyn_76xxdata->hw_info.ic_fw_checksum);
        HYN_INFO("boot_is_pass %d", hyn_76xxdata->boot_is_pass);
        if(fw_checksum == hyn_76xxdata->hw_info.ic_fw_checksum && 0 != hyn_76xxdata->boot_is_pass){
            HYN_INFO("program checksum ok");
            break;
        }
        else{
            ret = -2;
        }
    }
// UPDATA_END:
    hyn_set_i2c_addr(hyn_76xxdata,MAIN_I2C_ADDR);
    cst76xx_rst();
    if(ret == 0){
        msleep(50);
        cst76xx_updata_tpinfo();
        hyn_76xxdata->fw_updata_process = 100;
        HYN_INFO("updata_fw success");
    }
    else{
        hyn_76xxdata->fw_updata_process |= 0x80;
        HYN_ERROR("updata_fw failed fw_checksum:%#x ic_checksum:%#x",fw_checksum,hyn_76xxdata->hw_info.ic_fw_checksum);
    }
    hyn_irq_set(hyn_76xxdata,ENABLE);
    return ret;
}

static u32 cst76xx_check_esd(void)
{
    int ok = FALSE;
    u8 i2c_buf[6];
    u8 flag = 0,retry = 4;
    u32 esd_value = 0;

    while (retry--) {
        hyn_wr_reg(hyn_76xxdata,0xD0000D00,4,i2c_buf,0);
        udelay(200);
        ok = hyn_wr_reg(hyn_76xxdata,0xD0000D00,4,i2c_buf,1);
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
        if (esd_value == hyn_76xxdata->esd_last_value && esd_value != 0x00) {
            break;
        }

        switch (hyn_76xxdata->work_mode) {
            case NOMAL_MODE:
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
            // HYN_ERROR("ESD mode error,work_mode:%d esd_value:0x%x flag:%d",hyn_76xxdata->work_mode, esd_value, flag);
            continue;
        }

        break;
    }
	
    HYN_INFO("ESD data:0x%04x,0x%04x", esd_value, hyn_76xxdata->esd_last_value);
    if (flag) {
        HYN_ERROR("ESD mode error,work_mode:%d esd_value:0x%x flag:%d",hyn_76xxdata->work_mode, esd_value, flag);
        return 2;
    }

    hyn_76xxdata->esd_last_value = esd_value;
    return 0;
}

static int red_dbg_data(u8 *buf, u16 len ,u32 *cmd_list,u8 type)
{
    struct tp_info *ic = &hyn_76xxdata->hw_info;
    int ret = 0;
    u16 read_len = (ic->fw_sensor_rxnum*ic->fw_sensor_txnum)*type;
    u16 total_len = (ic->fw_sensor_rxnum*ic->fw_sensor_txnum + ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*type;
    if(total_len > len || read_len == 0){
        HYN_ERROR("buf too small or fw_sensor_rxnum fw_sensor_txnum erro");
        return -1;
    } 
    ret |= hyn_wr_reg(hyn_76xxdata,*cmd_list++,0x80|4,buf,read_len); //m cap
    buf += read_len;
    read_len = ic->fw_sensor_rxnum*type;
    ret |= hyn_wr_reg(hyn_76xxdata,*cmd_list++,0x80|4,buf,read_len); //s rx cap
    buf += read_len;
    read_len = ic->fw_sensor_txnum*type;
    ret |= hyn_wr_reg(hyn_76xxdata,*cmd_list++,0x80|4,buf,read_len); //s tx cap
    // ret |= hyn_wr_reg(hyn_76xxdata,0xD00002AB,4,0,0); //end
    return ret < 0 ? -1:total_len;
}

static int cst76xx_get_dbg_data(u8 *buf, u16 len)
{
    int read_len = -1,ret = 0;
    HYN_ENTER();
    switch(hyn_76xxdata->work_mode){
        case DIFF_MODE:
            read_len = red_dbg_data(buf,len,(u32[]){0xD0120000,0xD01A0000,0xD0160000},2);
            break;
        case RAWDATA_MODE:
            read_len = red_dbg_data(buf,len,(u32[]){0xD0110000,0xD0190000,0xD0150000},2);
            break;
        case BASELINE_MODE:
            read_len = red_dbg_data(buf,len,(u32[]){0xD0130000,0xD01B0000,0xD0170000},2);
            break;
        case CALIBRATE_MODE:{
                u16 tmp_len = len/2;
                len /= 2;
                read_len = red_dbg_data(buf+tmp_len,tmp_len,(u32[]){0xD0140000,0xD0180000,0xD01c0000},1);
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
            HYN_ERROR("work_mode:%d",hyn_76xxdata->work_mode);
            break;
    }
    // HYN_INFO("read_len:%d len:%d",(int)(sizeof(struct ts_frame)+read_len),len);
    if(read_len > 0 && len > (sizeof(struct ts_frame)+read_len)){
        ret = cst76xx_report();
        if(ret ==0){
            memcpy(buf+read_len+4,(void*)&hyn_76xxdata->rp_buf,sizeof(struct ts_frame));
            read_len += sizeof(struct ts_frame);
        }
    }
    else{
        ret = hyn_wr_reg(hyn_76xxdata,0xD00002AB,4,0,0); //end
    }
    return  read_len;
}

static int get_fac_test_data(u32 cmd ,u8 *buf, u16 len ,u8 rev)
{
    int ret = 0;
    ret  = hyn_wr_reg(hyn_76xxdata,cmd,4,0,0);
    ret |= hyn_wait_irq_timeout(hyn_76xxdata,100);
    ret  |= hyn_wr_reg(hyn_76xxdata,0xD0120000,0x80|4,buf+rev,len);
    ret  |= hyn_wr_reg(hyn_76xxdata,0xD00002AB,4,0,0);
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
#define FACTEST_ITEM      (MULTI_OPEN_TEST|MULTI_SHORT_TEST|MULTI_SCAP_TEST)

static int cst76xx_get_test_result(u8 *buf, u16 len)
{
    struct tp_info *ic = &hyn_76xxdata->hw_info;
    u16 mt_len = ic->fw_sensor_rxnum*ic->fw_sensor_txnum*2;
    u16 st_len = (ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*2;
    u8 *rbuf = buf;
    int ret = FAC_GET_DATA_FAIL;
    HYN_ENTER();
    if((mt_len*3 + st_len*2 + 4) > len || mt_len==0){
        HYN_ERROR("%s", mt_len ? "buf too small":" ic->fw_sensor_rxnum*ic->fw_sensor_txnum=0");
        return FAC_GET_DATA_FAIL;
    } 
    if(get_fac_test_data(0xD0002000,rbuf,mt_len,0)){ //read open high data
        HYN_ERROR("read open high failed");
        goto TEST_ERRO;
    }
    rbuf += mt_len;
    if(get_fac_test_data(0xD0002100,rbuf,mt_len,0)){ //read open low data
        HYN_ERROR("read open low failed");
        goto TEST_ERRO;
    }
    rbuf += mt_len;
    if(get_fac_test_data(0xD0002300,rbuf,st_len,1)){ //read short test data
        HYN_ERROR("read fac short failed");
        goto TEST_ERRO;
    }
    //must rest
    rbuf += st_len;
    if(get_fac_test_data(0xD0002500,rbuf,st_len,0)){ ///read scap test data
        HYN_ERROR("read scap failed");
        goto TEST_ERRO;
    }
    ////read data finlish start test
    // ret = factory_multitest(hyn_76xxdata ,FACTEST_PATH, buf,(s16*)(rbuf+st_len),FACTEST_ITEM);

TEST_ERRO:
    // if(0 == fac_test_log_save(FACTEST_LOG_PATH,hyn_76xxdata,(s16*)buf,ret,FACTEST_ITEM)){
    //     HYN_INFO("fac_test log save success");
    // }
    cst76xx_resum();
    return ret;
}


const struct hyn_ts_fuc cst76xx_fuc = {
    .tp_rest = cst76xx_rst,
    .tp_report = cst76xx_report,
    .tp_supend = cst76xx_supend,
    .tp_resum = cst76xx_resum,
    .tp_chip_init = cst76xx_init,
    .tp_updata_fw = cst76xx_updata_fw,
    .tp_set_workmode = cst76xx_set_workmode,
    .tp_check_esd = cst76xx_check_esd,
    .tp_prox_handle = cst76xx_prox_handle,
    .tp_get_dbg_data = cst76xx_get_dbg_data,
    .tp_get_test_result = cst76xx_get_test_result
};



