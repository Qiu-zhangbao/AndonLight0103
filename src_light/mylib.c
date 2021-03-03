//******************************************************************************
//*
//* 文 件 名 : 
//* 文件描述 :        
//* 作    者 : 张威/Andon Health CO.Ltd
//* 版    本 : V0.0
//* 日    期 : 
//*
//* 更新历史 : 
//*     日期       作者    版本     描述
//*         
//*          
//******************************************************************************

///*****************************************************************************
///*                         包含文件说明
///*****************************************************************************
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

///*****************************************************************************
///*                         宏定义区
///*****************************************************************************

///*****************************************************************************
///*                         Strcut结构变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         函数声明区
///*****************************************************************************

///*****************************************************************************
///*                         常量定义区
///*****************************************************************************

///*****************************************************************************
///*                         外部全局变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         文件内部全局变量定义区
///*****************************************************************************

///*****************************************************************************
///*                         函数实现区
///*****************************************************************************
//*****************************************************************************
// 函数名称: mylib_sprintf
// 函数描述: 
// 函数输入:  
// 函数返回值: 
//*****************************************************************************/
void mylib_sprintf(char *str, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vsprintf(str, format, args);
    va_end(args);
}

/*******************************************************************************
* Function Name  : HexStr2Int8U
* Description    : 把十六进制格式字符串转换成十进制数
* Input          : p:字符串指针  i:转换长度，填入数据时包含负号的长度
* Output         : None
* Return         : 十进制数
*******************************************************************************/
uint8_t HexStr2Int8U(uint8_t *p)
{
	uint8_t bb = 0;
	uint8_t pp[2];
	uint8_t j;

	pp[0] = p[0];
	pp[1] = p[1];

	for(j=0; j<2; j++)
	{
		switch(pp[j])
		{
			case '0': case '1': case '2': case '3': case '4': 
			case '5': case '6': case '7': case '8': case '9': 
				bb |= (pp[j] - '0') << (4 * (2 - j - 1));
				break;
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
				bb |= (pp[j] - 'a' + 10) << (4 * (2 - j - 1));
				break;

			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				bb |= (pp[j] - 'A' + 10) << (4 * (2 - j - 1));
				break;
		}
	}

	return(bb);
}


/*******************************************************************************
* Function Name  : abs
* Description    : 计算两个数的差值的绝对值
* Input          : 
* Output         : 
* Return         : 
*******************************************************************************/
uint32_t mylib_abs(uint32_t a, uint32_t b){
	if(a>b){
		return a-b;
	}else{
		return b-a;
	}
}
