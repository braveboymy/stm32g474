
# NB-IoT 智能终端标准通信协议


### 范围

本标准对NB-IoT智能远传智能终端设备和主站之间的通信协议进行规范。

### 术语和定义


### 主站  master  station

指部署在远端的采集服务器。

### 终端设备 terminal device

指具备和远端采集服务器通信的智能终端。

### SHA256 算法 secure hash algorithm 256

安全哈希算法的一个版本。计算机安全领域广泛使用的一种散列函数，用以提供消息的完整性保护。

### AES128 算法 advanced encryption standard 128 algorithm

一种标准的分组加密对称密钥加密算法。

### HMAC keyed-hash message authentication code

一种带密钥的哈希消息认证码。

### 大小端约定

没有特殊说明，多字节数据存储及传输按照大端格式。

### 网络环境

本协议可部署于基于NB-IoT的网络环境下。本标准对应用层协议进行了规范。

### 协议格式


### 帧结构

表 1 帧结构

| 字	段 | 长	度 | 代	号 |
|---|---|---|
| 帧头 | 1 | HEAD |
| 协议类型 | 1 | T |
| 协议框架版本 | 1 | V |
| 帧长度 | 2 | L |
| 消息序号 | 1 | MID |
| 控制域 | 1 | C |
| 数据对象ID | 2 | DID |
| 数据域 | L-12 | D |
| 校验域 | 2 | CRC |
| 帧尾 | 1 | TAIL |


### 帧头（HEAD）

数值为68H，一帧的开始。

### 协议类型（T）

协议类型号。为后期扩展预留。本协议对应的类型号为1。

### 协议框架版本（V）

协议框架的版本号。为后期扩展预留。本协议对应的版本号为1。

### 帧长度（L）

整帧长度，包括帧头。

### 消息序号（MID）

下行报文和最近一次的上行报文一致。智能终端表每次发送后数值自增1。
系统端收到报文MID与上一次3分钟内收到的相同，则重发上一次应答给表端。

### 数据对象 ID （DID）

参考附录A中对相关产品的数据对象定义。

### 控制域 (C)


<table border="1" cellpadding="4" style="border-collapse:collapse">
  <tr><th>Bit7</th><th>Bit6</th><th>Bit5</th><th>Bit4</th><th>Bit3</th><th>Bit2</th><th>Bit1</th><th>Bit0</th></tr>
  <tr><td>方向</td><td colspan="2">保留</td><td colspan="5">功能码</td></tr>
</table>

Bit7: 方向标识。0——上行报文，1——下行报文Bit0~Bit4: 功能码。具体内容参考下表。
表 2 功能码

<table border="1" cellpadding="4" style="border-collapse:collapse">
  <tr><th>功能码</th><th>名称</th><th colspan="2">数据域说明</th></tr>
  <tr><td>01H</td><td>主动上报</td><td>上行</td><td>数据对象（明文或密文）+MAC</td></tr>
  <tr><td>02H</td><td>结束帧</td><td>下行</td><td>数据对象（明文或密文）+MAC。终端收到结束帧后，可休眠，下次通信必须从“注册帧上行”开始。</td></tr>
  <tr><td>03H</td><td>预留</td><td colspan="2">预留功能码</td></tr>
  <tr><td rowspan="2">04H</td><td rowspan="2">读数据</td><td>下行</td><td>无</td></tr>
  <tr><td>上行</td><td>数据对象（明文或密文）+MAC</td></tr>
  <tr><td rowspan="2">05H</td><td rowspan="2">写数据</td><td>下行</td><td>数据对象（密文）+MAC</td></tr>
  <tr><td>上行</td><td>2字节错误码（明文，参考 错误码一节）+MAC</td></tr>
  <tr><td rowspan="2">07H</td><td rowspan="2">读记录</td><td>下行</td><td>记录数据对象的下行数据（明文或密文）+MAC</td></tr>
  <tr><td>上行</td><td>记录数据对象的上行数据（明文或密文）+MAC</td></tr>
  <tr><td rowspan="2">08H</td><td rowspan="2">写回读</td><td>下行</td><td>数据对象（密文）+MAC</td></tr>
  <tr><td>上行</td><td>成功时回复：[0x00 0x00+回读数据]（加密） +MAC。<br>失败时回复：2字节错误码（密文，参考 错误码一节）+MAC。</td></tr>
  <tr><td rowspan="2">09H</td><td rowspan="2">注册帧</td><td>上行</td><td>注册信息（明文）+MAC</td></tr>
  <tr><td>下行</td><td>注册结果（明文）+MAC。</td></tr>
  <tr><td>0AH~1FH</td><td>预留</td><td colspan="2">预留功能码</td></tr>
</table>

说明：
数据对象的定义参考附录A中的相关定义。

### 数据域 （D）


### 加密数据补齐算法

补齐算法使用PKCS7Padding算法
PKCS7Padding：填充的原则是，如果需要N字节补齐，报文长度少于N个字节，需要补满N个字节， 补(N-len)个(N-len)。如果报文长度正好时N字节的整数倍，则需要补N个十进制N。
举例：
需要8字节补齐，”123”这个节符串有3个字节，8-3= 5,补满后如：”123”+5个字节的5，如果字符串长度正好是8字节的整数倍，则需要再补8个字节的8。
说明：
本协议采用AES128 ECB加密算法。补齐对齐位数为128位，即16字节。

### MAC 算法

本协议采用“会话MAC密钥”经HMAC-SHA256算法生成MAC。本协议所有报文的数据域都带有MAC。

### 明文+MAC

明文传输且带有 MAC 信息时，首先在数据对象前面添加上“随机数”（该随机数为数据对象“设备通信参数”中的通信随机码），然后经过 MAC 算法计算出 MAC 值。最后将 MAC 追加在数据对象内容后作为数据域。

![图1](nbiot_100\images\img_001.png)

图 2 明文+MAC

### 密文+MAC

当仅密文传输时，首先根据分组加密的分组长度进行数据补齐（补齐算法采用PKCS7标准）。补齐后的数据经过加密生成加密数据域。随机数+加密数据域经过MAC算法计算出MAC。最后将MAC追加到加密数据域后，生成数据域。

![图2](nbiot_100\images\img_002.png)

图 4 密文+MAC

### 校验域 （CRC）

使用CRC16进行计算。计算算法参考附录B。校验范围从“消息序号”到“数据域”。

### 帧尾（TAIL）

数值为16H，一帧的结尾。

### 密钥使用


### 主密钥

主密钥为设备进行加解密和MAC计算的基础密钥，对应附录A中的 2009H数据对象。它不直接用于数据加解密和MAC计算，需要先分解成相应功能的“会话密钥”后再被使用。
说明：主密钥根据密钥状态的不同，可以是“默认密钥”或者“非默认密钥”。

### 加密密钥

“加密密钥”由用“主密钥”对注册数据中的“通信随机码”进行HMAC-SHA256计算取前16字节得到。用于加解密通信数据。

### MAC 密钥

“MAC密钥”由用“主密钥”对注册数据中的“通信随机码”（000CH）进行AES128 ECB加密结果的前16字节。用于对通信数据进行MAC计算。

### 交互流程

终端先发送“注册请求”（详见“附录A”），请求中带有终端厂商商ID，表号信息等数据。主站应答，应答数据中带有时钟等信息，终端可用该时钟信息校时。

![图3](nbiot_100\images\img_003.png)

之后终端上报需要上报的数据对象（具体在实际产品方案中定义）。如果主站有其他操作缓存，则进行交互，交互结束后下发结束帧。当密钥状态（数据对象000EH，随注册帧上报）为“默认”时，本次会话使用默认密钥通信。当密钥状态为“非默认密钥”时，本次会话使用非默认密钥通信。主站可对终端下发“非默认密钥”，并更新密钥状态。

### 终端厂商自定义数据对象范围

数据对象ID从A000H~AFFFH为厂商自定义数据对象域。该范围内的数据对象内容由终端厂商根据指定型号进行定义。不同终端厂商、型号之间可以不同。但同一厂商的相同表型内容定义必须相同。

### 应用协议版本

“应用协议版本”用于说明协议的应用场景。

## 附录 A

（规范性附录） 数据对象定义

### A.1状态数据

状态数据数据上报、读、写格式相同。读请求数据域没有数据。写请求应答2字节错误码（参考“错误码”一节)。是否支持读写在表格中“读写”字段说明。
表 A.1.1 状态数据对象

<table border="1" cellpadding="4" style="border-collapse:collapse">
  <tr><th>ID</th><th>数据对象名称</th><th>读<br>写</th><th colspan="2">长度</th><th colspan="2">内容</th><th>写<br>加密</th><th>读<br>加密</th><th></th></tr>
  <tr><td>0001H</td><td>阀门控制状态</td><td>RW</td><td colspan="2">1</td><td colspan="2">对于没有阀门的产品，该数据对象内容为0，且不允许写操作。<br>阀门状态(0——开阀、解锁；1——关阀门；2——关阀门并锁定）</td><td>√</td><td>-</td><td></td></tr>
  <tr><td rowspan="6">0002H</td><td rowspan="6">时钟</td><td rowspan="6">RW</td><td rowspan="6">6</td><td>1</td><td colspan="2">年，BCD码，00H~99H代表2000~2099</td><td rowspan="6">√</td><td rowspan="6">-</td><td></td></tr>
  <tr><td>1</td><td colspan="2">月，BCD码，01H~12H代表1月~12月</td><td></td></tr>
  <tr><td>1</td><td colspan="2">日，BCD码，01H~31H代表1号~31号</td><td></td></tr>
  <tr><td>1</td><td colspan="2">时，BCD码，00H~23H代表0点~23点</td><td></td></tr>
  <tr><td>1</td><td colspan="2">分，BCD码，00H~59H代表0分~59分</td><td></td></tr>
  <tr><td>1</td><td colspan="2">秒，BCD码，00H~59H代表0秒~59秒</td><td></td></tr>
  <tr><td>0003H</td><td>当前累积量</td><td>R</td><td colspan="2">4</td><td colspan="2">无符号整数（HEX），单位 0.001m3 。</td><td>-</td><td>√</td><td></td></tr>
  <tr><td>0004H</td><td>主电电压</td><td>R</td><td colspan="2">2</td><td colspan="2">无符号整数（HEX），单位0.001V</td><td>-</td><td>-</td><td></td></tr>
  <tr><td>0005H</td><td>主电电量百分比</td><td>R</td><td colspan="2">1</td><td colspan="2">无符号整数（HEX）0~100</td><td>-</td><td>-</td><td></td></tr>
  <tr><td>0006H</td><td>备电电压</td><td>R</td><td colspan="2">2</td><td colspan="2">无符号整数（HEX），单位0.001V</td><td>-</td><td>-</td><td></td></tr>
  <tr><td>0007H</td><td>备电电量百分比</td><td>R</td><td colspan="2">1</td><td colspan="2">无符号整数（HEX）0~100</td><td>-</td><td>-</td><td></td></tr>
  <tr><td>0008H</td><td>预留量</td><td>RW</td><td colspan="2">4</td><td colspan="2">无符号整数，单位0.001m3。</td><td>√</td><td>-</td><td></td></tr>
  <tr><td>0009H</td><td>剩余气量</td><td>RW</td><td colspan="2">4</td><td colspan="2">有符号整数，单位0.001m3。仅当是气量表时有意义。</td><td>√</td><td>-</td><td></td></tr>
  <tr><td>000AH</td><td>透支状态</td><td>RW</td><td colspan="2">1</td><td colspan="2">0——非透支；1——透支</td><td>√</td><td>-</td><td></td></tr>
  <tr><td>000BH</td><td>终端状态</td><td>R</td><td colspan="2">4</td><td colspan="2">表A.1.2</td><td>-</td><td>-</td><td></td></tr>
  <tr><td>000CH</td><td>通信随机码</td><td>R</td><td colspan="2">16</td><td colspan="2">随机数</td><td>-</td><td>-</td><td></td></tr>
  <tr><td>000DH</td><td>单价</td><td>RW</td><td colspan="2">4</td><td colspan="2">无符号整数（HEX），单位0.0001元。</td><td>√</td><td>-</td><td></td></tr>
  <tr><td>000EH</td><td>开户状态</td><td>RW</td><td colspan="2">1</td><td colspan="2">无符号整数（HEX）。0-未开户；1-开户</td><td>√</td><td>-</td><td></td></tr>
  <tr><td>000FH</td><td>剩余金额</td><td>RW</td><td colspan="3">4</td><td>有符号整数（HEX），单位0.01元。仅当时金额<br>表时有效。</td><td>√</td><td colspan="2">-</td></tr>
</table>

表 A.1.1 状态数据对象（续）

<table border="1" cellpadding="4" style="border-collapse:collapse">
  <tr><th>ID</th><th>数据对象名称</th><th>读<br>写</th><th>长度</th><th>内容</th><th>写<br>加密</th><th>读<br>加密</th></tr>
  <tr><td>0010H</td><td>余量状态</td><td>RW</td><td>1</td><td>0--余量正常；1--余量不足（余量可表示剩余金额，也可表示剩余气量）</td><td>√</td><td>-</td></tr>
  <tr><td rowspan="2">0011H</td><td rowspan="2">NB网络信号强度</td><td rowspan="2">R</td><td>2</td><td>RSRP。有符号整数（HEX），单位dBm</td><td>-</td><td>-</td></tr>
  <tr><td>2</td><td>SNR。有符号整数（HEX）,单位dB</td><td>-</td><td>-</td></tr>
  <tr><td>0013H</td><td>ECL覆盖等级</td><td>R</td><td>1</td><td>有符号整数 (HEX)</td><td>-</td><td>-</td></tr>
  <tr><td>0014H</td><td>CellId</td><td>R</td><td>6</td><td>BCD码，最多12位，不足高位补0</td><td>-</td><td>-</td></tr>
  <tr><td>0015H</td><td>REAL_NEARFCN</td><td>R</td><td>2</td><td>实际接入频点,无符号整数（HEX）</td><td>-</td><td>-</td></tr>
  <tr><td>0016H</td><td>IMEI</td><td>R</td><td>15</td><td>模组IMEI号</td><td>-</td><td>-</td></tr>
  <tr><td>0017H</td><td>模组固件版本</td><td>R</td><td>20</td><td>最大20字节字符，不足后补数值0</td><td>-</td><td>-</td></tr>
  <tr><td>0018H</td><td>模组型号</td><td>R</td><td>10</td><td>最大10字节字符，不足后补数值0</td><td>-</td><td>-</td></tr>
  <tr><td>0019H</td><td>表具型号</td><td>R</td><td>1</td><td>0--未定义，1—G1.6,2—G2.5，3—G4,4—G6，5—G10,6—G16,7—G25,8—G40，9—G4W，10—G6W，11—G10W，12—G16W，13—G40W</td><td>-</td><td>-</td></tr>
  <tr><td>001AH</td><td>秘钥状态</td><td>RW</td><td>1</td><td>无符号整数（HEX）。0-默认秘钥；1-客户秘钥</td><td>√</td><td>-</td></tr>
  <tr><td>0100H</td><td>厂商自定义终端状态</td><td>R</td><td>4</td><td>不同厂商、不同型号的表可以定义不同的内容。<br>这部分内容不在本文档中规定。但同一厂商的相同表型内容定义必须相同。</td><td>-</td><td>-</td></tr>
  <tr><td>0101H</td><td>当前工况累积量</td><td>R</td><td>4</td><td>无符号整数（HEX），数值扩大1000倍用于保留3位小数。</td><td>-</td><td>-</td></tr>
  <tr><td>0102H</td><td>当前标况累积量</td><td>R</td><td>4</td><td>无符号整数（HEX），数值扩大1000倍用于保留3位小数。</td><td>-</td><td>-</td></tr>
  <tr><td>0103H</td><td>当前声速</td><td>R</td><td>2</td><td>有符号整数（HEX），数值扩大10倍用于保留1位小数。0x7FFF表示声速异常；单位：m/s；</td><td>-</td><td>-</td></tr>
  <tr><td>0104H</td><td>当前温度</td><td>R</td><td>2</td><td>有符号整数（HEX），数值扩大100倍用于保留2位小数。0x7FFF表示温度异常；单位：℃</td><td>-</td><td>-</td></tr>
  <tr><td>0105H</td><td>当前压力</td><td>R</td><td>4</td><td>无符号整数（HEX），数值扩大1000倍用于保留3位小数。0xFFFFFFFF表示压力异常；单位：kPa</td><td>-</td><td>-</td></tr>
  <tr><td>0106H</td><td>工况瞬时流量</td><td>R</td><td>4</td><td>有符号整数(HEX)，数值扩大1000倍用于保留3位小数。单位 m3/h。</td><td>-</td><td>-</td></tr>
  <tr><td>0107H</td><td>标况瞬时流量</td><td>R</td><td>4</td><td>有符号整数(HEX)，数值扩大1000倍用于保留3位小数。单位 m3/h。</td><td>-</td><td>-</td></tr>
  <tr><td>0108H</td><td>结算累积量类型</td><td>R</td><td>1</td><td>无符号整数（HEX）。0-工况结算；1-标况结算</td><td>-</td><td>-</td></tr>
  <tr><td>0109H</td><td>当前大气压力</td><td>R</td><td>4</td><td>无符号整数（HEX），数值扩大1000倍用于保留3位小数。0xFFFFFFFF表示压力异常；单位：kPa</td><td></td><td></td></tr>
  <tr><td>010AH</td><td>压力传感器超期未校准剩余时间</td><td>R</td><td>1</td><td>无符号整数(HEX)。单位：月。</td><td>-</td><td>-</td></tr>
</table>

表 A.1.2 终端状态

<table border="1" cellpadding="4" style="border-collapse:collapse">
  <tr><th>字节</th><th>BIT</th><th>说明</th></tr>
  <tr><td rowspan="8">BYTE0</td><td>BIT0</td><td>阀门状态（0：关；1：开）</td></tr>
  <tr><td>BIT1</td><td>表具被强制命令关阀（0：否；1：是）</td></tr>
  <tr><td>BIT2</td><td>主电电压低电（0：否；1：是）</td></tr>
  <tr><td>BIT3</td><td>备电电压低（0：否；1：是）</td></tr>
  <tr><td>BIT4</td><td>微小流（0：否；1：是）</td></tr>
  <tr><td>BIT5</td><td>异常大流量（0：否；1：是）</td></tr>
  <tr><td>BIT6</td><td>阀门直通（0：否；1：是）</td></tr>
  <tr><td>BIT7</td><td>燃气报警器联动告警（0：否；1：是）</td></tr>
  <tr><td rowspan="8">BYTE1</td><td>BIT0</td><td>计量异常（0：否；1：是）</td></tr>
  <tr><td>BIT1</td><td>多天不用气告警（0：否；1：是）</td></tr>
  <tr><td>BIT2</td><td>多天不上传告警（0：否；1：是）</td></tr>
  <tr><td>BIT3</td><td>电磁干扰（0：否；1：是）</td></tr>
  <tr><td>BIT4</td><td>主电掉电或不足（0：否；1：是）</td></tr>
  <tr><td>BIT5</td><td>持续流告警（0：否；1：是）</td></tr>
  <tr><td>BIT6</td><td>管道防拆报警（0：否；1：是）</td></tr>
  <tr><td>BIT7</td><td>外壳防拆报警（0：否；1：是）</td></tr>
  <tr><td rowspan="8">BYTE2</td><td>BIT0</td><td>反向流（0：否；1：是）</td></tr>
  <tr><td>BIT1</td><td>温度传感器异常（0：否；1：是）</td></tr>
  <tr><td>BIT2</td><td>压力传感器异常（0：否；1：是）</td></tr>
  <tr><td>BIT3</td><td>温度过高报警（0：否；1：是）</td></tr>
  <tr><td>BIT4</td><td>温度过低报警（0：否；1：是）</td></tr>
  <tr><td>BIT5</td><td>压力过高报警（0：否；1：是）</td></tr>
  <tr><td>BIT6</td><td>压力过低报警（0：否；1：是）</td></tr>
  <tr><td>BIT7</td><td>燃气报警器断开（0：否；1：是）</td></tr>
  <tr><td rowspan="8">BYTE3</td><td>BIT0</td><td>预结算余额不足一级报警（0：否；1：是）</td></tr>
  <tr><td>BIT1</td><td>预结算透支报警（0：否；1：是）</td></tr>
  <tr><td>BIT2</td><td>计量电池断开报警（0：否；1：是）</td></tr>
  <tr><td>BIT3</td><td>保压测试结果（BIT4-BIT3：00无测试，01正常，10异常，11被打断）</td></tr>
  <tr><td>BIT4</td><td></td></tr>
  <tr><td>BIT5</td><td>相对压力过高报警（0：否；1：是）</td></tr>
  <tr><td>BIT6</td><td>相对压力过低报警（0：否；1：是）</td></tr>
  <tr><td>BIT7</td><td>压力传感器超期未校准报警（0：否；1：是）</td></tr>
</table>


表 A.1.3 终端厂商自定义终端状态

<table border="1" cellpadding="4" style="border-collapse:collapse">
  <tr><th>字节</th><th>BIT</th><th>说明</th></tr>
  <tr><td rowspan="8">BYTE0</td><td>BIT0</td><td>环境大气压传感器异常（0：否；1：是）</td></tr>
  <tr><td>BIT1</td><td></td></tr>
  <tr><td>BIT2</td><td></td></tr>
  <tr><td>BIT3</td><td></td></tr>
  <tr><td>BIT4</td><td></td></tr>
  <tr><td>BIT5</td><td></td></tr>
  <tr><td>BIT6</td><td></td></tr>
  <tr><td>BIT7</td><td></td></tr>
</table>

备注： 不支持的报警，bit位 置0 。

#### A.2记录数据

所有记录数据都是只读的，使用读取记录功能码读取或通过“数据上报”功能码主动上报。
表 A.2 记录数据对象

<table border="1" cellpadding="4" style="border-collapse:collapse">
  <tr><th>ID</th><th>数据对象名称</th><th>长度</th><th>内容</th><th>加<br>密</th></tr>
  <tr><td rowspan="6">1000H</td><td rowspan="4">读时间段事件记录（下行）</td><td>2</td><td>事件代码，FFFFH表示所有类型，其他类型参考表A.7</td><td>-</td></tr>
  <tr><td>6</td><td>起始时间，YYMMDDhhmmss,FFFFFFFFFFFFH表示结束时间之前</td><td>-</td></tr>
  <tr><td>6</td><td>结束时间，YYMMDDhhmmss,FFFFFFFFFFFFH表示结束时间之后</td><td>-</td></tr>
  <tr><td>1</td><td>记录条数，1字符，应答不超过该条数。FFH表示不限定</td><td>-</td></tr>
  <tr><td rowspan="2">读时间段事件记录（上行）</td><td>1</td><td>本帧记录条数（1字节），记为N</td><td>-</td></tr>
  <tr><td>N*9</td><td>事件记录数量（事件记录格式参考附录A.6）</td><td>-</td></tr>
  <tr><td rowspan="4">1001H</td><td rowspan="2">读最新事件记录请求<br>（通过“读记录”下行指令）</td><td>2</td><td>事件代码，FFFFH表示所有类型，其他类型参考表A.7</td><td>-</td></tr>
  <tr><td>1</td><td>记录条数，应答不超过该条数。FFH表示不限定</td><td></td></tr>
  <tr><td rowspan="2">读最新事件记录应答<br>（通过“读记录”上行指令）</td><td>1</td><td>本帧记录条数（1字节）,记为N</td><td rowspan="2">-</td></tr>
  <tr><td>N*9</td><td>事件记录数据（事件记录格式参考“事件记录格式”<br>一节）</td></tr>
  <tr><td rowspan="5">1002H</td><td rowspan="2">读每小时用气日志请求<br>（通过“读记录”下行指令）</td><td>3</td><td>起始日期，BCD码， YYMMDD</td><td rowspan="2">-</td></tr>
  <tr><td>1</td><td>天数（1-3）</td></tr>
  <tr><td rowspan="3">读每小时用气日志应答<br>（通过“读记录”上行指令）</td><td>3</td><td>日期，BCD码， YYMMDD</td><td rowspan="3">√</td></tr>
  <tr><td>1</td><td>天数（1-3）</td></tr>
  <tr><td>4*24*天数</td><td>每小时用气数据,无符号整数，单位0.001m3。依次为 1点、2点……次日0点的小时整点累积量数值。如数值2300，代表2.3方。（超声表根据上报类型上报对应的工况/标况累积量）</td></tr>
  <tr><td rowspan="5">1004H</td><td rowspan="2">读日用气记录请求<br>（通过“读记录”下行指令）</td><td>3</td><td>起始日期年月日，BCD码， YYMMDD</td><td rowspan="2">-</td></tr>
  <tr><td>1</td><td>天数（1-31）</td></tr>
  <tr><td rowspan="3">读日用气记录应答<br>（通过“读记录”上行指令）</td><td>3</td><td>起始日期年月日，BCD码， YYMMDD</td><td rowspan="3">√</td></tr>
  <tr><td>1</td><td>天数</td></tr>
  <tr><td>4*天数</td><td>日用气累积量，单位0.001m3。（超声表上报标况累积量）（上报每天24点的数据，举例，比如读取的起始日期是5号，读取3天的数据，上报的数据应该是5/6/7号24点的数据；超声表根据上报类型上报对应的工况/标况累积量）</td></tr>
  <tr><td rowspan="3">1006H</td><td>读月用气记录请求<br>（通过“读记录”下行指<br>令）</td><td>1</td><td>年 BCD码 YY</td><td>-</td></tr>
  <tr><td rowspan="2">读月用气记录应答<br>（通过“读记录”上行指令）</td><td>1</td><td>年 BCD码 YY</td><td rowspan="2">√</td></tr>
  <tr><td>12*4=48</td><td>月用气累积量，单位0.001m3。（超声表根据上报类型上报对应的工况/标况累积量）</td></tr>
  <tr><td rowspan="8">1007H</td><td rowspan="2">读每小时工况、标况累积量请求（通过“读记录”下行指令）</td><td>3</td><td>起始日期，BCD码， YYMMDD</td><td rowspan="2">-</td></tr>
  <tr><td>1</td><td>天数（1）</td></tr>
  <tr><td rowspan="6">读每小时工况、标况累积量应答（通过“读记录”上行指令）</td><td>3</td><td>日期，BCD码， YYMMDD</td><td rowspan="6">-√</td></tr>
  <tr><td>1</td><td>天数（一帧数据最多1天）</td></tr>
  <tr><td rowspan="4">(4+4+2+2)*24*天数</td><td>每小时标况用气累积量,无符号整数值扩大1000倍以保留3位小数。</td></tr>
  <tr><td>每小时工况用气累积量,无符号整数值扩大1000倍以保留3位小数。</td></tr>
  <tr><td>每小时温度值，有符号整数，扩大100倍以保留2位小数。</td></tr>
  <tr><td>每小时压力值，无符号整数，扩大100倍以保留2位小数。</td></tr>
  <tr><td rowspan="8">1008H</td><td rowspan="2">读日工况、标况用气记录请求（通过“读记录”下行指令）</td><td>3</td><td>起始日期年月日，BCD码， YYMMDD</td><td rowspan="2">-</td></tr>
  <tr><td>1</td><td>天数（1-31）</td></tr>
  <tr><td rowspan="6">读日工况、标况用气记录应答（通过“读记录”上行指令）</td><td>3</td><td>起始日期年月日，BCD码， YYMMDD</td><td rowspan="6">-√</td></tr>
  <tr><td>1</td><td>天数（一帧数据最多31天）</td></tr>
  <tr><td rowspan="4">(4+4+2+2)*天数</td><td>日标况用气累积量，整数值扩大1000倍以保留3位小数。</td></tr>
  <tr><td>日工况用气累积量,无符号整数值扩大1000倍以保留3位小数。</td></tr>
  <tr><td>每日温度值，有符号整数，扩大100倍以保留2位小数。</td></tr>
  <tr><td>每日压力值，无符号整数，扩大100倍以保留2位小数。</td></tr>
  <tr><td rowspan="6">1009H</td><td>读月工况、标况用气记录请求（通过“读记录”下行指令）</td><td>1</td><td>年 BCD码 YY</td><td>-</td></tr>
  <tr><td rowspan="5">读月工况、标况用气记录应答（通过“读记录”上行指令）</td><td>1</td><td>年 BCD码 YY</td><td rowspan="5">√</td></tr>
  <tr><td rowspan="4">(4+4+2+2)*12</td><td>月标况用气累积量，整数值扩大1000倍以保留3位小数</td></tr>
  <tr><td>月工况用气累积量,无符号整数值扩大1000倍以保留3位小数。</td></tr>
  <tr><td>每月温度值，有符号整数，扩大100倍以保留2位小数。</td></tr>
  <tr><td>每月压力值，无符号整数，扩大100倍以保留2位小数。</td></tr>
  <tr><td rowspan="3">100AH</td><td>读10分钟用气记录请求（通过“读记录”下行指令）</td><td>3</td><td>起始日期年月日，BCD码， YYMMDD</td><td>-</td></tr>
  <tr><td rowspan="2">读10分钟用气记录应答（通过“读记录”上行指令）</td><td>3</td><td>起始日期年月日，BCD码， YYMMDD</td><td rowspan="2">√</td></tr>
  <tr><td>2*6*24</td><td>每10分钟间隔用气累积增量,无符号整数，单位0.001m3。（超声表根据上报类型上报对应的工况/标况累积增量），记录从00：10开始；</td></tr>
</table>

说明：
掉电期间用气数据（小时用气量、天用气记录、月用气记录）需要按照上电后的当前累积量进行补全。
读取记录超过表端存储时间范围的，表端回复数据为0。

#### A.3设置数据

设置数据数据上报、写请求格式与读应答格式相同。读请求数据域没有数据。写请求应答2 字节错误码（参考“错误码”一节)。是否支持读写在表格中“读写”字段说明。
表 A.3 设置数据对象

<table border="1" cellpadding="4" style="border-collapse:collapse">
  <tr><th>ID</th><th>数据对象名称</th><th>读<br>写</th><th>长<br>度</th><th>内容</th><th>写<br>加密</th><th>读<br>加密</th></tr>
  <tr><td>2000H</td><td>厂商ID</td><td>R</td><td>2</td><td>无符号整数（HEX）<br>金卡厂商ID为 0005</td><td>-</td><td>-</td></tr>
  <tr><td>2001H</td><td>软件版本</td><td>R</td><td>4</td><td>BCD码。厂家自行定义。</td><td>-</td><td>-</td></tr>
  <tr><td>2002H</td><td>终端型号</td><td>R</td><td>2</td><td>无符号整数（HEX）。膜式燃气表--0，超声波燃气表--1</td><td>-</td><td>-</td></tr>
  <tr><td rowspan="2">2003H</td><td rowspan="2">表号编码</td><td rowspan="2">R</td><td>1</td><td>表号长度，1~32</td><td>-</td><td>-</td></tr>
  <tr><td>32</td><td>表号内容，最大32字节字符，不足后补数值0</td><td>-</td><td>-</td></tr>
  <tr><td>2004H</td><td>带阀门</td><td>R</td><td>1</td><td>0——不带阀门；1——带阀门</td><td>-</td><td>-</td></tr>
  <tr><td>2005H</td><td>通信模式</td><td>R</td><td>1</td><td>0，NB-IoT；1，GPRS；2，4G</td><td>-</td><td>-</td></tr>
  <tr><td rowspan="4">2006H</td><td rowspan="4">定时上传参数</td><td rowspan="4">RW</td><td>1</td><td>天数/月份区分位，无符号整数（HEX）。<br>0：表示按天来发送，例如2天一次，3天一次<br>1：表示按月来发送，例如每月的2号，每月的3号</td><td rowspan="4">√</td><td rowspan="4">-</td></tr>
  <tr><td>1</td><td>无符号整数（HEX）。周期值，如果配置为按天传，则该字段表示几天。如果配置为按月传，则该字段表示每月的几号</td></tr>
  <tr><td>1</td><td>上传时间，BCD码，表示时。</td></tr>
  <tr><td>1</td><td>上传时间，BCD码，表示分。</td></tr>
  <tr><td rowspan="2">2007H</td><td rowspan="2">采集服务参数</td><td rowspan="2">RW</td><td>32</td><td>采集服务地址：最大32字节字符，不足后补数值0</td><td rowspan="2">√</td><td rowspan="2">-</td></tr>
  <tr><td>2</td><td>采集服务器端口：无符号整数（HEX）</td></tr>
  <tr><td>2008H</td><td>结算方式</td><td>R</td><td>1</td><td>无符号整数（HEX）0. 金额表；1. 气量表；</td><td>-</td><td>-</td></tr>
  <tr><td rowspan="3">2009H</td><td rowspan="3">密钥参数</td><td rowspan="3">W</td><td>1</td><td>密钥长度。无符号整数（HEX），1~32，单位字节。<br>本协议密钥长度为16字节。</td><td rowspan="3">√</td><td rowspan="3">-</td></tr>
  <tr><td>1</td><td>密钥版本</td></tr>
  <tr><td>32</td><td>密钥。本协议取前16字节作为主密钥。</td></tr>
  <tr><td>200AH</td><td>SIM卡信息</td><td>R</td><td>20</td><td>20字节字符，对应SIM卡的ICCID</td><td>-</td><td>-</td></tr>
  <tr><td>200BH</td><td>运营商信息</td><td>R</td><td>1</td><td>0--电信；1--移动；2--联通</td><td>-</td><td>-</td></tr>
  <tr><td>200CH</td><td>应用协议版本</td><td>R</td><td>2</td><td>BCD码，默认值0000H</td><td>-</td><td>-</td></tr>
  <tr><td>200DH</td><td>供电类型</td><td>R</td><td>1</td><td>0--碱电；1--锂电</td><td>-</td><td>-</td></tr>
  <tr><td>200EH</td><td>错峰间隔时间</td><td>RW</td><td>2</td><td>无符号整数（HEX）。单位秒，范围15~43。</td><td>√</td><td>-</td></tr>
  <tr><td>200FH</td><td>APN</td><td>RW</td><td>32</td><td>最大32字节字符，不足后补数值0。</td><td>√</td><td>-</td></tr>
  <tr><td>2010H</td><td>系统识别号</td><td>R</td><td>2</td><td>系统识别号。默认值0000H</td><td>-</td><td>-</td></tr>
  <tr><td>2020H</td><td>多天不上传参数</td><td>RW</td><td>1</td><td>无符号整数（HEX）。1字节：0--禁止；1~255表示天数。</td><td>√</td><td>-</td></tr>
  <tr><td>2021H</td><td>多天不用气参数</td><td>RW</td><td>1</td><td>无符号整数（HEX）。1字节：0--禁止；1~255 表示<br>天数。</td><td>√</td><td>-</td></tr>
  <tr><td>2022H</td><td>管道防拆报警使能</td><td>RW</td><td>1</td><td>无符号整数（HEX）。0：禁止；1：使能；默认：禁能；</td><td>√</td><td>-</td></tr>
  <tr><td>2023H</td><td>外壳防拆使能</td><td>RW</td><td>1</td><td>无符号整数。0：禁止；1：使能; 默认为0，其他值无效</td><td>√</td><td>-</td></tr>
  <tr><td>2024H</td><td>燃气报警器联动使能</td><td>RW</td><td>1</td><td>无符号整数。0：禁止；1：使能; 默认为0，其他值无效</td><td>√</td><td>-</td></tr>
  <tr><td>2025H</td><td>燃气报警器在线检测</td><td>RW</td><td>1</td><td>无符号整数。0：禁止；1：使能，其他值无效。该指令仅针对支持具有燃气报警器在线功能的设备。</td><td>√</td><td>-</td></tr>
  <tr><td rowspan="4">2026H</td><td rowspan="4">高温报警参数设置</td><td rowspan="4">RW</td><td>1</td><td>1--修改，0--不修改<br>bit0 高温报警功能设置<br>bit1 高温报警门限设置<br>bit2 高温报警持续时间设置<br>对应bit为0时，不操作其对应的功能；<br>应答时，该字段为0。</td><td rowspan="4">√</td><td rowspan="4">-</td></tr>
  <tr><td>1</td><td>高温报警功能设置:<br>bit0--0功能不开启；1-功能开启<br>bit1--0不关阀；1关阀<br>bit2--0普通关阀；1锁定关阀(bit1=0时，bit2无效；只有bit1=1时，bit2才有效，以下其他指令阀门设置都是一样的)</td></tr>
  <tr><td>2</td><td>高温报警阈值设置:<br>有符号整数，低位在前，高位在后；单位：0.01℃。<br>例如7C15H，代表高温报警阈值为55℃。</td></tr>
  <tr><td>2</td><td>高温报警持续时间设置:<br>低位在前，高位在后；无符号整数，单位秒。该参数需大于0；</td></tr>
  <tr><td rowspan="4">2027H</td><td rowspan="4">低温报警参数设置</td><td rowspan="4">RW</td><td>1</td><td>1--修改，0--不修改<br>bit0 低温报警功能设置<br>bit1 低温报警门限设置<br>bit2 低温报警持续时间设置<br>对应bit为0时，不操作其对应的功能；<br>应答时，该字段为0。</td><td rowspan="4">√</td><td rowspan="4">-</td></tr>
  <tr><td>1</td><td>低温报警功能设置:<br>bit0--0功能不开启；1-功能开启<br>bit1--0不关阀；1关阀<br>bit2--0普通关阀；1锁定关阀(bit1=0时，bit2无效；只有bit1=1时，bit2才有效，以下其他指令阀门设置都是一样的)</td></tr>
  <tr><td>2</td><td>低温报警阈值设置:<br>有符号整数，低位在前，高位在后；单位：0.01℃。<br>例如3CF6H，代表低温报警阈值为-25℃。</td></tr>
  <tr><td>2</td><td>低温报警持续时间设置:<br>低位在前，高位在后；无符号整数，单位秒。该参数需大于0；</td></tr>
  <tr><td rowspan="4">2028H</td><td rowspan="4">高压报警参数设置</td><td rowspan="4">RW</td><td>1</td><td>1--修改，0--不修改<br>bit0 高压报警功能设置<br>bit1 高压报警门限设置<br>bit2 高压报警持续时间设置<br>对应bit为0时，不操作其对应的功能；<br>应答时，该字段为0。</td><td rowspan="4">√</td><td rowspan="4">-</td></tr>
  <tr><td>1</td><td>高压报警功能设置:<br>bit0--0功能不开启；1-功能开启<br>bit1--0不关阀；1关阀<br>bit2--0普通关阀；1锁定关阀(bit1=0时，bit2无效；只有bit1=1时，bit2才有效，以下其他指令阀门设置都是一样的)</td></tr>
  <tr><td>4</td><td>高压报警阈值设置:<br>有符号整数，低位在前，高位在后；单位：Pa。<br>例如D0FB0100H，代表高压报警阈值为130000Pa。</td></tr>
  <tr><td>2</td><td>高压报警持续时间设置:<br>低位在前，高位在后；无符号整数，单位秒。该参数需大于0；</td></tr>
  <tr><td rowspan="4">2029H</td><td rowspan="4">低压报警参数设置</td><td rowspan="4">RW</td><td>1</td><td>1--修改，0--不修改<br>bit0 低压报警功能设置<br>bit1 低压报警门限设置<br>bit2 低压报警持续时间设置<br>对应bit为0时，不操作其对应的功能；<br>应答时，该字段为0。</td><td rowspan="4">√</td><td rowspan="4">-</td></tr>
  <tr><td>1</td><td>低压报警功能设置:<br>bit0--0功能不开启；1-功能开启<br>bit1--0不关阀；1关阀<br>bit2--0普通关阀；1锁定关阀(bit1=0时，bit2无效；只有bit1=1时，bit2才有效，以下其他指令阀门设置都是一样的)</td></tr>
  <tr><td>4</td><td>低压报警阈值设置:<br>有符号整数，低位在前，高位在后；单位：Pa。<br>例如18730100H，代表低压报警阈值为95000Pa。</td></tr>
  <tr><td>2</td><td>低压报警持续时间设置:<br>低位在前，高位在后；无符号整数，单位秒。该参数需大于0；</td></tr>
  <tr><td rowspan="4">2100H</td><td rowspan="4">异常大流量功能参数设置</td><td rowspan="4">RW</td><td>1</td><td>1--修改，0--不修改<br>bit0 异常大流量功能设置<br>bit1 异常大流量流速设置<br>bit2 异常大流量时间设置<br>对应bit为0时，不操作其对应的功能；<br>应答时，该字段为0。</td><td rowspan="4">√</td><td rowspan="4">-</td></tr>
  <tr><td>1</td><td>异常大流量功能设置:<br>bit0--0功能不开启；1-功能开启<br>bit1--0异常大流量不关阀；1异常大流量关阀<br>bit2--0异常大流量普通关阀；1异常大流量锁定关阀(bit1=0时，bit2无效；只有bit1=1时，bit2才有效，以下其他指令阀门设置都是一样的)</td></tr>
  <tr><td>4</td><td>异常大流量流速设置:<br>低位在前，高位在后；单位：L/h，例如5000,代表流速为5m³/h，该值范围[1,100000]即流速范围[0.001,100m³/h]</td></tr>
  <tr><td>1</td><td>异常大流量时间设置:<br>无符号整数，单位秒。该参数范围[10,255]；</td></tr>
  <tr><td rowspan="4">2101H</td><td rowspan="4">微小流功能参数设置</td><td rowspan="4">RW</td><td>1</td><td>1--修改，0--不修改<br>bit0 微小流功能设置<br>bit1 微小流流速设置<br>bit2 微小流检测时间阈值<br>对应bit为0时，不操作。<br>应答时，该字段为0。</td><td rowspan="4">√</td><td rowspan="4">-</td></tr>
  <tr><td>1</td><td>微小流功能设置:<br>bit0--0功能不开启    1-功能开启<br>bit1--0微小流不关阀；1微小流关阀<br>bit2--0微小流普通关阀；1微小流锁定关阀</td></tr>
  <tr><td>4</td><td>微小流流速设置：包括流速上限和流速下限设置。<br>其中高2字节为微小流流速上限值，低位在前，高位在后；单位：0.1L/h。例如0x6400,代表流速为10L/h，该参数范围[10,30000],即流速范围[1,3000L/h]；<br>其中低2字节为微小流流速下限值，低位在前，高位在后；单位：0.1L/h，该参数范围[10,30000],即流速范围[1,3000L/h]；<br>备注：上限值要大于下限值。</td></tr>
  <tr><td>2</td><td>微小流检测时间阈值:<br>低位在前，高位在后；单位：min，该参数范围[10,65535]</td></tr>
  <tr><td rowspan="5">2102H</td><td rowspan="5">持续流功能参数设置</td><td rowspan="5">RW</td><td>1</td><td>1-修改，0--不修改<br>bit0 修改持续流功能设置<br>bit1 修改持续流时间阈值<br>bit2 修改持续流流速设置<br>bit3 修改持续流流速波动门限值</td><td rowspan="5">√</td><td rowspan="5">-</td></tr>
  <tr><td>1</td><td>持续流功能设置：<br>bit0--0不启用；1启用<br>bit1--0持续流不关阀；1持续流关阀<br>bit2--0持续流触发普通关阀；1持续流触发锁定关阀</td></tr>
  <tr><td>2</td><td>持续流时间阈值：<br>低位在前，高位在后；单位：min，该参数范围[10,65535]</td></tr>
  <tr><td>8</td><td>持续流流速设置：<br>其中高4字节为持续流流速上限值，低位在前，高位在后；单位：0.1L/h，例如0x6400,代表流速为10L/h，该参数范围[100,0xFFFFFFFF],其中0xFFFFFFFF表示不限制上限流速<br>其中低4字节为持续流流速下限值，低位在前，高位在后；单位：0.1L/h，该参数范围[30,10000],即流速范围[3,1000L/h]。<br>备注：上限值要大于下限值。</td></tr>
  <tr><td>1</td><td>持续流流速波动门限值：<br>无符号整数，单位：百分比%。例50表示流速波动范围50%。<br>为0时，表示无波动限制；</td></tr>
  <tr><td rowspan="3">2103H</td><td rowspan="3">预结算功能参数设置</td><td rowspan="3">RW</td><td>1</td><td>1-修改，0--不修改<br>bit0 修改预结算功能设置<br>bit1 修改预结算门限值设置</td><td rowspan="3">√</td><td rowspan="3">-</td></tr>
  <tr><td>1</td><td>预结算功能设置：<br>bit0--0功能不开启    1-功能开启<br>bit1--0预结算余额不足一级不关阀；1预结算余额不足一级关阀<br>bit2--0预结算余额不足一级普通关阀；1预结算余额不足一级锁定关阀<br>bit3--0预结算透支不关阀；1预结算透支关阀<br>bit4--0预结算透支普通关阀；1预结算透支锁定关阀</td></tr>
  <tr><td>4</td><td>预结算门限值设置：<br>高2字节为预结算余额不足一级门限值，低位在前，高位在后，有符号整数。<br>低2字节为预结算透支门限值，低位在前，高位在后，有符号整数。<br>单位元，预结算余额不足一级门限值大于预结算透支门限值。</td></tr>
  <tr><td rowspan="3">2104H</td><td rowspan="3">反向流参数设置</td><td rowspan="3">RW</td><td>1</td><td>1-修改，0--不修改<br>bit0 修改反向流功能设置</td><td rowspan="3">√</td><td rowspan="3">-</td></tr>
  <tr><td>1</td><td>反向流功能设置：<br>bit0--0不启用；1启用<br>bit1--0反向流不关阀；1反向流关阀<br>bit2--0反向流触发普通关阀；1反向流触发锁定关阀</td></tr>
  <tr><td>30</td><td>预留</td></tr>
  <tr><td rowspan="3">2105H</td><td rowspan="3">温度传感器异常参数设置(基表内)</td><td rowspan="3">RW</td><td>1</td><td>1-修改，0--不修改<br>bit0 修改温度传感器异常功能设置</td><td rowspan="2">√</td><td rowspan="2">-</td></tr>
  <tr><td>1</td><td>温度传感器异常功能设置：<br>bit0--0不关阀；1关阀<br>bit1--0普通关阀；1锁定关阀</td></tr>
  <tr><td>20</td><td>预留</td><td></td><td></td></tr>
  <tr><td rowspan="3">2106H</td><td rowspan="3">压力传感器异常参数设置(基表内)</td><td rowspan="3">RW</td><td>1</td><td>1-修改，0--不修改<br>bit0 修改压力传感器异常功能设置</td><td rowspan="2">√</td><td rowspan="2">-</td></tr>
  <tr><td>1</td><td>压力传感器异常功能设置：<br>bit0--0不关阀；1关阀<br>Bit1--0普通关阀；1锁定关阀</td></tr>
  <tr><td>20</td><td>预留</td><td></td><td></td></tr>
  <tr><td rowspan="3">2107H</td><td rowspan="3">燃气报警器联动参数设置</td><td rowspan="3">RW</td><td>1</td><td>1-修改，0--不修改<br>bit0 修改燃气报警器联动参数设置</td><td rowspan="3">√</td><td rowspan="3">-</td></tr>
  <tr><td>1</td><td>燃气报警器联动参数设置：<br>bit0--0不启用；1启用<br>bit1--0普通关阀；1锁定关阀</td></tr>
  <tr><td>20</td><td>预留</td></tr>
  <tr><td rowspan="4">2108H</td><td rowspan="4">多天不用气参数设置</td><td rowspan="4">RW</td><td>1</td><td>1-修改，0--不修改<br>bit0 修改多天不用气参数设置<br>bit1 多天不用气天数阈值</td><td>√</td><td>-</td></tr>
  <tr><td>1</td><td>多天不用气参数设置：<br>bit0--0不关阀；1关阀<br>bit1--0普通关阀；1锁定关阀</td><td></td><td></td></tr>
  <tr><td>1</td><td>多天不用气天数阈值:<br>单位：天数。(天数不为0代表功能开启)</td><td></td><td></td></tr>
  <tr><td>20</td><td>预留</td><td></td><td></td></tr>
  <tr><td rowspan="4">2109H</td><td rowspan="4">多天不上传参数设置</td><td rowspan="4">RW</td><td>1</td><td>1-修改，0--不修改<br>bit0 修改多天不上传参数设置<br>bit1 多天不上传天数阈值</td><td>√</td><td>-</td></tr>
  <tr><td>1</td><td>多天不上传参数设置：<br>bit0--0不关阀；1关阀<br>bit1--0普通关阀；1锁定关阀</td><td></td><td></td></tr>
  <tr><td>1</td><td>多天不上传天数阈值:<br>单位：天数。(天数不为0代表功能开启)</td><td></td><td></td></tr>
  <tr><td>20</td><td>预留</td><td></td><td></td></tr>
  <tr><td rowspan="3">210AH</td><td rowspan="3">管道防拆参数设置</td><td rowspan="3">RW</td><td>1</td><td>1-修改，0--不修改<br>bit0 修改管道防拆参数设置</td><td>√</td><td>-</td></tr>
  <tr><td>1</td><td>管道防拆参数设置：<br>bit0--0不启用；1启用<br>bit1--0不关阀；1关阀<br>bit2--0普通关阀；1锁定关阀</td><td></td><td></td></tr>
  <tr><td>20</td><td>预留</td><td></td><td></td></tr>
  <tr><td rowspan="3">210BH</td><td rowspan="3">外壳防拆功能参数设置</td><td rowspan="3">RW</td><td>1</td><td>1-修改，0--不修改<br>bit0 修改外壳防拆功能参数设置</td><td>√</td><td>-</td></tr>
  <tr><td>1</td><td>外壳防拆功能参数设置：<br>bit0--0不启用；1启用<br>bit1--0不关阀；1关阀<br>bit2--0普通关阀；1锁定关阀</td><td></td><td></td></tr>
  <tr><td>20</td><td>预留</td><td></td><td></td></tr>
  <tr><td rowspan="7">210CH</td><td rowspan="7">启动保压测试</td><td rowspan="7">RW</td><td>6</td><td>保压测试时间，BCD码，YYMMDDhhmmss</td><td rowspan="7">√</td><td rowspan="7">-</td></tr>
  <tr><td>1</td><td>启动测试流速门限，无符号整数（HEX），单位L/h</td><td></td><td></td></tr>
  <tr><td>1</td><td>保压测试时长，无符号整数（HEX），单位分</td><td></td><td></td></tr>
  <tr><td>2</td><td>保压测试采集间隔，无符号整数（HEX），单位秒</td><td></td><td></td></tr>
  <tr><td>2</td><td>保压压差阈值，无符号整数（HEX），单位Pa</td><td></td><td></td></tr>
  <tr><td>1</td><td>保压异常告警设置：<br>bit0--0不锁定阀门，1锁定阀门</td><td></td><td></td></tr>
  <tr><td>10</td><td>预留</td><td></td><td></td></tr>
  <tr><td rowspan="5">210DH</td><td rowspan="5">相对高压报警参数设置</td><td rowspan="5">RW</td><td>1</td><td>1--修改，0--不修改<br>bit0 相对高压报警功能设置<br>bit1 相对高压报警门限设置<br>bit2 相对高压报警持续时间设置<br>对应bit为0时，不操作其对应的功能；<br>应答时，该字段为0。</td><td rowspan="5">√</td><td rowspan="5">-</td></tr>
  <tr><td>1</td><td>相对高压报警功能设置:<br>bit0--0功能不开启；1-功能开启<br>bit1--0不关阀；1关阀   <br>bit2--0普通关阀；1锁定关阀(bit1=0时，bit2无效；只有bit1=1时，bit2才有效，以下其他指令阀门设置都是一样的)</td><td></td><td></td></tr>
  <tr><td>4</td><td>相对高压报警阈值设置:<br>有符号整数，低位在前，高位在后；单位：Pa。<br>例如D0FB0100H，代表高压报警阈值为130000Pa。</td><td></td><td></td></tr>
  <tr><td>2</td><td>相对高压报警持续时间设置:<br>低位在前，高位在后；无符号整数，单位秒。该参数需大于0；</td><td></td><td></td></tr>
  <tr><td>20</td><td>预留</td><td></td><td></td></tr>
  <tr><td rowspan="5">210EH</td><td rowspan="5">相对低压报警参数设置</td><td rowspan="5">RW</td><td>1</td><td>1--修改，0--不修改<br>bit0 相对低压报警功能设置<br>bit1 相对低压报警门限设置<br>bit2 相对低压报警持续时间设置<br>对应bit为0时，不操作其对应的功能；<br>应答时，该字段为0。</td><td rowspan="5">√</td><td rowspan="5">-</td></tr>
  <tr><td>1</td><td>相对低压报警功能设置:<br>bit0--0功能不开启；1-功能开启<br>bit1--0不关阀；1关阀   <br>bit2--0普通关阀；1锁定关阀(bit1=0时，bit2无效；只有bit1=1时，bit2才有效，以下其他指令阀门设置都是一样的)</td><td></td><td></td></tr>
  <tr><td>4</td><td>相对低压报警阈值设置:<br>有符号整数，低位在前，高位在后；单位：Pa。<br>例如D0FB0100H，代表高压报警阈值为130000Pa。</td><td></td><td></td></tr>
  <tr><td>2</td><td>相对低压报警持续时间设置:<br>低位在前，高位在后；无符号整数，单位秒。该参数需大于0；</td><td></td><td></td></tr>
  <tr><td>20</td><td>预留</td><td></td><td></td></tr>
  <tr><td rowspan="2">210FH</td><td rowspan="2">压力传感器超期未校准参数设置</td><td rowspan="2">RW</td><td>1</td><td>1--修改，0--不修改<br>bit0 压力传感器超期未校准时间门限设置</td><td rowspan="2"></td><td rowspan="2"></td></tr>
  <tr><td>1</td><td>压力传感器超期未校准时间门限；单位：月；<br>默认：0月（0-关闭超期未校准报警功能，非0-开启超期未校准报警功能）</td><td></td><td></td></tr>
</table>

备注：对于协议中设置的不支持的参数、不在限定范围内的参数，终端返回错误码0007写参数值非法。

#### A.4数据集

数据集数据对象时多种数据对象的集合，相关数据对象的定义参考A.1，A.2，A.3。无特殊定义，数据上报、下发、写请求格式与读应答格式相同。读请求数据域没有数据。写请求应答2字节错误码（参考“错误码”一节)。是否支持读写在表格中“读写”字段说明。
表 A.4 数据集数据对象

<table border="1" cellpadding="4" style="border-collapse:collapse">
  <tr><th>ID</th><th>数据对象名称</th><th>长度</th><th>内容</th><th>加<br>密</th></tr>
  <tr><td rowspan="23">3001H</td><td rowspan="21">注册请求<br>（通过“注册帧”上行报文，不支持读写功能码）</td><td>6</td><td>时钟</td><td rowspan="21">-</td></tr>
  <tr><td>2</td><td>系统识别号。默认值0000H</td></tr>
  <tr><td>2</td><td>厂商ID。</td></tr>
  <tr><td>2</td><td>终端型号</td></tr>
  <tr><td>33</td><td>表号参数</td></tr>
  <tr><td>1</td><td>开户状态</td></tr>
  <tr><td>1</td><td>运营商信息</td></tr>
  <tr><td>1</td><td>通信模式</td></tr>
  <tr><td>4</td><td>软件版本</td></tr>
  <tr><td>2</td><td>应用协议版本</td></tr>
  <tr><td>16</td><td>通信随机码</td></tr>
  <tr><td>4</td><td>NB网络信号强度</td></tr>
  <tr><td>1</td><td>ECL覆盖等级</td></tr>
  <tr><td>6</td><td>CellId</td></tr>
  <tr><td>2</td><td>REAL_NEARFCN</td></tr>
  <tr><td>15</td><td>IMEI</td></tr>
  <tr><td>10</td><td>模组型号</td></tr>
  <tr><td>20</td><td>模组固件版本</td></tr>
  <tr><td>1</td><td>密钥版本</td></tr>
  <tr><td>1</td><td>表具型号</td></tr>
  <tr><td>1</td><td>秘钥状态</td></tr>
  <tr><td rowspan="2">注册应答<br>（通过“注册帧”下行报文， 不支持读写功能码）</td><td>2</td><td>错误码</td><td rowspan="2">-</td></tr>
  <tr><td>6</td><td>时钟</td></tr>
  <tr><td rowspan="7">3002H</td><td rowspan="7">通信结束<br>（通过“数据下发”下行报文， 不支持读写功能码）</td><td>2</td><td>错误码</td><td rowspan="7">√</td></tr>
  <tr><td>6</td><td>时钟</td></tr>
  <tr><td>4</td><td>剩余气量</td></tr>
  <tr><td>1</td><td>透支状态</td></tr>
  <tr><td>1</td><td>余量状态（余量可表示剩余金额，也可表示剩余气量）</td></tr>
  <tr><td>4</td><td>单价</td></tr>
  <tr><td>4</td><td>剩余金额</td></tr>
</table>

表 A.4 数据集数据对象（续）

<table border="1" cellpadding="4" style="border-collapse:collapse">
  <tr><th>ID</th><th>数据对象名称</th><th>长度</th><th>内容</th><th>加<br>密</th></tr>
  <tr><td rowspan="11">3003H</td><td rowspan="11">主动上报数据集（膜表）<br>（通过“数据上报”上行报文，不支持读写功能码）</td><td>6</td><td>时钟</td><td>√</td></tr>
  <tr><td>4</td><td>当前累积气量</td><td></td></tr>
  <tr><td>4</td><td>终端状态</td><td></td></tr>
  <tr><td>4</td><td>终端厂商自定义终端状态</td><td></td></tr>
  <tr><td>4</td><td>NB网络信号强度</td><td></td></tr>
  <tr><td>1</td><td>供电类型</td><td></td></tr>
  <tr><td>2</td><td>主电电池电压</td><td></td></tr>
  <tr><td>1</td><td>主电电量百分比</td><td></td></tr>
  <tr><td>100</td><td>上一冻结日的每小时用气日志。前四字节是起始日期和天数。</td><td></td></tr>
  <tr><td>24</td><td>前5冻结日的日用气记录。(上报日期为最早日期的24点的数据，比如当前是11号，上报的日期是6号，五日数据分别为6/7/8/9/10日的24点数据)，前四字节是起始日期和天数。</td><td></td></tr>
  <tr><td>1</td><td>上报方式:<br>0：自动定时<br>1：手动按键<br>3：磁攻击上报（指霍尔、干簧管等磁性元件计量时）<br>5：主电电压低上报<br>6：主电掉电上报<br>7：上电池上报<br>9：计量异常上报（指非磁性元件计量时）<br>10：燃气报警器联动告警上报<br>11：燃气报警器断线上报<br>15：异常大流量上报<br>16：预结算余额不足一级告警上报<br>17：预结算透支告警上报<br>18：管道防拆告警上报<br>19：外壳防拆告警上报<br>20：多天不用气上报<br>21：阀门直通上报<br>22：阀门动作上报<br>23：微小流上报<br>24：持续流告警上报<br>26：压力过高告警上报<br>27：压力过低告警上报<br>28：温度过高告警上报<br>29：温度过低告警上报<br>30：压力传感器异常上报<br>31：温度传感器异常上报<br>255：其它方式上报</td><td></td></tr>
</table>


<table border="1" cellpadding="4" style="border-collapse:collapse">
  <tr><th>ID</th><th>数据对象名称</th><th>方向</th><th>长度</th><th>内容</th><th>加密</th></tr>
  <tr><td rowspan="16">3004H</td><td rowspan="16">主动上报数据集（超声）<br>（通过“数据上报”上行报文，不支持读写功能码）</td><td rowspan="16">↑</td><td>6</td><td>时钟</td><td rowspan="16">√</td></tr>
  <tr><td>4</td><td>当前工况累积气量</td></tr>
  <tr><td>4</td><td>当前标况累积气量</td></tr>
  <tr><td>4</td><td>终端状态</td></tr>
  <tr><td>4</td><td>终端厂商自定义终端状态</td></tr>
  <tr><td>4</td><td>NB网络信号强度</td></tr>
  <tr><td>1</td><td>供电类型</td></tr>
  <tr><td>2</td><td>主电电池电压</td></tr>
  <tr><td>1</td><td>主电电量百分比</td></tr>
  <tr><td>2</td><td>声速</td></tr>
  <tr><td>2</td><td>温度</td></tr>
  <tr><td>4</td><td>压力</td></tr>
  <tr><td>292</td><td>昨天每小时用气日志 参见1007电文格式，前四字节是起始日期和天数</td></tr>
  <tr><td>64</td><td>前5天日用气记录  参见1008电文格式，前四字节是起始日期和天数</td></tr>
  <tr><td>1</td><td>上报方式:<br>0：自动定时<br>1：手动按键<br>5：主电电压低上报<br>6：主电掉电上报<br>7：上电池上报<br>9：计量异常上报（指非磁性元件计量时）<br>10：燃气报警器联动告警上报<br>11：燃气报警器断线上报<br>15：异常大流量上报<br>16：预结算余额不足一级告警上报<br>17：预结算透支告警上报<br>18：管道防拆告警上报<br>19：外壳防拆告警上报<br>20：多天不用气上报<br>21：阀门直通上报<br>22：阀门动作上报<br>23：微小流上报<br>24：持续流告警上报<br>25：反向流上报<br>26：压力过高告警上报<br>27：压力过低告警上报<br>28：温度过高告警上报<br>29：温度过低告警上报<br>30：压力传感器异常上报<br>31：温度传感器异常上报<br>32：计量电池电压低上报<br>33：计量电池断开告警上报<br>34：保压测试结束上报<br>255：其它方式上报</td></tr>
  <tr><td>1</td><td>结算累积量类型</td></tr>
</table>


<table border="1" cellpadding="4" style="border-collapse:collapse">
<tr><th>ID</th><th>数据对象名称</th><th>方向</th><th>长度</th><th>内容</th><th>加密</th></tr>
  <tr><td rowspan="20">3005H</td><td rowspan="20">主动上报数据集（超声-差压）<br>（通过“数据上报”上行报文，不支持读写功能码）</td><td rowspan="20">↑</td><td>6</td><td>时钟</td><td rowspan="20">√</td></tr>
  <tr><td>4</td><td>当前工况累积气量</td><td></td></tr>
  <tr><td>4</td><td>当前标况累积气量</td><td></td></tr>
  <tr><td>4</td><td>终端状态</td><td></td></tr>
  <tr><td>4</td><td>终端厂商自定义终端状态</td><td></td></tr>
  <tr><td>4</td><td>NB网络信号强度</td><td></td></tr>
  <tr><td>1</td><td>供电类型</td><td></td></tr>
  <tr><td>2</td><td>主电电池电压</td><td></td></tr>
  <tr><td>1</td><td>主电电量百分比</td><td></td></tr>
  <tr><td>2</td><td>声速</td><td></td></tr>
  <tr><td>2</td><td>温度</td><td></td></tr>
  <tr><td>4</td><td>压力 （管道压力）</td><td></td></tr>
  <tr><td>4</td><td>大气压力</td><td></td></tr>
  <tr><td>4</td><td>工况瞬时流量</td><td></td></tr>
  <tr><td>4</td><td>标况瞬时流量</td><td></td></tr>
  <tr><td>1</td><td>压力传感器超期未校准剩余时间</td><td></td></tr>
  <tr><td>292</td><td>昨天每小时用气日志 参见1007电文格式，前四字节是起始日期和天数</td><td></td></tr>
  <tr><td>64</td><td>前5天日用气记录  参见1008电文格式，前四字节是起始日期和天数</td><td></td></tr>
  <tr><td>1</td><td>上报方式:<br>0：自动定时<br>1：手动按键<br>5：主电电压低上报<br>6：主电掉电上报<br>7：上电池上报<br>9：计量异常上报（指非磁性元件计量时）<br>10：燃气报警器联动告警上报<br>11：燃气报警器断线上报<br>15：异常大流量上报<br>16：预结算余额不足一级告警上报<br>17：预结算透支告警上报<br>18：管道防拆告警上报<br>19：外壳防拆告警上报<br>20：多天不用气上报<br>21：阀门直通上报<br>22：阀门动作上报<br>23：微小流上报<br>24：持续流告警上报<br>25：反向流上报<br>26：压力过高告警上报<br>27：压力过低告警上报<br>28：温度过高告警上报<br>29：温度过低告警上报<br>30：压力传感器异常上报<br>31：温度传感器异常上报<br>32：计量电池电压低上报<br>33：计量电池断开告警上报<br>34：保压测试结束上报<br>35：相对压力过高告警上报<br>36：相对压力过低告警上报<br>37：压力传感器超期未校准报警上报<br>38：环境大气压传感器异常上报<br>255：其它方式上报</td><td></td></tr>
  <tr><td>1</td><td>结算累积量类型</td><td></td></tr>
</table>


<table border="1" cellpadding="4" style="border-collapse:collapse">
<tr><th>ID</th><th>数据对象名称</th><th>长度</th><th>内容</th><th>加<br>密</th></tr>
  <tr><td rowspan="15">3006H</td><td rowspan="15">主动上报数据集（膜表-差压）<br>（通过“数据上报”上行报文，不支持读写功能码）</td><td>6</td><td>时钟</td><td>√</td></tr>
  <tr><td>4</td><td>当前累积气量</td><td></td></tr>
  <tr><td>4</td><td>终端状态</td><td></td></tr>
  <tr><td>4</td><td>终端厂商自定义终端状态</td><td></td></tr>
  <tr><td>4</td><td>NB网络信号强度</td><td></td></tr>
  <tr><td>1</td><td>供电类型</td><td></td></tr>
  <tr><td>2</td><td>主电电池电压</td><td></td></tr>
  <tr><td>1</td><td>主电电量百分比</td><td></td></tr>
  <tr><td>2</td><td>温度</td><td></td></tr>
  <tr><td>4</td><td>压力 （管道压力）</td><td></td></tr>
  <tr><td>4</td><td>大气压力</td><td></td></tr>
  <tr><td>1</td><td>压力传感器超期未校准剩余时间</td><td></td></tr>
  <tr><td>100</td><td>上一冻结日的每小时用气日志。前四字节是起始日期和天数。</td><td></td></tr>
  <tr><td>24</td><td>前5冻结日的日用气记录。(上报日期为最早日期的24点的数据，比如当前是11号，上报的日期是6号，五日数据分别为6/7/8/9/10日的24点数据)，前四字节是起始日期和天数。</td><td></td></tr>
  <tr><td>1</td><td>上报方式:<br>0：自动定时<br>1：手动按键<br>3：磁攻击上报（指霍尔、干簧管等磁性元件计量时）<br>5：主电电压低上报<br>6：主电掉电上报<br>7：上电池上报<br>9：计量异常上报（指非磁性元件计量时）<br>10：燃气报警器联动告警上报<br>11：燃气报警器断线上报<br>15：异常大流量上报<br>16：预结算余额不足一级告警上报<br>17：预结算透支告警上报<br>18：管道防拆告警上报<br>19：外壳防拆告警上报<br>20：多天不用气上报<br>21：阀门直通上报<br>22：阀门动作上报<br>23：微小流上报<br>24：持续流告警上报<br>26：压力过高告警上报<br>27：压力过低告警上报<br>28：温度过高告警上报<br>29：温度过低告警上报<br>30：压力传感器异常上报<br>31：温度传感器异常上报<br>35：相对压力过高告警上报<br>36：相对压力过低告警上报<br>37：压力传感器超期未校准报警上报<br>38：环境大气压传感器异常上报<br>255：其它方式上报</td><td></td></tr>
</table>


#### A.5用户自定义数据

数据对象ID从A000H~AFFFH为厂商自定义数据对象域。该范围内的数据对象内容由表厂根据指定型号进行定义。不同表厂、型号之间可以不同。但同一厂商的相同表型内容定义必须相同。

#### A.6事件记录格式

事件记录格式：AAAA+YYMMDDHHMMSS+XX；
AAAA:事件类型，2字节，HEX，详见附录A.7；
YYMMDDHHmmSS：事件时间6字节,BCD；
XX预留1字节（用于表示事件详情）；

#### A.7远传终端事件记录类型

事件代码2字节，内容参考下表。A000H~AFFFH为终端厂商自定义数据记录。
表 1 远传表事件记录类型

| 事件代码 | 事件名称 | 说明 |
|---|---|---|
| 0001H | 开阀 | 执行开阀动作 |
| 0002H | 关阀 | 执行关阀动作 |
| 0003H | 重新启动 |  |
| 0004H | 电量低 | 电量比较低，还能支持终端正常工作 |
| 0005H | 电量不足 | 电量很低，不足以支持终端正常工作 |
| 0006H | 磁干扰 | 针对磁采样的终端产品收到磁攻击。 |
| 0007H | 电源断电 | 检测到电池拔出 |
| 0008H | 异常大流量 |  |
| 0009H | 计量异常 |  |
| 000AH | 微小流 |  |
| 000BH | 持续流 |  |
| 000CH | 燃气报警器联动告警 |  |
| 000DH | 直通 |  |
| 000EH | 多天不上传 |  |
| 000FH | 多天不用气 |  |
| 0010H | 逆流 |  |
| 0011H | 外壳拆卸 |  |
| 0012H | 管道拆卸 |  |
| 0013H | 温度传感器异常 |  |
| 0014H | 压力传感器异常 |  |
| 0015H | 温度过高 |  |
| 0016H | 温度过低 |  |
| 0017H | 压力过高 |  |
| 0018H | 压力过低 |  |
| 0019H | 报警器断线 |  |
| 001AH | 预结算余额不足一级报警（余额不足） |  |
| 001BH | 预结算透支报警（透支） |  |
| 001CH | 计量电池低电 |  |
| 001DH | 保压测试结果 | 0正常，1异常，2被打断 |
| 001EH | 相对压力过高 |  |
| 001FH | 相对压力过低 |  |
| 0020H | 压力传感器超期未校准 |  |
| 0021H | 环境大气压传感器异常 |  |
| A000H~AFFFH | 终端厂商自定义事件<br>码 |  |


#### A.8 错误码

本错误码用于定义注册应答时，以及写应答、写回读应答时的错误码。
表 2 错误码

| 错误码 | 说明 |
|---|---|
| 0 | 无错误 |
| 1 | 数据对象ID不支持 |
| 2 | 日期非法 |
| 3 | 协议类型不支持 |
| 4 | 协议框架版本不支持 |
| 5 | MAC认证错误 |
| 6 | 应用协议版本不支持 |
| 7 | 写参数值非法 |
| 8 | 表号非法 |
| 9 | 厂家编号不存在 |
| 10~99 | 应用错误码，在具体产品方案中定义。 |


# 附 录 B

（资料性附录） 校验算法

#### B.1 校验算法

unsigned short CRC16(unsigned char *puchMsg, unsigned int usDataLen)
{
unsigned short wCRCin = 0x0000; unsigned short wCPoly = 0x1021; unsigned char wChar = 0;
while (usDataLen--)
{
wChar = *(puchMsg++); wCRCin ^= (wChar << 8); for(int i = 0;i < 8;i++)
{
if(wCRCin & 0x8000)
wCRCin = (wCRCin << 1) ^ wCPoly; else
wCRCin = wCRCin << 1;
}
}
return (wCRCin) ;
}