/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-16 13:18
#
# Filename: coctx.h
#
# Description: 
#
=============================================================================*/
#ifndef __COCTX_H__
#define __COCTX_H__

struct coctx_t
{
	void *regs[ 14 ];	//保存14个寄存器的值
};

enum {
kRBP = 6,		//栈顶
kRDI = 7,		//第一个参数，存储
kRSI = 8,		//第二个参数
kRETAddr = 9,	//下一条指令
kRSP = 13,	//栈顶指针
};

/**
 * @brief 使用汇编语言coctx_swap.S，保存当前所有寄存器到第一个coctx，并转向新协程（将第二个协程保存的寄存器值赋值给相应的寄存器）
 * @a 切换协程
 * @b extern关键字
 * @cite https://zhuanlan.zhihu.com/p/123269132
 */
extern "C" {
extern void coctx_swap(coctx_t*, coctx_t*) asm("coctx_swap");
};
#endif

