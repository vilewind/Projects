/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-16 13:19
#
# Filename: coroutine.h
#
# Description: 
#
=============================================================================*/
#ifndef __COROUTINE_H__
#define __COROUTINE_H__
#include "coctx.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

class Coroutine;

/// @brief 协程的运行状态
enum class CoStat
{
	CO_READY = 0,
	CO_SUSPEND,
	CO_RUNNING,
	CO_DEAD
};

/**
 * @brief 协程自愿管理器，管理主从模式下的main_co和共享栈
 */
struct CoroutineManager
{
	Coroutine* main_co;					
	Coroutine* cur_co;
	char* shared_stack;
	std::vector<Coroutine*> co_pool;
	const static int SSIZE = 1024*512;

	CoroutineManager();
	~CoroutineManager();
};

using Task = std::function<void()>;

class Coroutine
{
public:
	Coroutine();
	~Coroutine();

	void setTask( const Task& );
	Task getTask() { return  _task; }

	static void resume( Coroutine* );
	static void yield();

	static bool isMainCoroutine();
	static Coroutine* getCoroutine();
private:
	void coroutineMake();
	void stackCopy( char* bottom );

public:
	coctx_t _ctx;		
	char* _stack { nullptr };
	ptrdiff_t _cap { 0 };	
	ptrdiff_t _size { 0 };
	bool _isUsed { false };							//标记是否被使用，便于协程重用
	bool _isExecFunc { false };						//标记正在执行任务，保证一个协程处理一个任务
	CoStat _status { CoStat::CO_READY };
	int _id { -1 };								//协程在协程池中的标记
private:
	Task _task;
};

#endif
