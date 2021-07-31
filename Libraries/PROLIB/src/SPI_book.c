#include "SPI_book.h"
#include "Systick_book.h"


static __IO  uint32_t SPITimeout = FLASH_SPI_LONG_TIMEOUT;


/**
  * @brief  SPII/O配置
  * @param  无
  * @retval 无
  */
static void SPI_GPIO_Config(void){
  GPIO_InitTypeDef  GPIO_InitStructure;
  //使能与SPI 有关的时钟
  _FLASH_SPI_APBxClock_FUN(_FLASH_SPI_CLK , ENABLE);
  _FLASH_SPI_GPIO_APBxClock_FUN(_FLASH_SPI_GPIO_CLK , ENABLE);
  
  //MISO MOSI SCK
  GPIO_InitStructure.GPIO_Pin = _FLASH_SPI_SCL_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  
  GPIO_Init(_FLASH_SPI_SCL_PORT,&GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin = _FLASH_SPI_MISO_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  
  GPIO_Init(_FLASH_SPI_SCL_PORT,&GPIO_InitStructure);
    
  GPIO_InitStructure.GPIO_Pin = _FLASH_SPI_MOSI_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  
  GPIO_Init(_FLASH_SPI_SCL_PORT,&GPIO_InitStructure);
  
  //初始化CSS引脚，使能软件控制，所以直接设置为推挽输出
  GPIO_InitStructure.GPIO_Pin = _FLASH_SPI_CSS_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  
  GPIO_Init(_FLASH_SPI_SCL_PORT,&GPIO_InitStructure);
  
  _FLASH_CSS_HIGH();
}



/**
  * @brief  static void SPI_Mode_Config(void) 配置
  * @param  无
  * @retval 无
  */
static void SPI_Mode_Config(void){
  SPI_InitTypeDef  SPI_InitStructure;
  
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2 ; //波特率预分频值为 2
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge ;        //数据捕获于第二个时钟沿 
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;          //时钟悬空高
  SPI_InitStructure.SPI_CRCPolynomial = 0;             //不使用CRC功能，数值随便写
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;    //SPI 发送接收 8 位帧结构
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex ; //双线全双工
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;   //数据传输从 MSB 位开始
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master ;       //设置为主 SPI
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;            //内部 NSS 信号有 SSI 位控制
  
  SPI_Init(_FLASH_SPIx , &SPI_InitStructure ); //写入配置到寄存器
  SPI_Cmd(_FLASH_SPIx , ENABLE);   //使能SPI
}

/**
  * @brief  void SPI_FLASH_Init(void) 初始化
  * @param  无
  * @retval 无
  */
void SPI_FLASH_Init(void){
  SPI_GPIO_Config();
  SPI_Mode_Config();
}


/**
  * @brief  Basic management of the timeout situation.
  * @param  errorCode：错误代码，可以用来定位是哪个环节出错.
  * @retval 返回0，表示SPI读取失败.
  */
static  uint32_t SPI_TIMEOUT_UserCallback(uint8_t errorCode)
{
  /* Block communication and all processes */
  FLASH_ERROR("SPI 等待超时!errorCode = %d",errorCode);
  
  return 0;
}
/**
  * @brief  uint8_t SPI_FLASH_Send_Byte(uint8_t data) 初始化
  * @param  发送并且接收一个字节
  * @retval 无
  */
static uint8_t SPI_FLASH_Send_Byte(uint8_t data){
  SPITimeout = FLASH_SPI_TIMEOUT;
  //检查并等待至TX缓冲区
  while(SPI_I2S_GetFlagStatus(_FLASH_SPIx,SPI_I2S_FLAG_TXE) == RESET){//发送缓存空标志位
    if(SPITimeout--==0) {return SPI_TIMEOUT_UserCallback(0);}
  }
  //判断程序已经为空
  SPI_I2S_SendData(_FLASH_SPIx , data);
  //判断接受缓存非空 
  SPITimeout = FLASH_SPI_TIMEOUT;
  while(SPI_I2S_GetFlagStatus(_FLASH_SPIx,SPI_I2S_FLAG_RXNE) == RESET){//接受缓存非空标志位标志位
    if(SPITimeout--==0) {return SPI_TIMEOUT_UserCallback(0);}
  }
  //程序发送完毕.并且需要接收一个字节
  return SPI_I2S_ReceiveData(_FLASH_SPIx);
}




/**
  * @brief  uint32_t SPI_Read_ID(void)
  * @param  读取ID号
  * @retval  
  */
uint32_t SPI_Read_ID(void){
  uint32_t flash_id;
  //片选使能
  _FLASH_CSS_LOW();
  SPI_FLASH_Send_Byte(FLASH_SPI_READ_JEDEC_ID);
  flash_id = SPI_FLASH_Send_Byte(FLASH_SPI_DUMMY);//Memory typeID
  flash_id<<=8;
  flash_id|=SPI_FLASH_Send_Byte(FLASH_SPI_DUMMY);//Capcity typeID
  flash_id<<=8;
  flash_id|=SPI_FLASH_Send_Byte(FLASH_SPI_DUMMY);//Capcity typeID
  _FLASH_CSS_HIGH();
  return flash_id;
}


/**
  * @brief  void SPI_Write_Enable(void)
  * @param  写入使能
  * @retval  
  */
static void SPI_Write_Enable(void){
  //片选使能
  _FLASH_CSS_LOW();
  SPI_FLASH_Send_Byte(FLASH_SPI_WRITE_ENABLE);
  _FLASH_CSS_HIGH();  
}

/**
  * @brief  static void SPI_WaitForWriteEnd(void);
  * @param  //等待FLASH内部时序操作完成
  * @retval  
  */
void SPI_WaitForWriteEnd(void){
  uint8_t status_reg = 0;  //判断最低位S0 erse or write in progress
  // 片选指令
  _FLASH_CSS_LOW();
  SPI_FLASH_Send_Byte(FLASH_SPI_READ_STATUS);
  do{
    status_reg = SPI_FLASH_Send_Byte(FLASH_SPI_DUMMY); //想要读取数据需要继续发送
  }while((status_reg & 0x01)==1);  //校验最低位进行校验
  _FLASH_CSS_HIGH(); 
 
}
  
/** 
  * @brief  svoid SPI_Erase_Sector(uint32_t addr)
  * @param  擦除FLASH指定扇区
  * @retval  
  */
void SPI_Erase_Sector(uint32_t addr){
  SPI_Write_Enable();
  //片选使能
  _FLASH_CSS_LOW();
  SPI_FLASH_Send_Byte(FLASH_SPI_REASE_SECTOR);
  SPI_FLASH_Send_Byte((addr>>16)&0xff);
  SPI_FLASH_Send_Byte((addr>>8)&0xff);
  SPI_FLASH_Send_Byte(addr&0xff);
  _FLASH_CSS_HIGH(); 
  SPI_WaitForWriteEnd();
}
  

  
/** 
  * @brief  void SPI_Read_Data(uint8_t *readBuffer , uint32_t addr ,uint32_t  numByteToRead ));
  * @param  读取FLASH的内容
  * @retval  
  */
void SPI_Read_Data(uint8_t *readBuffer , uint32_t addr ,uint32_t  numByteToRead ){
  //片选使能
  _FLASH_CSS_LOW();
  //发送地址
  SPI_FLASH_Send_Byte(FLASH_SPI_READ_DATA);
  SPI_FLASH_Send_Byte((addr>>16)&0xff);
  SPI_FLASH_Send_Byte((addr>>8)&0xff);
  SPI_FLASH_Send_Byte(addr&0xff);
  
  while(numByteToRead--){
    *readBuffer = SPI_FLASH_Send_Byte(FLASH_SPI_DUMMY);
    readBuffer++;
  }
  _FLASH_CSS_HIGH();
}


/** 
  * @brief  void SPI_Write_Data(uint8_t *readBuffer , uint32_t addr ,uint32_t  numByteToRead ));
  * @param  读取FLASH的内容
  * @retval  
  */
void SPI_Write_Data(uint8_t *writeBuffer , uint32_t addr ,uint32_t  numByteToRead ){
  SPI_Write_Enable();
  //片选使能
  _FLASH_CSS_LOW();
  SPI_FLASH_Send_Byte(FLASH_SPI_WRITE_DATA);
  SPI_FLASH_Send_Byte((addr>>16)&0xff);
  SPI_FLASH_Send_Byte((addr>>8)&0xff);
  SPI_FLASH_Send_Byte(addr&0xff);
  
  while(numByteToRead--){
    SPI_FLASH_Send_Byte(*writeBuffer);
    writeBuffer++;
  }
  _FLASH_CSS_HIGH();
  SPI_WaitForWriteEnd();
}


/** 
  * @brief  void SPI_Show_Data(uint8_t *readBuffer);
  * @param  读取FLASH的内容
  * @retval  
  */
void SPI_Show_Data(uint8_t *readBuffer , uint32_t  numByteToRead){
  uint32_t i;
  for(i=0 ;i<numByteToRead ;i++ ){
    if(i%10 == 0){
      printf("\r\n ");
    }
     printf("0x%x ",readBuffer[i]);
  }
}

 