/**************************
*
*AT指令接口定义
*接口名定义格式:AT_XX  xx为AT指令 如：AT_CCID
*********************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rt_misc.h>

#include "eat_modem.h"
#include "eat_interface.h"
#include "eat_uart.h"
#include "definedata.h"
#include "UartTask.h"
#include "AppUser1.h"
#include "gps.h"
#include "db.h"
#include "ATapi.h"

/*定义TCP/IP链接序号0-5
*/
#define TCPIP_MUX_COMM 0x00  //通用数据传输链接
#define TCPIP_MUX_GPS  0x01  //GPS星历数据链接
#define TCPIP_MUX_CUSTOM 0x02 //客户服务器链接

#define ATOVERTIME0 1000 //设置普通AT指令的超时时间
#define ATOVERTIME1 3000 //设置普通AT指令的超时时间
#define ATOVERTIME2 5000 //设置普通AT指令的超时时间
#define ATOVERTIME3 7000 //设置普通AT指令的超时时间
#define ATOVERTIME4 10000 //设置普通AT指令的超时时间
#define ATOVERTIME5 20000 //设置普通AT指令的超时时间
#define ATOVERTIME6 30000 //设置普通AT指令的超时时间
unsigned char STRC[256]=
{//该表不区分字母大小写
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
	16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
	32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
	48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,//A
	64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,//
	80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
	96,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
	80,81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,
	128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
	144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
	160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
	176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
	192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
	208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
	224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
	240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};
u8 LM2M_VER[200] = {0};

extern u16 MDM_write(u8 *data, u16 datalen);

static _SIMinf Lsiminf;
static _M2Minf Lm2minf;
static _M2Mstatus Lm2mstatus;

void AT_Init(u8 *seraddr, u32 port){
	Lsiminf.tellen = 0;
	Lsiminf.ccidlen = 0;
	Lsiminf.tsp = 0;
	Lsiminf.business = 0;
	Lsiminf.simfun = 0;
	Lsiminf.province = 0;
	Lsiminf.year = 0;
	Lsiminf.supplier = 0;
	
	Lm2minf.imeilen = 0;
	Lm2minf.cimilen = 0;
	Lm2minf.mmc = 0;
	Lm2minf.mnc = 0;
	
	Lm2mstatus.modemstart = 0;
	Lm2mstatus.gprsbond = 0;
	Lm2mstatus.gprslink = 0;
	Lm2mstatus.m2mstatus = 0;
	Lm2mstatus.csq = 0;
	
	memset((char *)LM2M_VER, 0, 200);
	Lm2mstatus.seraddr[0] = 0;
	if(0 == seraddr || 0 == *(seraddr + 0)){//辅IP异常时采用主IP地址
		user_debug("i:server IP");
		strcpy(Lm2mstatus.seraddr, db_svr_addrget());
	  Lm2mstatus.port = db_svr_portget();
	}
	else{
		user_debug("i:server1 IP[%s,%d]", seraddr, port);
	  if(seraddr != NULL && strlen(seraddr) < 128)strcpy(Lm2mstatus.seraddr, seraddr);
	  Lm2mstatus.port = port;
	}
}

/*临时修改服务器地址
*/
u8 m2m_svr_addrtemplerset(u8 *addr, u32 port){
	if(NULL == addr)return 1;
	user_debug("i:ser-ip-set:%s:%d", addr,port);
	strcpy((s8 *)Lm2mstatus.seraddr, (s8 *)addr);
	Lm2mstatus.port = port;
	
	
	return 0;
}

/*判断是否为同一个服务器地址
*/
u8 m2m_ser_addrcheck(u8 *addr){
	u8 len1,len2;
	
	if(NULL == addr)return 1;
	len1 = strlen((s8 *)addr);
	len2 = strlen((s8 *)Lm2mstatus.seraddr);
	if(len1 != len2)return 2;
	for(len2 = 0; len2 < len1; len2 ++){
		if(*(addr + len2) != Lm2mstatus.seraddr[len2])return 3;
	}
	return 0;
}

u8 m2m_svr_addrreback(void){
	strcpy((s8 *)Lm2mstatus.seraddr, (s8 *)db_svr_addrget());
	Lm2mstatus.port = db_svr_portget();
	
	return 0;
}

void AT_delay(u32 time){
	eat_sleep(time);
}
/*
*可以发送字符串以及BCD码数据，但需要给出数据长度
******************************************/
u16 AT_Send(u8 *data, u16 datalen){
	if(NULL == data || 0 == datalen)return 0;
	return MDM_write(data, datalen);
}
/***
*只能发送字符串数据
***********************/
u16 AT_SendEx(u8 *data){
	u16 datalen;
	if(NULL == data)return 0;
	datalen = strlen(data);
	return MDM_write(data, datalen);
}

/*
*可以发送字符串以及BCD码数据，但需要给出数据长度
******************************************/
u16 AT_Send1(u8 *data, u16 datalen){
	if(NULL == data || 00 == datalen)return 0;
	return MDM_write1(data, datalen);
}
/***
*只能发送字符串数据
***********************/
u16 AT_Send1Ex(u8 *data){
	u16 datalen;
	if(NULL == data)return 0;
	datalen = strlen(data);
	return MDM_write1(data, datalen);
}
/*AT指令接收
*该接口用于接收SIM模块的数据返回  并检测是否有要得数据
*Input: time = 超时时间 以ms为单位
	      str1 = 要查找的数据
	      str2 = 要查找的数据
	   如果str1/str2均为空表示接收到数据后立即返回
	   str1/str2中的'?'符号表示不需要匹配
*Return: 0 = 超时未收到任何数据
	       1 = 收到包含str1的内容
	       2 = 收到包含str2的内容
	       ff = 当str1/str2均为空  接收到数据
*/
u8 AT_Read(u32 time, u8 *str1, u8 *str2)
{
	u8 *rec;
	u32 t;
	u16 reclen,str1len, str2len, strflag,index;
	
	str1len = 0x00;
	str2len = 0x00;
	if(str1)str1len = strlen(str1);
	if(str2)str2len = strlen(str2);
	
	t = 0;
	while(1)
	{
		reclen = MDM_DataToApp(&rec);
		if(reclen != 0x00)
		{
			//user_debug("AT_Read[%d]:%s]",reclen,rec);
			if('E' == *(rec+0) && 'R' == *(rec+1)&& 'R' == *(rec+2)&& 'O' == *(rec+3)&& 'R' == *(rec+4)){
				return 0x7f;
			}
			if(0x00 == str1len && 0x00 == str2len)
			{
				return 0xff;
			}
			if(str1len)
			{//str1不为空
				 strflag = 0x00;
				 for(index = 0x00; index < reclen; index ++)
			   {
			   	   if('?' == *(str1 + strflag) || STRC[*(str1 + strflag)] == STRC[*(rec + index)])
			   	   {//?作为通配符
			   	   	   strflag ++;
			   	   	   if(strflag == str1len)
			   	   	   {
			   	   	   	  return 0x01;
			   	   	   }
			   	   }
			   	   else
			   	   {
			   	   	   strflag = 0x00;
			   	   }
			   }
			}
			if(str2len)
			{//str2不为空
				 strflag = 0x00;
				 for(index = 0x00; index < reclen; index ++)
			   {
			   	   if('?' == *(str2 + strflag) || STRC[*(str2 + strflag)] == STRC[*(rec + index)])
			   	   {//?作为通配符
			   	   	   strflag ++;
			   	   	   if(strflag == str2len)
			   	   	   {
			   	   	   	  return 0x02;
			   	   	   }
			   	   }
			   	   else
			   	   {
			   	   	   strflag = 0x00;
			   	   }
			   }
			}
		}
		t ++;
		if(t * 5  >= time)
		{
			user_debug("i:no data");
			return 0x00;
		}
		eat_sleep(5);
	}
}


/*当接收到的数据包含str1内容时返回该数据长度
*/
u16 AT_ReadEx(u32 time, u8 *str1, u8 **dataout)
{
	u8 *rec;
	u32 t;
	u16 reclen,str1len, strflag,index;
	
	str1len = 0x00;
	if(str1)str1len = strlen(str1);
	t = 0;
	while(1)
	{
		
		reclen = MDM_DataToApp(&rec);
		if(reclen != 0x00)
		{
			//user_debug("AT_ReadEx[%d]:%s]",reclen,rec);
			if('E' == *(rec+0) && 'R' == *(rec+1)&& 'R' == *(rec+2)&& 'O' == *(rec+3)&& 'R' == *(rec+4)){
				return 0;
			}
			if(str1len)
			{//str1不为空
				 strflag = 0x00;
				 for(index = 0x00; index < reclen; index ++)
			   {
			   	   if('?' == *(str1 + strflag) || STRC[*(str1 + strflag)] == STRC[*(rec + index)])
			   	   {//?作为通配符
			   	   	   strflag ++;
			   	   	   if(strflag == str1len)
			   	   	   {
			   	   	   	   *dataout = rec;
			   	   	   	   return reclen;
			   	   	   }
			   	   }
			   	   else
			   	   {
			   	   	   strflag = 0x00;
			   	   }
			   }
			}
			else{
			 *dataout = rec;
			 return reclen;
			}
		}
		t ++;
		if(t * 5 >= time)
		{
			return 0x00;
		}
		eat_sleep(5);
	}
}
/*返回str1位置起的数据
*/
u16 AT_ReadExEx(u32 time, u8 *str1, u8 **dataout)
{
	u8 *rec;
	u32 t;
	u16 reclen,str1len, strflag,index;
	
	str1len = 0x00;
	if(str1)str1len = strlen(str1);
	t = 0;
	while(1)
	{
		
		reclen = MDM_DataToApp(&rec);
		if(reclen != 0x00)
		{
			//user_debug("AT_ReadEx[%d]:%s]",reclen,rec);
			if('E' == *(rec+0) && 'R' == *(rec+1)&& 'R' == *(rec+2)&& 'O' == *(rec+3)&& 'R' == *(rec+4)){
				return 0;
			}
			if(str1len)
			{//str1不为空
				 strflag = 0x00;
				 for(index = 0x00; index < reclen; index ++)
			   {
			   	   if('?' == *(str1 + strflag) || STRC[*(str1 + strflag)] == STRC[*(rec + index)])
			   	   {//?作为通配符
			   	   	   strflag ++;
			   	   	   if(strflag == str1len)
			   	   	   {
			   	   	   	   *dataout = rec + (index - strflag);
			   	   	   	   return reclen - index + strflag;
			   	   	   }
			   	   }
			   	   else
			   	   {
			   	   	   strflag = 0x00;
			   	   }
			   }
			}
			else{
			 *dataout = rec;
			 return reclen;
			}
		}
		t ++;
		if(t * 5 >= time)
		{
			return 0x00;
		}
		eat_sleep(5);
	}
}
/*连接SIM模块，主要用来检测SIM模块是否工作
*单片机是否与SIM进行连接 
*指令：AT
*回应：OK  如果有回显则SIM首先回应 AT \r\n \r\n
*只要在5s钟之内有回应且非error则认为正常
*支持带回显和不带回显
*接收过程应该是：
*    [AT\r\n]
*    \r\n
*    OK\r\n
*Return: 0=失败
	       1=成功
*/
u8 AT_AT(void)
{
	u8 *AT = "AT";
	u8 index;
	
	for(index = 0x00; index < 2; index ++)
	{
	   AT_SendEx(AT);
	   if(0x01 == AT_Read(ATOVERTIME1,"OK",NULL))return 0x01;
	}
	return 0x00;
}

/*自动接听使能
*/
u8 ATS0(void)
{
	u8 *AT = "ATS0=1";
	u8 index;
	
	for(index = 0x00; index < 2; index ++)
	{
	   AT_SendEx(AT);
	   if(0x01 == AT_Read(ATOVERTIME1,"OK",NULL))return 0x01;
	}
	return 0x00;
}


/**
*查询SIM卡是否存在，modem启动后立刻要检测SIM卡是否存在，
*如果不存在则不能执行后行操作
*指令：AT+CPIN?
*回应：+CPIN:READY
*       OK
*Return: 0=失败
	       1=成功
**************************************************/
u8 AT_CPIN(void)
{
	u8 *CPIN = "AT+CPIN?";
	u8 index;
	
	for(index = 0x00; index < 2; index ++)
	{
	   AT_SendEx(CPIN);
	   if(0x01 == AT_Read(500,"OK",NULL))return 0x01;
	}
	return 0x00;
}

/*软启动
*指令∶AT+CFUN=1,1
*回应：IIII
*      RDY
*      OK
*接收到Call Ready表示
*/
u8 AT_CFUN(u8 status)
{
	u8 CMD[32];
	u8 index;
	
	sprintf(CMD, "AT+CFUN=%d",status);
	for(index = 0; index < 3; index ++){
	   AT_SendEx(CMD);
	   if(1 == AT_Read(ATOVERTIME1,"OK",NULL))return 1;
	   eat_sleep(500);
  }
  return 0;
}

/*SIM基本设置  每次SIM启动（硬件、软件启动）需要进行该设置
*回显模式关闭：ATE0      
*              OK
*指令结果返回：ATQ0
*              OK
*返回结果为字符方式：ATV1
*                    OK
*ATE0:关回显
*ATQ0:每条AT执行后都会有相应返回
*ATV1：每条AT执行后返回的是字符信息
*支持带回显和不带回显
*/
u8 AT_EQV(void)
{
  u8 *CMD[3] = {"ATE0","ATQ0","ATV1"};
	u8 index;
	
	for(index = 0x00; index < 3; index ++)
	{
		AT_SendEx(CMD[index]);
		if(0x00 == AT_Read(ATOVERTIME1,"OK",NULL))return 0x00;
	}
	return 0x01;
}


/*获取信号质量
*指令：AT+CSQ
*回应：+CSQ: 23,0
*    ：OK
<RSSI>     0              小于等于-115dBm
                 1               -111dBm
                 2～30       -110～-54dBm
                 31            大于等于-52dBm
                 99            未知参数
*/
u8 AT_CSQ(void)
{
	char *CMD = "AT+CSQ";
	u16 datalen,dataindex;
	u8 *res;
	u8 csq;
	
	csq = 0;
	AT_SendEx(CMD);
	datalen = AT_ReadEx(ATOVERTIME0, "CSQ", &res);
	//user_debug("AT_CSQ:[%d]%s",datalen, res);
	if(datalen > 5 && datalen < 250){
		for(dataindex = 0; dataindex < datalen - 4; dataindex ++){
		   if('C' == *(res + dataindex) && 'S' == *(res + dataindex + 1) &&'Q' == *(res + dataindex + 2)){
		   	dataindex += 3;
		   	while(1){
		   		if(*(res + dataindex) >= '0' && *(res + dataindex) <= '9')break;
		   		dataindex ++;
		   		if(dataindex >= datalen){
		   			user_infor("e:CSQ1:[%d-%d]",dataindex,datalen);
		   			return csq;
		   		}
		   	}
		   	while(1){
		   		if(*(res + dataindex) >= '0' && *(res + dataindex) <= '9'){
		   			csq = csq * 10 + (*(res + dataindex) - '0');
		   			dataindex ++;
		   			if(dataindex >= datalen){
		   				user_infor("e:CSQ2:[%d-%d]",dataindex,csq);
		   				return csq;
		   			}
		   		}
		   		else{
		   			 //user_debug("CSQ3:[%d-%d]",dataindex,csq);
		   			 return csq;
		   		}
		   	}
		  }
		}
	}
	return csq;
}


/*查询SIM是否符着GPRS
*指令：AT+CGATT?                // 查询模块是否附着GPRS网络
*回应：+CGATT: 1       //如果为0 则需要通过AT+CGATT=1进行设置，否则不需要
*      OK
*检测SIM是否符着GPRS，如果符着直接返回，否则则设置符着 
*/
u8 AT_CGATT(void)
{
  u8 *CMD = "AT+CGATT?";
	u8 *CMD1 = "AT+CGATT=1";
	u8 res;
	
	AT_SendEx(CMD);
	res = AT_Read(ATOVERTIME1,"CGATT: 1",NULL); 
	if(0x01 ==  res)
	{
		return 0x01;
	}
	else
	{//未注册
		AT_SendEx(CMD1);
	  if(0x01 == AT_Read(ATOVERTIME1,"OK",NULL))return 0x01; 
	}
	return 0x00;
}

/*获取网络注册信息
*指令：AT+CREG?
*返回：+CREG: 1,1     主要看第二字节，如果是1或5表示已经注册，否则未注册，如果            
                      未注册则采用AT+CREG=1进行注册
*    ：OK
*检测网络是否注册，如果已经注册直接返回01 返回进行注册并检测结果，如果注册失败则返回0
*/
u8 AT_CREG(void)
{
	u8 *CMD = "AT+CREG?";
	u8 *CMD1 = "AT+CREG=1";
	u8 res;
	
	AT_SendEx(CMD);
	res = AT_Read(ATOVERTIME1,"CREG: ?,1","CREG: ?,5"); 
	if(0x01 ==  res || 0x02 == res)
	{
		return res;
	}
	else
	{//未注册
		AT_SendEx(CMD1);
	  if(0x01 == AT_Read(ATOVERTIME1,"OK",NULL))return 0x01; 
	}
	return 0x00;
}

/*查询GPRS网络注册状态
*指令：AT+CGREG?
*回应：+CGREG: 1     //如果为0则需要通过AT+CGREG=1进行测试，否则不需要
*回应：OK
*检测GPRS是否注册到网络  如果已经注册直接返回  否则返回进行注册
*/
u8 AT_CGREG(void)
{
	u8 *CMD = "AT+CGREG?";
	u8 *CMD1 = "AT+CGREG=1";
	u8 res;
	
	AT_SendEx(CMD);
	res = AT_Read(ATOVERTIME1,"CGREG: ?,1","CGREG: ?,5"); 
	if(0x01 ==  res || 0x02 ==  res)
	{
		return 0x01;
	}
	else
	{//未注册
		AT_SendEx(CMD1);
	  if(0x01 == AT_Read(ATOVERTIME1,"OK",NULL))return 0x01; 
	}
	return 0x00;
}

/*设置多TCP/IP链接
*/
u8 AT_CIPMUX(void){
	u8 *CMD = "AT+CIPMUX=1";
	
	AT_SendEx(CMD);
	
	if(0x01 == AT_Read(ATOVERTIME0,"OK",NULL)){
		return 0;
	}
	return 1;
}


/*获取本机号码  不是所有SIM都能获取
*指令：AT+CNUM
*回应：号码
*      OK
*失败返回0
*成功返回号码长度 11
*/
u8 AT_CNUM(u8 *num)
{
	u8 *CMD = "AT+CNUM";
	u16 datalen,dataindex,index;
	u8 *res,data[13];
	
	AT_SendEx(CMD);
	datalen = AT_ReadEx(ATOVERTIME1,"CNUM:", &res); 
	if(0x00 ==  datalen)
	{
		return 0x00;
	}
	else
	{//读出号码 数据在ATDATA中
		 if(datalen < 20)return 0x00;
		 dataindex = 0x00;
	   for(index = 0x00; index < datalen; index ++)
	   {
	   	 if('"' == *(res + index))
	   	 {
	   	 	  if(dataindex != 0x00 && dataindex <11)
	   	 	  {
	   	 	  	dataindex = 0x00;
	   	 	  }
	   	 	  else if(dataindex != 0x00 && dataindex >= 11)
	   	 	  {
	   	 	  	data[dataindex] = 0x00;
	   	 	  	strcpy((char *)num, (char *)&data[0x01]);
	   	 	  	return 11;
	   	 	  }
	   	 	  data[dataindex] = 't';
	   	 	  dataindex ++;
	   	 }
	   	 else if(dataindex)
	   	 {
	   	 	  data[dataindex] = *(res  + index);
	   	 	  dataindex ++;
	   	 }
	   }
	}
	return 0x00;
}

/*AT+CMGF
*/
/*AT+CNMI 短信设置  设备M2M接收到短信后自动返回
*/
u8 AT_CNMI(void){
	u8 *CMGDA = "AT+CNMI=2,2,0,0,0";
	u8 loop;
	
	for(loop = 0; loop < 3; loop ++){
	   AT_SendEx(CMGDA);
	   if(1 == AT_Read(ATOVERTIME1,"OK",NULL))return 1;
	}
	user_debug("i:AT_CNMI error");
	return 0;
}


u8 AT_CMGDA(void){
	u8 *CMGDA = "AT+CMGDA=6";//删除所有短信 PDU格式
	u8 *CMGDAt = "AT+CMGDA=\"DEL ALL\"";//删除所有短信 text格式
	
	AT_SendEx(CMGDA);
	AT_Read(ATOVERTIME1,NULL,NULL);
	AT_SendEx(CMGDAt);
	AT_Read(ATOVERTIME1,NULL,NULL);
	return 1;
}

u8 AT_SMSinit(void){
	static u8 flag = 0;
	u8 *CMGF = "AT+CMGF=1";
  u8 *CSCS = "AT+CSCS=\"GSM\"";
  u8 *CSMP = "AT+CSMP=17,167,0,241";
  s8 *CNMI = "AT+CNMI=2,2,0,0,0";
  u8 result,loop;
  
  if(0x0f == (flag & 0x0f))return 0;
  result = 0;
  if((flag & 0x01) == 0){
      for(loop = 0; loop  < 1; loop ++){
         AT_SendEx(CMGF);
         if(1 == AT_Read(ATOVERTIME0,"OK",NULL)){
         	flag |= 0x01;
         	break;
         }
         eat_sleep(100);
      }
  }
  if(0 == (flag & 0x02)){
      for(loop = 0; loop < 1; loop ++){
         AT_SendEx(CSCS);
         if(1 == AT_Read(ATOVERTIME0,"OK",NULL)){
         	flag |= 0x02;
         	break;
         }
         eat_sleep(100);
      }
  }
  if(0 == (flag & 0x04)){
      for(loop = 0; loop < 1; loop ++){
         AT_SendEx(CSMP);
         if(1 == AT_Read(ATOVERTIME0,"OK",NULL)){
         	flag |= 0x04;
         	break;
        }
         eat_sleep(100);
      }
  }
  if(0 == (flag & 0x08)){
      for(loop = 0; loop < 1; loop ++){
         AT_SendEx(CNMI);
         if(1 == AT_Read(ATOVERTIME0,"OK",NULL)){
         	flag |= 0x08;
         	break;
        }
        eat_sleep(100);
      }
  }
  user_debug("i:AT_SMSinit=%02x", flag);
  return flag;
}
/*********短信发送
*发送英文短信
*AT+CMGS=“1372876114”
***********************/
u8 AT_SMSENDex(u8 *tell, u8 *data)
{
 u8 send[64];
 u8 *tellex;
 u8 *SMsend="AT+CMGS=";
 
 if(NULL == data)return 1;
 if(NULL == tell)tellex = db_svr_ttellget();
 else tellex = tell;
 if(NULL == tellex)return 1;
 
 user_debug("i:SMS:%s", data);
 strcpy(send, SMsend);
 //strcat(send,"\"");
// strcat(send,"13728716114");
 //strcat(send,"\"");
 
 strcat((s8 *)send,"\"");
 strcat((s8 *)send,(s8 *)tellex);
 strcat((s8 *)send,"\"");
 
 AT_SendEx(send);
 if(0xff == AT_Read(ATOVERTIME1,NULL,NULL));//只要收到返回立刻发送
 else
 {
  return 0x01;
 }
 AT_SendEx(data);
 send[0] = 0x1a;
 send[1] = 0;
 AT_Send1(send, 1);
 return 0x00;
}

/*获取IMEI
*指令：AT+GSN
*回应：IMEI
*      OK
*OUTPUT: IMEI --- 存放IMEI
*RETURN: 0 =失败
	       非0 = IMEI字节个数  必须为15
*/
u8 AT_GSN(u8 *IMEI)
{
	u8 *CMD = "AT+GSN";
	u16 datalen, index,index1,loopn;
	u8 *res;
	
	if(0x00 == IMEI)
	{
		return 0x00;
	}
	AT_SendEx(CMD);
	for(loopn = 0x00; loopn < 3; loopn ++)
	{
		  datalen = AT_ReadEx(ATOVERTIME1,NULL,&res);
	    if(datalen >= 15)
	    {
	        index1 = 0x00;
	        for(index = 0x00; index < datalen; index ++)
	        {
	        	if(*(res + index) != 0x0a && *(res + index) != 0x0d)
	        	{
	        		*(IMEI + index1) = *(res + index);
	        		index1 ++;
	        		if(15 == index1)
	        		{
	        			return index1;
	        		}
	        	}
	        	else
	        	{
	        		index1 = 0x00;
	        	}
	        }
	    }
  }
	return 0x00;
}


/*获取CIMI
*指令：AT+CIMI
*回应：CIMI
       OK
*OUTPUT: CIMI --- CIMI
*RETURN: 0 =失败
	       非0 = CIMI字节个数  必须为15
*/
u8 AT_CIMI(u8 *CIMI)
{
	u8 *CMD = "AT+CIMI";
	u16 datalen, index,index1,loopn;
	u8 *res;
	
	if(0x00 == CIMI)
	{
		return 0x00;
	}
	AT_SendEx(CMD);
	for(loopn = 0x00; loopn < 3; loopn ++)
	{
		  datalen = AT_ReadEx(ATOVERTIME1,NULL,&res);
	    if(datalen >= 15)
	    {
	        index1 = 0x00;
	        for(index = 0x00; index < datalen; index ++)
	        {
	        	if(*(res + index) != 0x0a && *(res + index) != 0x0d)
	        	{
	        		*(CIMI + index1) = *(res + index);
	        		index1 ++;
	        		if(15 == index1)
	        		{
	        			return index1;
	        		}
	        	}
	        	else
	        	{
	        		index1 = 0x00;
	        	}
	        }
	    }
	}
	return 0x00;
}

//来电接听
void AT_ATA(void){
	u8 *ATA= "ATA";
	
	AT_SendEx(ATA);
}
//来电挂断
void AT_ATH(void){
	u8 *ATH = "ATH";
	
	AT_SendEx(ATH);
}


/*电压检测 返回电池电压
*/
static u32 Lvol = 0;
u32 m2m_volget(void){
	return Lvol;
}
u32 AT_CBC(void){
	char *CBC = "AT+CBC";
	u32 vol;
	u8 *dataout;
	u8 res;
	u8 loop,u8t1,u8t2;
	
	for(loop = 0; loop < 2; loop++){
	    AT_SendEx(CBC);
	    dataout = NULL;
	    res = AT_ReadEx(ATOVERTIME0,"CBC",&dataout);
	    if(res && dataout != NULL){
	    	//user_debug("CBC:%s",dataout);
	    	for(u8t1 = 0; u8t1 < strlen(dataout); u8t1 ++){
	    		if(',' == *(dataout + u8t1)){
	    			u8t1 ++;
	    			for(; u8t1 < strlen(dataout); u8t1 ++){
	    				if(',' == *(dataout + u8t1)){
	    					u8t1 ++;
	    					vol = 0;
	    					for(; u8t1 < strlen(dataout); u8t1 ++){
	    						if(0x0d == *(dataout + u8t1) || 0x0a == *(dataout + u8t1)){
	    							Lvol = vol;
	    							return vol;
	    						}
	    						vol = vol * 10 + (*(dataout + u8t1) - '0');
	    					}
	    					Lvol = vol;
	    					return vol;
	    				}
	    			}
	    		}
	    	}
	    }
  }
  return 0xffff;
}


/*ADC检测 通过该检测获取设备是否已经拔出
*返回: 0=正常
*      1=电压异常 可能被拔出
*连续3次检测如果电压均小于1V则表示异常退出
*报警原则: 检测到低电平后报警,不连续报警,报警后只有等再次检测到正常电压后才能再次报警
*/
static u8 CADC_WARNING = 0;
u8 AT_CADC(void){
	s8 *AT = "AT+CADC?";
	u8 index,*res,u8t1;
	u16 datalen;
	u32 vol;



	
	for(index = 0x00; index < 3; index ++)
	{
	   AT_SendEx(AT);
	   datalen = AT_ReadEx(ATOVERTIME0,"CADC:", &res); 
	   if(0 == datalen)return 0;
	   //user_debug("CADC:%s", res);
	   for(u8t1 = 0; u8t1 < datalen; u8t1 ++){
	   	if(',' == *(res + u8t1) && '1' == *(res + u8t1 - 1)){
	   		vol = 0;
	   		for(; u8t1 < datalen; u8t1 ++){
	   			if(*(res + u8t1) >= '0' && *(res + u8t1) <= '9'){
	   				vol = vol * 10 + (*(res + u8t1) - '0');
	   			}
	   		}
		
	   		if((vol > 1000)&&(vol!=2800)){
	   			CADC_WARNING = 0;
	   			return 0;
	   		}
	   	}
	  }
	  eat_sleep(300);
	}
	if(CADC_WARNING != 0x55)
	{
		CADC_WARNING = 0x55;
		obd_vol_offenable();//2015/12/17 13:35 fangcuisong
		return 0x01;
	}
	else return 0x00;
}


/*获取CCID  SIM上的ID
*指令：AT+CCID
*回应：CIMI
*      OK
*OUTPUT: CCID --- CCID
*RETURN: 0 =失败
	       非0 = CCID字节个数  必须为20
*/
u8 AT_CCID(u8 *CCID)
{
	u8 *CMD = "AT+CCID";
	u16 datalen, index,index1,loopn;
	u8 *res;
	
	if(0x00 == CCID)
	{
		return 0x00;
	}
	AT_SendEx(CMD);
	for(loopn = 0x00; loopn < 3; loopn ++)
	{
		  datalen = AT_ReadEx(ATOVERTIME1,NULL,&res);
	    if(datalen >= 20)
	    {
	        index1 = 0x00;
	        for(index = 0x00; index < datalen; index ++)
	        {
	        	if(*(res + index) != 0x0a && *(res + index) != 0x0d)
	        	{
	        		*(CCID + index1) = *(res + index);
	        		index1 ++;
	        		if(20 == index1)
	        		{
	        			return index1;
	        		}
	        	}
	        	else
	        	{
	        		index1 = 0x00;
	        	}
	        }
	    }
	}
	return 0x00;
}
u8 *m2m_ccidget(void){
	return (u8 *)&Lsiminf.ccid[0];
}
void m2m_ccidread(void){
	
	if(Lsiminf.ccid[0] != 0)return;
	AT_CCID(Lsiminf.ccid);
}


/*激活移动现场
*指令：AT+CIICR     超时设置40s
*返回：OK
*/
u8 AT_CIICR(void)
{
	char *CMD = "AT+CIICR";
	unsigned char res;
	
	AT_SendEx(CMD);
	res = AT_Read(ATOVERTIME3,"OK",NULL);
	if(0x01 == res)return 0x01;
	else
	{
		return 0x00;
	}
}


/*声音输出设置 通道选择 AT+CSDVC=3  AT+CHFA=1  at+CLVL
*/
u8 AT_CHFA(void){
	char *CMD = "AT+CHFA=1";
	u8 index;//chfa
	
	for(index = 0x00; index < 2; index ++)
	{
	   AT_SendEx(CMD);
	   if(0x01 == AT_Read(ATOVERTIME0,"OK",NULL))return 0x01;
	}
	return 0x00;
}

/*音量设置
*/
u8 AT_CLVL(u8 leve){
	s8 CMD[32];
	u8 index;//chfa
	
	sprintf(CMD, "AT+CLVL=%d", leve);
	for(index = 0x00; index < 2; index ++)
	{
	   AT_SendEx(CMD);
	   if(0x01 == AT_Read(ATOVERTIME0,"OK",NULL))return 0x01;
	}
	return 0x00;
}

/*获取本机IP 
*指令：AT+CIFSR
*返回：IP
*    ：OK
*\CR\LF10.91.107.16\CR\L
*/
u8 AT_CIFSR(u8 *ip)
{
	u8 *CMD = "AT+CIFSR";
	u8 datalen,dataindex,index,loopn;
	u8 *res;
	u8 data[25];
	
	AT_SendEx(CMD);
	for(loopn = 0x00; loopn < 2; loopn ++)
	{
		  datalen = AT_ReadEx(ATOVERTIME2,NULL,&res);
	    if(datalen >= 7)
	    {
	    	dataindex = 0x00;
	      for(index = 0x00; index < datalen; index ++)
	      {
	   	     if(*(res + index) == 0x0a || *(res + index) == 0x0d);//过滤掉回车换行
	   	     else
	   	     {
	   	     	  data[dataindex] = *(res + index);
	   	     	  dataindex ++;
	   	     	  if(dataindex >= 24)break;
	   	     }
	      }
	      data[dataindex] = 0x00;
	      strcpy((char *)ip, (char *)&data[0x00]);
	      return dataindex;
	    }
	}
	return 0x00;
}


/*设置APN
*指令：AT+CSTT=”cmnet”  移动cmnet、cmwap(主要是彩信设置wap) 
*                           联通uninet、3gnet
*回应：OK
*INPUT: flag = 0:移动
	             1:连通
	             2：连通3G
成功返回1
失败返回0
*/
u8 AT_CSTT(u8 flag)
{
	//u8 *CMD[3] = {"AT+CSTT=\"cmnet\"","AT+CSTT=\"uninet\"","AT+CSTT=\"3gnet\""};
	s8 CMD[64];
	s8 *cmdc[3] = {"cmnet", "uninet", "3gnet"};
	u8 *apn;
	u8 index;
	u8 res;
	
	
	if(flag > 2)return 0x00;
	apn = db_svr_apnget();
	if(NULL == apn || 0 == *(apn)){
		sprintf((s8 *)CMD, "AT+CSTT=\"%s\"", cmdc[flag]);
	}
	else sprintf((s8 *)CMD, "AT+CSTT=\"%s\"", (s8 *)apn);
	
	user_debug("i:AT_CSTT:%d,%s",flag, CMD);
	for(index = 0x00; index < 2; index ++)
	{
	   AT_SendEx(CMD);
	   res = AT_Read(ATOVERTIME1,"OK",NULL);
	   if(0x01 == res)return 0x01;
	   AT_delay(ATOVERTIME2);
  } 
  return 0x00;
}


/*服务器IP以及端口设置
*指令：AT+CIPSTART=“TCP”,“116.236.221.75”,7015
*返回：connect ok
*执行该接口前必须执行simAPNset(),  simCIICRset(), simIPget()
*/
void AT_TTS(char *mus){
	u8 CMD[136];
	
	if(NULL == mus)return;
	if(strlen(mus) >= 128)return;
	sprintf((char *)CMD, "AT+CTTS=2,\"%s\"", mus);
	AT_SendEx(CMD);
}

/*.wav文件播放
*/
void AT_CREC(char *file, u8 mu){
	u8 CMD[136];
	
	if(NULL == file)return;
	if(strlen(file) >= 128)return;
	sprintf((char *)CMD, "AT+CREC=4,\"%s\",1,%d", file, mu);
	AT_SendEx(CMD);
}

/*测试端口 用CIPSTART5
*/
u8 AT_CIPSTART_test(u8 *ip, u32 port){
	u8 CMD[64];
	u8 data[16];
	
	if(0x00 == ip)
	{
		user_debug("i:IP IS NULL");
		return 0x00;
	}
	strcpy((char *)&CMD[0x00],"AT+CIPSTART=5,");
	strcat((char *)&CMD[0x00],"\"TCP\",\"");
	strcat((char *)&CMD[0x00],ip);
	strcat((char *)&CMD[0x00],"\",");
	sprintf((char *)&data[0x00],"%d",port);
	strcat((char *)&CMD[0x00],(char *)&data[0x00]);
	AT_SendEx(CMD);
	user_debug(CMD);
	if(0x01 == AT_Read(ATOVERTIME3,"CONNECT OK",NULL))return 0x01;
	else
	{
		return 0x00;
	} 
}

u8 AT_CIPSEND_test(u8 *data, u16 datalen)
{
	u8 CMD[32];//="AT+CIPSEND=%d";
	u8 u8result, u8t1;
	u8 loop;
	
	if(NULL == data || 0 == datalen)return 0;
	
  loop = 0;
_loop:
  sprintf((s8 *)CMD, "AT+CIPSEND=5,%d", datalen);
	AT_SendEx(CMD);
	u8result = AT_Read(ATOVERTIME2,">",NULL);
	if(0x01 == u8result);//只要收到返回立刻发送
	else if(0x7f == u8result){//返回error
		  if(loop <3){
		  	eat_sleep(100);
		  	loop ++;
		  	goto _loop;
		  }
	    m2m_statusSet(3);
	    user_debug("i:Restart:error");
	    return 0x7f;
	}
	else
	{
		return 0x02;
	}
	AT_Send1(data, datalen);
	
	eat_sleep(100);//休眠100ms
	return 0x00;
}

/*服务器IP以及端口设置
*指令：AT+CIPSTART=“TCP”,"  ”,7015
*返回：connect ok
*执行该接口前必须执行simAPNset(),  simCIICRset(), simIPget()
*/
u8 AT_CIPSTART(u8 *ip, u32 port)
{
	u8 CMD[64];
	u8 data[16];
	
	if(0x00 == ip)
	{
		user_debug("i:IP IS NULL");
		return 0x00;
	}
	user_debug("lilei-make socket\r\n");
	strcpy((char *)&CMD[0x00],"AT+CIPSTART=0,");
	strcat((char *)&CMD[0x00],"\"TCP\",\"");
	strcat((char *)&CMD[0x00],ip);
	strcat((char *)&CMD[0x00],"\",");
	sprintf((char *)&data[0x00],"%d",port);
	strcat((char *)&CMD[0x00],(char *)&data[0x00]);
	AT_SendEx(CMD);
	user_debug(CMD);
	if(0x01 == AT_Read(ATOVERTIME3,"CONNECT OK",NULL))return 0x01;
	else
	{
		return 0x00;
	} 
	
}

/*服务器IP以及端口设置  用于启动GPS连接
*指令：AT+CIPSTART=“TCP”,“116.236.221.75”,7015
*返回：connect ok
*执行该接口前必须执行simAPNset(),  simCIICRset(), simIPget()
*/
u8 AT_CIPSTART_1(u8 *ip, u32 port)
{
	u8 CMD[64];
	u8 data[16];
	
	if(0x00 == ip)
	{
		user_debug("i:IP IS NULL");
		return 0x00;
	}
	strcpy((char *)&CMD[0x00],"AT+CIPSTART=1,");
	strcat((char *)&CMD[0x00],"\"TCP\",\"");
	strcat((char *)&CMD[0x00],ip);
	strcat((char *)&CMD[0x00],"\",");
	sprintf((char *)&data[0x00],"%d",port);
	strcat((char *)&CMD[0x00],(char *)&data[0x00]);
	AT_SendEx(CMD);
	user_debug(CMD);
	if(0x01 == AT_Read(ATOVERTIME3,"CONNECT OK",NULL)){
		return 0x01;
	}
	else
	{
		return 0x00;
	} 
	
}


/*关闭移动场景
*指令：AT+CIPSHUT
*返回：
*当网络链路被断开后需要先用该指令关移动场景 10s再重新建立连接
*/
u8 AT_CIPSHUT(void)
{
	char *CMD="AT+CIPSHUT";
	
	AT_SendEx(CMD);
	if(0x01 == AT_Read(ATOVERTIME6,"OK",NULL))return 0x01;
	return 0x00;
}

/*断开网络
*指令：AT+CIPCLOSE=1    //为1表示快速关闭100ms，为0慢关 2分钟左右
*返回：OK
模块设置超时20s，一般TCP/IP连接异常使用AT+CIPSHUT后，重新执行AT+CSTT等重新建立TCP/IP连接，建议间隔时间10s，否则可能会因为移动场景关闭还没有彻底释放，导致重新激活移动场景失败。
*/
u8 AT_CIPCLOSE(void)
{
	char *CMD="AT+CIPCLOSE=0,1";
	
	AT_SendEx(CMD);
	if(0x01 == AT_Read(ATOVERTIME5,"OK",NULL))return 0x01;
	return 0x00;
}

/*断开网络 断开与GPS星历服务器的连接
*指令：AT+CIPCLOSE=1    //为1表示快速关闭100ms，为0慢关 2分钟左右
*返回：OK
模块设置超时20s，一般TCP/IP连接异常使用AT+CIPSHUT后，重新执行AT+CSTT等重新建立TCP/IP连接，建议间隔时间10s，否则可能会因为移动场景关闭还没有彻底释放，导致重新激活移动场景失败。
*/
u8 AT_CIPCLOSE_1(void)
{
	char *CMD="AT+CIPCLOSE=1,1";
	
	AT_SendEx(CMD);
	if(0x01 == AT_Read(ATOVERTIME5,"OK",NULL))return 0x01;
	return 0x00;
}


u8 *m2m_verget(void){
	//user_debug("---[%s]", (char *)LM2M_VER);
	return (u8 *)&LM2M_VER[0];
}
/*获取固件版本信息
*/
u32 AT_VER(void){
	char *CMD = "AT+GSV";
	u32 datalen;
	u8 *res;
	
	AT_SendEx(CMD);
	res = 0;
	datalen = AT_ReadEx(ATOVERTIME1,NULL,&res);
	if(datalen > 5){
	    if(datalen > 180){
	    	datalen = 180;
	    	strncpy((char *)LM2M_VER, (char *)res, datalen);
	    }
	    else strcpy((char *)LM2M_VER, (char *)res);
	    user_debug("i:M2M_VER=%s\r\n",LM2M_VER);                   //add by lilei-2016-04-07
  }
	else datalen = 0;
	return datalen;
}

u8 AT_CIPCLOSE_test(void)
{
	char *CMD="AT+CIPCLOSE=5,1";
	
	AT_SendEx(CMD);
	return 0x01;
}

/*数据发送
*指令：AT+CIPSEND
*回应：>                //输入要发送的数据  1A结尾
*    ：send OK
*/
u8 AT_CIPSEND(u8 *data, u16 datalen)
{
	u8 CMD[32];//="AT+CIPSEND=%d";
	u8 u8result, u8t1;
	u8 loop;
	
	if(NULL == data || 0 == datalen)return 0;
	
	/*u8t1 = AT_CSQ();
	if(u8t1 < 4 ){//信号太弱
		user_debug("AT_CIPSEND:CSQ low:%d", u8t1);
		return 0x01;
	}*/
	
  loop = 0;
   user_debug("\r\ni:CIPSEND-ToSVR-CMD:%02X,CMD=%02X,Len=%d\r\n",*(data + 8),*(data + 9),datalen);
_loop:
  sprintf((s8 *)CMD, "AT+CIPSEND=0,%d", datalen);
	AT_SendEx(CMD);
	u8result = AT_Read(ATOVERTIME2,">",NULL);
	if(0x01 == u8result);//只要收到返回立刻发送
	else if(0x7f == u8result){//返回error
		  if(loop <3){
		  	eat_sleep(100);
		  	loop ++;
		  	goto _loop;
		  }
	    m2m_statusSet(3);
	    user_debug("i:Restart:error");
	    return 0x7f;
	}
	else
	{
		return 0x02;
	}
	AT_Send1(data, datalen);
	
	eat_sleep(100);//休眠100ms
	return 0x00;
}


/*数据发送 用于发送GPS星历数据
*指令：AT+CIPSEND
*回应：>                //输入要发送的数据  1A结尾
*    ：send OK
*/
u8 AT_CIPSEND_1(u8 *data, u16 datalen)
{
	u8 CMD[32];//="AT+CIPSEND=%d";
	u8 u8result, u8t1;
	u8 loop;
	
	if(NULL == data || 0 == datalen)return 0;
	
  loop = 0;
_loop:
  sprintf((s8 *)CMD, "AT+CIPSEND=1,%d", datalen);
	AT_SendEx(CMD);
	u8result = AT_Read(ATOVERTIME2,">",NULL);
	if(0x01 == u8result);//只要收到返回立刻发送
	else if(0x7f == u8result){//返回error
		  if(loop <3){
		  	eat_sleep(100);
		  	loop ++;
		  	goto _loop;
		  }
	    user_debug("i:Restart:error");
	    return 0x7f;
	}
	else
	{
		return 0x02;
	}
	AT_Send1(data, datalen);
	
	eat_sleep(100);//休眠100ms
	return 0x00;
}


/*获取基站信息
*指令：AT+CENG开启或关闭工程模式
*返回：+CENG:0,...
*
***************************************************************/
u8 AT_CENG(void)
{
	u8 CMD1[32] ="AT+CENG=1";
	u8 CMD[32] ="AT+CENG?";
	u16 datalen;
	u8 *res;
	
	user_infor("e:AT_CENG");
	AT_SendEx(CMD1);
	if(0x00 == AT_Read(ATOVERTIME1,"OK",NULL))return 0x01;
	
	AT_SendEx(CMD);
	datalen = AT_ReadExEx(ATOVERTIME1,"CENG: 0",&res);
	if(datalen){//将接收到的数据发送到GPS缓冲区
		gps_mobileinfor(res);
		return 0;
	}
	return 0x03;
}

void AT_CENG_CELL(u32 *lac, u32 *cell){
	u8 CMD[32] ="AT+CENG?";
	u16 datalen, dataindex;
	u8 *res,flag;
	u32 u32lac, u32cell;
	
	if(NULL == lac || NULL == cell)return;
	AT_SendEx(CMD);
	datalen = AT_ReadExEx(ATOVERTIME1,"CENG:",&res);
	u32lac = 0;
	u32cell = 0;
	if(datalen){
		for(dataindex = 0; dataindex < datalen; dataindex ++){
			if('E' == *(res + dataindex) && 'N' == *(res + dataindex +1) && 'G' == *(res + dataindex +2) && ':' == *(res + dataindex+3) && '0' == *(res + dataindex+5)){
				dataindex += 8;
				flag = 0;
				for(; dataindex < datalen; dataindex ++){
					if(0x0d == *(res + dataindex) || 0x0a == *(res + dataindex))break;
					if(',' == *(res + dataindex))flag ++;
					else if(6 == flag){//cellid
						if(*(res + dataindex) >= '0' && *(res + dataindex) <= '9')u32cell = (u32cell << 4) + (*(res + dataindex) - '0');
						else if(*(res + dataindex) >= 'a' && *(res + dataindex) <= 'f')u32cell = (u32cell << 4) + (*(res + dataindex) - 'a') + 0x0a;
						else if(*(res + dataindex) >= 'A' && *(res + dataindex) <= 'F')u32cell = (u32cell << 4) + (*(res + dataindex) - 'A') + 0x0a;
					}
					else if(9 == flag){//lac id
						if(*(res + dataindex) >= '0' && *(res + dataindex) <= '9')u32lac = (u32lac << 4) + (*(res + dataindex) - '0');
						else if(*(res + dataindex) >= 'a' && *(res + dataindex) <= 'f')u32lac = (u32lac << 4) + (*(res + dataindex) - 'a') + 0x0a;
						else if(*(res + dataindex) >= 'A' && *(res + dataindex) <= 'F')u32lac = (u32lac << 4) + (*(res + dataindex) - 'A') + 0x0a;
					}
					if(flag > 9)break;
				}
				break;
			}
		}
	}
	*lac = u32lac;
	*cell = u32cell;
}

/******************************************************************
*UDP通讯 专纹教ㄐ樯杓�
*2015/5/7 21:18
********************************************************************************/
unsigned char UDP_Creat(void){
	//char *cip = "AT+CIPSTART=4,\"UDP\",\"\",30005";//4,%d,\"\",30005
	char cip[64];
	char *cipclose = "AT+CIPCLOSE=4,1";
	unsigned char u8result;//db_svr_addrget
	
	//2015/9/28 10:50 fangcuisong
	sprintf(cip, "AT+CIPSTART=4,\"UDP\",\"%s\",%d", db_svr_addrget(), db_svr_portget());
	AT_SendEx(cip);
	u8result = AT_Read(ATOVERTIME1,"OK","ERROR:");
	if(1 == u8result)return 0;
	else{
	  	AT_SendEx(cip);
	  	u8result = AT_Read(ATOVERTIME1,"OK","ERROR:");
	  	if(1 == u8result)return 0;
	  	else{
	  		 AT_SendEx(cipclose);
	  		 return 1;
	  	}
	}
	return 0;
}

void UDP_Cipclose(void){
	char *cipclose = "AT+CIPCLOSE=4,1";
	
	AT_SendEx(cipclose);
	AT_Read(ATOVERTIME1,"OK","ERROR:");
}

unsigned char UDP_Send(unsigned char *send, int sendlen){
	char CMD[32];
	unsigned char u8result;
	
	if(NULL == send || 0 == sendlen)return 1;
	sprintf(CMD, "AT+CIPSEND=4,%d", sendlen);
	AT_SendEx(CMD);
	u8result = AT_Read(ATOVERTIME1,">","ERROR:");
	if(2 == u8result || 0x7f == u8result){
		return 2;
	}
	AT_Send1(send, sendlen);
	
	return 0;
}

/***
*检测modem是否启动正常
*如果已启动则设置modemstart
************************************************/
void m2m_startcheck(void){
	u8 u8result;
	
	user_infor("e:m2m_startcheck >>");
	if(0 == AT_AT())
	{
		Lm2mstatus.modemstart = 0;
		user_debug("i:m2m_startcheck:AT error");
		return;
	}
	
	if(0 == ATS0()){
		Lm2mstatus.modemstart = 0;
		user_debug("m2m_startcheck:ATS0 error");
		return;
	}																					//add by lilei-2017-0623															
	//参数设置
	if(0 == AT_EQV())
	{
		Lm2mstatus.modemstart = 0;
		user_debug("i:m2m_startcheck:EQV error");
		return;
	}
	
	AT_CHFA();
	eat_sleep(200);
	AT_CLVL(99);
	
	Lm2mstatus.modemstart = 0x01;
	Lm2mstatus.m2mstatus = 1;
	user_debug("i:m2m_startcheck OK");
	AT_delay(100);
}

/*
*读取SIM信息
**********************************************/
void sim_information(void){
	u8 u8t1;
	
	AT_CBC();
	user_infor("e:sim_information >>");
	if(0 == AT_CPIN())
	{
		user_debug("i:m2m_startcheck:CPIN error");
		return;
	}
  	memset(Lm2minf.cimi, 0, 16);
	u8t1 = AT_CIMI(Lm2minf.cimi);
	if(u8t1)
	{
		if(u8t1 > 15)Lm2minf.cimilen = 15;
		else Lm2minf.cimilen = u8t1;
		if(u8t1 >= 3)
		{
			Lm2minf.mmc = Lm2minf.cimi[0] - '0';
			Lm2minf.mmc = (Lm2minf.mmc << 4) + (Lm2minf.cimi[1] - '0');
			Lm2minf.mmc = (Lm2minf.mmc << 4) + (Lm2minf.cimi[2] - '0');
		}
		if(u8t1 >= 5)
		{
			Lm2minf.mnc = Lm2minf.cimi[3] - '0';
			Lm2minf.mnc = (Lm2minf.mnc << 4) + (Lm2minf.cimi[4] - '0');
		}
		user_infor("e:MNC:%s",Lm2minf.mnc);
	}
	memset(Lsiminf.ccid, 0, 21);
	u8t1 = AT_CCID(Lsiminf.ccid);
	if(u8t1)
	{
		if(u8t1 > 20)Lsiminf.ccidlen = 20;
		else Lsiminf.ccidlen = u8t1;
		if(u8t1 >= 6)
		{
			if('8' == Lsiminf.ccid[0] && '9' == Lsiminf.ccid[1] && '8' == Lsiminf.ccid[2] && '6' == Lsiminf.ccid[3] && '0' == Lsiminf.ccid[4] && '0' ==Lsiminf.ccid[5])
			{
			    Lsiminf.tsp = 1;
			}
			else if('8' == Lsiminf.ccid[0] && '9' == Lsiminf.ccid[1] && '8' == Lsiminf.ccid[2] && '6' == Lsiminf.ccid[3] && '0' == Lsiminf.ccid[4] && '1' ==Lsiminf.ccid[5])
			{
			    Lsiminf.tsp = 2;
			}
		}
		if(u8t1 >= 7)Lsiminf.business = Lsiminf.ccid[6] - '0';
		if(u8t1 >= 8)Lsiminf.simfun = Lsiminf.ccid[7] - '0';
		if(u8t1 >= 10)
		{
			Lsiminf.province = Lsiminf.ccid[8] - '0';
			Lsiminf.province = (Lsiminf.province << 4) + (Lsiminf.ccid[9] - '0');
		}
		if(u8t1 >= 12)
		{
			Lsiminf.year = Lsiminf.ccid[10] - '0';
			Lsiminf.year = (Lsiminf.year << 4) + (Lsiminf.ccid[11] - '0');
		}
		if(u8t1 >= 13)
		{
			Lsiminf.supplier = Lsiminf.ccid[12] - '0';
		}
		user_infor("e:CCID:%s",Lsiminf.ccid);
	}
	//如果CCID与FLASH中保存的不同则需要重新获取电话号码 否则直接使用FLASH中的保存的号码
	
	if(1)
	{
		  memset(Lsiminf.tel, 0, 12);
	    	  u8t1 = AT_CNUM(Lsiminf.tel);
	    	  if(u8t1)
		 {
	    		if(u8t1 > 11)Lsiminf.tellen = 11;
	    		else Lsiminf.tellen = u8t1;
	    		user_infor("e:tel:%s",Lsiminf.tellen);
	    	  }
	    	  else{//SIM卡中没有电话号码 需要通过其他方式获得
	   }
  }
 
  AT_SMSinit(); 
  user_infor("e:sim_information OK");
  Lm2mstatus.m2mstatus = 3;
  AT_delay(100);
}

u8 * m2m_imeiget(void){
	return (u8 *)Lm2minf.imei;
}
/*
*读取M2M信息
*********************************************************/
void m2m_information(void){
	u8 u8t1;
	
	AT_CBC();
	user_infor("e:m2m_information >>");
	memset(Lm2minf.imei, 0, 16);
	u8t1 = AT_GSN(Lm2minf.imei);
	if(u8t1)
	{
		if(u8t1 > 15)Lm2minf.imeilen = 15;
		else Lm2minf.imeilen = u8t1;
		user_infor("e:IMEI:%s",Lm2minf.imei);
	}
	
	
  
	if(0 == AT_VER())
	{
		user_debug("i:AT_VER error");
		return;
	}
	
	if(AT_CIPMUX() != 0)
	{//2015/1/15 9:51  采用多链接方式
		user_debug("i:AT_CIPMUX error");
		return;
  	}
  
  //AT_SMSinit();
	Lm2mstatus.m2mstatus = 2;
	user_infor("e:m2m_information OK");
	AT_delay(100);
}


/**
*检测M2M的网络附着状态
*如果已经附着到GPRS，需要设置Lm2mstatus.gprsbond参数
*
***************************************************/
void m2m_gprsbond(void){
	u8 u8t1;
	user_infor("e:m2m_gprsbond >>");
	/*u8t1 = AT_CSQ();
	if(u8t1 < 4 ){//信号太弱
		Lm2mstatus.gprsbond = 0;
		user_debug("m2m_gprsbond:CSQ low:%d", u8t1);
		AT_delay(500);
		return;
	}*/
	Lm2mstatus.csq = u8t1;
	AT_delay(300);
	if(0 == AT_CGREG()){//查询网络注册信息
		Lm2mstatus.gprsbond = 0;
		user_debug("i:m2m_gprsbond:CGREG error");
		AT_delay(500);
		return;
	}
	AT_delay(300);
	if(0 == AT_CGATT()){//查询SIM卡是否附着GPRS
		Lm2mstatus.gprsbond = 0;
		user_debug("i:m2m_gprsbond:CGATT error");
		return;
	}
	AT_delay(300);
	Lm2mstatus.gprsbond = 0x01;
	Lm2mstatus.m2mstatus = 4;
	user_infor("e:m2m_gprsbond OK");
}


/**
*m2m连接到GPRS
************************************************/
void m2m_gprslink(void){
	u8 u8t1;
	static u8 flag = 0;
	
	AT_CIPSHUT();//首先关闭现场  不关心是否成功
	eat_sleep(1000);//等待1秒
	user_debug("i:m2m_gprslink >>");
	if(1 == Lsiminf.tsp)
	{
		if(0 == AT_CSTT(0))
		{
			Lm2mstatus.gprslink = 0;
			user_debug("i:m2m_gprslink:CSTT error");
			AT_delay(500);
		  	return;
		}
	}
	else if(2 == Lsiminf.tsp)
	{
		if(0 == AT_CSTT(1))
		{
			Lm2mstatus.gprslink = 0;
			user_debug("i:m2m_gprslink:CSTT1 error");
			AT_delay(500);
		  	return;
		}
	}
	else
	{
		user_debug("i:m2m_gprslink:CSTT1 tsp[%d]", Lsiminf.tsp);
		if(0 == flag)
		{
			AT_CSTT(0);
			flag = 0x55;
		}
		else
		{
			AT_CSTT(1);
			flag = 0;
		}
	}
	AT_delay(400);
	if(0 == AT_CIICR())
	{
		Lm2mstatus.gprslink = 0;
		user_debug("i:m2m_gprslink:CIICR error");
		return;
	}
	AT_delay(400);
	u8t1 = AT_CIFSR(Lm2mstatus.localip);
	user_debug("i:AT_CIFSR:%s", Lm2mstatus.localip);
	if(u8t1)
	{
		if(u8t1 < 25)Lm2mstatus.localiplen = 25;
		Lm2mstatus.localiplen = u8t1;
	}
	else
	{
		Lm2mstatus.gprslink = 0;
		user_debug("i:m2m_gprslink:CIFSR error");
		return;
	}
	AT_delay(400);
	//连接服务器
	if(0 == AT_CIPSTART("39.108.229.40",9999))//"39.108.246.45",9999))//Lm2mstatus.seraddr, Lm2mstatus.port))
	{
		Lm2mstatus.gprslink = 0;
		user_debug("i:m2m_gprslink:CIPSTART error:%s[%d]","39.108.246.45",9999);//Lm2mstatus.seraddr,Lm2mstatus.port);
		return;
	}
	Lm2mstatus.gprslink = 1;
	Lm2mstatus.m2mstatus = 5;
	AT_delay(200);
	user_infor("e:m2m_gprslink OK");
}




u8 m2m_status_modemstart(void){
	return Lm2mstatus.modemstart;
}
u8 m2m_status_gprsbond(void){
	return Lm2mstatus.gprsbond;
}
u8 m2m_status_gprslink(void){
	return Lm2mstatus.gprslink;
}
u8 m2m_status(void){
	return Lm2mstatus.m2mstatus;
}

void m2m_statusSet(u8 status){
	Lm2mstatus.m2mstatus = status;
}
