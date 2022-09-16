/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-16 13:37
#
# Filename: coroutine.cpp
#
# Description: 
#
=============================================================================*/
#include "coroutine.h"
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <thread>
#include <iostream>

#define TEST

/// @brief c++11后，static保证只初始化一次
static thread_local CoroutineManager t_cm;

void wrapper(Coroutine* co) 
{
	Task task = co->getTask();
	if ( task )
	{
		task();
	}
/* 处理完任务后，将当前协程设置为可重用*/
	co->_isExecFunc = false;
	co->_isUsed = false;
	co->_status = CoStat::CO_READY;
/* 切回主协程*/
	Coroutine::yield();
}

CoroutineManager::CoroutineManager()
{
	if ( main_co == nullptr )
	{
		main_co = new Coroutine();
		cur_co = main_co;
		shared_stack = new char[SSIZE]();
	}
	else
	{
		std::cerr << __func__ << " error" << std::endl;
		::exit(EXIT_FAILURE);
	}
}

CoroutineManager::~CoroutineManager()
{
	if ( main_co )
	{
		if ( cur_co != nullptr && cur_co != main_co )
		{
			delete cur_co;
			cur_co = nullptr;
		}
		delete main_co;
		main_co = nullptr;
	}

	if ( shared_stack )
	{
		delete shared_stack;
		shared_stack = nullptr;
	}

	for ( auto it = co_pool.begin(); it != co_pool.end(); )
	{
		delete *it++;
	}
	co_pool.clear();
	std::cout << __func__ << std::endl;
}

Coroutine::Coroutine()
{
	::memset( &_ctx, 0, sizeof _ctx);
	// std::cout << __func__ << std::endl;
}

Coroutine::~Coroutine()
{
	if ( _stack )
	{
		delete _stack;
		_stack = nullptr;
	}

	_status = CoStat::CO_DEAD;
	/* ... */
}

Coroutine* Coroutine::getCoroutine()
{
	int cur = 0;
	for ( ; cur < t_cm.co_pool.size() && !t_cm.co_pool.empty(); ++cur)
	{
		if ( t_cm.co_pool[cur] != nullptr && !t_cm.co_pool[cur]->_isUsed 
				&& t_cm.co_pool[cur]->_status == CoStat::CO_READY && !t_cm.co_pool[cur]->_isExecFunc)
		{
			break;
		}
	}

	if ( cur >= t_cm.co_pool.size() )
	{
		for (int i = cur; i < cur + 18; ++i)
		{
			t_cm.co_pool.emplace_back( new Coroutine() );
			t_cm.co_pool.back()->_id = i;
		}
	}
	t_cm.co_pool[cur]->_isUsed = true;

	return t_cm.co_pool[cur];
}

/**
 * @brief 设置线程的任务，保证一个协程最多同时执行一个任务
 */
void Coroutine::setTask( const Task & task )
{
	if ( _isExecFunc )
	{
		std::cerr << "current coroutine already has a tack in execing" << std::endl;
		//::exit(EXIT_FAILURE);
	}
	_task = task;
	_isExecFunc = true;
}

bool Coroutine::isMainCoroutine()
{
	return t_cm.main_co == nullptr || t_cm.main_co == t_cm.cur_co;
}

void Coroutine::resume( Coroutine * co )
{
/* 协程为空 || 协程未被使用 || 协程无任务 || 当前运行协程不是主协程*/	
	if ( co == nullptr || co->_isUsed == false || co->_isExecFunc == false 
			|| !isMainCoroutine())
	{
		return;
	}

	if ( co->_status == CoStat::CO_READY )
	{
		co->coroutineMake();
	}
	else if ( co->_status == CoStat::CO_SUSPEND )
	{
		/* 保存被唤醒协程的栈*/
		::memcpy( t_cm.shared_stack + t_cm.SSIZE - co->_size, co->_stack, co->_size);
	}
	else 
	{
		assert(__assert_fail);
	}
	co->_status = CoStat::CO_RUNNING;
	t_cm.cur_co = co;
	coctx_swap(&t_cm.main_co->_ctx, &co->_ctx);
}

void Coroutine::yield()
{
	if ( isMainCoroutine() )
	{
		return;
	}

	Coroutine* co = t_cm.cur_co;
	if ( co == nullptr )
	{
		return;
	}

	t_cm.cur_co = t_cm.main_co;
	co->_status = CoStat::CO_SUSPEND;
	co->stackCopy(t_cm.shared_stack + t_cm.SSIZE);
	coctx_swap(&co->_ctx, &t_cm.main_co->_ctx);
}


/**
 * @brief 协程切换时，保存协程堆上栈内容
 * @param bottom为堆上栈的栈底，bottom = shared_stack + SSIZE
 */
void Coroutine::stackCopy( char* bottom )
{
	char top = 0;							//局部变量存在于栈中，top处于堆上栈的栈顶
	assert( static_cast<int>( bottom - &top ) <= t_cm.SSIZE);

/* 防止共享栈的内容超过当前协程的“栈”容量，需要重新分配*/	
	if (_cap < bottom - &top )
	{
		if ( _stack != t_cm.shared_stack )
		{
			delete _stack;
		}
		_stack = nullptr;
		_cap = bottom - &top;
		_stack = new char[_cap]();
	}

	_size = _cap;
	::memcpy(_stack, &top, _size);
}

/**
 * @brief 唤醒处于READY状态的协程时，使用coctx结构体记录栈和执行函数（及参数）；栈为共享栈shared_stack
 */
void Coroutine::coroutineMake()
{
	_stack = t_cm.shared_stack;
	_size = t_cm.SSIZE;
	char* bottom = _stack + _size;						//栈底
	bottom = reinterpret_cast<char*>( ( reinterpret_cast<unsigned long>( bottom) ) & -16LL );										//字节对齐
	
	::memset( &_ctx, 0 , sizeof &_ctx );
	_ctx.regs[kRSP] = bottom;
	_ctx.regs[kRBP] = bottom;
	_ctx.regs[kRETAddr] = reinterpret_cast<char*>( wrapper );
	_ctx.regs[kRDI] = reinterpret_cast<char*>( this );
}



/*===========================================test===========================================================*/
#ifdef TEST

void foo(int a)
{
    for (int i = a; i < a + 5; ++i)
    {
        std::cout << __func__ << " " << i << std::endl;
        Coroutine::yield();
    }
}

int main()
{
	Coroutine* c1 = Coroutine::getCoroutine();
    Coroutine* c2 = Coroutine::getCoroutine();
    
    c1->setTask( std::bind(foo, 0) ) ;
    c2->setTask( std::bind(foo, 5) );

    while(c1->_isExecFunc && c2->_isExecFunc)
    {
        Coroutine::resume(c1);
        Coroutine::resume(c2);
    }

	return 0;
}

#endif