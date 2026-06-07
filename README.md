# Kali NetHunter 内核 - CD12M (xintaiqi_uws6152_1h10)

> 专为 **CD12M** 设备适配的 Kali NetHunter 攻击内核  
> 基于 `xintaiqi_uws6152_1h10_test` 源码构建 | 支持外接无线网卡 / HID 攻击 / USB 伪装

##  核心内核配置 (defconfig 节选)

**文件：`arch/arm64/configs/cd12m_defconfig`**

```bash
# ==================== NetHunter 必需 ====================
CONFIG_NETFILTER_XT_MATCH_OWNER=y
CONFIG_NETFILTER_XT_MATCH_MAC=y
CONFIG_NETFILTER_XT_MATCH_COMMENT=y
CONFIG_IP_NF_NAT=y
CONFIG_IP_NF_TARGET_MASQUERADE=y
CONFIG_NETFILTER_XT_TARGET_TCPMSS=y

# ==================== HID 攻击 ====================
CONFIG_HIDRAW=y
CONFIG_UHID=y
CONFIG_USB_F_HID=y
CONFIG_USB_CONFIGFS_F_HID=y

# ==================== 外接 WiFi ====================
CONFIG_RTL8187=m
CONFIG_RTL8180=m
CONFIG_RT2800USB=m
CONFIG_ATH9K_HTC=m
CONFIG_ATH10K_USB=m
CONFIG_MT76x0U=m

# ==================== USB 攻击 (DriveDroid/RNDIS) ====================
CONFIG_USB_GADGET=y
CONFIG_USB_F_RNDIS=y
CONFIG_USB_F_ECM=y
CONFIG_USB_F_MASS_STORAGE=y
CONFIG_USB_CONFIGFS_MASS_STORAGE=y

# ==================== SELinux 宽松模式 ====================
CONFIG_SECURITY_SELINUX_DEVELOP=y
```

---

##  设备树关键节点 (dts)

**设备：CD12M**  
**文件：`arch/arm64/boot/dts/.../cd12m.dts` (示例)**

```dts
&usb0 {
    status = "okay";
    dr_mode = "peripheral";
};

&usb2_phy {
    status = "okay";
};

&sdhc_1 {
    status = "okay";
    supports-cqe;
    mmc-hs400-1_8v;
};

/* 为 NetHunter 预留 SPI 接口（攻击模块） */
&spi0 {
    status = "okay";
    spidev@0 {
        compatible = "spidev";
        reg = <0>;
        spi-max-frequency = <1000000>;
    };
};
```

---

##  Kali NetHunter 补丁栈（关键 patch 摘要）

以下补丁已被直接合并进本内核树：

```diff
diff --git a/drivers/usb/gadget/function/f_hid.c b/drivers/usb/gadget/function/f_hid.c
+ // Force HID descriptor for Kali keyboard attacks
+ hidg_desc = (struct hidg_func_descriptor){
+    .subclass = 0, /* no subclass */
+    .protocol = 1, /* keyboard */
+    .report_length = 64,
+    .report_desc_length = sizeof(hid_keyboard_report_desc),
+    .report_desc = hid_keyboard_report_desc,
+ };
```

**补丁功能**：让 USB HID 设备在任意模式下都能模拟键盘，绕过某些厂商限制。

---

## 编译

```bash
# 设置架构和交叉编译工具链
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export PATH=/opt/toolchain/bin:$PATH

# 编译内核
make cd12m_defconfig
make -j$(nproc)

# 生成的镜像
ls -lh arch/arm64/boot/Image.gz
```

---

## 刷入方式

```bash
# 使用 AnyKernel3 或 mkbootimg 打包 boot.img
spd_dump boot

```

完成后安装 NetHunter App 并部署 rootfs 即可开始使用。

---

## 免责

仅用于安全研究或经过授权的测试。刷入本内核可能丢失数据、损坏设备，请自行承担风险。
